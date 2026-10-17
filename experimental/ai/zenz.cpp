// SPDX-License-Identifier: GPL-3.0-or-later
#include "llama.h"
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
int main(int argc,char** argv) {
    if(argc!=2) return 1;
    llama_log_set([](ggml_log_level,const char*,void*){},nullptr);
    llama_backend_init();
    auto mp=llama_model_default_params();mp.n_gpu_layers=99;
    llama_model* model=llama_model_load_from_file(argv[1],mp);
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
            auto prefix=tokenize(vocab,request.at("prompt").get<std::string>());
            if(prefix.empty() || prefix.size()>256)throw std::runtime_error("prompt length");
            std::vector<json> ranked;
            auto choices=request.at("choices").get<std::vector<std::string>>();
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
            std::cout<<json({{"ranked",ranked}}).dump()<<std::endl;
        }catch(const std::exception&){std::cout<<json({{"error","Zenz evaluation failed"}}).dump()<<std::endl;}
    }
    llama_free(ctx);llama_model_free(model);llama_backend_free();
}
