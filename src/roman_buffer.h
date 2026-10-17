// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <vector>
namespace tailkana {
struct Syllable { std::string roman, kana; size_t start = 0, length = 0; };
struct Cross { Syllable current; std::array<Syllable, 4> choices; }; // up,left,right,down
class RomanBuffer {
public:
    std::string raw;
    size_t cursor = 0;
    void insert(char c);
    bool backspace();
    bool erase();
    void move(int delta);
    void clear();
    Cross cross() const;
    bool replaceTail(int direction);
    static const std::map<std::string, std::string>& table();
private:
    std::vector<Syllable> syllables() const;
};
}
