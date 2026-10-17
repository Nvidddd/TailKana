// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "unix/fcitx5/mozc_client_interface.h"
namespace tailkana {
struct Candidate { int id; std::string text, comment, reading; };
class MozcBackend {
public:
    MozcBackend();
    bool query(const std::string& roman, bool convert, bool katakana=false, bool predictDuringReplay=false);
    bool highlight(int index);
    std::string commit(int index);
    std::string submitReading();
    void clear();
    std::vector<Candidate> candidates;
    std::string reading, composingReading;
private:
    std::unique_ptr<fcitx::MozcClientInterface> client;
    mozc::commands::Output output;
    mozc::commands::CompositionMode mode_=mozc::commands::HIRAGANA;
    bool command(mozc::commands::SessionCommand::CommandType type, int id=0);
    bool key(char c, bool suppressSuggestions=false);
    void parse();
};
}
