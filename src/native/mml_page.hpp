#pragma once
#include "original_ui.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace Native {
struct MmlPage {
    struct Line {float top{},bottom{};size_t first{},last{};};
    std::vector<OriginalControl> text;std::vector<Line> lines;float height{};
};
// PagePane's report-text subset. Interpret styles once, then flow whole words,
// 16-pixel tabs and default-font paragraph spacers, bottom-aligning each line.
// Dynamic String values remain literal rather than reparsed as markup.
inline MmlPage compileMmlPage(OriginalUi& ui,std::string_view source,float width,const UiStrings& strings={}){
    MmlPage out;auto style=ui.font("mmlpane","helpFont");auto color=ui.color("mmlpane","helpColor");
    std::unordered_map<std::string,OriginalControl> styles;
    auto metrics=[&](const OriginalControl& s){return ui.assets().font(s.font,s.fontHeight,s.fontBold);};
    const float paragraphHeight=metrics(style)->height;float x=0,y=0,lineHeight=0;size_t lineStart=0;
    auto finish=[&]{
        for(size_t i=lineStart;i<out.text.size();++i)out.text[i].bounds.y=y+lineHeight-out.text[i].bounds.w;
        if(lineHeight)out.lines.push_back({y,y+lineHeight,lineStart,out.text.size()});
        y+=lineHeight;x=0;lineHeight=0;lineStart=out.text.size();
    };
    auto word=[&](std::string value){
        value+=' ';auto font=metrics(style);float advance=0;for(unsigned char c:value)advance+=font->glyphs[c].z;
        if(x && x+advance>width)finish();
        bool merge=out.text.size()>lineStart && out.text.back().font==style.font && out.text.back().fontHeight==style.fontHeight && out.text.back().fontBold==style.fontBold && out.text.back().color==color && out.text.back().bounds.x+out.text.back().bounds.z==x;
        if(merge){auto& previous=out.text.back();previous.text+=value;previous.bounds.z+=advance;}
        else{auto c=style;c.name="detailsText";c.text=std::move(value);c.color=color;c.singleLine=true;c.bounds={x,y,advance,font->height};out.text.push_back(std::move(c));}
        x+=advance;lineHeight=std::max(lineHeight,float(font->height));
    };
    for(size_t at=0;at<source.size();){
        char ch=source[at];if(ch==' ' || ch=='\r' || ch=='\n'){++at;continue;}
        if(ch=='\t'){float advance=(std::floor(x/16)+1)*16-x;if(x && x+advance>width)finish();x+=advance;++at;continue;}
        if(ch!='<'){auto end=source.find_first_of(" \t\r\n<",at);if(end==source.npos)end=source.size();word(std::string(source.substr(at,end-at)));at=end;continue;}
        ++at;if(at>=source.size())throw std::runtime_error("MML: unfinished tag");
        if(source[at]=='<' || source[at]=='>'){word(std::string(1,source[at++]));continue;}
        auto end=source.find('>',at);if(end==source.npos)throw std::runtime_error("MML: unfinished tag");auto tag=source.substr(at,end-at);at=end+1;
        if(tag.starts_with('!'))continue;
        if(tag=="p"){finish();lineHeight=paragraphHeight;continue;}
        auto split=tag.find('|');if(split==tag.npos)throw std::runtime_error("MML: invalid tag");auto name=std::string(tag.substr(split+1));
        if(tag.substr(0,split)=="Color")color=ui.color("mmlpane",name);
        else if(tag.substr(0,split)=="Font"){
            auto found=styles.find(name);if(found==styles.end())found=styles.emplace(name,ui.font("mmlpane",name)).first;style=found->second;
        }else if(tag.substr(0,split)=="String"){
            auto found=strings.find(name);word(found==strings.end()?ui.string("mmlpane",name):found->second);
        }else throw std::runtime_error("MML: unsupported report tag");
    }
    finish();out.height=y;return out;
}
}
