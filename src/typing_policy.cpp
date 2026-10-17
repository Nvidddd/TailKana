// SPDX-License-Identifier: GPL-3.0-or-later
#include "typing_policy.h"
namespace tailkana {
std::string punctuation(char symbol,const std::string& mode,bool japanese) {
    const bool full=mode=="Full" || (mode!="Half" && japanese);
    if(!full)return std::string(1,symbol);
    if(japanese) {
        switch(symbol) {
        case '.':return "。";case ',':return "、";
        case '[':return "「";case ']':return "」";
        default:break;
        }
    }
    const unsigned int value=static_cast<unsigned char>(symbol)+0xfee0;
    return {char(0xe0|(value>>12)),char(0x80|((value>>6)&0x3f)),char(0x80|(value&0x3f))};
}
}
