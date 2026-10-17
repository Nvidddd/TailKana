// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "roman_buffer.h"
#include "mozc_backend.h"
#include "language_support.h"
#include "ai_worker.h"
#include <fcitx-utils/eventdispatcher.h>
#include <atomic>
#include <fcitx-config/configuration.h>
#include <fcitx-config/option.h>
#include <fcitx-config/iniparser.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/inputcontextproperty.h>
namespace fcitx {
FCITX_CONFIGURATION(TailKanaConfig,
    Option<std::string> defaultInputMode{this,"DefaultInputMode","默认输入方式","Japanese"};
    Option<int,IntConstrain> panelWidth{this,"PanelWidth","候选框宽度（像素）",520,IntConstrain(360,1000)};
    Option<bool> aiEnabled{this,"AiEnabled","实时 AI 上下文推荐",true};
    Option<int,IntConstrain> aiMaxLength{this,"AiMaxLength","最长预测长度（字符）",32,IntConstrain(1,128)};
    Option<int,IntConstrain> aiContextLength{this,"AiContextLength","参考上下文长度（字符）",40,IntConstrain(0,128)};
    Option<bool> showCandidate{this,"ShowCandidate","实时假名预编辑",true};
    Option<bool> meanings{this,"ShowMeaning","右侧显示中文释义",false};
    Option<bool> smartChinese{this,"SmartChinese","智能中文第三候选",false};
    Option<bool> corrections{this,"Corrections","键盘误按纠错",true};
    Option<bool> pinyinCorrections{this,"PinyinCorrections","模糊拼音",true};
    Option<bool> pinyinXi{this,"PinyinXi","模糊拼音：xi→し",true};
    Option<bool> pinyinQi{this,"PinyinQi","模糊拼音：qi→ち",true};
    Option<bool> pinyinCi{this,"PinyinCi","模糊拼音：ci→つ",true};
    Option<bool> pinyinSi{this,"PinyinSi","模糊拼音：si→す",true};
    Option<bool> pinyinZi{this,"PinyinZi","模糊拼音：zi→ず",true};
    Option<bool> pinyinXGlides{this,"PinyinXGlides","模糊拼音：x 系拗音",true};
    Option<bool> pinyinQGlides{this,"PinyinQGlides","模糊拼音：q 系拗音",true};
    Option<bool> pinyinJGlides{this,"PinyinJGlides","模糊拼音：j 系拗音",true};
    Option<std::string> punctuationMode{this,"Punctuation","标点全角／半角","Auto"};
    Option<bool> customColors{this,"CustomColors","启用自定义颜色",false};
    Option<bool> glass{this,"Glass","透明毛玻璃",true};
    Option<std::string> backgroundColor{this,"BackgroundColor","自定义背景色","#202020"};
    Option<std::string> textColor{this,"TextColor","自定义文字色","#edf3ef"};
    Option<std::string> highlightColor{this,"HighlightColor","自定义高亮色","#355b47"};
    Option<int,IntConstrain> opacity{this,"TintOpacity","玻璃底色浓度（%）",20,IntConstrain(0,100)};
    Option<bool> katakana{this,"Katakana","默认使用片假名",false};
    Option<bool> swapKeys{this,"SwapKeys","空格确定，Enter 变换",false};
    Option<bool> leftShift{this,"LeftShift","按住左 Shift 临时切换假名",true};
    Option<bool> rightShift{this,"RightShift","右 Shift ＋方向键修改末尾",true};
    Option<bool> furigana{this,"Furigana","汉字候选显示假名",true};
    Option<std::string> appearance{this,"Appearance","墨绿主题明暗","System"};
    Option<int,IntConstrain> panelScale{this,"PanelScale","面板大小（%）",100,IntConstrain(80,120)};
);
class TailKana;
class TailState : public InputContextProperty {
public:
    tailkana::RomanBuffer buffer;
    std::unique_ptr<tailkana::MozcBackend> backend;
    bool rightShift=false, leftShift=false, katakana=false;
    bool suppressShiftRelease=false;
    bool autoExpanded=false;
    uint64_t layoutRevision=0;
    bool expanded=false, converted=false, showCandidate=false, swapKeys=false;
    std::string collapsedRaw;
    uint64_t aiRevision=0;
    std::string aiKey,aiText,aiReading,aiStatus;
    int aiElapsedMs=0,aiContextCharacters=0;
    struct Choice {
        std::string text, reading; int japaneseIndex=-1; bool chinese=false;
        std::shared_ptr<tailkana::MozcBackend> correctionBackend;
        std::string correctedRoman;
        bool pinyinCorrection=false;
    };
    std::vector<Choice> choices;
    std::vector<int> visibleIndices;
    int chineseIndex=-1, compactPage=0;
    int selected=0;
    tailkana::MozcBackend& mozc() { if(!backend)backend=std::make_unique<tailkana::MozcBackend>(); return *backend; }
};
class TailKana final : public InputMethodEngineV2 {
public:
    explicit TailKana(Instance* instance);
    ~TailKana() override;
    const Configuration* getConfig() const override { return &config_; }
    void setConfig(const RawConfig&) override;
    void reloadConfig() override;
    void keyEvent(const InputMethodEntry&,KeyEvent&) override;
    void reset(const InputMethodEntry&,InputContextEvent&) override;
    void deactivate(const InputMethodEntry&,InputContextEvent&) override;
    void activate(const InputMethodEntry&,InputContextEvent&) override;
    void action(InputContext*,int id);
    void select(InputContext*,int index);
    void refresh(InputContext*,bool query);
    bool handleTypingPolicy(KeyEvent&);
private:
    uint32_t pinyinRuleMask() const;
    uint64_t aiSequence_=0,layoutSequence_=0;
    Instance* instance_;
    TailKanaConfig config_;
    FactoryFor<TailState> factory_;
    std::unique_ptr<HandlerTableEntry<EventHandler>> shiftHandler_;
    std::unique_ptr<HandlerTableEntry<EventHandler>> defaultModeHandler_;
    TailState& state(InputContext* ic) { return *ic->propertyFor(&factory_); }
    void clear(InputContext*);
    void convert(InputContext*);
    void confirm(InputContext*);
    void queueAi(InputContext*);
    EventDispatcher aiDispatcher_;
    std::shared_ptr<std::atomic<bool>> aiAlive_=std::make_shared<std::atomic<bool>>(true);
    std::unique_ptr<tailkana::AiWorker> ai_;
};
class TailKanaFactory : public AddonFactory {
public: AddonInstance* create(AddonManager* manager) override { return new TailKana(manager->instance()); }
};
}
