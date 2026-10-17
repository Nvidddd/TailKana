// SPDX-License-Identifier: GPL-3.0-or-later
#include "tailkana.h"
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/surroundingtext.h>
#include <fcitx/addonloader.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputmethodgroup.h>

#include <fcitx-utils/event.h>
#include <fcitx-utils/utf8.h>
#include <fcitx-utils/misc.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <cstdlib>
fcitx::StaticAddonRegistry& getAiAddons(){static fcitx::StaticAddonRegistry registry;return registry;}
using namespace fcitx;
static void check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
class AiClient final:public InputContext {
public:
 AiClient(InputContextManager& m):InputContext(m,"tailkana-ai-test"){created();setCapabilityFlags(CapabilityFlags{CapabilityFlag::Preedit,CapabilityFlag::SurroundingText});focusIn();}
 ~AiClient(){destroy();}
 const char* frontend()const override{return "tailkana-ai-test";}
 std::string committed;
 void commitStringImpl(const std::string& s)override{committed+=s;}
 void deleteSurroundingTextImpl(int,unsigned int)override{}
 void forwardKeyImpl(const ForwardKeyEvent&)override{}
 void updatePreeditImpl()override{}
};
int main(){
 check(tailkana::contextBefore("aあ𠮷z",3,2)=="あ𠮷","context limit must use codepoints at cursor");
 check(tailkana::contextBefore("aあ𠮷z",3,0).empty(),"zero context means no preceding text");
 Instance instance(0,nullptr);instance.addonManager().registerDefaultLoader(&getAiAddons());instance.initialize();
 auto* loaded=dynamic_cast<TailKana*>(instance.addonManager().addon("tailkana",false));check(loaded,"AI engine must load");TailKana& engine=*loaded;
 InputMethodGroup group("AITest");group.inputMethodList().emplace_back("keyboard-us");group.inputMethodList().emplace_back("tailkana");group.setDefaultInputMethod("tailkana");
 if(!instance.inputMethodManager().group("AITest"))instance.inputMethodManager().addEmptyGroup("AITest");instance.inputMethodManager().setGroup(group);instance.inputMethodManager().setCurrentGroup("AITest");
 AiClient client(instance.inputContextManager());instance.setCurrentInputMethod(&client,"tailkana",false);InputMethodEntry entry("tailkana","TailKana","ja","tailkana");
 RawConfig config;config["AiEnabled"]="True";config["AiMaxLength"]="32";config["AiContextLength"]="40";config["Corrections"]="False";config["PinyinCorrections"]="False";config["SmartChinese"]="False";engine.setConfig(config);
 auto type=[&](const std::string& text){for(char c:text){KeyEvent event(&client,Key(KeySym(c)),false);engine.keyEvent(entry,event);check(event.accepted(),"typing accepted");}};
 auto meta=[&](){return nlohmann::json::parse(client.inputPanel().auxDown().toString().substr(9));};
 auto reset=[&]{InputContextEvent event(&client,EventType::InputContextReset);engine.reset(entry,event);client.committed.clear();};
 auto context=[&](const std::string& text){client.surroundingText().setText(text,fcitx::utf8::length(text),fcitx::utf8::length(text));};
 auto wait=[&](const auto& predicate){
  bool success=false;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);
  auto timer=instance.eventLoop().addTimeEvent(CLOCK_MONOTONIC,now(CLOCK_MONOTONIC)+10000,0,[&](EventSourceTime* source,uint64_t){if(predicate()){success=true;instance.eventLoop().exit();return false;}if(std::chrono::steady_clock::now()>end){instance.eventLoop().exit();return false;}source->setNextInterval(10000);source->setEnabled(true);return true;});
  timer->setAccuracy(10000);timer->setNextInterval(10000);
  instance.eventLoop().exec();check(success,"AI asynchronous result deadline");
 };
 context("ご飯を食べるために");type("hashi");
 const auto first=client.inputPanel().candidateList()->candidate(0).text().toString();
 wait([&]{return meta()["ai"]["text"]=="箸";});
 check(client.inputPanel().candidateList()->candidate(0).text().toString()==first,"AI must not reorder numbered candidates");
 const int oldAction=meta()["ai"]["action"];
 KeyEvent extra(&client,Key(FcitxKey_a),false);engine.keyEvent(entry,extra);
 engine.action(&client,oldAction);check(client.committed.empty(),"stale AI click must not submit newer input");
 reset();context("川を渡るために");type("hashi");wait([&]{return meta()["ai"]["text"]=="橋";});
 KeyEvent confirm(&client,Key(FcitxKey_Return,KeyState::Alt),false);engine.keyEvent(entry,confirm);
 check(confirm.accepted()&&client.committed=="橋"&&client.inputPanel().empty(),"Option Enter commits exactly the displayed AI text");
 reset();RawConfig shortLimit;shortLimit["AiMaxLength"]="1";engine.setConfig(shortLimit);type("nihongo");wait([&]{return meta()["ai"]["status"]=="length";});check(meta()["ai"]["text"]=="","length cap must not truncate or submit words");
 reset();RawConfig noContext;noContext["AiMaxLength"]="32";noContext["AiContextLength"]="0";engine.setConfig(noContext);type("hashi");wait([&]{return meta()["ai"]["status"]=="ready";});
 check(meta()["ai"]["contextCharacters"]==0,"zero context setting must reach the model request");
 RawConfig off;off["AiEnabled"]="False";engine.setConfig(off);check(!meta().contains("ai"),"off setting must remove the AI strip immediately");
 reset();std::cout<<"PASS: native async IME AI, independent slots, UTF-8/context/length limits, stale click and Option Enter\n";
}
