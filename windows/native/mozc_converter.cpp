// SPDX-License-Identifier: GPL-3.0-or-later
#include "win32/tip/mozc_converter.h"
#include <algorithm>
#include <climits>
#include <set>
#include <string>
#include <vector>
#include "client/client.h"
#include "protocol/candidate_window.pb.h"
namespace mozc::win32::tsf {
namespace tk = ::tailkana::windows;
namespace {
std::string preedit(const commands::Output& output) {
    std::string text;if (output.has_preedit()) for (const auto& part:output.preedit().segment()) text += part.value();return text;
}
class NativeConverter final : public tk::Converter {
public:
    explicit NativeConverter(client::ClientInterface* client = nullptr) : owned_(client ? nullptr:client::ClientFactory::NewClient()),client_(client ? client:owned_.get()) {}
    bool query(const std::string& raw,bool converted,bool katakana,tk::Conversion& result) override {
        const auto mode = katakana ? commands::FULL_KATAKANA:commands::HIRAGANA;
        const bool append = !converted && !wasConverted_ && mode == mode_ && raw.size() == raw_.size()+1 && raw.starts_with(raw_);
        mode_ = mode;
        if (append) { if (!key(raw.back(),false)) return fail(); }
        else {
            reset();mode_ = mode;
            commands::SessionCommand change;change.set_type(commands::SessionCommand::TURN_ON_IME);change.set_composition_mode(mode_);
            if (!client_->SendCommand(change,&output_)) return fail();
            for (size_t i=0;i<raw.size();++i) if (!key(raw[i],converted || i+1<raw.size())) return fail();
        }
        const auto reading = preedit(output_);
        if (converted && !raw.empty() && !key(' ',false)) return fail();
        raw_ = raw;wasConverted_ = converted;words_.clear();ids_.clear();
        result.reading = reading;
        std::set<std::string> seen;
        int focus = 0;std::vector<std::string> values,readings;
        if (output_.has_preedit()) for (const auto& segment:output_.preedit().segment()) {
            if (segment.annotation() == commands::Preedit::Segment::HIGHLIGHT) focus = static_cast<int>(values.size());
            values.push_back(segment.value());readings.push_back(segment.key().empty() ? segment.value():segment.key());
        }
        if (output_.has_all_candidate_words()) for (const auto& word:output_.all_candidate_words().candidates()) {
            std::string text,kana;
            if (converted) for (int i=0;i<focus;++i) {text += values[i];kana += readings[i];}
            text += word.value();kana += word.key().empty() ? reading:word.key();
            if (converted) for (size_t i=focus+std::max(1,word.num_segments_in_candidate());i<values.size();++i) {text += values[i];kana += readings[i];}
            if (seen.insert(text).second) { words_.push_back({text,kana,word.id()});ids_.push_back(word.id()); }
        }
        if (!reading.empty() && seen.insert(reading).second) { words_.push_back({reading,reading,INT_MIN});ids_.push_back(INT_MIN); }
        result.words = words_;return true;
    }
    std::string highlight(size_t index) override {
        if (index >= ids_.size()) return {};
        if (ids_[index] == INT_MIN) return words_[index].text;
        commands::SessionCommand command;command.set_type(commands::SessionCommand::HIGHLIGHT_CANDIDATE);command.set_id(ids_[index]);
        if (!client_->SendCommand(command,&output_)) return {};
        wasConverted_ = true;
        const auto value = preedit(output_);return value.empty() ? words_[index].text:value;
    }
    std::string commit(size_t index) override {
        if (index >= ids_.size()) return {};
        if (ids_[index] == INT_MIN) { const auto text = words_[index].text;reset();return text; }
        commands::SessionCommand command;command.set_type(commands::SessionCommand::SELECT_CANDIDATE);command.set_id(ids_[index]);
        if (!client_->SendCommand(command,&output_)) return {};
        std::string text = output_.has_result() ? output_.result().value():"";
        if (output_.has_preedit()) {
            command.set_type(commands::SessionCommand::SUBMIT);
            if (!client_->SendCommand(command,&output_)) return {};
            if (output_.has_result()) text += output_.result().value();
        }
        raw_.clear();return text;
    }
    void reset() override {
        commands::SessionCommand command;command.set_type(commands::SessionCommand::RESET_CONTEXT);client_->SendCommand(command,&output_);raw_.clear();wasConverted_ = false;
    }
private:
    bool fail() { raw_.clear();wasConverted_ = true;return false; }
    bool key(char value,bool suppress) {
        commands::KeyEvent event;event.set_mode(mode_);event.set_activated(true);
        if (value == ' ') event.set_special_key(commands::KeyEvent::SPACE);else event.set_key_code(static_cast<unsigned char>(value));
        commands::Context context;context.set_suppress_suggestion(suppress);return client_->SendKeyWithContext(event,context,&output_);
    }
    std::unique_ptr<client::ClientInterface> owned_;
    client::ClientInterface* client_;
    commands::Output output_;
    commands::CompositionMode mode_ = commands::HIRAGANA;
    bool wasConverted_ = true;
    std::string raw_;
    std::vector<tk::Word> words_;
    std::vector<int> ids_;
};
}
std::shared_ptr<tk::Converter> MakeTailKanaConverter(client::ClientInterface* borrowed) {return std::make_shared<NativeConverter>(borrowed);}
}
