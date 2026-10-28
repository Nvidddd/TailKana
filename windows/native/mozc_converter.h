// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
#include "client/client_interface.h"
#include "win32/tailkana/session.h"
namespace mozc::win32::tsf {
std::shared_ptr<::tailkana::windows::Converter> MakeTailKanaConverter(client::ClientInterface* borrowed = nullptr);
}
