// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <filesystem>
namespace tailkana {
std::filesystem::path languageData();
std::string chineseCandidate(const std::string& roman);
struct Definition {std::string text, source;};
Definition chineseDefinition(const std::string& word,const std::string& reading);
}
