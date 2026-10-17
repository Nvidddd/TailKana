// SPDX-License-Identifier: GPL-3.0-or-later
#include "ai_worker.h"
#include <nlohmann/json.hpp>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <sys/wait.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <string_view>
#include <vector>
extern char** environ;
namespace tailkana {
using Clock=std::chrono::steady_clock;
std::string contextBefore(const std::string& text,size_t cursor,size_t characters) {
    if(!characters)return {};
    std::vector<size_t> boundaries;
    for(size_t i=0;i<text.size();++i)if((static_cast<unsigned char>(text[i])&0xc0)!=0x80)boundaries.push_back(i);
    boundaries.push_back(text.size());cursor=std::min(cursor,boundaries.size()-1);
    return text.substr(boundaries[cursor>characters?cursor-characters:0],boundaries[cursor]-boundaries[cursor>characters?cursor-characters:0]);
}
std::string utf8Tail(const std::string& text,size_t characters){return contextBefore(text,text.size(),characters);}
struct AiWorker::Impl {
    Callback callback;
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::optional<AiRequest> latest;
    Clock::time_point due;
    std::atomic<bool> stopping=false,enabled=false;
    std::string state="off";
    bool failed=false;
    pid_t child=-1;
    int input=-1,output=-1;
    std::string received;
    std::thread thread;
    explicit Impl(Callback cb):callback(std::move(cb)),thread([this]{loop();}){}
    ~Impl(){stopping=true;changed.notify_all();thread.join();}
    void setState(std::string value){std::lock_guard lock(mutex);state=std::move(value);}
    void closeChild() {
        if(input>=0){close(input);input=-1;}if(output>=0){close(output);output=-1;}
        if(child>0){
            kill(child,SIGTERM);
            for(int i=0;i<30;++i){if(waitpid(child,nullptr,WNOHANG)!=0){child=-1;break;}std::this_thread::sleep_for(std::chrono::milliseconds(5));}
            if(child>0){kill(child,SIGKILL);while(waitpid(child,nullptr,0)<0 && errno==EINTR){}child=-1;}
        }
        received.clear();
    }
    void startChild(){
        std::filesystem::path helper,model,profile;
        if(const auto* custom=std::getenv("TAILKANA_AI_HOME")){
            auto home=std::filesystem::path(custom);helper=home/"bin/TailKanaAI";model=home/"zenz.gguf";profile=home/"mozc-profile";
        }else{
            uint32_t length=0;_NSGetExecutablePath(nullptr,&length);std::string path(length,'\0');_NSGetExecutablePath(path.data(),&length);
            auto contents=std::filesystem::path(path.c_str()).parent_path().parent_path();
            helper=contents/"bin/TailKanaAI";model=contents/"share/tailkana/ai/zenz.gguf";
            const char* home=std::getenv("HOME");profile=std::filesystem::path(home?home:"/tmp")/"Library/Application Support/TailKana/AI/Mozc";
        }
        if(!std::filesystem::is_regular_file(helper)||!std::filesystem::is_regular_file(model))throw std::runtime_error("missing assets");
        int in[2],out[2];if(pipe(in)!=0)throw std::runtime_error("pipe");
        if(pipe(out)!=0){close(in[0]);close(in[1]);throw std::runtime_error("pipe");}
        for(int fd:{in[0],in[1],out[0],out[1]})fcntl(fd,F_SETFD,FD_CLOEXEC);
        // Writing a pipe whose child crashed must never terminate the IME.
        fcntl(in[1],F_SETNOSIGPIPE,1);
        posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions,in[0],STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions,out[1],STDOUT_FILENO);
        posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
        posix_spawnattr_t attributes;posix_spawnattr_init(&attributes);
        posix_spawnattr_setflags(&attributes,POSIX_SPAWN_CLOEXEC_DEFAULT);
        std::vector<std::string> env;
        for(char** e=environ;*e;++e)if(std::string_view(*e).find("TAILKANA_MOZC_PROFILE=")!=0)env.emplace_back(*e);
        env.push_back("TAILKANA_MOZC_PROFILE="+profile.string());
        std::vector<char*> envp;for(auto& e:env)envp.push_back(e.data());envp.push_back(nullptr);
        std::string executable=helper.string(),weights=model.string();char* args[]={executable.data(),weights.data(),nullptr};
        int result=posix_spawn(&child,executable.c_str(),&actions,&attributes,args,envp.data());
        posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attributes);close(in[0]);close(out[1]);
        if(result!=0){close(in[1]);close(out[0]);child=-1;throw std::runtime_error("spawn");}
        input=in[1];output=out[0];fcntl(input,F_SETFL,O_NONBLOCK);
    }
    nlohmann::json rpc(const nlohmann::json& request){
        std::string message=request.dump()+'\n';size_t sent=0;
        const auto deadline=Clock::now()+std::chrono::seconds(20);
        while(true){
            if(stopping||!enabled||Clock::now()>deadline)throw std::runtime_error("cancelled or timed out");
            if(sent<message.size()){
                pollfd fd{input,POLLOUT,0};int ready=poll(&fd,1,30);
                if(ready<0 && errno==EINTR)continue;
                if(ready>0){auto count=write(input,message.data()+sent,message.size()-sent);if(count>0)sent+=count;else if(errno!=EAGAIN&&errno!=EINTR)throw std::runtime_error("write");}
                continue;
            }
            auto newline=received.find('\n');
            if(newline!=std::string::npos){auto line=received.substr(0,newline);received.erase(0,newline+1);return nlohmann::json::parse(line);}
            pollfd fd{output,POLLIN,0};int ready=poll(&fd,1,30);
            if(ready<0 && errno==EINTR)continue;
            if(ready<0)throw std::runtime_error("poll");
            if(ready>0){char buffer[8192];auto size=read(output,buffer,sizeof(buffer));if(size<=0)throw std::runtime_error("child exited");received.append(buffer,size);if(received.size()>1024*1024)throw std::runtime_error("oversized reply");}
        }
    }
    void loop(){
        while(!stopping){
            {
                std::unique_lock lock(mutex);
                changed.wait(lock,[&]{return stopping||enabled;});
                if(stopping)break;
                if(failed){changed.wait(lock,[&]{return stopping||!enabled||!failed;});continue;}
            }
            try{
                if(child<0){setState("loading");startChild();auto warm=rpc({{"warm",true}});if(warm.contains("error"))throw std::runtime_error("warm failed");setState("ready");callback({{},"","","ready",0});}
                std::optional<AiRequest> job;
                {
                    std::unique_lock lock(mutex);
                    changed.wait(lock,[&]{return stopping||!enabled||latest.has_value();});
                    if(stopping||!enabled){lock.unlock();closeChild();setState("off");continue;}
                    if(Clock::now()<due){auto wake=due;changed.wait_until(lock,wake);continue;}
                    job=std::move(latest);latest.reset();
                }
                auto begin=Clock::now();
                auto result=rpc({{"raw",job->raw},{"context",job->context},{"max_length",job->maxLength},{"katakana",job->katakana}});
                if(result.contains("error"))throw std::runtime_error("inference failed");
                AiResult reply{*job,result.value("text",""),result.value("reading",""),result.value("status","ready"),int(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-begin).count())};
                callback(std::move(reply));
            }catch(const std::exception&){
                closeChild();
                if(enabled&&!stopping){std::lock_guard lock(mutex);state="unavailable";failed=true;latest.reset();}
                else setState("off");
                callback({{},"","",enabled?"unavailable":"off",0});
            }
        }
        closeChild();
    }
};
AiWorker::AiWorker(Callback cb):impl_(std::make_unique<Impl>(std::move(cb))){}
AiWorker::~AiWorker()=default;
void AiWorker::enable(bool value){
    {std::lock_guard lock(impl_->mutex);if(value!=impl_->enabled){impl_->enabled=value;impl_->failed=false;impl_->latest.reset();impl_->state=value?"loading":"off";}}
    impl_->changed.notify_all();
}
void AiWorker::submit(AiRequest request){
    {std::lock_guard lock(impl_->mutex);if(!impl_->enabled||impl_->failed)return;impl_->latest=std::move(request);impl_->due=Clock::now()+std::chrono::milliseconds(16);}
    impl_->changed.notify_all();
}
void AiWorker::cancel(const std::array<uint8_t,16>& uuid){std::lock_guard lock(impl_->mutex);if(impl_->latest&&impl_->latest->uuid==uuid)impl_->latest.reset();}
std::string AiWorker::status()const{std::lock_guard lock(impl_->mutex);return impl_->state;}
}
