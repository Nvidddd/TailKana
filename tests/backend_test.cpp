#include "mozc_backend.h"
#include "base/init_mozc.h"
#include <cstdlib>
#include <iostream>
void check(bool v,const char* s){if(!v){std::cerr<<s<<'\n';std::exit(1);}}
int main(int argc,char** argv){mozc::InitMozc(argv[0],&argc,&argv);tailkana::MozcBackend m;
for(const auto* roman:{"h","hash","hashi","kikan","nihongo","taberu","watasihanihongowobenkyoushiteimasu","ashitahatoukyounoatarashiikaishadehatarakimasu"}) {
    check(m.query(roman,true,false,true),"prediction replay reference");
    const auto reference=m.candidates;
    const auto reading=m.composingReading;
    check(m.query(roman,true,false,false),"conversion-only replay");
    check(m.composingReading==reading,"conversion-only replay must preserve reading");
    check(m.candidates.size()==reference.size(),"conversion-only replay candidate count");
    for(size_t i=0;i<reference.size();++i) {
        check(m.candidates[i].text==reference[i].text,"conversion-only replay must preserve candidate order");
        check(m.candidates[i].reading==reference[i].reading,"conversion-only replay candidate reading");
    }
}
for(bool converted:{false,true})for(bool katakana:{false,true})for(const auto* roman:{"h","has","hashi","kikan","nihongo","arigatpu","watasihanihongowobenkyoushiteimasu","ashitahatoukyounoatarashiikaishadehatarakimasu"}) {
    check(m.query(roman,converted,katakana,true),"reference prediction replay");
    const auto reference=m.candidates;const auto reading=m.reading;const auto composing=m.composingReading;
    check(m.query(roman,converted,katakana,false),"single final prediction replay");
    check(m.reading==reading && m.composingReading==composing,"optimized replay must preserve all composition text");
    check(m.candidates.size()==reference.size(),"optimized replay candidate count must match");
    for(size_t i=0;i<reference.size();++i){
        check(m.candidates[i].text==reference[i].text && m.candidates[i].reading==reference[i].reading,"optimized replay must preserve every candidate and its reading");
    }
}
for(auto roman:{"ti","tyo","tyu","konnnitiha","nihongo","watasi"}) {
    check(m.query(roman,false),"Mozc query failed");check(!m.reading.empty(),"missing actual reading");check(!m.candidates.empty(),"missing actual candidates");
    std::cout<<roman<<" -> "<<m.reading<<" candidates="<<m.candidates.size()<<" first="<<m.candidates[0].text<<'\n';
}
// A displayed numbered candidate must commit its own text, including the
// explicitly added kana row and prediction rows selected before Space.
int selectionFailures=0;
for(bool converted:{false,true})for(const auto* roman:{"a","i","ti","tyo","ni","nihongo","taberu","sushi"}) {
    for(int index=0;index<9;++index) {
        check(m.query(roman,converted),"numbered selection query");
        if(index>=int(m.candidates.size()))break;
        const auto candidate=m.candidates[index];
        const auto actual=m.commit(index);
        if(actual!=candidate.text) {
            std::cerr<<"NUMBERED MISMATCH roman="<<roman<<" converted="<<converted<<" slot="<<index+1<<" id="<<candidate.id<<" expected="<<candidate.text<<" actual="<<actual<<'\n';
            ++selectionFailures;
        }
    }
}
check(selectionFailures==0,"numbered candidate must commit the displayed text");
for(bool katakana:{false,true}) {
    bool testedLiteral=false;
    for(const auto* roman:{"ti","sushi","ni","q"}) {
        check(m.query(roman,false,katakana),"literal kana query");
        int literal=-1;
        for(int i=0;i<int(m.candidates.size());++i)if(m.candidates[i].id==-2147483647)literal=i;
        if(literal<0)continue; // Learning may already supply a real kana row.
        const auto expected=m.candidates[literal].text;
        check(m.highlight(literal) && m.reading==expected,"literal highlight must match the panel");
        check(m.commit(literal)==expected,"literal highlight and commit must agree in both scripts");
        check(m.reading.empty() && m.candidates.empty(),"literal commit must clear backend composition");
        testedLiteral=true;break;
    }
    check(testedLiteral,"must exercise a synthetic row in both scripts");
}
check(m.query("ti",false)&&m.reading=="ち","ti conversion");
check(m.query("tyo",false)&&m.reading=="ちょ","tyo conversion");
check(m.query("tyu",false)&&m.reading=="ちゅ","tyu conversion");
check(m.query("nihongo",true),"explicit conversion");
int japanese=-1;for(int i=0;i<int(m.candidates.size());++i)if(m.candidates[i].text=="日本語")japanese=i;
check(japanese>=0,"real Japanese dictionary result");check(m.candidates[japanese].reading=="にほんご","same-key conversion must retain kana reading");check(m.commit(japanese)=="日本語","candidate commit");
check(m.query("konnnitiha",false)&&m.submitReading()=="こんにちは","reading submit");
check(m.query("watasihanihongo",true),"multi-segment query");
check(m.commit(0)=="私は日本語","multi-segment commit must retain trailing segments");
check(m.query("tyo",false,true)&&m.reading=="チョ","katakana composing mode");
check(m.submitReading()=="チョ","katakana reading commit");
check(m.query("nihongo",true),"candidate highlight query");
const auto original=m.reading;check(m.candidates.size()>3,"full candidate list");
check(m.highlight(1),"highlight another real candidate");
check(!m.reading.empty(),"highlight must yield full composition text");
const auto highlighted=m.reading;check(m.commit(1)==highlighted,"highlighted preedit must match the committed text");
check(m.query("ti",false),"short kanji prediction");
for(const auto& c:m.candidates)if(c.text=="血")check(c.reading=="ち","short word furigana");
check(m.query("tennde",false),"expanded prediction reading");
bool expandedReading=false;
for(const auto& c:m.candidates) {
    std::cout<<c.text<<" ["<<c.reading<<"]\n";
    if(c.text=="点ではない") {check(c.reading=="てんではない","prediction must use its own full reading");expandedReading=true;}
}
check(expandedReading,"fixture should contain the longer kanji prediction");
std::cout<<"Mozc backend passed\n";}
