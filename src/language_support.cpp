// SPDX-License-Identifier: GPL-3.0-or-later
#include "language_support.h"
#include <libime/pinyin/pinyincontext.h>
#include <libime/pinyin/pinyinime.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/core/userlanguagemodel.h>
#include <sqlite3.h>
#include <mach-o/dyld.h>
#include <memory>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>
namespace tailkana {
std::filesystem::path languageData() {
    if(const auto* custom=std::getenv("TAILKANA_LANGUAGE_DATA"))return custom;
    uint32_t length=0;_NSGetExecutablePath(nullptr,&length);
    std::string executable(length,'\0');_NSGetExecutablePath(executable.data(),&length);
    auto packaged=std::filesystem::path(executable.c_str()).parent_path().parent_path()/"share/tailkana";
    if(std::filesystem::exists(packaged/"pinyin.dict"))return packaged;
    return TAILKANA_BUILD_DATA_DIR;
}
std::string chineseCandidate(const std::string& roman) {
    if(roman.empty() || roman.size()>128 || roman.find_first_not_of("abcdefghijklmnopqrstuvwxyz'")!=std::string::npos)return {};
    try {
        // Both dictionary and LM are loaded lazily, only for enabled Chinese mode.
        static auto ime=[] {
            auto result=std::make_unique<libime::PinyinIME>(std::make_unique<libime::PinyinDictionary>(),std::make_unique<libime::UserLanguageModel>((languageData()/"pinyin.lm").c_str()));
            result->dict()->load(libime::PinyinDictionary::SystemDict,(languageData()/"pinyin.dict").c_str(),libime::PinyinDictFormat::Binary);
            result->setNBest(1);
            return result;
        }();
        static libime::PinyinContext context(ime.get());
        static std::string previous, result;
        if(roman==previous)return result;
        if(roman.size()>previous.size() && roman.compare(0,previous.size(),previous)==0) {
            context.type(roman.substr(previous.size()));
        } else {context.clear();context.type(roman);}
        result=context.sentence();previous=roman;
        // Never label a half-converted Latin remainder as a Chinese candidate.
        if(std::any_of(result.begin(),result.end(),[](unsigned char c){return c<128;}))result.clear();
        return result;
    } catch(const std::exception&) {return {};}
}
Definition chineseDefinition(const std::string& word,const std::string& reading) {
    static sqlite3* database=[] {
        sqlite3* db=nullptr;
        if(sqlite3_open_v2((languageData()/"ja-zh.sqlite").c_str(),&db,SQLITE_OPEN_READONLY|SQLITE_OPEN_NOMUTEX,nullptr)!=SQLITE_OK){if(db)sqlite3_close(db);return static_cast<sqlite3*>(nullptr);}
        return db;
    }();
    if(!database || word.empty())return {};
    static std::unordered_map<std::string,Definition> cache;
    const auto key=word+'\t'+reading;
    if(auto it=cache.find(key);it!=cache.end())return it->second;
    sqlite3_stmt* statement=nullptr;
    const char* sql="SELECT meaning,source FROM gloss WHERE word=?1 OR word IN (SELECT target FROM alias WHERE word=?1) GROUP BY meaning,source ORDER BY MIN(CASE WHEN reading=?2 THEN 0 WHEN reading='' THEN 1 ELSE 2 END),MIN(priority),MIN(entryid) LIMIT 3";
    Definition result;
    std::string lastSource;
    if(sqlite3_prepare_v2(database,sql,-1,&statement,nullptr)==SQLITE_OK) {
        sqlite3_bind_text(statement,1,word.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(statement,2,reading.c_str(),-1,SQLITE_TRANSIENT);
        while(sqlite3_step(statement)==SQLITE_ROW) {
            const auto* text=reinterpret_cast<const char*>(sqlite3_column_text(statement,0));
            const auto* source=reinterpret_cast<const char*>(sqlite3_column_text(statement,1));
            if(text && result.text.find(text)==std::string::npos && result.text.size()<600) {if(!result.text.empty())result.text+="；";if(source && word!=source && lastSource!=source)result.text+=std::string(source)+"：";result.text+=text;if(source)lastSource=source;}
            if(source && result.source.empty())result.source=source;
        }
    }
    sqlite3_finalize(statement);
    if(cache.size()>2048)cache.clear();
    cache.emplace(key,result);
    return result;
}
}
