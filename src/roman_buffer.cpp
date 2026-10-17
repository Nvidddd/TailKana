// SPDX-License-Identifier: GPL-3.0-or-later
#include "roman_buffer.h"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace tailkana {
const std::map<std::string, std::string>& RomanBuffer::table() {
    static const std::map<std::string, std::string> value = {
#include "roman_table.inc"
    };
    return value;
}
void RomanBuffer::insert(char c) { raw.insert(cursor++, 1, c); }
bool RomanBuffer::backspace() { if (!cursor) return false; raw.erase(--cursor, 1); return true; }
bool RomanBuffer::erase() { if (cursor == raw.size()) return false; raw.erase(cursor, 1); return true; }
void RomanBuffer::move(int delta) { cursor = std::clamp<int>(int(cursor) + delta, 0, int(raw.size())); }
void RomanBuffer::clear() { raw.clear(); cursor = 0; }
std::vector<Syllable> RomanBuffer::syllables() const {
    std::vector<Syllable> result;
    for (size_t pos = 0; pos < raw.size();) {
        size_t length = std::min<size_t>(4, raw.size() - pos);
        for (; length > 0; --length) {
            auto it = table().find(raw.substr(pos, length));
            if (it != table().end()) {
                // A single n is pending before a vowel/y; nn or n' is explicit.
                if (it->first == "n" && pos + 1 < raw.size() &&
                    std::string("aiueoy").find(raw[pos+1]) != std::string::npos) continue;
                result.push_back({it->first, it->second, pos, length});
                break;
            }
        }
        if (length) { pos += length; continue; }
        if (pos + 1 < raw.size() && raw[pos] == raw[pos+1] &&
            std::string("bcdfghjklmpqrstvwxyz").find(raw[pos]) != std::string::npos) {
            result.push_back({raw.substr(pos, 1), "っ", pos, 1});
        } else { result.push_back({raw.substr(pos, 1), "", pos, 1}); }
        ++pos;
    }
    return result;
}
static int distance(const std::string& a, const std::string& b) {
    int d[6][6] = {};
    for (int i=0; i<=int(a.size()); ++i) d[i][0]=i;
    for (int j=0; j<=int(b.size()); ++j) d[0][j]=j;
    for (int i=1; i<=int(a.size()); ++i) for (int j=1; j<=int(b.size()); ++j)
        d[i][j]=std::min({d[i-1][j]+1,d[i][j-1]+1,d[i-1][j-1]+(a[i-1]!=b[j-1])});
    return d[a.size()][b.size()];
}
Cross RomanBuffer::cross() const {
    Cross c;
    auto parsed = syllables();
    if (parsed.empty()) return c;
    c.current = parsed.back();
    if (c.current.kana.empty()) return c; // Never silently replace the preceding syllable.
    const auto& roman = c.current.roman;
    std::set<std::string> used{roman};
    auto put = [&](int index, const std::string& r) {
        auto it=table().find(r);
        if (it==table().end() || used.count(r)) return false;
        c.choices[index]={r,it->second,c.current.start,c.current.length}; used.insert(r); return true;
    };
    // Preserve the user's example exactly. Right and down are yōon branches.
    if (roman == "ti" || roman == "chi") { put(0,"ki"); put(1,"qi"); put(2,"tyo"); put(3,"tyu"); }
    else {
        std::string stem;
        auto y=roman.find('y');
        if (y!=std::string::npos) stem=roman.substr(0,y);
        else if (roman == "shi") stem="s";
        else if (roman == "ji") stem="j";
        else if (roman.size()>1) stem=roman.substr(0,roman.size()-1);
        for (const auto& [index, suffix]: std::array<std::pair<int,std::string>,2>{{{2,"yo"},{3,"yu"}}}) {
            if (!put(index,stem+suffix)) {
                // Keep right/down in their yo/yu families after recentering too.
                for (const auto& prefix: {"k","t","s","n","h","m","r","g","b","p","q",""})
                    if (put(index,std::string(prefix)+suffix)) break;
            }
        }
        std::vector<std::tuple<int,std::string>> ranked;
        for (const auto& [r,k]:table()) {
            if (r.size()>4 || used.count(r) || r[0]=='x' || r[0]=='l' ||
                r.find_first_not_of("abcdefghijklmnopqrstuvwxyz")!=std::string::npos) continue;
            int score=distance(roman,r)*10;
            if (!roman.empty() && r.back()==roman.back()) score-=3;
            if (k == c.current.kana) score+=20; // prefer actual phonetic changes
            ranked.emplace_back(score,r);
        }
        std::sort(ranked.begin(),ranked.end());
        for (int index: {0,1,2,3}) {
            if (!c.choices[index].roman.empty()) continue;
            for (const auto& [_,r]:ranked) if (put(index,r)) break;
        }
    }
    return c;
}
bool RomanBuffer::replaceTail(int direction) {
    if (direction<0 || direction>3) return false;
    auto c=cross();
    const auto& choice=c.choices[direction];
    if (choice.roman.empty()) return false;
    // The tail is at the end even when the editing caret is elsewhere.
    const bool atEnd=cursor==raw.size();
    raw.replace(c.current.start,c.current.length,choice.roman);
    if (atEnd) cursor=raw.size(); else cursor=std::min(cursor,c.current.start);
    return true;
}
}
