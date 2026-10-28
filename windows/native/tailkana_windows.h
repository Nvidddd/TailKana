// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <msctf.h>
#include <memory>
#include "client/client_interface.h"
#include "protocol/renderer_command.pb.h"
namespace mozc::win32::tsf {
class TipTextService;
class TailKanaWindows {
public:
    explicit TailKanaWindows(client::ClientInterface*);
    ~TailKanaWindows();
    bool ProcessKey(TipTextService*,ITfContext*,bool test,bool open,commands::CompositionMode mode,bool down,WPARAM,LPARAM,const BYTE*,BOOL* eaten);
    bool Finish(TipTextService*,ITfContext*,bool cancel);
    bool CandidateAction(TipTextService*,ITfContext*,int candidate,bool submit);
    bool Render(TipTextService*,ITfContext*,const commands::RendererCommand&);
    static void HideAll();
private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};
}
