#include "tailkana.h"
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <algorithm>
using namespace fcitx;
class Client final:public InputContext {
public:
    Client(InputContextManager& m):InputContext(m,"tailkana-test") {created();setCapabilityFlags(CapabilityFlag::Preedit);}
    ~Client(){destroy();}
    const char* frontend()const override{return "tailkana-test";}
    std::string committed;
    void commitStringImpl(const std::string& s)override{committed+=s;}
    void deleteSurroundingTextImpl(int,unsigned int)override{}
    void forwardKeyImpl(const ForwardKeyEvent&)override{}
    void updatePreeditImpl()override{}
};
void check(bool b,const char* s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
int main(){Instance instance(0,nullptr);TailKana engine(&instance);RawConfig defaults;defaults["ShowCandidate"]="False";defaults["SmartChinese"]="False";defaults["Corrections"]="True";defaults["PinyinCorrections"]="False";defaults["ShowMeaning"]="False";defaults["Katakana"]="False";defaults["SwapKeys"]="False";defaults["LeftShift"]="True";defaults["RightShift"]="True";defaults["Furigana"]="True";defaults["PanelScale"]="100";defaults["Appearance"]="System";defaults["Glass"]="True";defaults["TintOpacity"]="20";defaults["CustomColors"]="False";engine.setConfig(defaults);Client client(instance.inputContextManager());InputMethodEntry entry("tailkana","TailKana","ja","tailkana");
    auto key=[&](KeySym sym,KeyStates mods=KeyStates(),bool up=false){KeyEvent e(&client,Key(sym,mods),up);engine.keyEvent(entry,e);return e.accepted();};
    auto meta=[&](){auto text=client.inputPanel().auxDown().toString();return nlohmann::json::parse(text.substr(9));};
    auto type=[&](const std::string& s){for(char c:s)check(key(KeySym(c)),"letter rejected");};
    auto reset=[&](){InputContextEvent e(&client,EventType::InputContextReset);engine.reset(entry,e);};
    // Before Space, the literal kana row can be second (ti/sushi) or
    // third (nihongo). Exercise displayed slots through keys and clicks.
    for(const auto* roman:{"ti","sushi","nihongo"})for(bool mouse:{false,true}) {
        reset();client.committed.clear();type(roman);
        auto list=client.inputPanel().candidateList();
        check(list && list->size()>=2,"numbered selection fixture needs two candidates");
        const auto expected=list->candidate(1).text().toString();
        const auto first=list->candidate(0).text().toString();
        check(expected!=first,"selection fixture must distinguish the first two candidates");
        if(mouse)list->candidate(1).select(&client);else key(FcitxKey_2);
        check(client.committed==expected && client.inputPanel().empty(),"2 and second-row clicks must commit the displayed second candidate");
    }
    reset();client.committed.clear();type("nihongo");
    const auto compactTotal=meta()["candidateTotal"].get<int>();
    engine.action(&client,209);
    check(meta()["expanded"]==true && client.inputPanel().candidateList()->size()==compactTotal,"overflow expansion must expose the complete engine list");
    engine.action(&client,200);
    check(meta()["expanded"]==false && meta()["autoExpandAllowed"]==false,"manual collapse must suppress immediate automatic re-expansion");
    engine.action(&client,209);check(meta()["expanded"]==false,"stale overflow report must respect manual collapse");
    type("a");check(meta()["autoExpandAllowed"]==true,"new spelling may auto-expand again");
    engine.action(&client,209);
    const auto expandedSecond=client.inputPanel().candidateList()->candidate(1).text().toString();
    key(FcitxKey_2);check(client.committed==expandedSecond && client.inputPanel().empty(),"automatic expansion must preserve numeric selection mapping");
    reset();type("nihongo");
    const int staleLayout=meta()["autoExpandAction"];
    type("a");engine.action(&client,staleLayout);check(meta()["expanded"]==false,"old layout report must not expand a different input");
    engine.action(&client,meta()["autoExpandAction"]);check(meta()["autoExpanded"]==true,"automatic expansion records its origin");
    const int oldFold=meta()["autoCollapseAction"];
    key(FcitxKey_BackSpace);engine.action(&client,oldFold);check(meta()["expanded"]==true,"old fit report cannot fold newer candidates");
    engine.action(&client,meta()["autoCollapseAction"]);check(meta()["expanded"]==false && meta()["autoExpandAllowed"]==true,"automatic fold restores compact layout without a manual-collapse veto");
    check(client.inputPanel().candidateList()->size()==3,"automatic fold restores numbered three-candidate mapping");
    engine.action(&client,200);check(meta()["expanded"]==true && meta()["autoExpanded"]==false,"Tab expansion is manual");
    engine.action(&client,meta()["autoCollapseAction"]);check(meta()["expanded"]==true,"automatic fit cannot fold a manually expanded list");
    reset();client.committed.clear();
    RawConfig pinyinProfile;pinyinProfile["PinyinCorrections"]="True";pinyinProfile["Corrections"]="False";engine.setConfig(pinyinProfile);
    for(const auto& sample:std::vector<std::pair<std::string,std::string>>{{"xi","し"},{"qi","ち"},{"ci","つ"},{"xiao","しょ"},{"qiao","ちょ"},{"xigoto","仕事"},{"qikatetsu","地下鉄"},{"cinami","津波"},{"qixiki","知識"}}) {
        reset();client.committed.clear();type(sample.first);
        check(meta()["raw"]==sample.first && client.committed.empty(),"pinyin habits must preserve raw composition");
        if(meta()["pinyinCorrectionSlots"].empty())std::cerr<<"No pinyin correction for "<<sample.first<<" metadata="<<meta().dump()<<'\n';
        check(!meta()["pinyinCorrectionSlots"].empty() && meta()["pinyinCorrectionSlots"][0]==1,"pinyin correction should be the second candidate");
        const auto actual=client.inputPanel().candidateList()->candidate(1).text().toString();
        if(actual!=sample.second)std::cerr<<"Pinyin fixture "<<sample.first<<" expected="<<sample.second<<" actual="<<actual<<'\n';
        check(actual==sample.second,"pinyin habits must resolve standalone kana and whole Japanese words");
        key(FcitxKey_2);check(client.committed==sample.second,"pinyin-habit numeric selection must commit exactly the displayed text");
    }
    reset();client.committed.clear();RawConfig personalized;personalized["PinyinQi"]="False";engine.setConfig(personalized);engine.reloadConfig();type("qi");
    check(meta()["pinyinCorrectionSlots"].empty(),"individual qi setting must survive reload and suppress only its group");
    reset();type("xi");check(!meta()["pinyinCorrectionSlots"].empty(),"other phonetic groups remain enabled");
    reset();RawConfig qiOn;qiOn["PinyinQi"]="True";engine.setConfig(qiOn);
    reset();client.committed.clear();RawConfig pinyinMixed;pinyinMixed["SmartChinese"]="True";engine.setConfig(pinyinMixed);type("ci");
    check(meta()["pinyinCorrectionSlots"]==nlohmann::json::array({1}) && meta()["chineseSlots"]==nlohmann::json::array({2}),"pinyin habit kana and Chinese third slot must coexist");
    const auto originalCi=client.inputPanel().candidateList()->candidate(0).text().toString();
    key(FcitxKey_1);check(client.committed==originalCi,"standard ci input must remain selectable");
    reset();client.committed.clear();RawConfig pinyinOff;pinyinOff["PinyinCorrections"]="False";engine.setConfig(pinyinOff);type("xi");
    check(meta()["pinyinCorrectionSlots"].empty() && meta()["corrections"].empty(),"pinyin profile can be disabled independently");
    reset();engine.setConfig(defaults);client.committed.clear();
    for(const auto& sample:std::vector<std::pair<std::string,std::string>>{{"arigatpu","ありがとう"},{"liroku","記録"}}) {
        reset();client.committed.clear();type(sample.first);
        const auto original=client.inputPanel().clientPreedit().toString();
        auto metadata=meta();
        check(metadata["raw"]==sample.first && client.committed.empty(),"suggesting a correction must not mutate or commit the raw input");
        bool found=false;
        for(auto it=metadata["corrections"].begin();it!=metadata["corrections"].end();++it) {
            const int slot=std::stoi(it.key());
            auto list=client.inputPanel().candidateList();
            if(list->candidate(slot).text().toString()!=sample.second)continue;
            check(slot==1,"first corrected candidate should be second, preserving the original first row");
            key(FcitxKey_2);found=true;break;
        }
        check(found && client.committed==sample.second,"corrected numbered choice must commit the dictionary result");
    }
    reset();client.committed.clear();RawConfig correctionOff;correctionOff["Corrections"]="False";engine.setConfig(correctionOff);
    type("arigatpu");check(meta()["corrections"].empty(),"correction toggle must remove correction candidates");
    reset();engine.setConfig(defaults);client.committed.clear();type("arigatpu");
    key(FcitxKey_space);key(FcitxKey_space);
    check(client.inputPanel().clientPreedit().toString()=="ありがとう","conversion highlight must display the corrected candidate's own composition");
    key(FcitxKey_Return);check(client.committed=="ありがとう","converted Enter must submit the corrected session");
    reset();client.committed.clear();type("arigatpu");key(FcitxKey_space);key(FcitxKey_space);
    InputContextEvent blur(&client,EventType::InputContextSwitchInputMethod);engine.deactivate(entry,blur);
    check(client.committed=="ありがとう","deactivation must commit the corrected session rather than the original typo");
    RawConfig chineseCorrection;chineseCorrection["SmartChinese"]="True";engine.setConfig(chineseCorrection);
    client.committed.clear();type("shili");
    check(meta()["corrections"].contains("1") && meta()["chineseSlots"]==nlohmann::json::array({2}),"corrections must coexist with the pinned Chinese third row");
    const auto chineseThird=client.inputPanel().candidateList()->candidate(2).text().toString();
    key(FcitxKey_3);check(client.committed==chineseThird,"Chinese numeric mapping must survive correction insertion");
    reset();engine.setConfig(defaults);client.committed.clear();
    for(bool enabled:{false,true}) {
        RawConfig option;option["Corrections"]=enabled?"True":"False";engine.setConfig(option);
        std::vector<double> latencies;
        for(int round=0;round<3;++round)for(const auto* roman:{"arigatpu","liroku","shili"}) {
            reset();
            for(char c:std::string(roman)) {
                const auto start=std::chrono::steady_clock::now();key(KeySym(c));
                latencies.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            }
        }
        std::sort(latencies.begin(),latencies.end());
        std::cout<<"Correction latency enabled="<<enabled<<" p95Ms="<<latencies[latencies.size()*95/100]<<" maxMs="<<latencies.back()<<'\n';
    }
    reset();engine.setConfig(defaults);client.committed.clear();
    type("kiti");check(client.inputPanel().clientPreedit().toString()=="kiti"&&client.committed.empty(),"roman preedit must be inline but uncommitted");
    check(client.inputPanel().candidateList()->size()<=3,"collapsed candidates");
    engine.action(&client,201);check(meta()["kanaMode"]==true,"top display switch");
    check(client.inputPanel().clientPreedit().toString()=="きち", "live preedit must show kana, not a prediction");
    engine.action(&client,201);check(meta()["kanaMode"]==false,"return to roman display");
    key(FcitxKey_Shift_L,KeyState::Shift);key(FcitxKey_Right,KeyState::Shift);
    check(meta()["raw"]=="kiti","left shift must not correct");
    key(FcitxKey_Shift_R,KeyState::Shift);key(FcitxKey_Right,KeyState::Shift);
    check(meta()["raw"]=="kityo"&&meta()["current"]["roman"]=="tyo","right shift tail correction");
    key(FcitxKey_Shift_R,KeyStates(),true);key(FcitxKey_Left);
    check(meta()["cursor"]==4&&meta()["raw"]=="kityo","plain arrows move caret");
    reset();type("nihongo");key(FcitxKey_Tab);check(meta()["expanded"]==true,"Tab expansion");
    key(FcitxKey_space);check(client.inputPanel().candidateList() && client.inputPanel().candidateList()->size()>0,"conversion candidates");
    bool found=false;auto list=client.inputPanel().candidateList();for(int i=0;i<list->size();++i)if(list->candidate(i).text().toString()=="日本語"){list->candidate(i).select(&client);found=true;break;}
    check(found&&client.committed=="日本語","real Fcitx candidate click must commit");
    check(client.inputPanel().empty(),"panel must clear after commit");
    type("ti");engine.action(&client,102);check(meta()["raw"]=="tyo","mouse cross action");
    key(FcitxKey_Return);check(client.committed=="日本語ちょ","unconverted Enter submits reading");
    type("ti");key(FcitxKey_Escape);check(client.inputPanel().empty()&&client.committed=="日本語ちょ","Escape cancels without host mutation");
    type("kit");engine.action(&client,102);check(meta()["raw"]=="kit","incomplete tail guard in engine");
    reset();type("ti");check(!key(FcitxKey_c,KeyState::Super),"Command shortcuts must pass through");
    reset();client.committed.clear();type("nihongo");key(FcitxKey_space);
    auto first=client.inputPanel().clientPreedit().toString();check(first=="日本語","Space must visibly convert inline");
    const int total=meta()["candidateTotal"].get<int>();check(total>3,"dictionary must offer candidates beyond the compact three");
    bool changed=false;
    for(int i=1;i<total;++i) {
        key(FcitxKey_space);changed|=client.inputPanel().clientPreedit().toString()!=first;
        check(meta()["candidateOffset"].get<int>()+meta()["selected"].get<int>()==i,"Space must cycle every candidate, including compact pages");
        check(client.inputPanel().candidateList()->cursorIndex()>=0,"selection must remain visible");
    }
    check(changed,"Space must visibly change selected text");key(FcitxKey_space);
    check(client.inputPanel().clientPreedit().toString()==first,"Space must wrap around");
    const auto beforeTheme=client.inputPanel().clientPreedit().toString();
    const auto beforeThemeMeta=meta();
    RawConfig theme;theme["Appearance"]="Dark";theme["Glass"]="False";theme["TintOpacity"]="0";theme["CustomColors"]="True";theme["BackgroundColor"]="#112244";engine.setConfig(theme);
    check(meta()["appearance"]=="Dark" && meta()["converted"]==true,"theme must reach the panel without cancelling conversion");
    check(client.inputPanel().clientPreedit().toString()==beforeTheme && meta()["selected"]==beforeThemeMeta["selected"] && meta()["candidateOffset"]==beforeThemeMeta["candidateOffset"] && client.committed.empty(),"theme must preserve preedit and candidate selection");
    check(meta()["glass"]==false && meta()["opacity"]==0 && meta()["customColors"]==true && meta()["backgroundColor"]=="#112244","glass and custom colors must reach the active panel");
    engine.reloadConfig();check(meta()["appearance"]=="Dark" && meta()["glass"]==false && meta()["opacity"]==0,"theme must survive disk reload");
    RawConfig glass;glass["Glass"]="True";engine.setConfig(glass);
    check(meta()["glass"]==true && meta()["backgroundColor"]=="#112244" && meta()["opacity"]==0 && meta()["converted"]==true,"glass toggle preserves tint, custom color and conversion");
    engine.action(&client,205);check(client.committed=="日本語","confirm button must commit exactly the displayed conversion");
    client.committed.clear();type("tyo");engine.action(&client,202);
    check(meta()["katakana"]==true&&meta()["current"]["kana"]=="チョ","script button updates cross and backend");
    key(FcitxKey_Return);check(client.committed=="チョ","persistent katakana output");
    engine.action(&client,202);client.committed.clear();type("tyu");
    check(key(FcitxKey_Shift_L,KeyState::Shift),"left Shift must be reserved");
    check(meta()["katakana"]==true,"held left Shift temporarily switches script");
    key(FcitxKey_Shift_L,KeyStates(),true);check(meta()["katakana"]==false,"left Shift release restores script");
    key(FcitxKey_Shift_L,KeyState::Shift);key(FcitxKey_Return,KeyState::Shift);
    check(client.committed=="チュ","confirm while left Shift is held must use temporary katakana");
    key(FcitxKey_Shift_L,KeyStates(),true);client.committed.clear();
    engine.action(&client,203);type("nihongo");key(FcitxKey_Return);
    check(meta()["swapKeys"]==true&&meta()["converted"]==true&&client.committed.empty(),"swapped Enter converts without committing");
    key(FcitxKey_Return);auto selected=client.inputPanel().clientPreedit().toString();
    key(FcitxKey_space);check(client.committed==selected,"swapped Space confirms selected conversion");
    engine.action(&client,203);client.committed.clear();type("nihongo");engine.action(&client,201);
    auto shown=client.inputPanel().clientPreedit().toString();key(FcitxKey_Return);
    check(client.committed==shown,"live kana display must confirm the shown reading");
    reset();RawConfig preferences;
    preferences["ShowCandidate"]="False";preferences["Katakana"]="True";preferences["SwapKeys"]="True";
    preferences["LeftShift"]="False";preferences["Furigana"]="False";preferences["PanelScale"]="90";
    engine.setConfig(preferences);engine.reloadConfig();type("tyo");
    check(meta()["katakana"]==true && meta()["swapKeys"]==true && meta()["panelScale"]==90 && meta()["furigana"]==false,"settings must survive disk reload");
    check(key(FcitxKey_Shift_L,KeyState::Shift),"disabled left Shift still reserves the default English toggle");check(meta()["katakana"]==true,"disabled left Shift must preserve script");
    reset();engine.setConfig(defaults);
    RawConfig mixed;mixed["SmartChinese"]="True";mixed["ShowCandidate"]="True";mixed["ShowMeaning"]="True";engine.setConfig(mixed);
    client.committed.clear();type("nihao");
    check(meta()["chineseSlots"]==nlohmann::json::array({2}),"Chinese candidate must occupy third compact slot");
    check(client.inputPanel().candidateList()->candidate(2).text().toString()=="你好","real pinyin engine must return 你好");
    key(FcitxKey_3);check(client.committed=="你好" && client.inputPanel().empty(),"3 must commit the Chinese candidate and clear composition");
    type("nihao");key(FcitxKey_space);
    for(int i=0;i<5;++i) {key(FcitxKey_space);check(meta()["chineseSlots"]==nlohmann::json::array({2}),"third Chinese slot must remain pinned across pages");}
    reset();type("nihongo");check(client.inputPanel().clientPreedit().toString()=="にほんご","real-time preedit must retain full Japanese reading");
    for(int i=0;i<6;++i) {
        key(FcitxKey_Left);
        const auto& preedit=client.inputPanel().clientPreedit();const auto text=preedit.toString();const int caret=preedit.cursor();
        check(caret>=0 && caret<=int(text.size()) && (caret==int(text.size()) || (static_cast<unsigned char>(text[caret])&0xc0)!=0x80),"live kana caret must stay at a UTF-8 character boundary");
    }
    auto meaning=tailkana::chineseDefinition("日本語","にほんご");check(meaning.text.find("日语")!=std::string::npos,"offline dictionary must provide real Chinese definition");
    check(tailkana::chineseDefinition("unrecorded-gibberish","").text.empty(),"missing definition must not be fabricated");
    for(const auto& sample:std::vector<std::pair<std::string,std::string>>{{"taberu","吃"},{"tabemashita","吃"},{"gakkou","学校"},{"jikan","时间"}}) {
        reset();type(sample.first);engine.action(&client,204);
        check(meta().contains("definition") && meta()["definition"]["text"].get<std::string>().find(sample.second)!=std::string::npos,"whole-word meaning must reach candidate-panel metadata through the real Mozc pipeline");
    }
    reset();engine.setConfig(defaults);
    std::cout<<"Fcitx engine: inline preedit, full conversion cycling, temporary/persistent kana script, swapped keys and commits passed\n";
}
