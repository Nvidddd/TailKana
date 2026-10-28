// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <functional>
#include <vector>
#include "session.h"
namespace tailkana::windows {
class Panel {
public:
    using Action = std::function<void(int,uint64_t)>;
    explicit Panel(Action);
    ~Panel();
    void update(Session&,const RECT& anchor);
    void hide();
    void setDark(bool value) { dark_ = value; }
    HWND window() const { return window_; }
private:
    struct Hit { RECT rect; int action; };
    Action action_;
    HWND window_ = nullptr;
    Session* session_ = nullptr;
    std::vector<Hit> hits_;
    uint64_t revision_ = 0, pressedRevision_ = 0;
    int pressedAction_ = -9999, scroll_ = 0;
    int dpi_ = 96, width_ = 520, height_ = 100;
    bool dark_ = true;
    static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
    void paint();
    int px(int value) const { return MulDiv(value,dpi_,96); }
    int hit(POINT) const;
};
std::wstring wide(const std::string&);
}
