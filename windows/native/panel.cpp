// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "panel.h"
#include <algorithm>
#include <atomic>
#include <string>
#include <windowsx.h>
namespace tailkana::windows {
namespace {
constexpr wchar_t ClassName[] = L"TailKana.CandidatePanel.1";
std::atomic<unsigned> windows{0};
HMODULE module() {
    HMODULE value = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&module),&value);
    return value;
}
struct Font {
    HFONT value;
    HDC dc;
    HGDIOBJ previous;
    Font(HDC target,int size,int dpi) : value(CreateFontW(-MulDiv(size,dpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Yu Gothic UI")),dc(target),previous(SelectObject(dc,value)) {}
    ~Font() { SelectObject(dc,previous);DeleteObject(value); }
};
int textHeight(HDC dc,const std::wstring& text,int size,int dpi,int width) {
    Font font(dc,size,dpi);RECT measure{0,0,std::max(1,width),0};
    DrawTextW(dc,text.data(),static_cast<int>(text.size()),&measure,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
    return std::max<int>(MulDiv(size+2,dpi,96),measure.bottom);
}
int textWidth(HDC dc,const std::wstring& text,int size,int dpi) {
    Font font(dc,size,dpi);SIZE extent{};GetTextExtentPoint32W(dc,text.data(),static_cast<int>(text.size()),&extent);return extent.cx;
}
}
std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if (length <= 0) return {};
    std::wstring result(length,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),length);return result;
}
Panel::Panel(Action action) : action_(std::move(action)) {
    WNDCLASSEXW type{};type.cbSize = sizeof(type);type.lpfnWndProc = procedure;type.hInstance = module();type.hCursor = LoadCursorW(nullptr,IDC_ARROW);type.lpszClassName = ClassName;
    if (!RegisterClassExW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
    window_ = CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,ClassName,L"TailKana",WS_POPUP,0,0,520,100,nullptr,nullptr,type.hInstance,this);
    if (window_) ++windows;
}
Panel::~Panel() {
    if (window_) { DestroyWindow(window_);if (--windows == 0) UnregisterClassW(ClassName,module()); }
}
void Panel::hide() { if (window_) ShowWindow(window_,SW_HIDE);session_ = nullptr;pressedAction_ = -9999; }
int Panel::hit(POINT point) const {
    for (const auto& item:hits_) if (PtInRect(&item.rect,point)) return item.action;
    return -9999;
}
void Panel::update(Session& session,const RECT& anchor,bool followSelection) {
    if (!window_ || !session.active()) { hide();return; }
    session_ = &session;anchor_ = anchor;
    dpi_ = 96;
    using GetDpi = UINT(WINAPI*)(HWND);
    if (const auto function = reinterpret_cast<GetDpi>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"))) {
        const UINT dpi = function(GetForegroundWindow());if (dpi) dpi_ = static_cast<int>(dpi);
    }
    width_ = std::clamp(session.options.width,360,1000);
    const int cell = (px(width_-150)-px(24))/3;
    HDC dc = GetDC(window_);bool fits = true;
    for (size_t index:session.visible()) {
        const auto& word = session.candidates[index].word;
        if (textWidth(dc,wide(word.text),16,dpi_) > cell-px(18) || (session.options.furigana && textWidth(dc,wide(word.reading),8,dpi_) > cell-px(18))) fits = false;
    }
    session.fit(fits);revision_ = session.revision();rowHeights_.clear();
    if (session.expanded) {
        const int count = static_cast<int>(session.candidates.size());
        scroll_ = std::clamp(scroll_,0,std::max(0,count-1));
        if (followSelection && (static_cast<int>(session.selected) < scroll_ || static_cast<int>(session.selected) >= scroll_+5)) scroll_ = static_cast<int>(session.selected);
        int total = 0;
        for (size_t index=scroll_;index<session.candidates.size() && index<size_t(scroll_+8);++index) {
            const auto& word = session.candidates[index].word;
            int height = textHeight(dc,wide(word.text),20,dpi_,px(width_-190));
            if (session.options.furigana && word.reading!=word.text) height += textHeight(dc,wide(word.reading),10,dpi_,px(width_-190));
            const int logical = std::max(42,MulDiv(height,96,dpi_)+10);
            rowHeights_.push_back(logical);total += logical;
            if (total>=320) break;
        }
        height_ = std::max(104,std::min(360,total+34));
    } else { scroll_ = 0;height_ = 104; }
    ReleaseDC(window_,dc);
    const int width = px(width_),height = px(height_);
    RECT work{};MONITORINFO monitor{sizeof(monitor)};
    if (GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&monitor)) work = monitor.rcWork;
    else SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    const int x = std::clamp<LONG>(anchor.left,work.left,std::max<LONG>(work.left,work.right-width));
    int y = anchor.bottom+px(5);if (y+height > work.bottom) y = anchor.top-height-px(5);
    y = std::clamp<LONG>(y,work.top,std::max<LONG>(work.top,work.bottom-height));
    HRGN region = CreateRoundRectRgn(0,0,width+1,height+1,px(14),px(14));if (!SetWindowRgn(window_,region,FALSE)) DeleteObject(region);
    SetWindowPos(window_,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    InvalidateRect(window_,nullptr,FALSE);
}
void Panel::paint() {
    PAINTSTRUCT info{};HDC target = BeginPaint(window_,&info);
    RECT client{};GetClientRect(window_,&client);
    HDC dc = CreateCompatibleDC(target);HBITMAP bitmap = CreateCompatibleBitmap(target,client.right,client.bottom);HGDIOBJ previous = SelectObject(dc,bitmap);
    const COLORREF background = dark_ ? RGB(28,28,28):RGB(247,248,247);
    const COLORREF ink = dark_ ? RGB(230,242,233):RGB(29,61,47);
    const COLORREF muted = dark_ ? RGB(180,206,191):RGB(82,107,97);
    const COLORREF highlight = dark_ ? RGB(53,91,71):RGB(220,238,226);
    const COLORREF line = dark_ ? RGB(76,104,89):RGB(183,203,189);
    auto fill = [&](RECT rect,COLORREF color) { HBRUSH brush = CreateSolidBrush(color);FillRect(dc,&rect,brush);DeleteObject(brush); };
    auto rounded = [&](RECT rect,COLORREF color) {HBRUSH brush=CreateSolidBrush(color);auto oldBrush=SelectObject(dc,brush);auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,rect.left,rect.top,rect.right,rect.bottom,px(14),px(14));SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(brush);};
    auto text = [&](const std::wstring& value,RECT rect,int size,COLORREF color,UINT alignment = DT_LEFT,bool wrap = false) {
        Font font(dc,size,dpi_);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,value.data(),static_cast<int>(value.size()),&rect,(wrap ? DT_WORDBREAK:DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS)|DT_NOPREFIX|alignment);
    };
    auto rect = [&](int x,int y,int w,int h) { return RECT{px(x),px(y),px(x+w),px(y+h)}; };
    fill(client,background);HBRUSH border = CreateSolidBrush(line);FrameRect(dc,&client,border);DeleteObject(border);
    hits_.clear();
    auto button = [&](int id,const std::wstring& label,RECT bounds,int size = 10) { hits_.push_back({bounds,id});text(label,bounds,size,muted,DT_CENTER); };
    if (session_) {
        auto& s = *session_;const int left = width_-148;
        auto candidate = [&](size_t index,int slot,RECT bounds,bool list) {
            if (index >= s.candidates.size()) return;
            if (index == s.selected) rounded(bounds,highlight);
            const auto& item = s.candidates[index];
            RECT number = bounds;number.right = number.left+px(17);
            text(std::to_wstring(list ? index+1:slot+1),number,10,muted,DT_CENTER);
            if (item.badge) { RECT mark = number;mark.top += px(19);text(std::wstring(1,wchar_t(item.badge)),mark,8,muted,DT_CENTER); }
            RECT word = bounds;word.left += px(21);word.right -= px(3);int size = 20;
            while (!list && size > 16 && textWidth(dc,wide(item.word.text),size,dpi_) > word.right-word.left) --size;
            if (s.options.furigana && item.word.reading != item.word.text && !item.word.reading.empty()) {
                RECT reading = word;const int height=list ? textHeight(dc,wide(item.word.reading),size/2,dpi_,word.right-word.left):px(15);reading.bottom=reading.top+height;text(wide(item.word.reading),reading,size/2,muted,DT_LEFT,list);word.top += height;
            }
            text(wide(item.word.text),word,size,ink,DT_LEFT,list);bounds.bottom=std::min<LONG>(bounds.bottom,px(height_-30));if (bounds.bottom>bounds.top) hits_.push_back({bounds,static_cast<int>(index)});
        };
        if (s.expanded) {
            const int saved=SaveDC(dc);IntersectClipRect(dc,0,0,px(left),px(height_-30));
            int y=5;for (size_t row=0;row<rowHeights_.size();++row) {candidate(size_t(scroll_)+row,0,rect(7,y,left-12,rowHeights_[row]-2),true);y += rowHeights_[row];}
            RestoreDC(dc,saved);
        } else {
            const auto indices = s.visible();const int cell = (left-30)/3;
            for (int slot=0;slot<static_cast<int>(indices.size());++slot) candidate(indices[slot],slot,rect(7+slot*cell,6,cell-3,61),false);
            button(-1,L"+",rect(left-25,10,24,50),22);
        }
        if (s.candidates.empty()) text(wide(s.error.empty() ? s.preedit:s.error),rect(12,12,left-18,50),12,muted);
        const int bottom = height_-26;fill(rect(8,bottom-2,left-8,1),line);
        button(-1,s.expanded ? L"Tab 收起":L"Tab 展开",rect(8,bottom,60,22),9);
        button(-4,s.options.swapKeys ? L"Enter 变换":L"空格 变换",rect(72,bottom,70,22),9);
        button(-5,s.options.swapKeys ? L"空格 确定":L"Enter 确定",rect(146,bottom,70,22),9);
        button(-3,L"⇄",rect(left-52,bottom,24,22),14);button(-10,L"⚙",rect(left-27,bottom,22,22),12);
        fill(rect(left+4,7,1,84),line);
        const auto cross = s.buffer.cross();const int x = left+9;
        auto kana = [&](const std::string& raw) {auto value=wide(raw);if (s.katakana()) for (auto& c:value) if (c>=0x3041 && c<=0x3096) c+=0x60;return value;};
        text(wide(std::to_string(cross.current.length)),rect(x,8,40,22),12,ink,DT_CENTER);
        text(L"⇧ + 方向",rect(x+80,8,40,24),8,muted,DT_CENTER);
        const int positions[4][2] = {{40,0},{0,28},{80,28},{40,56}};
        for (int i=0;i<4;++i) {
            const auto& branch = cross.choices[i];const auto [dx,dy] = positions[i];
            const RECT bounds = rect(x+dx,8+dy,40,28);hits_.push_back({bounds,-100-i});
            text(kana(branch.kana),rect(x+dx,8+dy,40,17),13,ink,DT_CENTER);text(wide(branch.roman),rect(x+dx,25+dy,40,10),8,muted,DT_CENTER);
        }
        rounded(rect(x+40,36,40,28),highlight);
        text(kana(cross.current.kana),rect(x+40,36,40,18),14,ink,DT_CENTER);text(wide(cross.current.roman),rect(x+40,54,40,10),8,muted,DT_CENTER);
        button(-7,s.options.showKana ? L"假名":L"罗马字",rect(x,67,40,24),8);button(-2,s.options.katakana ? L"ア":L"あ",rect(x+80,67,40,24),15);
    }
    BitBlt(target,0,0,client.right,client.bottom,dc,0,0,SRCCOPY);SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);EndPaint(window_,&info);
}
LRESULT CALLBACK Panel::procedure(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    auto* self = reinterpret_cast<Panel*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if (message == WM_NCCREATE) { self = static_cast<Panel*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self)); }
    if (!self) return DefWindowProcW(window,message,wparam,lparam);
    switch (message) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: self->paint();return 0;
    case WM_LBUTTONDOWN:
        self->pressedAction_ = self->hit({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});self->pressedRevision_ = self->revision_;return 0;
    case WM_LBUTTONUP: {
        const int action = self->hit({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});
        const bool valid = action != -9999 && action == self->pressedAction_ && self->revision_ == self->pressedRevision_;
        const auto revision = self->revision_;auto callback = self->action_;self->pressedAction_ = -9999;if (valid) callback(action,revision);return 0;
    }
    case WM_MOUSEWHEEL:
        if (self->session_ && self->session_->expanded) {
            self->scroll_ = std::clamp(self->scroll_+(GET_WHEEL_DELTA_WPARAM(wparam)>0 ? -1:1),0,std::max(0,static_cast<int>(self->session_->candidates.size())-1));
            self->update(*self->session_,self->anchor_,false);
        }
        return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
}
