// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace tailkana {
enum class PinyinGroup : uint32_t {Xi=1,Qi=2,Ci=4,Si=8,Zi=16,XGlides=32,QGlides=64,JGlides=128};
constexpr uint32_t AllPinyinGroups=255;
struct RomanCorrection {
    std::string roman;int edits=0;
    bool pinyin=false,kanaOnly=false;
    int penalty=0;
};
std::vector<RomanCorrection> romanCorrections(const std::string& raw,bool pinyin=false,bool keyboard=true,uint32_t groups=AllPinyinGroups);
}
