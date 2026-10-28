// SPDX-License-Identifier: GPL-3.0-or-later
#include "win32/tip/mozc_converter.h"
#include "base/init_mozc.h"
#include "win32/tip/tip_input_mode_manager.h"
#include <array>
#include <msctf.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cstdlib>
using namespace tailkana::windows;
void check(bool value,const char* message) {if (!value) {std::cerr<<message<<'\n';std::exit(1);}}
int main(int argc,char** argv) {
    mozc::InitMozc(argv[0],&argc,&argv);
    mozc::win32::tsf::TipInputModeManager modes(mozc::win32::tsf::TipInputModeManager::Config{});
    modes.OnInitialize(true,TF_CONVERSIONMODE_NATIVE|TF_CONVERSIONMODE_FULLSHAPE);
    for (const auto scope:{IS_PASSWORD,IS_NUMERIC_PASSWORD,IS_NUMERIC_PIN,IS_ALPHANUMERIC_PIN,IS_ALPHANUMERIC_PIN_SET}) {
        modes.OnChangeInputScope(std::array<InputScope,1>{scope});
        check(modes.TailKanaSensitiveInput() && !modes.GetEffectiveOpenClose(),"password/PIN scope bypasses Japanese composition");
    }
    modes.OnChangeInputScope(std::array<InputScope,1>{IS_DEFAULT});check(!modes.TailKanaSensitiveInput(),"ordinary text scope restores input");
    auto converter=mozc::win32::tsf::MakeTailKanaConverter();Conversion out;
    check(converter->query("ti",false,false,out) && out.reading=="ち","real Windows Mozc live kana");
    auto literal=std::find_if(out.words.begin(),out.words.end(),[](const Word& word) {return word.text=="ち";});
    check(literal!=out.words.end(),"literal candidate present");
    check(converter->commit(static_cast<size_t>(literal-out.words.begin()))=="ち","literal commits itself");
    check(converter->query("tyo",false,false,out) && out.reading=="ちょ","real Windows tail correction reading");
    check(converter->query("ti",false,true,out) && out.reading=="チ","real Windows katakana");
    check(converter->query("nihongo",true,false,out),"real Windows conversion");
    auto japanese=std::find_if(out.words.begin(),out.words.end(),[](const Word& word) {return word.text=="日本語";});
    check(japanese!=out.words.end(),"real Japanese dictionary result");
    check(converter->commit(static_cast<size_t>(japanese-out.words.begin()))=="日本語","candidate commits displayed text");
    for (const auto* raw:{"watasihanihongowobenkyoushiteimasu","ashitahatoukyounoatarashiikaishadehatarakimasu"}) {
        check(converter->query(raw,true,false,out) && !out.words.empty(),"long multi-segment query");
        const auto expected=out.words.front().text;
        const auto actual=converter->commit(0);
        if (actual!=expected) std::cerr<<"multi-segment expected "<<expected<<" got "<<actual<<'\n';
        check(actual==expected,"multi-segment displayed text matches commit");
    }
    std::vector<double> times;std::string raw;
    converter->reset();
    for (char c:std::string("ashitahatoukyounoatarashiikaishadehatarakimasu")) {
        raw+=c;const auto begin=std::chrono::steady_clock::now();check(converter->query(raw,false,false,out),"incremental long query");
        times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
    }
    std::sort(times.begin(),times.end());
    std::cout<<"PASS real Windows converter and multi-segment commit; warm live-key p50="<<times[times.size()/2]<<"ms max="<<times.back()<<"ms\n";
    converter->reset();
}
