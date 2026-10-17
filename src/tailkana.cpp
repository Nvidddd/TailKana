// SPDX-License-Identifier: GPL-3.0-or-later
#include "tailkana.h"
#include "typing_policy.h"
#include "roman_correction.h"
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/surroundingtext.h>
#include <fcitx-utils/utf8.h>
#include <nlohmann/json.hpp>
#include "base/init_mozc.h"
#include "base/strings/japanese.h"
#include "base/util.h"
#include <algorithm>
#include <cctype>
#include <tuple>
#include <set>
#include <dlfcn.h>
namespace fcitx {
static void preferencesChanged() {if(auto notify=reinterpret_cast<void(*)()>(dlsym(RTLD_DEFAULT,"tailkana_preferences_changed")))notify();}
class TailWord final:public CandidateWord {
public:
    TailWord(TailKana* engine,int index,const tailkana::Candidate& c):CandidateWord(Text(c.text)),engine_(engine),index_(index){setComment(Text(c.reading));}
    void select(InputContext* ic)const override { engine_->select(ic,index_); }
private: TailKana* engine_; int index_;
};
class TailTabs final:public TabbedCandidateList {
public:
    TailTabs(TailKana* engine,InputContext* ic):engine_(engine),ic_(ic){}
    std::span<const CandidateAction> tabActions() override { return {}; }
    void triggerTabAction(int id)override { engine_->action(ic_,id); }
private: TailKana* engine_; InputContext* ic_;
};
TailKana::TailKana(Instance* instance):instance_(instance),factory_([](InputContext&){return new TailState;}) {
    reloadConfig();
    instance->inputContextManager().registerProperty("tailKanaState",&factory_);
    int argc=1;char name[]="tailkana";char* args[]={name};char** argv=args;mozc::InitMozc(name,&argc,&argv);
    aiDispatcher_.attach(&instance_->eventLoop());
    std::weak_ptr<std::atomic<bool>> alive=aiAlive_;
    ai_=std::make_unique<tailkana::AiWorker>([this,alive](tailkana::AiResult result){
        aiDispatcher_.schedule([this,alive,result=std::move(result)]{
            const auto guard=alive.lock();if(!guard||!guard->load())return;
            if(result.request.revision==0){
                instance_->inputContextManager().foreach([this](InputContext* ic){if(ic->hasFocus()&&!state(ic).buffer.raw.empty())refresh(ic,false);return true;});return;
            }
            auto* ic=instance_->inputContextManager().findByUUID(result.request.uuid);
            if(!ic||!ic->hasFocus()||!*config_.aiEnabled)return;
            auto& s=state(ic);
            if(s.aiRevision!=result.request.revision||s.buffer.raw!=result.request.raw||s.buffer.raw.empty()||ic->capabilityFlags().testAny(CapabilityFlags{CapabilityFlag::Password,CapabilityFlag::Disable}))return;
            s.aiText=result.text;s.aiReading=result.reading;s.aiStatus=result.status;s.aiElapsedMs=result.elapsedMs;
            refresh(ic,false);
        });
    });
    ai_->enable(*config_.aiEnabled);
    // Apply the configured mode on focus/source activation, never on each key.
    // A temporary Shift+Tab/menu switch remains valid in the current field.
    defaultModeHandler_=instance->watchEvent(EventType::InputContextFocusIn,EventWatcherPhase::PostInputMethod,
        [this](Event& event){
            auto* ic=static_cast<InputContextEvent&>(event).inputContext();
            if(ic->capabilityFlags().testAny(CapabilityFlags{CapabilityFlag::Password,CapabilityFlag::Disable}))return;
            const auto* entry=instance_->inputMethodEntry(ic);
            if(!entry || (entry->uniqueName()!="tailkana" && entry->uniqueName()!="keyboard-us"))return;
            // Preserve the frontend's application-specific defaults and its
            // existing same-application focus behavior.
            if(auto* frontend=instance_->addonManager().addon("macosfrontend",false);frontend&&frontend->getConfig()){
                RawConfig frontendConfig;frontend->getConfig()->save(frontendConfig);
                if(auto rules=frontendConfig.get("AppDefaultIM"))for(const auto& name:rules->subItems()){
                    try{
                        auto rule=nlohmann::json::parse(rules->get(name)->value());
                        if(rule.is_object() && rule.contains("appId") && rule["appId"].is_string() && rule["appId"].get<std::string>()==ic->program())return;
                    }catch(const nlohmann::json::exception&){}
                }
            }
            instance_->setCurrentInputMethod(ic,*config_.defaultInputMode=="English"?"keyboard-us":"tailkana",true);
        });
    // Reserve left Shift before Fcitx's default temporary-English shortcut.
    shiftHandler_=instance->watchEvent(EventType::InputContextKeyEvent,EventWatcherPhase::PreInputMethod,
        [this,instance](Event& event) {
            auto& key=static_cast<KeyEvent&>(event);
            if(handleTypingPolicy(key))return;
            const auto* entry=instance->inputMethodEntry(key.inputContext());
            if(!entry || entry->uniqueName()!="tailkana")return;
            const auto mods=key.rawKey().states();
            if(key.rawKey().sym()==FcitxKey_Shift_L && !mods.testAny(KeyStates{KeyState::Ctrl,KeyState::Super,KeyState::Alt}))
                keyEvent(*entry,key);
        });
}
bool TailKana::handleTypingPolicy(KeyEvent& event) {
    auto* ic=event.inputContext();
    if(ic->capabilityFlags().test(CapabilityFlag::Password) || ic->capabilityFlags().test(CapabilityFlag::Disable))return false;
    const auto* entry=instance_->inputMethodEntry(ic);
    if(!entry || (entry->uniqueName()!="tailkana" && entry->uniqueName()!="keyboard-us"))return false;
    auto symbol=event.rawKey().sym();if(symbol==0)symbol=event.origKey().sym();
    const auto modifiers=event.rawKey().states()|event.origKey().states();
    auto& current=state(ic);
    if(event.isRelease() && (symbol==FcitxKey_Shift_L || symbol==FcitxKey_Shift_R) && current.suppressShiftRelease) {
        current.suppressShiftRelease=false;current.leftShift=false;current.rightShift=false;
        event.filterAndAccept();return true;
    }
    if(modifiers.testAny(KeyStates{KeyState::Ctrl,KeyState::Alt,KeyState::Super}))return false;
    if((symbol==FcitxKey_Tab || symbol==FcitxKey_ISO_Left_Tab) && modifiers.test(KeyState::Shift)) {
        if(!event.isRelease()) {
            current.suppressShiftRelease=true;
            instance_->setCurrentInputMethod(ic,entry->uniqueName()=="tailkana"?"keyboard-us":"tailkana",false);
        }
        event.filterAndAccept();return true;
    }
    const bool japanese=entry->uniqueName()=="tailkana";
    // Shift by itself is a modifier in English; mode changes use Shift+Tab.
    if(!japanese && (symbol==FcitxKey_Shift_L || symbol==FcitxKey_Shift_R)) {
        event.filterAndAccept();return true;
    }
    const auto utf8=Key::keySymToUTF8(symbol);
    if(utf8.size()!=1 || !std::ispunct(static_cast<unsigned char>(utf8[0])))return false;
    // These two keys are part of Japanese romanization, not punctuation.
    if(japanese && !current.converted && !current.buffer.raw.empty() &&
       (utf8[0]=='-' || (utf8[0]=='\'' && current.buffer.cursor>0 && current.buffer.raw[current.buffer.cursor-1]=='n')))return false;
    if(!event.isRelease()) {
        if(modifiers.test(KeyState::Shift))current.suppressShiftRelease=true;
        if(japanese && !current.buffer.raw.empty())confirm(ic);
        ic->commitString(tailkana::punctuation(utf8[0],*config_.punctuationMode,japanese));
    }
    event.filterAndAccept();return true;
}
TailKana::~TailKana(){*aiAlive_=false;ai_.reset();aiDispatcher_.detach();}
void TailKana::reloadConfig() { readAsIni(config_, "conf/tailkana.conf");if(ai_)ai_->enable(*config_.aiEnabled); }
void TailKana::queueAi(InputContext* ic){
    auto& s=state(ic);
    if(!*config_.aiEnabled||s.buffer.raw.empty()||ic->capabilityFlags().testAny(CapabilityFlags{CapabilityFlag::Password,CapabilityFlag::Disable})){
        if(!s.aiKey.empty()){s.aiRevision=++aiSequence_;s.aiKey.clear();s.aiText.clear();s.aiReading.clear();s.aiStatus.clear();ai_->cancel(ic->uuid());}
        return;
    }
    std::string context;
    const auto& surrounding=ic->surroundingText();
    if(surrounding.isValid())context=tailkana::contextBefore(surrounding.text(),std::min(surrounding.cursor(),surrounding.anchor()),*config_.aiContextLength);
    s.aiContextCharacters=context.empty()?0:int(utf8::length(context));
    const bool katakana=s.katakana!=s.leftShift;
    const auto key=nlohmann::json::array({s.buffer.raw,context,s.buffer.cursor,*config_.aiMaxLength,katakana}).dump();
    if(key==s.aiKey)return;
    s.aiKey=key;s.aiRevision=++aiSequence_;s.aiText.clear();s.aiReading.clear();s.aiStatus="pending";s.aiElapsedMs=0;
    ai_->submit({ic->uuid(),s.aiRevision,s.buffer.raw,context,*config_.aiMaxLength,katakana});
}
uint32_t TailKana::pinyinRuleMask() const {
    return (*config_.pinyinXi?1u:0u)|(*config_.pinyinQi?2u:0u)|(*config_.pinyinCi?4u:0u)|
        (*config_.pinyinSi?8u:0u)|(*config_.pinyinZi?16u:0u)|(*config_.pinyinXGlides?32u:0u)|
        (*config_.pinyinQGlides?64u:0u)|(*config_.pinyinJGlides?128u:0u);
}
void TailKana::setConfig(const RawConfig& raw) {
    const auto previousInput=std::tuple{*config_.showCandidate,*config_.katakana,*config_.leftShift,*config_.rightShift,*config_.smartChinese,*config_.corrections,*config_.pinyinCorrections,pinyinRuleMask()};
    config_.load(raw,true);
    ai_->enable(*config_.aiEnabled);
    const bool inputChanged=previousInput!=std::tuple{*config_.showCandidate,*config_.katakana,*config_.leftShift,*config_.rightShift,*config_.smartChinese,*config_.corrections,*config_.pinyinCorrections,pinyinRuleMask()};
    safeSaveAsIni(config_, "conf/tailkana.conf");
    instance_->inputContextManager().foreach([this,inputChanged](InputContext* ic) {
        auto& s=state(ic);
        if(inputChanged) {s.leftShift=false;s.converted=false;}
        if(!s.buffer.raw.empty())refresh(ic,inputChanged);
        return true;
    });
}
void TailKana::clear(InputContext* ic) {
    auto& s=state(ic);s.aiRevision=++aiSequence_;s.aiKey.clear();s.aiText.clear();s.aiReading.clear();s.aiStatus.clear();ai_->cancel(ic->uuid());s.buffer.clear();s.choices.clear();s.visibleIndices.clear();s.chineseIndex=-1;s.compactPage=0;s.selected=0;s.expanded=false;s.autoExpanded=false;s.layoutRevision=++layoutSequence_;s.converted=false;s.collapsedRaw.clear();
    if(s.backend)s.backend->clear();ic->inputPanel().reset();ic->updatePreedit();ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}
void TailKana::reset(const InputMethodEntry&,InputContextEvent& e) {
    auto& s=state(e.inputContext());s.rightShift=false;s.leftShift=false;clear(e.inputContext());
}
void TailKana::activate(const InputMethodEntry&,InputContextEvent& e) {
    auto& s=state(e.inputContext());s.rightShift=false;s.leftShift=false;refresh(e.inputContext(),false);
}
void TailKana::deactivate(const InputMethodEntry&,InputContextEvent& e) {
    auto* ic=e.inputContext();auto& s=state(ic);
    if(!s.buffer.raw.empty()) {
        std::string text;
        if(s.converted && s.selected<int(s.choices.size())) {
            const auto& candidate=s.choices[s.selected];
            text=candidate.chinese?candidate.text:(candidate.correctionBackend?candidate.correctionBackend->commit(candidate.japaneseIndex):s.mozc().commit(candidate.japaneseIndex));
        } else text=s.mozc().submitReading();
        if(text.empty())text=s.buffer.raw;ic->commitString(text);
    }
    s.rightShift=false;s.leftShift=false;clear(ic);
}
void TailKana::refresh(InputContext* ic,bool query) {
    auto& s=state(ic);s.showCandidate=*config_.showCandidate;s.katakana=*config_.katakana;s.swapKeys=*config_.swapKeys;auto& panel=ic->inputPanel();panel.reset();
    queueAi(ic);
    if(s.buffer.raw.empty()) {ic->updatePreedit();ic->updateUserInterface(UserInterfaceComponent::InputPanel);return;}
    const bool katakana=s.katakana!=s.leftShift;
    if(query) {
        s.mozc().query(s.buffer.raw,s.converted,katakana);s.selected=0;s.choices.clear();s.chineseIndex=-1;s.compactPage=0;
        for(int i=0;i<int(s.mozc().candidates.size());++i) {
            const auto& c=s.mozc().candidates[i];s.choices.push_back({c.text,c.reading,i,false});
        }
        if(*config_.corrections || *config_.pinyinCorrections) {
            std::set<std::string> seen;for(const auto& choice:s.choices)seen.insert(choice.text);
            int evaluated=0,added=0;
            for(const auto& variant:tailkana::romanCorrections(s.buffer.raw,*config_.pinyinCorrections,*config_.corrections,pinyinRuleMask())) {
                if(++evaluated>4 || added>=2)break;
                auto backend=std::make_shared<tailkana::MozcBackend>();
                if(!backend->query(variant.roman,!variant.kanaOnly,katakana) || backend->candidates.empty())continue;
                int candidateIndex=0;
                if(variant.kanaOnly) {
                    const auto literal=std::find_if(backend->candidates.begin(),backend->candidates.end(),[&](const auto& candidate){return candidate.text==backend->composingReading;});
                    if(literal==backend->candidates.end())continue;
                    candidateIndex=int(literal-backend->candidates.begin());
                } else if(backend->candidates.front().id==-2147483647)continue;
                if(!backend->highlight(candidateIndex))continue;
                const auto text=backend->reading;
                // A correction must resolve to Japanese dictionary text, not a
                // partial Latin remainder or a synthetic reading-only row.
                if(text.empty() || mozc::Util::ContainsScriptType(text,mozc::Util::ALPHABET) ||
                   mozc::Util::ContainsScriptType(text,mozc::Util::NUMBER) ||
                   std::any_of(text.begin(),text.end(),[](unsigned char c){return c<128;}) || !seen.insert(text).second)continue;
                TailState::Choice correction{text,mozc::japanese::KatakanaToHiragana(backend->composingReading),candidateIndex,false,backend,variant.roman,variant.pinyin};
                if(added==0)s.choices.insert(s.choices.begin()+std::min<size_t>(1,s.choices.size()),std::move(correction));
                else s.choices.push_back(std::move(correction));
                ++added;
            }
        }
        if(*config_.smartChinese) {
            auto chinese=tailkana::chineseCandidate(s.buffer.raw);
            if(!chinese.empty()) {s.chineseIndex=int(std::min<size_t>(2,s.choices.size()));s.choices.insert(s.choices.begin()+s.chineseIndex,{chinese,s.buffer.raw,-1,true});}
        }
    }
    if(s.converted && s.selected<int(s.choices.size()) && !s.choices[s.selected].chinese) {
        const auto& candidate=s.choices[s.selected];
        if(candidate.correctionBackend)candidate.correctionBackend->highlight(candidate.japaneseIndex);
        else s.mozc().highlight(candidate.japaneseIndex);
    }
    const auto& choices=s.choices;
    std::string display=s.buffer.raw;
    if(s.converted && s.selected<int(choices.size())) {
        const auto& candidate=choices[s.selected];
        display=candidate.chinese?candidate.text:(candidate.correctionBackend?candidate.correctionBackend->reading:s.mozc().reading);
    }
    else if(s.showCandidate)display=s.mozc().composingReading;
    Text preedit(display);
    int caret=int(display.size());
    if(!s.converted && !s.showCandidate)caret=int(s.buffer.cursor);
    else if(!s.converted && s.buffer.cursor<s.buffer.raw.size()) {
        auto prefix=mozc::japanese::RomanjiToHiragana(s.buffer.raw.substr(0,s.buffer.cursor));
        if(katakana)prefix=mozc::japanese::HiraganaToKatakana(prefix);
        size_t boundary=0;
        while(boundary<prefix.size() && boundary<display.size() && prefix[boundary]==display[boundary])++boundary;
        while(boundary>0 && boundary<display.size() && (static_cast<unsigned char>(display[boundary])&0xc0)==0x80)--boundary;
        caret=int(boundary);
    }
    preedit.setCursor(caret);
    panel.setClientPreedit(preedit);
    auto c=s.buffer.cross();
    if(katakana) {
        c.current.kana=mozc::japanese::HiraganaToKatakana(c.current.kana);
        for(auto& v:c.choices)v.kana=mozc::japanese::HiraganaToKatakana(v.kana);
    }
    const int compactOffset=(s.selected/3)*3;
    const int offset=s.expanded?0:compactOffset;
    std::vector<int> compactIndices;
    if(s.chineseIndex>=0) {
        if(s.selected!=s.chineseIndex)s.compactPage=(s.selected-(s.selected>s.chineseIndex?1:0))/2;
        for(int i=0;i<2;++i) {
            int index=s.compactPage*2+i;if(index>=s.chineseIndex)++index;
            compactIndices.push_back(index<int(choices.size())?index:-1);
        }
        compactIndices.push_back(s.chineseIndex);
    }else{for(int i=compactOffset;i<std::min(compactOffset+3,int(choices.size()));++i)compactIndices.push_back(i);}
    s.visibleIndices.clear();
    if(s.expanded){for(int i=0;i<int(choices.size());++i)s.visibleIndices.push_back(i);}
    else s.visibleIndices=compactIndices;
    s.layoutRevision=++layoutSequence_;
    const auto selectedSlot=std::find(s.visibleIndices.begin(),s.visibleIndices.end(),s.selected)-s.visibleIndices.begin();
    nlohmann::json meta={{"raw",s.buffer.raw},{"cursor",s.buffer.cursor},{"expanded",s.expanded},{"autoExpandAllowed",s.collapsedRaw!=s.buffer.raw},{"autoExpanded",s.autoExpanded},{"compactIndices",compactIndices},
        {"autoExpandAction",-int((s.layoutRevision%500000000)*2+1)},{"autoCollapseAction",-int((s.layoutRevision%500000000)*2+2)},
        {"appearance",*config_.appearance},{"showMeaning",*config_.meanings},{"smartChinese",*config_.smartChinese},
        {"glass",*config_.glass},{"customColors",*config_.customColors},{"backgroundColor",*config_.backgroundColor},{"textColor",*config_.textColor},{"highlightColor",*config_.highlightColor},{"opacity",*config_.opacity},{"furigana",*config_.furigana},{"panelScale",*config_.panelScale},{"panelWidth",*config_.panelWidth},{"leftShiftEnabled",*config_.leftShift},{"rightShiftEnabled",*config_.rightShift},{"kanaMode",s.showCandidate},{"selected",selectedSlot},{"candidateOffset",offset},
        {"katakana",katakana},{"swapKeys",s.swapKeys},{"converted",s.converted},{"candidateTotal",choices.size()},
        {"current",{{"roman",c.current.roman},{"kana",c.current.kana},{"length",c.current.length}}}};
    if(*config_.aiEnabled && !ic->capabilityFlags().testAny(CapabilityFlags{CapabilityFlag::Password,CapabilityFlag::Disable})) {
        meta["ai"]={{"text",s.aiText},{"reading",s.aiReading},{"status",ai_->status()=="ready"?s.aiStatus:ai_->status()},{"action",10000+int(s.aiRevision%2000000000)},{"elapsedMs",s.aiElapsedMs},{"contextCharacters",s.aiContextCharacters},{"maxLength",*config_.aiMaxLength}};
    }
    meta["chineseSlots"]=nlohmann::json::array();
    for(int i=0;i<int(s.visibleIndices.size());++i)if(s.visibleIndices[i]>=0 && choices[s.visibleIndices[i]].chinese)meta["chineseSlots"].push_back(i);
    meta["emptySlots"]=nlohmann::json::array();for(int i=0;i<int(s.visibleIndices.size());++i)if(s.visibleIndices[i]<0)meta["emptySlots"].push_back(i);
    meta["corrections"]=nlohmann::json::object();
    for(int i=0;i<int(s.visibleIndices.size());++i)if(s.visibleIndices[i]>=0 && !choices[s.visibleIndices[i]].correctedRoman.empty())meta["corrections"][std::to_string(i)]=choices[s.visibleIndices[i]].correctedRoman;
    meta["pinyinCorrectionSlots"]=nlohmann::json::array();
    for(int i=0;i<int(s.visibleIndices.size());++i)if(s.visibleIndices[i]>=0 && choices[s.visibleIndices[i]].pinyinCorrection)meta["pinyinCorrectionSlots"].push_back(i);
    if(*config_.meanings && !choices.empty()) {
        const auto& selected=choices[std::clamp(s.selected,0,int(choices.size())-1)];
        auto definition=selected.chinese?tailkana::Definition{"拼音候选："+s.buffer.raw,{}}:tailkana::chineseDefinition(selected.text,selected.reading);
        meta["definition"]={{"word",selected.text},{"reading",selected.reading},{"text",definition.text},{"source",definition.source}};
    }
    meta["choices"]=nlohmann::json::array();
    for(auto& v:c.choices)meta["choices"].push_back({{"roman",v.roman},{"kana",v.kana}});
    panel.setAuxDown(Text("tailkana:"+meta.dump()));
    auto list=std::make_unique<CommonCandidateList>();list->setPageSize(500);list->setLayoutHint(CandidateLayoutHint::Horizontal);
    const int count=int(s.visibleIndices.size());
    std::vector<std::string> labels;
    for(int i=0;i<count;++i) {
        int index=s.visibleIndices[i];
        const auto c=index>=0?choices[index]:TailState::Choice{"—","",-1,false};
        list->append(std::make_unique<TailWord>(this,index,tailkana::Candidate{0,c.text,"",c.chinese?"拼音："+c.reading:c.reading}));labels.push_back(std::to_string(i+1));
    }
    list->setLabels(labels);list->setGlobalCursorIndex(count>0?int(selectedSlot):-1);
    list->setTabbedImpl(std::make_unique<TailTabs>(this,ic));panel.setCandidateList(std::move(list));
    ic->updatePreedit();ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}
void TailKana::select(InputContext* ic,int index) {
    auto& s=state(ic);if(index<0 || index>=int(s.choices.size()))return;
    const auto& candidate=s.choices[index];
    const auto text=candidate.chinese?candidate.text:(candidate.correctionBackend?candidate.correctionBackend->commit(candidate.japaneseIndex):s.mozc().commit(candidate.japaneseIndex));if(text.empty())return;ic->commitString(text);clear(ic);
}
void TailKana::convert(InputContext* ic) {
    auto& s=state(ic);if(s.buffer.raw.empty())return;
    if(!s.converted) {s.converted=true;refresh(ic,true);}
    else {s.selected=(s.selected+1)%std::max(1,int(s.choices.size()));refresh(ic,false);}
}
void TailKana::confirm(InputContext* ic) {
    auto& s=state(ic);if(s.buffer.raw.empty())return;
    if(s.converted)select(ic,s.selected);
    else {auto text=s.mozc().submitReading();if(text.empty())text=s.buffer.raw;ic->commitString(text);clear(ic);}
}
void TailKana::action(InputContext* ic,int id) {
    auto& s=state(ic);
    if(id<0){
        const auto code=-int64_t(id);
        if(uint64_t((code-1)/2)!=s.layoutRevision%500000000)return;
        id=(code%2)?209:211;
    }
    if(id>=10000){
        if(*config_.aiEnabled && id==10000+int(s.aiRevision%2000000000) && !s.buffer.raw.empty() && !s.aiText.empty() && !ic->capabilityFlags().testAny(CapabilityFlags{CapabilityFlag::Password,CapabilityFlag::Disable})){
            const auto text=s.aiText;ic->commitString(text);clear(ic);
        }
        return;
    }
    if(id==207 && !s.choices.empty()) {
        const auto& c=s.choices[std::clamp(s.selected,0,int(s.choices.size())-1)];
        const auto definition=tailkana::chineseDefinition(c.text,c.reading);
        if(!definition.source.empty())if(auto open=reinterpret_cast<void(*)(const char*)>(dlsym(RTLD_DEFAULT,"tailkana_open_dictionary")))open(definition.source.c_str());
        return;
    }
    if(id==208) {config_.meanings.setValue(!*config_.meanings);safeSaveAsIni(config_,"conf/tailkana.conf");preferencesChanged();refresh(ic,false);return;}
    if(id==206) {if(auto open=reinterpret_cast<void(*)()>(dlsym(RTLD_DEFAULT,"tailkana_open_settings")))open();return;}
    // These preferences remain usable before the first letter and across commits.
    if(id==201) {s.showCandidate=!s.showCandidate;config_.showCandidate.setValue(s.showCandidate);safeSaveAsIni(config_,"conf/tailkana.conf");preferencesChanged();s.converted=false;refresh(ic,!s.buffer.raw.empty());return;}
    if(id==202) {s.katakana=!s.katakana;config_.katakana.setValue(s.katakana);safeSaveAsIni(config_,"conf/tailkana.conf");preferencesChanged();s.converted=false;refresh(ic,!s.buffer.raw.empty());return;}
    if(id==203) {s.swapKeys=!s.swapKeys;config_.swapKeys.setValue(s.swapKeys);safeSaveAsIni(config_,"conf/tailkana.conf");preferencesChanged();refresh(ic,false);return;}
    if(s.buffer.raw.empty())return;
    if(id>=100 && id<104) {if(s.buffer.replaceTail(id-100)) {s.converted=false;refresh(ic,true);}return;}
    if(id==200) {s.autoExpanded=false;s.expanded=!s.expanded;if(!s.expanded)s.collapsedRaw=s.buffer.raw;refresh(ic,false);}
    // Native rendering reports actual text overflow; expanding must also change
    // the engine's visible indices so numeric selection matches the new list.
    if(id==209 && !s.expanded && s.collapsedRaw!=s.buffer.raw) {s.expanded=true;s.autoExpanded=true;refresh(ic,false);}
    if(id==211 && s.expanded && s.autoExpanded){s.expanded=false;s.autoExpanded=false;refresh(ic,false);}
    if(id==204)convert(ic);
    if(id==205)confirm(ic);
}
void TailKana::keyEvent(const InputMethodEntry&,KeyEvent& e) {
    auto* ic=e.inputContext();auto& s=state(ic);const auto sym=e.key().sym();const auto mods=e.rawKey().states();
    s.showCandidate=*config_.showCandidate;s.katakana=*config_.katakana;s.swapKeys=*config_.swapKeys;
    if(sym==FcitxKey_Shift_L && !mods.testAny(KeyStates{KeyState::Ctrl,KeyState::Super,KeyState::Alt})) {
        if(*config_.leftShift) {s.leftShift=!e.isRelease();s.converted=false;refresh(ic,!s.buffer.raw.empty());} e.filterAndAccept();return;
    }
    if(sym==FcitxKey_Shift_R) {s.rightShift=!e.isRelease();if(!s.buffer.raw.empty())e.filterAndAccept();return;}
    if(e.isRelease())return;
    if(!mods.test(KeyState::Shift)) {s.rightShift=false;if(s.leftShift){s.leftShift=false;refresh(ic,!s.buffer.raw.empty());}}
    if(*config_.aiEnabled && mods.test(KeyState::Alt) && !mods.testAny(KeyStates{KeyState::Ctrl,KeyState::Super,KeyState::Shift}) && (sym==FcitxKey_Return||sym==FcitxKey_KP_Enter) && !s.buffer.raw.empty()){
        action(ic,10000+int(s.aiRevision%2000000000));e.filterAndAccept();return;
    }
    if(mods.test(KeyState::Super)||mods.test(KeyState::Ctrl)||mods.test(KeyState::Alt))return;
    if(*config_.rightShift && s.rightShift && mods.test(KeyState::Shift)) {
        int d=sym==FcitxKey_Up?0:sym==FcitxKey_Left?1:sym==FcitxKey_Right?2:sym==FcitxKey_Down?3:-1;
        if(d>=0 && !s.buffer.raw.empty()) {action(ic,100+d);e.filterAndAccept();return;}
    }
    if((sym>=FcitxKey_a && sym<=FcitxKey_z) || (sym>=FcitxKey_A && sym<=FcitxKey_Z)) {
        s.buffer.insert(char(sym>=FcitxKey_A && sym<=FcitxKey_Z?sym-FcitxKey_A+FcitxKey_a:sym));
        s.converted=false;refresh(ic,true);e.filterAndAccept();return;
    }
    if(s.buffer.raw.empty())return;
    if(sym==FcitxKey_Escape) {if(s.expanded){s.expanded=false;s.autoExpanded=false;s.collapsedRaw=s.buffer.raw;refresh(ic,false);}else clear(ic);}
    else if(sym==FcitxKey_Tab && !mods.test(KeyState::Shift))action(ic,200);
    else if(sym==FcitxKey_space && (!mods.test(KeyState::Shift)||s.leftShift)) {if(s.swapKeys)confirm(ic);else convert(ic);}
    else if(sym==FcitxKey_Return || sym==FcitxKey_KP_Enter) {if(s.swapKeys)convert(ic);else confirm(ic);}
    else if(sym==FcitxKey_BackSpace) {s.buffer.backspace();s.converted=false;refresh(ic,true);}
    else if(sym==FcitxKey_Delete) {s.buffer.erase();s.converted=false;refresh(ic,true);}
    else if(sym==FcitxKey_Left || sym==FcitxKey_Right) {s.buffer.move(sym==FcitxKey_Left?-1:1);s.converted=false;refresh(ic,true);}
    else if(sym==FcitxKey_Home || sym==FcitxKey_Up) {s.buffer.cursor=0;s.converted=false;refresh(ic,true);}
    else if(sym==FcitxKey_End || sym==FcitxKey_Down) {s.buffer.cursor=s.buffer.raw.size();s.converted=false;refresh(ic,true);}
    else if(sym>=FcitxKey_1 && sym<=FcitxKey_9 && !mods.test(KeyState::Shift)) {
        const int slot=int(sym-FcitxKey_1);if(slot<int(s.visibleIndices.size()))select(ic,s.visibleIndices[slot]);
    }
    else if(sym==FcitxKey_apostrophe || sym==FcitxKey_minus || sym==FcitxKey_comma || sym==FcitxKey_period) {s.buffer.insert(char(sym));s.converted=false;refresh(ic,true);}
    else return;
    e.filterAndAccept();
}
}
FCITX_ADDON_FACTORY_V2(tailkana,fcitx::TailKanaFactory);
