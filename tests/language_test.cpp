#include "language_support.h"
#include <chrono>
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <vector>
static void check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
int main() {
    using clock=std::chrono::steady_clock;
    const auto begin=clock::now();
    check(tailkana::chineseCandidate("nihao")=="你好","real dictionary: nihao");
    const double cold=std::chrono::duration<double,std::milli>(clock::now()-begin).count();
    check(tailkana::chineseCandidate("woaini")=="我爱你","real phrase composition: woaini");
    check(tailkana::chineseCandidate("zhonghuarenmingongheguo")=="中华人民共和国","real language model: long phrase");
    check(tailkana::chineseCandidate("abc/123").empty(),"invalid pinyin must not produce a Chinese candidate");
    check(tailkana::chineseDefinition("日本語","にほんご").text.find("日语")!=std::string::npos,"Japanese→Chinese definition");
    const auto japanese=tailkana::chineseDefinition("日本語","にほんご");
    check(japanese.source=="日本語" && japanese.text.find("日語")==std::string::npos,"simplify Chinese meanings without changing Japanese source spelling");
    const auto school=tailkana::chineseDefinition("学校","がっこう");
    check(school.text.find("学校")!=std::string::npos && school.text.find("學校")==std::string::npos,"whole-word meanings must use Simplified Chinese");
    check(tailkana::chineseDefinition("血","ち").text.find("血液")!=std::string::npos,"reading-specific definition");
    check(tailkana::chineseDefinition("食べる","たべる").text.find("吃")!=std::string::npos,"whole verb must retain okurigana");
    check(tailkana::chineseDefinition("食べました","たべました").text.find("吃")!=std::string::npos,"inflected verb must resolve to its lemma");
    check(!tailkana::chineseDefinition("時間","じかん").text.empty(),"common multi-kanji noun");
    check(!tailkana::chineseDefinition("日本","にほん").text.empty(),"common whole word from merged dictionary");
    check(tailkana::chineseDefinition("ありがとう","ありがとう").text.find("谢谢")!=std::string::npos,"whole kana word");
    check(!tailkana::chineseDefinition("食べる","unexpected-reading").text.empty(),"exact written word must not disappear because reading differs");
    check(tailkana::chineseDefinition("unrecorded-gibberish","").text.empty(),"unknown words must remain unknown");
    std::vector<double> times;
    const std::string text="woxiangxuexiriyu";
    for(int round=0;round<3;++round)for(size_t i=1;i<=text.size();++i) {
        const auto start=clock::now();tailkana::chineseCandidate(text.substr(0,i));
        times.push_back(std::chrono::duration<double,std::milli>(clock::now()-start).count());
    }
    std::sort(times.begin(),times.end());
    std::cout<<"{\"passed\":true,\"pinyinColdMs\":"<<cold<<",\"typingP95Ms\":"<<times[times.size()*95/100]<<"}\n";
}
