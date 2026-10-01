// Frozen from TrekInput::LoadMap / TranslateKeyMessage / HandleKeyMessage in
// 39ef2c072b1eac0e9f97ab1428bc96572cbd5854. Independent of the native lookup.
#pragma once
#include <array>
#include <vector>
namespace OriginalKeyMap {
struct Row {int vk,modifiers,command;};
class Map {
    std::array<std::array<int,256>,8> press_;
public:
    std::vector<Row> rows;
    Map(){for(auto& table:press_)table.fill(-1);}
    void add(Row row){rows.push_back(row);press_[row.modifiers&7][row.vk]=row.command;}
    int translate(int vk,bool shift,bool control,bool alt)const{
        if(shift){if(control){if(alt)return press_[7][vk];else return press_[3][vk];}
            else {if(alt)return press_[5][vk];else return press_[1][vk];}}
        else {if(control){if(alt)return press_[6][vk];else return press_[2][vk];}
            else {if(alt)return press_[4][vk];else return press_[0][vk];}}
    }
    bool held(int command,const std::array<bool,256>& keys,bool shift,bool control,bool alt)const{
        for(auto row:rows)if(row.command==command && keys[row.vk]) {
            if(row.modifiers==8)return true;
            if(bool(row.modifiers&1)==shift && bool(row.modifiers&2)==control && bool(row.modifiers&4)==alt)return true;
        }
        return false;
    }
};
}
