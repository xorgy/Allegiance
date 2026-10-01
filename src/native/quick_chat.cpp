#include "quick_chat.hpp"
#include "mdl_text.hpp"
#include <fstream>
#include <unordered_map>
#include <algorithm>
namespace Native {
namespace {
using Mdl::Expr;
struct Reader {
    std::unordered_map<std::string,Expr> values;
    std::unordered_map<const Expr*,int> sonicIds;
    const Expr& resolve(const Expr& e,unsigned depth=0)const {
        if(depth>128)throw std::runtime_error("Recursive quick chat symbol");
        if(e.kind!=Expr::Symbol)return e;
        auto i=values.find(e.text);if(i==values.end())throw std::runtime_error("Unknown quick chat symbol: "+e.text);
        return resolve(i->second,depth+1);
    }
    int number(const Expr& e,unsigned depth=0)const {
        if(depth>128)throw std::runtime_error("Quick chat number nesting limit");auto& v=resolve(e);
        if(v.kind==Expr::Number)return int(v.number);
        if(v.kind==Expr::Call && v.text=="BitOr" && v.args.size()==2)return number(v.args[0],depth+1)|number(v.args[1],depth+1);
        throw std::runtime_error("Quick chat number expected");
    }
    std::string string(const Expr& e)const{auto& v=resolve(e);if(v.kind!=Expr::String)throw std::runtime_error("Quick chat text expected");return v.text;}
    const std::vector<Expr>& call(const Expr& e,std::string_view name,size_t count)const {
        auto& v=resolve(e);if(v.kind!=Expr::Call || v.text!=name || v.args.size()!=count)throw std::runtime_error("Invalid quick chat node: "+v.text);return v.args;
    }
    const std::vector<Expr>& list(const Expr& e)const{auto& v=resolve(e);if(v.kind!=Expr::List)throw std::runtime_error("Quick chat list expected");return v.args;}
    int menu(const Expr& e,QuickChatCatalog& out,unsigned depth=0)const {
        if(depth>128)throw std::runtime_error("Recursive quick chat menu");int id=int(out.menus.size());out.menus.emplace_back();
        for(auto& entry:list(call(e,"QuickChatMenu",1)[0])) {
            auto& a=call(entry,"QuickChatMenuItem",3);QuickChatItem item;item.key=char(number(a[0]));item.label=string(a[1]);
            auto& node=resolve(a[2]);
            if(node.text=="QuickChatMenu")item.menu=menu(node,out,depth+1);
            else {
                auto& command=call(node,"QuickChatCommand",4);auto& sonic=resolve(command[0]);
                auto id=sonicIds.find(&sonic);if(id==sonicIds.end())throw std::runtime_error("Quick chat is missing from its authored sonicChats table");
                auto* voice=out.voiceForChat(id->second);
                QuickChatCommand value{voice?voice->sound:-1,number(command[1]),number(command[2]),number(command[3]),
                    voice?voice->text:"Unknown chat",voice?voice->soundName:"",id->second};
                item.command=int(out.commands.size());out.commands.push_back(std::move(value));
            }
            out.menus[id].items.push_back(std::move(item));
        }
        return id;
    }
};
}
QuickChatCatalog::QuickChatCatalog(Assets& assets) {
    Reader reader;int id=1001;
#define DEFSOUND(name) reader.values.emplace(#name "Sound",Expr{Expr::Number,{},double(id++)});
#define DEFEMPTYSOUND(name) DEFSOUND(name)
#include "../Igc/sounds.h"
#undef DEFSOUND
#undef DEFEMPTYSOUND
    std::ifstream file(assets.find("quickchat.mdl"));std::string text{std::istreambuf_iterator<char>(file),{}};Mdl::Parser parser(text);
    while(!parser.end()){auto key=parser.word();if(key=="use"){parser.expression();parser.need(';');}
        else {parser.need('=');auto value=parser.expression();parser.need(';');reader.values.insert_or_assign(std::move(key),std::move(value));}}
    std::unordered_map<int,QuickChatSonic> chats;
    for(auto& entry:reader.list(reader.values.at("sonicChats"))) {
        auto& pair=reader.list(entry);if(pair.size()!=2)throw std::runtime_error("Invalid sonicChats index pair");
        int id=reader.number(pair[0]);if(id<0 || id>32767)throw std::runtime_error("Sonic chat ID is outside the original signed-short range");
        auto& sonic=reader.resolve(pair[1]);QuickChatSonic chat;chat.id=id;
        for(auto& entry:reader.list(reader.call(sonic,"SonicChat",1)[0])) {
            auto& voice=reader.call(entry,"SonicChatVoice",2);if(voice[0].kind!=Expr::Symbol)throw std::runtime_error("Named quick-chat voice expected");
            auto sound=reader.values.find(voice[0].text);
            chat.voices.push_back({sound==reader.values.end()?-1:reader.number(sound->second),reader.string(voice[1]),voice[0].text});
        }
        // TLookup overwrites a repeated table index and SetIndex changes a reused
        // object to its last listed ID. Both rules matter to outgoing commands.
        chats.insert_or_assign(id,std::move(chat));reader.sonicIds[&sonic]=id;
    }
    sonicChats.reserve(chats.size());for(auto& [id,chat]:chats)sonicChats.push_back(std::move(chat));
    std::sort(sonicChats.begin(),sonicChats.end(),[](auto& a,auto& b){return a.id<b.id;});
    reader.menu(reader.values.at("mainMenu"),*this);refreshSounds();
}
const QuickChatSonic* QuickChatCatalog::sonicChat(int id)const {
    auto found=std::lower_bound(sonicChats.begin(),sonicChats.end(),id,[](auto& chat,int id){return chat.id<id;});
    return found!=sonicChats.end() && found->id==id?&*found:nullptr;
}
const QuickChatVoice* QuickChatCatalog::voiceForChat(int id,size_t variant)const {
    auto* chat=sonicChat(id);return chat && variant<chat->voices.size()?&chat->voices[variant]:nullptr;
}
void QuickChatCatalog::refreshSounds() {
    soundIds.clear();for(auto& chat:sonicChats)for(auto& voice:chat.voices)if(voice.sound!=-1)soundIds.push_back(voice.sound);
    std::sort(soundIds.begin(),soundIds.end());soundIds.erase(std::unique(soundIds.begin(),soundIds.end()),soundIds.end());
    for(auto& command:commands)if(command.sonic>=0)if(auto* voice=voiceForChat(command.sonic))command.sound=voice->sound;
}
}
