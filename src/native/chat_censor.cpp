#include "chat_censor.hpp"
#include "../ClientCore/ChatCensor.h"
#include <array>
#include <algorithm>
namespace Native {
namespace {
struct Words {
    std::array<char,sizeof(Allegiance::ChatCensorWords)> bytes{};
    std::array<const char*,113> words{};int count{};
    Words() {
        std::copy(std::begin(Allegiance::ChatCensorWords),std::end(Allegiance::ChatCensorWords),bytes.begin());
        char* next=std::strchr(bytes.data(),'|')+1;
        while(*next!=3) {words[count++]=next;char* end=std::strchr(next,'|');*end=0;next=end+1;}
        std::stable_sort(words.begin(),words.begin()+count,[](auto* a,auto* b){return std::strlen(a)>std::strlen(b);});
    }
};
}
std::string censorChat(std::string_view text,bool enabled) {
    if(!enabled)return std::string(text);
    static const Words table;
    std::string result(text),scratch(text.size()+1,0);
    Allegiance::CensorChatInPlace(result.data(),scratch.data(),table.words.data(),table.count);
    return result;
}
}
