// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.h"
#include <cstdlib>
#include <iostream>
#include <map>
using namespace tailkana::windows;
void check(bool value,const char* message) { if (!value) { std::cerr<<message<<'\n';std::exit(1); } }
struct Fake final : Converter {
    std::vector<Word> words;
    static inline int instances = 0;
    Fake() { ++instances; }
    static inline int queries = 0;
    bool query(const std::string& raw,bool converted,bool kata,Conversion& result) override {
        ++queries;
        if (raw == "fail") return false;
        const std::map<std::string,std::string> dictionary{{"ti","ち"},{"tyo","ちょ"},{"tyu","ちゅ"},{"tsu","つ"},{"arigatou","ありがとう"},{"nihongo","にほんご"},{"a","あ"}};
        const auto found = dictionary.find(raw);
        result.reading = found == dictionary.end() ? raw:found->second;
        if (kata && raw == "ti") result.reading = "チ";
        words = {{converted ? "変換1":result.reading,result.reading,0},{"候補2",result.reading,1},{"候補3",result.reading,2},{"候補4",result.reading,3},{"候補5",result.reading,4}};
        result.words = words;return true;
    }
    std::string highlight(size_t index) override { return index < words.size() ? words[index].text:""; }
    std::string commit(size_t index) override { return highlight(index); }
    void reset() override {}
};
Session make() { Session session([] { return std::make_shared<Fake>(); });session.options.corrections = session.options.fuzzy = false;return session; }
void type(Session& s,const std::string& raw) { for (char c:raw) check(s.handle({Key::Character,c}).handled,"typing must be handled"); }
int main() {
    auto s = make();type(s,"ti");
    check(s.preedit == "ち" && !s.converted,"live preedit is kana");
    check(s.handle({Key::Right,0,true,true}).handled && s.buffer.raw == "tyo","right shift right changes only tail");
    s.handle({Key::Down,0,true,true});check(s.buffer.raw == "tyu","wheel recenters");
    s.clear();type(s,"nihongo");
    check(s.handle({Key::Enter}).commit == "にほんご" && !s.active(),"Enter before conversion commits literal reading");
    type(s,"ti");s.handle({Key::Space});check(s.preedit == "変換1","first Space converts");
    s.handle({Key::Space});check(s.preedit == "候補2","second Space keeps converting");
    s.handle({Key::Space});check(s.preedit == "候補3","third Space keeps converting");
    s.handle({Key::Space});check(s.preedit == "候補4" && s.visible()[0] == 3,"selection moves compact page");
    check(s.handle({Key::Character,'2'}).commit == "候補5","number 2 submits visible candidate 2");
    type(s,"ti");s.handle({Key::Tab});check(s.expanded && !s.autoExpanded,"manual expansion");
    s.fit(true);check(s.expanded,"manual expansion stays open");s.handle({Key::Escape});check(!s.expanded && s.active(),"Escape first collapses");
    s.fit(false);check(s.expanded && s.autoExpanded,"overflow expands");s.fit(true);check(!s.expanded,"shortening restores compact layout");
    check(s.handle({Key::Tab,0,true}).commit == "ち" && !s.options.japanese,"Shift Tab commits then switches language");
    check(!s.accepts({Key::Character,'a'}),"English letters pass through");s.handle({Key::Tab,0,true});check(s.options.japanese,"Shift Tab returns Japanese");
    type(s,"ti");s.handle({Key::Shift});check(s.preedit == "チ","left Shift temporary katakana");s.handle({Key::Shift,0,false,false,true});check(s.preedit == "ち","release restores hiragana");
    s.action(-100);check(s.buffer.raw == "ki","wheel action IDs never overlap candidate indices");
    s.clear();type(s,"a");s.handle({Key::Left});s.handle({Key::Character,'t'});check(s.buffer.raw == "ta","editing inserts at Roman caret");
    s.clear();s.options.swapKeys = true;type(s,"ti");s.handle({Key::Enter});check(s.converted,"swapped Enter converts");check(!s.handle({Key::Space}).commit.empty(),"swapped Space confirms");
    s.options.swapKeys = false;s.options.fuzzy = true;type(s,"ci");bool fuzzy = false;
    for (const auto& candidate:s.candidates) if (candidate.correctedRoman == "tsu") { fuzzy = candidate.badge == 'B'; }
    check(fuzzy,"Chinese ci maps to tsu through the shared rules");
    s.clear();s.options.fuzzy = s.options.corrections = true;type(s,std::string(64,'a'));check(s.buffer.raw.size() == 64,"long input is retained");
    s.clear();s.options.fuzzy = s.options.corrections = false;type(s,"fail");check(!s.error.empty() && s.buffer.raw == "fail","converter failure retains original input");
    check(s.handle({Key::Enter}).commit=="fail","conversion failure must not commit an older shorter reading");
    type(s,"ti");s.handle({Key::Space});s.handle({Key::Space});s.handle({Key::Space});s.handle({Key::Space});s.handle({Key::Tab});
    check(s.handle({Key::Character,'2'}).commit=="候補2","expanded number 2 matches row label 2 regardless of compact page");
    type(s,"nihongo");s.handle({Key::Left});s.handle({Key::Left});s.handle({Key::Left});s.handle({Key::Left});
    check(s.caret()==1,"Roman caret maps to kana boundary");s.clear();
    type(s,"ti");check(s.handle({Key::Character,'.'}).commit=="ち。","Japanese punctuation commits current reading with full-width mark");
    Fake::instances=0;auto pool=make();pool.options.corrections=pool.options.fuzzy=true;
    for (const auto* raw:{"arigatpu","qikatetsu","qixiki","liroku"}) {type(pool,raw);pool.clear();}
    check(Fake::instances<=5,"at most one primary plus four reusable correction sessions");
    std::cout<<"PASS Windows session: live kana, repeated conversion, visible numbering, wheel, modes, expansion, fuzzy rules, long input and failure recovery\n";
}
