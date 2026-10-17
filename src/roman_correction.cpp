// SPDX-License-Identifier: GPL-3.0-or-later
// Roman confusion table and weighted path approach adapted from azooKey.
// Copyright (c) 2023 Miwa / Ensan (MIT). Full notice and source retained in
// third_party/azookey-typo/. This bounded search gives each edit the upstream
// weight 3.5, up to three edits, while limiting alternatives for desktop latency.
#include "roman_correction.h"
#include <algorithm>
#include <array>
#include <string_view>
namespace tailkana {
std::vector<RomanCorrection> romanCorrections(const std::string& raw,bool pinyin,bool keyboard,uint32_t groups) {
    if(raw.size()<(pinyin?2:3) || raw.size()>48 || raw.find_first_not_of("abcdefghijklmnopqrstuvwxyz'-")!=std::string::npos)return {};
    static constexpr std::pair<std::string_view,std::string_view> rules[]={
#include "typo_rules.inc"
    };
    struct PinyinRule {std::string_view from,to;PinyinGroup group;};
    static constexpr PinyinRule allPinyinRules[]={
#include "pinyin_rules.inc"
    };
    std::vector<std::pair<std::string_view,std::string_view>> pinyinRules;
    for(const auto& rule:allPinyinRules)if(groups & static_cast<uint32_t>(rule.group))pinyinRules.emplace_back(rule.from,rule.to);
    constexpr size_t beam=9; // Original path + at most eight corrected paths.
    std::vector<std::vector<RomanCorrection>> paths(raw.size()+1);
    paths[0].push_back({});
    auto prune=[](auto& values) {
        std::sort(values.begin(),values.end(),[](const auto& a,const auto& b){return a.penalty!=b.penalty?a.penalty<b.penalty:a.roman<b.roman;});
        values.erase(std::unique(values.begin(),values.end(),[](const auto& a,const auto& b){return a.roman==b.roman;}),values.end());
        if(values.size()>beam)values.resize(beam);
    };
    for(size_t pos=0;pos<raw.size();++pos) {
        prune(paths[pos]);
        size_t longest=0;
        if(pinyin)for(const auto& rule:allPinyinRules)if(raw.compare(pos,rule.from.size(),rule.from)==0)longest=std::max(longest,rule.from.size());
        for(const auto& path:paths[pos]) {
            auto unchanged=path;unchanged.roman+=raw[pos];paths[pos+1].push_back(std::move(unchanged));
            if(path.edits>=3)continue;
            if(keyboard && raw.size()>=3)for(const auto& [from,to]:rules)if(raw.compare(pos,from.size(),from)==0)
                paths[pos+from.size()].push_back({path.roman+std::string(to),path.edits+1,path.pinyin,false,path.penalty+7});
            // Treat the longest matching pinyin syllable as a unit: xiao→sho
            // must not compete with xi→shi plus an unchanged "ao" suffix.
            if(longest)for(const auto& [from,to]:pinyinRules)if(from.size()==longest && raw.compare(pos,from.size(),from)==0)
                paths[pos+from.size()].push_back({path.roman+std::string(to),path.edits+1,true,false,path.penalty+4});
        }
    }
    auto result=std::move(paths.back());prune(result);
    std::erase_if(result,[&](const auto& value){return value.roman==raw;});
    if(pinyin) {
        // A learner may use pinyin for several syllables in the same word.
        // Validate that complete reading first, before partial mixed spellings
        // consume the small dictionary-query budget (e.g. qixiki→chishiki).
        RomanCorrection complete;
        for(size_t pos=0;pos<raw.size();) {
            const PinyinRule* best=nullptr;
            for(const auto& rule:allPinyinRules)if((!best || rule.from.size()>best->from.size()) && raw.compare(pos,rule.from.size(),rule.from)==0)best=&rule;
            if(best) {
                // A disabled long syllable must not fall back to a shorter
                // enabled prefix, e.g. disabling xiao while keeping xi.
                const bool enabled=groups & static_cast<uint32_t>(best->group);
                complete.roman+=enabled?best->to:best->from;
                pos+=best->from.size();if(enabled)++complete.edits;
            }
            else complete.roman+=raw[pos++];
        }
        if(complete.edits>0 && complete.edits<=3) {
            complete.pinyin=true;complete.penalty=complete.edits*4;
            std::erase_if(result,[&](const auto& value){return value.roman==complete.roman;});
            result.insert(result.begin(),std::move(complete));
            if(result.size()>8)result.resize(8);
        }
    }
    for(auto& value:result)if(value.pinyin && value.edits==1)
        for(const auto& [from,to]:pinyinRules)if(raw==from && value.roman==to)value.kanaOnly=true;
    return result;
}
}
