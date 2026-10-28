// SPDX-License-Identifier: GPL-3.0-or-later
#include "panel.h"
#include <cstdlib>
#include <iostream>
using namespace tailkana::windows;
void check(bool value,const char* message) { if (!value) { std::cerr<<message<<'\n';std::exit(1); } }
struct Fake final : Converter {
    bool query(const std::string&,bool,bool,Conversion& out) override {out={"ち",{{"血","ち",0},{"ち","ち",1},{"地","ち",2}}};return true;}
    std::string highlight(size_t) override {return "血";}
    std::string commit(size_t) override {return "血";}
    void reset() override {}
};
int main() {
    Session session([] {return std::make_shared<Fake>();});session.options.fuzzy=session.options.corrections=false;
    session.handle({Key::Character,'t'});session.handle({Key::Character,'i'});
    int calls=0;HWND window=nullptr;const HWND before=GetForegroundWindow();
    {
        Panel panel([&](int id,uint64_t revision) {check(id==0 && revision==session.revision(),"click mapping");++calls;});
        window=panel.window();check(IsWindow(window),"candidate HWND created");panel.update(session,{120,120,130,145});UpdateWindow(window);
        check(GetForegroundWindow()==before,"candidate window must not activate");
        check((GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_NOACTIVATE)!=0,"NOACTIVATE style");
        check(SendMessageW(window,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE,"mouse activation refused");
        auto click=MAKELPARAM(12,36);
        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,click);SendMessageW(window,WM_LBUTTONUP,0,click);check(calls==1,"fresh click works");
        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,click);session.handle({Key::Character,'a'});panel.update(session,{120,120,130,145});UpdateWindow(window);
        SendMessageW(window,WM_LBUTTONUP,0,click);check(calls==1,"stale click cannot commit a new candidate");
        panel.hide();check(!IsWindowVisible(window),"hide clears candidate window");
    }
    check(!IsWindow(window),"destruction removes native candidate window");
    std::cout<<"PASS Windows native panel: no keyboard focus theft, valid mouse click, stale click rejection and cleanup\n";
}
