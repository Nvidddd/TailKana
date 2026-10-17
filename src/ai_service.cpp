// SPDX-License-Identifier: GPL-3.0-or-later
#include "llama.h"
#include "mozc_backend.h"
#include "base/init_mozc.h"
#include "base/strings/japanese.h"
#include <chrono>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using json=nlohmann::json;

static std::vector<llama_token> tokenize(const llama_vocab* vocab, const std::string& text) {
    std::vector<llama_token> tokens(text.size()+16);
    int count=llama_tokenize(vocab,text.data(),text.size(),tokens.data(),tokens.size(),false,false);
    if(count<0) throw std::runtime_error("tokenize failed");
    tokens.resize(count);
    return tokens;
}
static std::vector<size_t> boundaries(const std::string& text) {
    std::vector<size_t> result;
    for(size_t i=0;i<text.size();++i)if((static_cast<unsigned char>(text[i])&0xc0)!=0x80)result.push_back(i);
    result.push_back(text.size());return result;
}
static std::string tail(const std::string& text,size_t count){auto b=boundaries(text);return text.substr(b[b.size()>count+1?b.size()-count-1:0]);}
static std::string normalized(std::string text){
    size_t i=0;while((i=text.find(' ',i))!=std::string::npos){text.replace(i,1,"　");i+=3;}
    text.erase(std::remove(text.begin(),text.end(),'\n'),text.end());
    text.erase(std::remove(text.begin(),text.end(),'\r'),text.end());return text;
}
int main(int argc,char** argv) {
    if(argc!=2) return 1;
    const std::string modelPath=argv[1];
    mozc::InitMozc(argv[0],&argc,&argv);
    tailkana::MozcBackend backend;
    llama_log_set([](ggml_log_level,const char*,void*){},nullptr);
    llama_backend_init();
    auto mp=llama_model_default_params();mp.n_gpu_layers=99;
    llama_model* model=llama_model_load_from_file(modelPath.c_str(),mp);
    if(!model) {std::cerr<<"Zenz model load failed\n";return 2;}
    auto cp=llama_context_default_params();cp.n_ctx=2560;cp.n_batch=512;cp.n_ubatch=512;cp.n_seq_max=5;
    cp.n_threads=4;cp.n_threads_batch=4;cp.no_perf=true;
    llama_context* ctx=llama_init_from_model(model,cp);
    if(!ctx){llama_model_free(model);return 3;}
    const auto* vocab=llama_model_get_vocab(model);
    const int vocabulary=llama_vocab_n_tokens(vocab);
    // The distributed GGUF metadata says EOS=2, but its token list explicitly
    // labels token 2 '<s>' and token 3 '</s>'. Resolve the actual end token.
    int eos=llama_vocab_eos(vocab);
    for(int i=0;i<std::min(8,vocabulary);++i) {
        const char* text=llama_vocab_get_text(vocab,i);
        if(text && std::string(text)=="</s>") eos=i;
    }
    std::string line;
    while(std::getline(std::cin,line)) {
        try {
            if(line.size()>32768) throw std::runtime_error("request too large");
            auto request=json::parse(line);
            const auto begin=std::chrono::steady_clock::now();
            const bool warm=request.value("warm",false);
            std::string reading,context=request.value("context","");
            std::vector<std::string> choices;
            if(warm){reading="あ";choices={"あ"};}
            else {
                const auto raw=request.value("raw","");
                const int limit=std::clamp(request.value("max_length",32),1,128);
                if(raw.empty()||raw.size()>96||raw.find_first_not_of("abcdefghijklmnopqrstuvwxyz'-")!=std::string::npos)throw std::runtime_error("invalid roman");
                if(!backend.query(raw,true,request.value("katakana",false),false))throw std::runtime_error("conversion failed");
                reading=backend.composingReading;
                size_t n=0;while((n=reading.find("ｎ",n))!=std::string::npos){reading.replace(n,std::string("ｎ").size(),"ん");n+=std::string("ん").size();}
                bool incomplete=false;
                for(char c:reading)if((c>='a'&&c<='z')||(c>='A'&&c<='Z'))incomplete=true;
                for(const auto* c:{"ａ","ｂ","ｃ","ｄ","ｅ","ｆ","ｇ","ｈ","ｉ","ｊ","ｋ","ｌ","ｍ","ｎ","ｏ","ｐ","ｑ","ｒ","ｓ","ｔ","ｕ","ｖ","ｗ","ｘ","ｙ","ｚ"})if(reading.find(c)!=std::string::npos)incomplete=true;
                if(incomplete){std::cout<<json({{"status","incomplete"}}).dump()<<std::endl;continue;}
                for(int i=0;i<std::min(24,int(backend.candidates.size()))&&choices.size()<12;++i){
                    if(!backend.highlight(i))continue;const auto& text=backend.reading;
                    if(!text.empty()&&boundaries(text).size()-1<=size_t(limit)&&std::find(choices.begin(),choices.end(),text)==choices.end())choices.push_back(text);
                }
                if(choices.empty()){std::cout<<json({{"status","length"}}).dump()<<std::endl;continue;}
            }
            context=tail(context,128);
            auto makePrompt=[&]{return (context.empty()?std::string{}:std::string("\xee\xb8\x82")+normalized(context))+"\xee\xb8\x80"+normalized(mozc::japanese::HiraganaToKatakana(reading))+"\xee\xb8\x81";};
            auto prefix=tokenize(vocab,makePrompt());
            while(prefix.size()>256&&!context.empty()){context=tail(context,boundaries(context).size()-2);prefix=tokenize(vocab,makePrompt());}
            if(prefix.empty() || prefix.size()>256)throw std::runtime_error("prompt length");
            std::vector<json> ranked;
            if(choices.size()>12) throw std::runtime_error("candidate limit");
            // Decode the shared prompt once. Four independent candidate paths
            // share those KV cells, then score together in each GPU batch.
            llama_kv_cache_clear(ctx);
            auto promptBatch=llama_batch_init(prefix.size(),0,1);
            promptBatch.n_tokens=prefix.size();
            for(int i=0;i<int(prefix.size());++i){
                promptBatch.token[i]=prefix[i];promptBatch.pos[i]=i;
                promptBatch.n_seq_id[i]=1;promptBatch.seq_id[i][0]=0;
                promptBatch.logits[i]=(i==int(prefix.size())-1);
            }
            int promptStatus=llama_decode(ctx,promptBatch);llama_batch_free(promptBatch);
            if(promptStatus)throw std::runtime_error("prompt decode failed");
            const float* initialLogits=llama_get_logits_ith(ctx,-1);
            if(!initialLogits)throw std::runtime_error("missing prompt logits");
            std::vector<float> firstLogits(initialLogits,initialLogits+vocabulary);
            auto logProbability=[&](const float* logits,llama_token target){
                const double maximum=*std::max_element(logits,logits+vocabulary);
                double sum=0;for(int j=0;j<vocabulary;++j)sum+=std::exp(logits[j]-maximum);
                return logits[target]-maximum-std::log(sum);
            };
            struct Path {std::string text;std::vector<llama_token> output;int offset;};
            size_t next=0;
            while(next<choices.size()) {
                for(int seq=1;seq<=4;++seq)llama_kv_cache_seq_rm(ctx,seq,-1,-1);
                auto batch=llama_batch_init(512,0,1);batch.n_tokens=0;
                std::vector<Path> paths;
                while(next<choices.size() && paths.size()<4){
                    const auto& text=choices[next];auto output=tokenize(vocab,text);
                    if(output.empty() || prefix.size()+output.size()+1>512){++next;continue;}
                    if(batch.n_tokens+output.size()>512)break;
                    const int seq=paths.size()+1;
                    llama_kv_cache_seq_cp(ctx,0,seq,0,prefix.size());
                    const int offset=batch.n_tokens;
                    for(int i=0;i<int(output.size());++i){
                        const int j=batch.n_tokens++;
                        batch.token[j]=output[i];batch.pos[j]=prefix.size()+i;
                        batch.n_seq_id[j]=1;batch.seq_id[j][0]=seq;batch.logits[j]=1;
                    }
                    paths.push_back({text,std::move(output),offset});++next;
                }
                if(paths.empty()){llama_batch_free(batch);continue;}
                int decodeStatus=llama_decode(ctx,batch);llama_batch_free(batch);
                if(decodeStatus)throw std::runtime_error("candidate decode failed");
                for(const auto& path:paths){
                    double score=logProbability(firstLogits.data(),path.output[0]);
                    for(int i=0;i<int(path.output.size());++i){
                        const float* logits=llama_get_logits_ith(ctx,path.offset+i);
                        if(!logits)throw std::runtime_error("missing candidate logits");
                        score+=logProbability(logits,i+1<int(path.output.size())?path.output[i+1]:eos);
                    }
                    ranked.push_back({{"text",path.text},{"score",score}});
                }
            }
            std::stable_sort(ranked.begin(),ranked.end(),[](const json&a,const json&b){return a["score"].get<double>()>b["score"].get<double>();});
            backend.clear();
            if(warm){std::cout<<json({{"status","ready"}}).dump()<<std::endl;continue;}
            const auto milliseconds=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
            std::cout<<json({{"status","ready"},{"text",ranked.empty()?std::string{}:ranked.front()["text"].get<std::string>()},{"reading",reading},{"elapsed_ms",milliseconds}}).dump()<<std::endl;
        }catch(const std::exception&){std::cout<<json({{"error","Zenz evaluation failed"}}).dump()<<std::endl;}
    }
    llama_free(ctx);llama_model_free(model);llama_backend_free();
}
