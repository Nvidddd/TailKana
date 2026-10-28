// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "win32/tip/tailkana_windows.h"
#include <algorithm>
#include <atomic>
#include <climits>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <wil/com.h>
#include "win32/tip/mozc_converter.h"
#include "protocol/candidate_window.pb.h"
#include "win32/tailkana/panel.h"
#include "win32/tailkana/session.h"
#include "win32/tip/tip_edit_session.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_status.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_range_util.h"
#include "win32/tip/tip_thread_context.h"
#include "win32/tip/tip_input_mode_manager.h"
namespace mozc::win32::tsf {
namespace tk = ::tailkana::windows;
namespace {
constexpr uint64_t OutputMarker = 0x5441494c4b414e41ULL;
class ModuleLifetime final : public TipComImplements<IUnknown> {};
constexpr int CandidateStart = 500000000;
constexpr int CandidateEnd = CandidateStart+4096*100000;
std::atomic<uint32_t> candidateSequence{0};
size_t chars(const std::string& text) { return std::count_if(text.begin(),text.end(),[](unsigned char c) { return (c&0xc0)!=0x80; }); }
std::optional<tk::Stroke> stroke(WPARAM key,LPARAM lparam,const BYTE* states,bool down) {
    using K = tk::Key;
    tk::Stroke result{K::Character};result.release = !down;
    result.shift = (states[VK_SHIFT]&0x80)!=0;result.rightShift = (states[VK_RSHIFT]&0x80)!=0;
    switch (key) {
    case VK_BACK: result.key=K::Backspace;break;case VK_DELETE: result.key=K::Delete;break;
    case VK_LEFT: result.key=K::Left;break;case VK_RIGHT: result.key=K::Right;break;case VK_UP: result.key=K::Up;break;case VK_DOWN: result.key=K::Down;break;
    case VK_HOME: result.key=K::Home;break;case VK_END: result.key=K::End;break;case VK_SPACE: result.key=K::Space;break;
    case VK_RETURN: result.key=K::Enter;break;case VK_ESCAPE: result.key=K::Escape;break;case VK_TAB: result.key=K::Tab;break;
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
        result.key=K::Shift;result.rightShift = key==VK_RSHIFT || ((key==VK_SHIFT) && ((lparam>>16)&0xff)==0x36);break;
    default:
        if (key>='A' && key<='Z') result.character=static_cast<char>('a'+key-'A');
        else {
            wchar_t text[8]{};
            const int length = ToUnicode(static_cast<UINT>(key),static_cast<UINT>((lparam>>16)&0xff),states,text,8,4);
            if (length!=1 || text[0]<0x20 || text[0]>0x7e) return std::nullopt;
            result.character = static_cast<char>(text[0]);
        }
    }
    return result;
}
bool sensitive(TipTextService* service) {auto* thread=service->GetThreadContext();return !thread || !thread->GetInputModeManager() || thread->GetInputModeManager()->TailKanaSensitiveInput();}
bool focused(TipTextService* service,ITfContext* context,bool requireThreadFocus=true) {
    if (!service->GetThreadManager()) return false;
    BOOL active=FALSE;if (requireThreadFocus && (FAILED(service->GetThreadManager()->IsThreadFocus(&active)) || !active)) return false;
    wil::com_ptr_nothrow<ITfDocumentMgr> current,owner;
    return SUCCEEDED(service->GetThreadManager()->GetFocus(&current)) && current && SUCCEEDED(context->GetDocumentMgr(&owner)) && current.get()==owner.get();
}
constexpr wchar_t SettingsKey[] = L"Software\\TailKana\\TailKana\\Panel";
DWORD setting(const wchar_t* name,DWORD fallback) {
    DWORD value=0,size=sizeof(value);if (RegGetValueW(HKEY_CURRENT_USER,SettingsKey,name,RRF_RT_REG_DWORD,nullptr,&value,&size)!=ERROR_SUCCESS) return fallback;return value;
}
void save(const wchar_t* name,DWORD value) {
    HKEY key=nullptr;if (RegCreateKeyExW(HKEY_CURRENT_USER,SettingsKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)==ERROR_SUCCESS) {RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<BYTE*>(&value),sizeof(value));RegCloseKey(key);}
}
}
class TailKanaWindows::Impl : public std::enable_shared_from_this<Impl> {
public:
    explicit Impl(client::ClientInterface* client) : session_([client,first=true]() mutable -> std::shared_ptr<tk::Converter> { auto converter = MakeTailKanaConverter(first ? client:nullptr);first = false;return converter; }) {
        auto& options=session_.options;
        options.width=std::clamp<int>(setting(L"Width",520),360,1000);options.katakana=setting(L"Katakana",0)!=0;options.corrections=setting(L"Corrections",1)!=0;options.fuzzy=setting(L"Fuzzy",1)!=0;
        options.fuzzyGroups=setting(L"FuzzyGroups",255)&255;options.furigana=setting(L"Furigana",1)!=0;options.swapKeys=setting(L"SwapKeys",0)!=0;options.showKana=setting(L"ShowKana",1)!=0;
        theme_=setting(L"Theme",0);
    }
    void initialize() {
        std::weak_ptr<Impl> weak=shared_from_this();panel_=std::make_unique<tk::Panel>([weak](int action,uint64_t revision) { if (auto self=weak.lock()) self->action(action,revision); });
        instances().push_back(weak);
    }
    // A POD TLS pointer avoids registering a C++ TLS destructor whose code
    // could be called after a host unloads the input-method DLL.
    static std::vector<std::weak_ptr<Impl>>*& storage() {static thread_local std::vector<std::weak_ptr<Impl>>* values=nullptr;return values;}
    static std::vector<std::weak_ptr<Impl>>& instances() {auto*& values=storage();if (!values) values=new std::vector<std::weak_ptr<Impl>>;return *values;}
    static void remove(Impl* target) {
        auto*& values=storage();if (!values) return;
        std::erase_if(*values,[&](const auto& weak) {auto value=weak.lock();return !value || value.get()==target;});
        if (values->empty()) {delete values;values=nullptr;}
    }
    void hide() {if (panel_) panel_->hide();service_=nullptr;context_=nullptr;}
    commands::Output output(const tk::Effect& effect) {
        commands::Output value;value.set_id(OutputMarker);value.set_consumed(true);
        value.mutable_status()->set_activated(session_.options.japanese);
        value.mutable_status()->set_mode(!session_.options.japanese ? commands::DIRECT:session_.katakana() ? commands::FULL_KATAKANA:commands::HIRAGANA);
        lastPublishedMode_=value.status().mode();
        if (!effect.commit.empty()) {auto* result=value.mutable_result();result->set_type(commands::Result::STRING);result->set_value(effect.commit);}
        if (session_.active()) {
            const auto text = session_.converted || session_.options.showKana ? session_.preedit:session_.buffer.raw;
            auto* edit=value.mutable_preedit();edit->set_cursor(static_cast<uint32_t>(std::min(chars(text),session_.caret())));
            auto* part=edit->add_segment();part->set_annotation(session_.converted ? commands::Preedit::Segment::HIGHLIGHT:commands::Preedit::Segment::UNDERLINE);part->set_value(text);part->set_value_length(static_cast<uint32_t>(chars(text)));part->set_key(session_.reading);
            if (generation_!=session_.generation()) {generation_=session_.generation();candidateBase_=CandidateStart+static_cast<int>((++candidateSequence)%100000)*4096;}
            if (!session_.candidates.empty()) {
                auto* window=value.mutable_candidate_window();window->set_size(static_cast<uint32_t>(session_.candidates.size()));window->set_position(0);window->set_page_size(3);window->set_category(session_.converted ? commands::CONVERSION:commands::SUGGESTION);
                auto* all=value.mutable_all_candidate_words();all->set_category(window->category());
                if (session_.converted) {window->set_focused_index(static_cast<uint32_t>(session_.selected));all->set_focused_index(static_cast<uint32_t>(session_.selected));}
                for (size_t i=0;i<session_.candidates.size();++i) {
                    const auto& item=session_.candidates[i];auto* word=all->add_candidates();word->set_id(candidateBase_+static_cast<int>(i));word->set_index(static_cast<uint32_t>(i));word->set_value(item.word.text);word->set_key(item.word.reading);word->set_num_segments_in_candidate(1);
                    auto* candidate=window->add_candidate();candidate->set_id(word->id());candidate->set_index(static_cast<uint32_t>(i));candidate->set_value(word->value());candidate->mutable_annotation()->set_description(item.word.reading);
                }
            }
        }
        return value;
    }
    bool process(TipTextService* service,ITfContext* context,bool test,bool open,commands::CompositionMode mode,bool down,WPARAM key,LPARAM lparam,const BYTE* states,BOOL* eaten) {
        if (sensitive(service) || nativeComposition_ || (open && mode!=commands::HIRAGANA && mode!=commands::FULL_KATAKANA)) return false;
        if (mode!=lastPublishedMode_) session_.options.katakana=mode==commands::FULL_KATAKANA;
        session_.options.japanese=open;
        const bool shortcut=(states[VK_CONTROL]&0x80) || (states[VK_MENU]&0x80) || (states[VK_LWIN]&0x80) || (states[VK_RWIN]&0x80);
        const auto input=shortcut ? std::optional<tk::Stroke>{}:stroke(key,lparam,states,down);
        const bool recognized=input && session_.accepts(*input);
        const bool pass=down && session_.active() && !recognized && key!=VK_SHIFT && key!=VK_LSHIFT && key!=VK_RSHIFT;
        if (!recognized && !pass) {
            if (!open && (!input || input->key!=tk::Key::Tab)) {*eaten=FALSE;return true;}
            return false;
        }
        *eaten=TRUE;
        if (test) return true;
        const auto effect=pass ? session_.action(-5):session_.handle(*input);
        if (effect.handled || !effect.commit.empty()) TipEditSession::OnOutputReceivedSync(service,context,output(effect));
        if (pass) *eaten=FALSE;
        return true;
    }
    bool finish(TipTextService* service,ITfContext* context,bool cancel) {
        if (!session_.active()) return false;
        session_.options.japanese=TipStatus::IsOpen(service->GetThreadManager());
        tk::Effect effect{true,true,{}};
        if (cancel) session_.clear();else effect=session_.action(-5);
        TipEditSession::OnOutputReceivedAsync(service,context,output(effect));return true;
    }
    bool candidate(TipTextService* service,ITfContext* context,int id,bool submit) {
        if (id<CandidateStart || id>=CandidateEnd) return false;
        if (sensitive(service) || !focused(service,context) || TipStatus::IsDisabledContext(context)) return true;
        const auto index=id-candidateBase_;
        if (index<0 || index>=static_cast<int>(session_.candidates.size())) return true;
        const auto effect=submit ? session_.choose(static_cast<size_t>(index)):session_.highlight(static_cast<size_t>(index));
        TipEditSession::OnOutputReceivedAsync(service,context,output(effect));return true;
    }
    bool dark() const {
        if (theme_==1) return false;if (theme_==2) return true;
        DWORD light=1,size=sizeof(light);
        RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);return !light;
    }
    bool render(TipTextService* service,ITfContext* context,const commands::RendererCommand& command) {
        if (sensitive(service) || TipStatus::IsDisabledContext(context)) {hide();return true;}
        if (command.output().id()!=OutputMarker) {
            if (session_.active()) session_.clear(false);nativeComposition_=command.output().has_preedit();hide();return false;
        }
        nativeComposition_=false;
        if (!session_.active()) {hide();return true;}
        if (!command.visible() || !command.has_application_info() || !command.application_info().has_composition_target()) {hide();return true;}
        const auto& anchor=command.application_info().composition_target();
        if (!anchor.has_top_left()) {hide();return true;}
        service_=service;context_=context;
        RECT rect{anchor.top_left().x(),anchor.top_left().y(),anchor.top_left().x()+1,anchor.top_left().y()+static_cast<LONG>(std::max<uint32_t>(1,anchor.line_height()))};
        panel_->setDark(dark());panel_->update(session_,rect);return true;
    }
    void action(int id,uint64_t revision) {
        if (!service_ || !context_ || revision!=session_.revision()) return;
        wil::com_ptr_nothrow<TipTextService> service{service_};
        wil::com_ptr_nothrow<ITfContext> context{context_};
        if (id==-10) {
            HMENU menu=CreatePopupMenu();auto& options=session_.options;
            auto item=[&](UINT code,const wchar_t* title,bool checked=false) {AppendMenuW(menu,MF_STRING|(checked ? MF_CHECKED:0),code,title);};
            item(1,L"日文输入",options.japanese);item(2,L"英文直输",!options.japanese);AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
            item(3,L"片假名",options.katakana);item(4,L"实时假名",options.showKana);item(5,L"空格确定 / Enter 变换",options.swapKeys);
            item(6,L"键盘误按纠错 · E",options.corrections);item(7,L"模糊拼音 · B",options.fuzzy);item(8,L"假名标注",options.furigana);AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
            item(10,L"主题：跟随系统",theme_==0);item(11,L"主题：浅色",theme_==1);item(12,L"主题：深色",theme_==2);
            HMENU widths=CreatePopupMenu();const int values[]{360,520,720,1000};for (int i=0;i<4;++i) AppendMenuW(widths,MF_STRING|(options.width==values[i] ? MF_CHECKED:0),20+i,std::to_wstring(values[i]).c_str());
            AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(widths),L"候选框宽度");
            item(30,L"默认日文输入",setting(L"DefaultJapanese",1)!=0);item(31,L"默认英文直输",setting(L"DefaultJapanese",1)==0);
            POINT point{};GetCursorPos(&point);const UINT choice=TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,point.x,point.y,panel_->window(),nullptr);DestroyMenu(menu);
            if (!service_ || !context_ || choice==0) return;
            if (choice==1 || choice==2) {if (options.japanese!=(choice==1)) id=-6;else return;}
            else if (choice==3) id=-2;else if (choice==4) id=-7;else if (choice==5) id=-3;
            else {
                if (choice==6) options.corrections=!options.corrections;else if (choice==7) options.fuzzy=!options.fuzzy;else if (choice==8) options.furigana=!options.furigana;
                else if (choice>=10 && choice<=12) theme_=choice-10;else if (choice>=20 && choice<=23) options.width=values[choice-20];else if (choice==30 || choice==31) save(L"DefaultJapanese",choice==30);
                session_.refresh();persist();TipEditSession::OnOutputReceivedAsync(service.get(),context.get(),output({true,true,{}}));return;
            }
        }
        const auto effect=session_.action(id);persist();
        if (effect.handled) TipEditSession::OnOutputReceivedAsync(service.get(),context.get(),output(effect));
    }
    void persist() {
        const auto& options=session_.options;save(L"Width",options.width);save(L"Katakana",options.katakana);save(L"Corrections",options.corrections);save(L"Fuzzy",options.fuzzy);save(L"Furigana",options.furigana);save(L"SwapKeys",options.swapKeys);save(L"ShowKana",options.showKana);save(L"Theme",theme_);
    }
    tk::Session session_;
    std::unique_ptr<tk::Panel> panel_;
    TipTextService* service_=nullptr;
    ITfContext* context_=nullptr;
    uint64_t generation_=UINT64_MAX;
    int candidateBase_=0;
    DWORD theme_=0;
    bool nativeComposition_=false;
    commands::CompositionMode lastPublishedMode_=commands::HIRAGANA;
};
TailKanaWindows::TailKanaWindows(client::ClientInterface* client) : impl_(new Impl(client),[lifetime=wil::com_ptr_nothrow<IUnknown>{new ModuleLifetime}](Impl* value) {(void)lifetime;delete value;}) {impl_->initialize();}
TailKanaWindows::~TailKanaWindows() {impl_->hide();Impl::remove(impl_.get());}
bool TailKanaWindows::ProcessKey(TipTextService* service,ITfContext* context,bool test,bool open,commands::CompositionMode mode,bool down,WPARAM key,LPARAM lparam,const BYTE* states,BOOL* eaten) {return impl_->process(service,context,test,open,mode,down,key,lparam,states,eaten);}
bool TailKanaWindows::Finish(TipTextService* service,ITfContext* context,bool cancel) {return impl_->finish(service,context,cancel);}
bool TailKanaWindows::CandidateAction(TipTextService* service,ITfContext* context,int candidate,bool submit) {return impl_->candidate(service,context,candidate,submit);}
bool TailKanaWindows::Render(TipTextService* service,ITfContext* context,const commands::RendererCommand& command) {return impl_->render(service,context,command);}
void TailKanaWindows::HideAll() {
    if (!Impl::storage()) return;
    auto& instances=*Impl::storage();for (auto& weak:instances) if (auto instance=weak.lock()) instance->hide();
    std::erase_if(instances,[](const auto& weak) {return weak.expired();});
}
}

