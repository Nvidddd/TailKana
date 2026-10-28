// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "roman_buffer.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace tailkana::windows {
struct Word { std::string text, reading; int id = 0; };
struct Conversion { std::string reading; std::vector<Word> words; };
class Converter {
public:
    virtual ~Converter() = default;
    virtual bool query(const std::string&, bool converted, bool katakana, Conversion&) = 0;
    virtual std::string highlight(size_t index) = 0;
    virtual std::string commit(size_t index) = 0;
    virtual void reset() = 0;
};
struct Options {
    bool japanese = true, katakana = false, corrections = true, fuzzy = true;
    bool swapKeys = false, furigana = true, showKana = true;
    uint32_t fuzzyGroups = 255;
    int width = 520;
};
struct Candidate { Word word; char badge = 0; std::string correctedRoman; std::shared_ptr<Converter> converter; size_t index = 0; };
enum class Key { Character, Backspace, Delete, Left, Right, Up, Down, Home, End, Space, Enter, Escape, Tab, Shift };
struct Stroke { Key key; char character = 0; bool shift = false, rightShift = false, release = false; };
struct Effect { bool handled = false, changed = false; std::string commit; };
class Session {
public:
    using Factory = std::function<std::shared_ptr<Converter>()>;
    explicit Session(Factory);
    Options options;
    RomanBuffer buffer;
    std::string reading, preedit, error;
    std::vector<Candidate> candidates;
    size_t selected = 0, page = 0;
    bool converted = false, expanded = false, autoExpanded = false;
    bool active() const { return !buffer.raw.empty(); }
    bool accepts(const Stroke&) const;
    Effect handle(const Stroke&);
    Effect action(int);
    Effect choose(size_t);
    bool refresh();
    void clear(bool resetBackend = true);
    std::vector<size_t> visible() const;
    void fit(bool compactFits);
    uint64_t revision() const { return revision_; }
    uint64_t generation() const { return generation_; }
    bool katakana() const { return script(); }
    size_t caret() const;
    Effect highlight(size_t index) { converted = true;return {true,select(index),{}}; }
private:
    Factory factory_;
    std::shared_ptr<Converter> primary_;
    uint64_t revision_ = 0, generation_ = 0;
    std::vector<std::shared_ptr<Converter>> correctionCache_;
    bool temporaryKatakana_ = false;
    bool script() const { return options.katakana != temporaryKatakana_; }
    Effect changed();
    Effect confirm();
    bool select(size_t);
};
}
