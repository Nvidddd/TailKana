// SPDX-License-Identifier: GPL-3.0-or-later
#include "tailkana.h"
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>
using namespace fcitx;
class LatencyClient final:public InputContext {
public:
 explicit LatencyClient(InputContextManager& manager):InputContext(manager,"latency-test"){created();setCapabilityFlags(CapabilityFlag::Preedit);}
 ~LatencyClient(){destroy();}
 const char* frontend()const override{return "latency-test";}
 void commitStringImpl(const std::string&)override{}
 void deleteSurroundingTextImpl(int,unsigned int)override{}
 void forwardKeyImpl(const ForwardKeyEvent&)override{}
 void updatePreeditImpl()override{}
};
int main(){
 Instance instance(0,nullptr);TailKana engine(&instance);LatencyClient client(instance.inputContextManager());InputMethodEntry entry("tailkana","TailKana","ja","tailkana");
 nlohmann::json result=nlohmann::json::array();
 for(const auto* profile:{"plain","corrections","user"}){
  RawConfig config;config["AiEnabled"]="False";config["Corrections"]=std::string(profile)=="plain"?"False":"True";config["PinyinCorrections"]=std::string(profile)=="plain"?"False":"True";config["SmartChinese"]=std::string(profile)=="user"?"True":"False";config["ShowMeaning"]=std::string(profile)=="user"?"True":"False";engine.setConfig(config);
  for(int pass=0;pass<2;++pass)for(const auto* roman:{"watasihanihongowobenkyoushiteimasu","ashitahatoukyounoatarashiikaishadehatarakimasu","qikatetsudenihongowobenkyoushimasu"}){
   InputContextEvent reset(&client,EventType::InputContextReset);engine.reset(entry,reset);int length=0;
   for(char c:std::string(roman)){
    const auto begin=std::chrono::steady_clock::now();KeyEvent key(&client,Key(KeySym(c)),false);engine.keyEvent(entry,key);
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    if(pass)result.push_back({{"profile",profile},{"length",++length},{"ms",ms}});else ++length;
   }
  }
 }
 std::cout<<result.dump(2)<<std::endl;
}
