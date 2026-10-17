#include "roman_buffer.h"
#include "roman_correction.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
using namespace tailkana;
void check(bool v,const char* what) { if(!v) { std::cerr<<what<<'\n'; std::exit(1); } }
void type(RomanBuffer& b,const std::string& s) { for(char c:s)b.insert(c); }
int main() {
    auto variants=romanCorrections("arigatpu");
    check(std::any_of(variants.begin(),variants.end(),[](const auto& c){return c.roman=="arigatou" && c.edits==1;}),"upstream azooKey pu→ou correction");
    check(romanCorrections("arigatou").empty(),"unchanged correct spelling needs no variants");
    check(romanCorrections("a").empty() && romanCorrections(std::string(49,'a')).empty(),"bounded word size");
    check(romanCorrections("abc/123").empty(),"non-romaji text must not enter typo search");
    check(romanCorrections("nolinolunolipu").size()<=8,"correction search must stay bounded");
    for(const auto& sample:std::vector<std::pair<std::string,std::string>>{{"xi","shi"},{"qi","chi"},{"ci","tsu"},{"xiao","sho"},{"qiao","cho"}}) {
        const auto results=romanCorrections(sample.first,true,false);
        check(results.size()==1 && results.front().roman==sample.second && results.front().kanaOnly && results.front().pinyin,"standalone pinyin approximation should be one complete kana alternative");
    }
    const auto word=romanCorrections("xigoto",true,false);
    check(!word.empty() && word.front().roman=="shigoto" && !word.front().kanaOnly,"pinyin approximation must also work inside a word");
    check(romanCorrections("qixiki",true,false).front().roman=="chishiki","normalize all pinyin syllables before trying partial corrections");
    check(romanCorrections("shi",true,false).empty() && romanCorrections("tsu",true,false).empty(),"canonical Japanese spelling must remain unchanged");
    check(romanCorrections("qi",true,false,static_cast<uint32_t>(PinyinGroup::Xi)).empty(),"disabled phonetic group must not generate alternatives");
    check(!romanCorrections("xi",true,false,static_cast<uint32_t>(PinyinGroup::Xi)).empty(),"enabled phonetic group remains active");
    check(romanCorrections("xiao",true,false,static_cast<uint32_t>(PinyinGroup::Xi)).empty(),"disabled glides must not fall back to the enabled xi prefix");
    RomanBuffer b; type(b,"katti");
    auto c=b.cross(); check(c.current.roman=="ti" && c.current.start==3 && c.current.length==2,"sokuon tail boundary");
    check(c.choices[2].roman=="tyo" && c.choices[3].roman=="tyu","user's direction mapping");
    check(b.replaceTail(2) && b.raw=="kattyo" && b.cursor==6,"replace tail without touching prefix");
    check(b.cross().current.roman=="tyo" && b.cross().current.kana=="ちょ","recenter yoon");
    check(b.cross().choices[2].roman.ends_with("yo") && b.cross().choices[3].roman.ends_with("yu"),"yo/yu directions remain fixed after recentering");
    check(b.replaceTail(3) && b.raw=="kattyu","regenerate choices after replacement");
    b.clear(); type(b,"kit"); check(b.cross().current.roman=="t" && !b.replaceTail(2),"incomplete tail must not replace ki");
    b.clear(); type(b,"kiti"); b.move(-4); check(b.replaceTail(2) && b.raw=="kityo" && b.cursor==0,"tail replacement with moved caret");
    b.insert('a'); check(b.raw=="akityo","ordinary caret editing");
    b.clear(); type(b,"konnnitiha"); check(b.cross().current.roman=="ha","parse n runs");
    for(const auto& [roman,kana]:RomanBuffer::table()) {
        b.clear();type(b,roman); auto cross=b.cross();
        if(!cross.current.kana.empty()) {
            for(int d=0;d<4;++d) if(!cross.choices[d].roman.empty()) {
                auto copy=b; check(copy.replaceTail(d),"valid cross replacement");
                check(copy.raw.starts_with(b.raw.substr(0,cross.current.start)),"all replacements preserve prefix");
            }
        }
    }
    std::cout<<"Roman buffer: all invariants passed\n";
}
