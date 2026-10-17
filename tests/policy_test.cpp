#include "tailkana.h"
#include <fcitx/addonloader.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdlib>
fcitx::StaticAddonRegistry& getPolicyAddons(){static fcitx::StaticAddonRegistry registry;return registry;}
using namespace fcitx;
struct Client final:InputContext {
    explicit Client(InputContextManager& manager):InputContext(manager,"tailkana-policy-test"){created();setCapabilityFlags(CapabilityFlag::Preedit);}
    ~Client(){destroy();}
    std::string committed;
    const char* frontend()const override{return "policy-test";}
    void commitStringImpl(const std::string& s)override{committed+=s;}
    void deleteSurroundingTextImpl(int,unsigned int)override{}
    void forwardKeyImpl(const ForwardKeyEvent&)override{}
    void updatePreeditImpl()override{}
};
void check(bool condition,const char* message){if(!condition){std::cerr<<message<<'\n';std::exit(1);}}
int main() {
    Instance instance(0,nullptr);
    instance.addonManager().registerDefaultLoader(&getPolicyAddons());
    instance.initialize();
    auto* engine=dynamic_cast<TailKana*>(instance.addonManager().addon("tailkana",false));
    check(engine,"TailKana policy must load even when English is the startup engine");
    InputMethodGroup group("Policy");group.inputMethodList().emplace_back("keyboard-us");group.inputMethodList().emplace_back("tailkana");group.setDefaultInputMethod("tailkana");
    if(!instance.inputMethodManager().group("Policy"))instance.inputMethodManager().addEmptyGroup("Policy");
    instance.inputMethodManager().setGroup(group);instance.inputMethodManager().setCurrentGroup("Policy");
    Client client(instance.inputContextManager());client.focusIn();
    auto send=[&](KeySym sym,KeyStates modifiers=KeyStates(),bool release=false) {
        KeyEvent event(&client,Key(sym,modifiers),release);client.keyEvent(event);return event.accepted();
    };
    auto mode=[&](const char* name){instance.setCurrentInputMethod(&client,name,false);};
    auto punctuation=[&](const char* name){RawConfig config;config["Punctuation"]=name;engine->setConfig(config);};
    punctuation("Auto");mode("tailkana");
    check(send(FcitxKey_comma) && client.committed=="、","Japanese automatic comma must be full width");
    client.committed.clear();send(FcitxKey_period);check(client.committed=="。","Japanese automatic period");
    mode("keyboard-us");client.committed.clear();send(FcitxKey_comma);check(client.committed==",","English automatic comma must be half width");
    punctuation("Full");client.committed.clear();send(FcitxKey_question,KeyState::Shift);check(client.committed=="？","manual full width also applies in English");
    punctuation("Half");mode("tailkana");client.committed.clear();send(FcitxKey_question,KeyState::Shift);check(client.committed=="?","manual half width also applies in Japanese");
    // Exercise physical Shift press/release too, so consumed symbol events
    // cannot accidentally trigger Fcitx's modifier-only shortcut afterwards.
    auto physical=[&](KeySym sym,KeyStates modifiers,uint32_t code,bool release=false) {
      KeyEvent event(&client,Key(sym,modifiers,code),release);client.keyEvent(event);return event.accepted();
    };
    mode("keyboard-us");punctuation("Full");client.committed.clear();
    physical(FcitxKey_Shift_L,KeyState::Shift,50);
    physical(FcitxKey_Shift_L,KeyStates(),50,true);
    check(instance.inputMethod(&client)=="keyboard-us","Shift alone must not change English/Japanese mode");
    physical(FcitxKey_Shift_L,KeyState::Shift,50);
    physical(FcitxKey_question,KeyState::Shift,61);
    physical(FcitxKey_Shift_L,KeyStates(),50,true);
    check(client.committed=="？" && instance.inputMethod(&client)=="keyboard-us","Shift punctuation must not toggle mode on release");
    physical(FcitxKey_Shift_L,KeyState::Shift,50);
    physical(FcitxKey_Tab,KeyState::Shift,23);
    physical(FcitxKey_Tab,KeyState::Shift,23,true);
    physical(FcitxKey_Shift_L,KeyStates(),50,true);
    check(instance.inputMethod(&client)=="tailkana","physical Shift+Tab sequence must return to Japanese exactly once");
    punctuation("Auto");mode("tailkana");
    check(send(FcitxKey_Tab,KeyState::Shift) && instance.inputMethod(&client)=="keyboard-us","Shift+Tab must switch Japanese to English");
    send(FcitxKey_Tab,KeyState::Shift,true);send(FcitxKey_Shift_L,KeyStates(),true);
    check(instance.inputMethod(&client)=="keyboard-us","releases must not switch back");
    send(FcitxKey_Tab,KeyState::Shift);
    check(instance.inputMethod(&client)=="tailkana","Shift+Tab must switch English to Japanese");
    send(FcitxKey_Tab,KeyState::Shift,true);send(FcitxKey_Shift_L,KeyStates(),true);
    for(char c:std::string("nihongo"))send(KeySym(c));
    client.committed.clear();send(FcitxKey_comma);
    check(client.committed=="にほんご、","punctuation must preserve and commit pending Japanese text exactly once");
    client.committed.clear();
    for(char c:std::string("ko-hi-"))send(KeySym(c));send(FcitxKey_Return);
    check(client.committed=="こーひー","punctuation policy must preserve the Japanese long-vowel marker");
    client.committed.clear();for(char c:std::string("n'ya"))send(KeySym(c));send(FcitxKey_Return);
    check(client.committed=="んや","punctuation policy must preserve romanization apostrophes");
    punctuation("Full");mode("keyboard-us");client.committed.clear();
    client.setCapabilityFlags(CapabilityFlags{CapabilityFlag::Preedit,CapabilityFlag::Password});
    send(FcitxKey_question,KeyState::Shift);
    check(client.committed!="？","secure fields must not receive transformed punctuation");
    client.setCapabilityFlags(CapabilityFlag::Preedit);
    KeyEvent command(&client,Key(FcitxKey_period,KeyState::Super),false);
    check(!engine->handleTypingPolicy(command) && !command.accepted(),"command shortcuts must bypass the typing policy");
    // Use the complete Fcitx dispatch path, including a physical digit key.
    mode("tailkana");client.committed.clear();
    for(char c:std::string("ti"))send(KeySym(c));
    const auto list=client.inputPanel().candidateList();
    check(list && list->size()>=2,"physical digit fixture needs two candidates");
    const auto second=list->candidate(1).text().toString();
    physical(FcitxKey_2,KeyStates(),11);
    physical(FcitxKey_2,KeyStates(),11,true);
    check(client.committed==second && client.inputPanel().empty(),"physical 2 must commit the displayed second row exactly once");
    // Existing TailKana profiles historically listed Japanese first.
    auto legacy=instance.inputMethodManager().currentGroup();legacy.inputMethodList().clear();
    legacy.inputMethodList().emplace_back("tailkana");legacy.inputMethodList().emplace_back("keyboard-us");legacy.setDefaultInputMethod("tailkana");
    instance.inputMethodManager().setGroup(legacy);mode("tailkana");
    send(FcitxKey_Tab,KeyState::Shift);check(instance.inputMethod(&client)=="keyboard-us","shortcut must support existing Japanese-first profiles");
    send(FcitxKey_Shift_L,KeyStates(),true);send(FcitxKey_Tab,KeyState::Shift);
    check(instance.inputMethod(&client)=="tailkana","legacy profile must switch back by name");
    // A saved preset must override a stale English group default on focus,
    // while explicit switches within the field keep working.
    RawConfig japaneseDefault;japaneseDefault["DefaultInputMode"]="Japanese";engine->setConfig(japaneseDefault);engine->reloadConfig();
    mode("keyboard-us");client.focusOut();client.focusIn();
    check(instance.inputMethod(&client)=="tailkana","Japanese preset must override saved English on focus");
    send(FcitxKey_Tab,KeyState::Shift);check(instance.inputMethod(&client)=="keyboard-us","temporary English switch remains available");
    send(FcitxKey_a);check(instance.inputMethod(&client)=="keyboard-us","a preset must not reset mode on every key");
    client.focusOut();client.focusIn();check(instance.inputMethod(&client)=="tailkana","refocusing uses the configured default again");
    RawConfig englishDefault;englishDefault["DefaultInputMode"]="English";engine->setConfig(englishDefault);engine->reloadConfig();
    client.focusOut();client.focusIn();check(instance.inputMethod(&client)=="keyboard-us","English preset survives config reload");
    send(FcitxKey_Tab,KeyState::Shift);check(instance.inputMethod(&client)=="tailkana","temporary Japanese switch works with English preset");
    client.focusOut();client.focusIn();check(instance.inputMethod(&client)=="keyboard-us","English preset is independent of temporary Japanese mode");
    engine->setConfig(japaneseDefault);client.focusOut();
    {Client other(instance.inputContextManager());other.focusIn();check(instance.inputMethod(&other)=="tailkana","a newly created input context uses the preset");other.focusOut();}
    client.focusIn();
    for(char c:std::string("nihongo"))send(KeySym(c));
    const auto preedit=client.inputPanel().clientPreedit().toString();
    RawConfig width;width["PanelWidth"]="720";engine->setConfig(width);engine->reloadConfig();
    const auto metadata=nlohmann::json::parse(client.inputPanel().auxDown().toString().substr(9));
    check(metadata["panelWidth"]==720,"custom width reaches the panel and survives reload");
    check(client.inputPanel().clientPreedit().toString()==preedit,"changing width preserves current composition");
    RawConfig stored;engine->getConfig()->save(stored);
    check(stored.get("DefaultInputMode")->value()=="Japanese"&&stored.get("PanelWidth")->value()=="720","both preferences persist independently");
    std::cout<<"PASS: real Fcitx event dispatch, English/Japanese Auto/Full/Half punctuation, Shift+Tab both ways and key releases\n";
}