namespace mozc::win32::tsf {
class DefaultModeSession final : public TipComImplements<ITfEditSession> {
public:
    DefaultModeSession(TipTextService* service,ITfContext* context) : service_(service),context_(context) {}
    STDMETHODIMP DoEditSession(TfEditCookie cookie) override {
        std::vector<InputScope> scopes;wil::com_ptr_nothrow<ITfRange> selection;TfActiveSelEnd end=TF_AE_NONE;
        if (!focused(service_.get(),context_.get(),false) || TipStatus::IsDisabledContext(context_.get())) return S_OK;
        if (FAILED(TipRangeUtil::GetDefaultSelection(context_.get(),cookie,&selection,&end)) || !selection || FAILED(TipRangeUtil::GetInputScopes(selection.get(),cookie,&scopes))) return S_OK;
        // Respect application-specific scopes, including password/PIN fields.
        if (std::any_of(scopes.begin(),scopes.end(),[](InputScope scope) {return scope!=IS_DEFAULT;})) return S_OK;
        TipEditSession::SwitchInputModeAsync(service_.get(),setting(L"DefaultJapanese",1) ? commands::HIRAGANA:commands::DIRECT);
        return S_OK;
    }
private:
    wil::com_ptr_nothrow<TipTextService> service_;
    wil::com_ptr_nothrow<ITfContext> context_;
};
void TailKanaWindows::ApplyDefaultMode(TipTextService* service) {
    if (!service || !service->GetThreadManager()) return;
    wil::com_ptr_nothrow<ITfDocumentMgr> document;wil::com_ptr_nothrow<ITfContext> context;
    if (FAILED(service->GetThreadManager()->GetFocus(&document)) || !document || FAILED(document->GetTop(&context)) || !context || TipStatus::IsDisabledContext(context.get())) return;
    wil::com_ptr_nothrow<ITfEditSession> edit{new DefaultModeSession(service,context.get())};HRESULT result=S_OK;
    context->RequestEditSession(service->GetClientID(),edit.get(),TF_ES_ASYNCDONTCARE|TF_ES_READ,&result);
}
}
