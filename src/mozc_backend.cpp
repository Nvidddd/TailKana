// SPDX-License-Identifier: GPL-3.0-or-later
#include "mozc_backend.h"
#include "protocol/candidate_window.pb.h"
#include "base/system_util.h"
#include "base/strings/japanese.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <set>
namespace tailkana {
MozcBackend::MozcBackend() {
    static const bool initialized = [] {
        const char* custom = std::getenv("TAILKANA_MOZC_PROFILE");
        const char* home = std::getenv("HOME");
        std::filesystem::path profile = custom ? custom :
            std::string(home ? home : "/tmp") + "/Library/Application Support/TailKana/Mozc";
        std::error_code error;
        std::filesystem::create_directories(profile,error);
        mozc::SystemUtil::SetUserProfileDirectory(profile.string());
        return true;
    }();
    (void)initialized;
    client=fcitx::createClient();
}
bool MozcBackend::command(mozc::commands::SessionCommand::CommandType type,int id) {
    mozc::commands::SessionCommand cmd; cmd.set_type(type); cmd.set_id(id);
    return client->SendCommand(cmd,&output);
}
bool MozcBackend::key(char c,bool suppressSuggestions) {
    mozc::commands::KeyEvent key;
    if(c==' ')key.set_special_key(mozc::commands::KeyEvent::SPACE);
    else key.set_key_code(static_cast<unsigned char>(c));
    key.set_mode(mode_);
    if(suppressSuggestions) {
        mozc::commands::Context context;
        context.set_suppress_suggestion(true);
        return client->SendKeyWithContext(key,context,&output);
    }
    return client->SendKeyWithContext(key,mozc::commands::Context::default_instance(),&output);
}
void MozcBackend::clear() { command(mozc::commands::SessionCommand::RESET_CONTEXT); candidates.clear();reading.clear();composingReading.clear(); }
bool MozcBackend::query(const std::string& roman,bool convert,bool katakana,bool predictDuringReplay) {
    clear();
    mode_=katakana?mozc::commands::FULL_KATAKANA:mozc::commands::HIRAGANA;
    mozc::commands::SessionCommand cmd;
    cmd.set_type(mozc::commands::SessionCommand::SWITCH_COMPOSITION_MODE);
    cmd.set_composition_mode(mode_);
    if(!client->SendCommand(cmd,&output))return false;
    // Reconstruct composition silently. Only the final live key (or Space for
    // explicit conversion) needs predictions; all intermediate results are
    // discarded. The reference mode remains available for parity tests.
    for(size_t i=0;i<roman.size();++i)
        if(!key(roman[i],!predictDuringReplay && (convert || i+1<roman.size())))return false;
    if(convert && !roman.empty()) {
        // Keep the actual composing reading before conversion, including all
        // segments. Correction candidates need this full reading for ruby.
        if(output.has_preedit())for(const auto& segment:output.preedit().segment())composingReading+=segment.value();
        if(!key(' '))return false;
    }
    parse();
    if(!convert)composingReading=reading;
    return true;
}
bool MozcBackend::highlight(int index) {
    if(index<0 || index>=int(candidates.size()))return false;
    if(candidates[index].id==-2147483647) {
        // This row is our literal reading, not a Mozc candidate. Highlighting a
        // dictionary row here would make a later SUBMIT commit that other row.
        reading=candidates[index].text;
        return true;
    }
    if(!command(mozc::commands::SessionCommand::HIGHLIGHT_CANDIDATE,candidates[index].id))return false;
    reading.clear();
    if(output.has_preedit())for(const auto& segment:output.preedit().segment())reading+=segment.value();
    if(reading.empty())reading=candidates[index].text;
    return true;
}
void MozcBackend::parse() {
    reading.clear(); candidates.clear();
    std::vector<std::string> segmentReadings;
    int focusedSegment=0;
    if(output.has_preedit())for(const auto& seg:output.preedit().segment()) {
        reading+=seg.value();
        if(seg.annotation()==mozc::commands::Preedit::Segment::HIGHLIGHT)focusedSegment=int(segmentReadings.size());
        segmentReadings.push_back(seg.has_key()?seg.key():seg.value());
    }
    auto sourceReading=[&](int segments) {
        std::string result;
        for(int i=focusedSegment;i<int(segmentReadings.size()) && i<focusedSegment+std::max(1,segments);++i)result+=segmentReadings[i];
        return mozc::japanese::KatakanaToHiragana(result);
    };
    std::string fullReading;
    for(const auto& key:segmentReadings)fullReading+=key;
    fullReading=mozc::japanese::KatakanaToHiragana(fullReading);
    std::set<std::string> seen;
    if(output.has_all_candidate_words()) for(const auto& c:output.all_candidate_words().candidates()) {
        const auto kana=mozc::japanese::KatakanaToHiragana(!c.key().empty()?c.key():sourceReading(c.num_segments_in_candidate()));
        if(seen.insert(c.value()).second) candidates.push_back({c.id(),c.value(),c.has_annotation()?c.annotation().description():"",kana});
    }
    if(candidates.empty() && output.has_candidate_window()) for(const auto& c:output.candidate_window().candidate()) {
        if(seen.insert(c.value()).second) candidates.push_back({c.id(),c.value(),c.has_annotation()?c.annotation().description():"",sourceReading(1)});
    }
    // Some very short/incomplete readings have no dictionary suggestions yet.
    if(!reading.empty() && seen.insert(reading).second) candidates.push_back({-2147483647,reading,"假名",fullReading});
}
std::string MozcBackend::submitReading() {
    if(!command(mozc::commands::SessionCommand::SUBMIT))return {};
    return output.has_result()?output.result().value():"";
}
std::string MozcBackend::commit(int index) {
    if(index<0 || index>=int(candidates.size()))return {};
    const int id=candidates[index].id;
    if(id==-2147483647) {
        // Commit exactly the displayed synthetic row. It has no corresponding
        // Mozc candidate ID and must not train a different dictionary entry.
        const auto result=candidates[index].text;
        clear();
        return result;
    }
    if(!command(mozc::commands::SessionCommand::SELECT_CANDIDATE,id))return {};
    std::string result=output.has_result()?output.result().value():"";
    // Conversion candidates can belong to just one segment. Submit the remaining
    // segments through Mozc rather than discarding them or guessing their text.
    if(output.has_preedit())result+=submitReading();
    return result;
}
}
