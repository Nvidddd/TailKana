// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.h"
#include "roman_correction.h"
#include "typing_policy.h"
#include <algorithm>
#include <set>
namespace tailkana::windows {
Session::Session(Factory factory) : factory_(std::move(factory)), primary_(factory_()) {}
void Session::clear(bool resetBackend) {
    buffer.clear();reading.clear();preedit.clear();error.clear();candidates.clear();
    selected = page = 0;converted = expanded = autoExpanded = false;
    temporaryKatakana_ = false;if (resetBackend) { primary_->reset();for (auto& converter:correctionCache_) converter->reset(); }++revision_;++generation_;
}
bool Session::refresh() {
    ++revision_;++generation_;error.clear();candidates.clear();selected = page = 0;
    if (!active()) { reading.clear();preedit.clear();primary_->reset();return true; }
    Conversion result;
    if (!primary_->query(buffer.raw, converted, script(), result)) {
        error = "日文词典暂不可用";preedit = buffer.raw;return false;
    }
    reading = result.reading;preedit = reading;
    for (size_t i = 0; i < result.words.size(); ++i) candidates.push_back({result.words[i],0,{},primary_,i});
    if (options.corrections || options.fuzzy) {
        std::set<std::string> seen;
        for (const auto& c : candidates) seen.insert(c.word.text);
        int queries = 0, added = 0;
        for (const auto& variant : romanCorrections(buffer.raw,options.fuzzy,options.corrections,options.fuzzyGroups)) {
            if (queries++ == 4 || added == 2) break;
            const size_t slot = size_t(queries-1);
            if (slot >= correctionCache_.size()) correctionCache_.push_back(factory_());
            auto backend = correctionCache_[slot];Conversion correction;
            if (!backend->query(variant.roman,!variant.kanaOnly,script(),correction) || correction.words.empty()) continue;
            size_t index = 0;
            if (variant.kanaOnly) {
                while (index < correction.words.size() && correction.words[index].text != correction.reading) ++index;
                if (index == correction.words.size()) continue;
            }
            const auto& word = correction.words[index];
            if (word.text.find_first_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos || !seen.insert(word.text).second) continue;
            Candidate choice{word,variant.pinyin ? 'B':'E',variant.roman,std::move(backend),index};
            if (!added) candidates.insert(candidates.begin()+std::min<size_t>(1,candidates.size()),std::move(choice));
            else candidates.push_back(std::move(choice));
            ++added;
        }
    }
    if (converted && !candidates.empty()) select(0);
    return true;
}
Effect Session::changed() { refresh();return {true,true,{}}; }
bool Session::select(size_t index) {
    if (index >= candidates.size()) return false;
    auto& c = candidates[index];const auto text = c.converter->highlight(c.index);
    if (text.empty()) return false;
    selected = index;page = index/3;
    if (converted) preedit = text;
    ++revision_;return true;
}
std::vector<size_t> Session::visible() const {
    std::vector<size_t> result;
    for (size_t i = page*3; i < std::min(candidates.size(),page*3+3); ++i) result.push_back(i);
    return result;
}
Effect Session::choose(size_t index) {
    if (index >= candidates.size()) return {true,false,{}};
    auto& c = candidates[index];auto text = c.converter->commit(c.index);
    if (text.empty()) { error = "候选提交失败，请重试";return {true,true,{}}; }
    clear();return {true,true,std::move(text)};
}
Effect Session::confirm() {
    if (!active()) return {};
    if (converted && !candidates.empty()) return choose(selected);
    auto text = reading.empty() ? buffer.raw:reading;
    clear();return {true,true,std::move(text)};
}
void Session::fit(bool compactFits) {
    if (autoExpanded && compactFits) { expanded = autoExpanded = false;++revision_; }
    else if (!expanded && !compactFits) { expanded = autoExpanded = true;++revision_; }
}
bool Session::accepts(const Stroke& s) const {
    if (s.key == Key::Tab && s.shift && !s.release) return true;
    if (!options.japanese) return false;
    if (s.key == Key::Shift) return active() && !s.rightShift;
    if (s.release) return false;
    if (s.key == Key::Character) return (s.character >= 'a' && s.character <= 'z') || (s.character == '\'' && active()) || s.character == '-' || std::string(",.!?:;()[]{}<>/\\\"=+_~`@#$%^&*|").find(s.character) != std::string::npos || (active() && s.character >= '1' && s.character <= '3');
    if (!active()) return false;
    return true;
}
Effect Session::handle(const Stroke& s) {
    if (!accepts(s)) return {};
    if (s.key == Key::Tab && s.shift) {
        auto effect = confirm();options.japanese = !options.japanese;
        temporaryKatakana_ = false;++revision_;effect.handled = effect.changed = true;return effect;
    }
    if (s.key == Key::Shift) {
        if (s.rightShift) return {};
        temporaryKatakana_ = !s.release;return changed();
    }
    if (s.release) return {};
    temporaryKatakana_ = s.shift && !s.rightShift;
    if (s.key == Key::Character) {
        if (s.character != '-' && s.character != '\'' && (s.character < 'a' || s.character > 'z') && (s.character < '1' || s.character > '3')) {
            auto effect = confirm();effect.commit += punctuation(s.character,"Auto",true);effect.handled = effect.changed = true;return effect;
        }
        if (s.character >= '1' && s.character <= '3') {
            const auto indices = visible();const size_t slot = s.character-'1';
            return slot < indices.size() ? choose(indices[slot]):Effect{true,false,{}};
        }
        buffer.insert(s.character);converted = false;return changed();
    }
    if (s.rightShift && (s.key == Key::Up || s.key == Key::Left || s.key == Key::Right || s.key == Key::Down)) {
        const int direction = s.key == Key::Up ? 0:s.key == Key::Left ? 1:s.key == Key::Right ? 2:3;
        if (!buffer.replaceTail(direction)) return {true,false,{}};
        converted = false;return changed();
    }
    const bool conversion = (s.key == Key::Space && !options.swapKeys) || (s.key == Key::Enter && options.swapKeys);
    if (conversion) {
        if (!converted) { converted = true;return changed(); }
        if (!candidates.empty()) select((selected+1)%candidates.size());
        return {true,true,{}};
    }
    if (s.key == Key::Enter || s.key == Key::Space) return confirm();
    switch (s.key) {
    case Key::Escape:
        if (expanded) { expanded = autoExpanded = false;++revision_; }
        else clear();
        break;
    case Key::Tab: expanded = !expanded;autoExpanded = false;++revision_;break;
    case Key::Backspace: buffer.backspace();converted = false;return changed();
    case Key::Delete: buffer.erase();converted = false;return changed();
    case Key::Left: buffer.move(-1);converted = false;return changed();
    case Key::Right: buffer.move(1);converted = false;return changed();
    case Key::Home: buffer.cursor = 0;converted = false;return changed();
    case Key::End: buffer.cursor = buffer.raw.size();converted = false;return changed();
    case Key::Up: if (!candidates.empty()) { converted = true;select((selected+candidates.size()-1)%candidates.size()); }break;
    case Key::Down: if (!candidates.empty()) { converted = true;select((selected+1)%candidates.size()); }break;
    default: return {};
    }
    return {true,true,{}};
}
Effect Session::action(int id) {
    if (id <= -100 && id >= -103) {
        if (!buffer.replaceTail(-id-100)) return {true,false,{}};
        converted = false;return changed();
    }
    if (id >= 0) return choose(size_t(id));
    if (id == -1) return handle({Key::Tab});
    if (id == -2) { options.katakana = !options.katakana;return changed(); }
    if (id == -3) { options.swapKeys = !options.swapKeys;++revision_;return {true,true,{}}; }
    if (id == -4) return handle({options.swapKeys ? Key::Enter:Key::Space});
    if (id == -5) return confirm();
    if (id == -6) return handle({Key::Tab,0,true});
    if (id == -7) { options.showKana = !options.showKana;++revision_;return {true,true,{}}; }
    return {};
}
size_t Session::caret() const {
    if (converted) return std::count_if(preedit.begin(),preedit.end(),[](unsigned char c) {return (c&0xc0)!=0x80;});
    if (!options.showKana) return buffer.cursor;
    const auto& table = RomanBuffer::table();size_t count = 0;
    for (size_t pos = 0; pos < buffer.raw.size();) {
        size_t length = std::min<size_t>(4,buffer.raw.size()-pos);std::string value;
        for (;length;--length) {
            auto it = table.find(buffer.raw.substr(pos,length));
            if (it == table.end()) continue;
            if (it->first == "n" && pos+1 < buffer.raw.size() && std::string("aiueoy").find(buffer.raw[pos+1]) != std::string::npos) continue;
            value = it->second;break;
        }
        if (!length) { length = 1;value = buffer.raw.substr(pos,1); }
        if (pos+length > buffer.cursor) break;
        count += std::count_if(value.begin(),value.end(),[](unsigned char c) {return (c&0xc0)!=0x80;});pos += length;
    }
    return count;
}

}
