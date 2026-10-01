#include "frontend.hpp"
#include "../Igc/igc.h"
#include "../ClientCore/InvestmentPresentation.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <unordered_set>

namespace Native {
uint32_t Frontend::investmentAsteroid()const {
    if(!original_ || screen_!=Screen::Research || consoleFlight_ || hasMessageBox())return 0;
    auto row=std::find_if(researchChoices_.begin(),researchChoices_.end(),[&](const auto& row){return row.id==researchHover_;});
    return row!=researchChoices_.end() ? row->buildMask : 0;
}
bool Frontend::researchVisible(const ResearchChoice& row)const {
    return row.group==researchTab_ && (researchShowAll_ || (row.canBuy && row.predecessorReady)) && (researchShowComplete_ || !row.complete);
}
bool Frontend::researchGroupEnabled(int group)const {
    if(group==2 && !session_.rules.supremacy)return false;
    if(group==3 && !session_.rules.tactical)return false;
    if(group==4 && !session_.rules.expansion)return false;
    if(group==5 && !session_.rules.shipyard)return false;
    return std::any_of(researchChoices_.begin(),researchChoices_.end(),[&](const auto& row){return row.group==group && (researchShowAll_ || (row.canBuy && row.predecessorReady));});
}
void Frontend::researchSelection(int id,bool reveal) {
    int index=0;for(const auto& row:researchChoices_)if(researchVisible(row)){
        if(row.id==id){researchSelected_=id;researchList_.select(index,reveal);return;}++index;
    }
}
void Frontend::refreshResearchChoices() {
    if(!researchService_)return;
    auto choices=researchService_();
    if(!researchSelectFirst_ && researchSelected_>=0 && std::none_of(choices.begin(),choices.end(),[&](const auto& row){return row.id==researchSelected_ && researchVisible(row);})){
        std::unordered_set<int> visible;visible.reserve(choices.size());
        for(const auto& row:choices)if(researchVisible(row))visible.insert(row.id);
        std::vector<int> previous;previous.reserve(researchChoices_.size());
        for(const auto& row:researchChoices_)if(researchVisible(row))previous.push_back(row.id);
        researchSelected_=Allegiance::InvestmentSelectionAfterRemoval(previous,researchSelected_,[&](int id){return visible.contains(id);});
        if(researchSelected_<0)researchList_.selection=-1;
    }
    researchChoices_=std::move(choices);
}
void Frontend::originalResearch() {
    if(docked_ && !trainingCommand_){screen_=Screen::Hangar;originalLayout();screen_=Screen::Research;}
    else {view_.original=true;view_.overlay=true;view_.items.clear();view_.artwork.clear();view_.text.clear();}
    auto& assets=original_->assets();
    auto panel=assets.image("conpurchbmp",true),info=assets.image("conpartinfobmp",true);
    const auto* overlay=overlays_->get(1,{},glm::vec2(view_.canvas));if(!overlay || !(overlay->mask&(overlayModes_|ofInvestment)))return;
    auto composition=original_->imageLayout("dialog","MergedInvestmentsPaneImage",{{"Money",money_}}, {},
        {{"InvestmentsPane",{panel->width,panel->height}},{"InvestmentsInfoPane",{info->width,info->height}}});
    auto anchor=glm::floor(overlay->on);
    auto partBounds=authoredControlBounds(composition,"InvestmentsPane"),infoBounds=authoredControlBounds(composition,"InvestmentsInfoPane");
    glm::vec2 at=anchor+glm::vec2(partBounds),infoAt=anchor+glm::vec2(infoBounds);
    view_.slide=overlaySlide(*overlay,researchEntered_);view_.slide->artworkBegin=view_.artwork.size();view_.slide->itemsBegin=view_.items.size();
    auto image=[&](std::string name,glm::vec2 p,bool keyed,std::optional<glm::vec4> clip={}){
        auto im=assets.image(name,keyed);OriginalControl c;c.kind=OriginalControl::Kind::Image;c.name=std::move(name);c.image=im;c.bounds={p,im->width,im->height};c.clip=clip;view_.artwork.push_back(c);return glm::vec2(im->width,im->height);
    };
    auto text=[&](std::string name,glm::vec2 p,std::string value,const char* font,glm::vec4 color=glm::vec4(1),std::optional<glm::vec4> clip={},float width=0,int justify=0){
        OriginalControl c;c.kind=OriginalControl::Kind::Text;c.name=std::move(name);c.bounds={p,width,13};c.text=std::move(value);c.font=font;c.color=color;c.clip=clip;c.justify=justify;c.surfaceText();view_.artwork.push_back(std::move(c));
    };
    auto button=[&](const char* name,glm::vec2 p,MenuAction action,bool enabled=true,bool checked=false,bool checkbox=false){
        auto im=assets.image(name,true);int faces=checkbox?8:4;OriginalControl c;c.kind=OriginalControl::Kind::Button;c.name=name;c.image=im;c.faces=checkbox?255:15;c.bounds={p,im->width,im->height/faces};
        // PurchasesPane builds these with CreateTrekButton directly: tabs
        // specify mouseclick. All other buttons, including checkboxes, use its
        // default positive cue. This differs from ButtonPane's MDL factory.
        c.clickSound=action.op==MenuOp::ResearchTab?mouseclickSound:positiveButtonClickSound;c.hoverSound=mouseoverSound;c.toggleOnRelease=checkbox;view_.artwork.push_back(c);
        view_.items.push_back({c.visibleBounds(),name,{},action,enabled,checked,c.name});return float(c.bounds.w);
    };
    image("conpurchbmp",at,true);
    static constexpr const char* tabs[]={"btndronesbmp","btnstarbasebmp","btnordnancebmp","btnelectronicsbmp","btnresearchbmp","btnshipyardbmp"};
    float y=at.y+12;
    for(int group=0;group<6;++group){y+=button(tabs[group],{at.x+17,y},{MenuOp::ResearchTab,group},researchGroupEnabled(group),group==researchTab_);if(group>0)++y;}
    std::vector<const ResearchChoice*> rows;rows.reserve(researchChoices_.size());
    for(const auto& row:researchChoices_)if(researchVisible(row))rows.push_back(&row);
    researchList_.wholeRows=true;researchList_.configure(int(rows.size()),72,{at+glm::vec2(227,9),203,432});
    if(researchSelectFirst_){researchSelected_=rows.empty()?-1:rows.front()->id;researchSelectFirst_=false;}
    auto selected=std::find_if(rows.begin(),rows.end(),[&](const auto* row){return row->id==researchSelected_;});
    researchList_.selection=selected==rows.end()?-1:int(selected-rows.begin());
    auto chosen=std::find_if(rows.begin(),rows.end(),[&](const auto* row){return row->id==researchSelected_;});
    bool canInvest=chosen!=rows.end() && (*chosen)->available && (*chosen)->bought<100 && money_>0 && !gameplayClosed();
    button("conbtninvestbmp",at+glm::vec2(63,315),{MenuOp::ResearchActivate},canInvest);
    button("btnshowallbmp",at+glm::vec2(30,360),{MenuOp::ResearchFilter,0},true,researchShowAll_,true);
    button("btnshowcpltbmp",at+glm::vec2(30,375),{MenuOp::ResearchFilter,1},true,researchShowComplete_,true);
    auto clip=researchList_.viewport;
    for(int i=int(researchList_.offset/72);i<int(rows.size());++i){
        glm::vec2 p{clip.x,clip.y+i*72-researchList_.offset};if(p.y>=clip.y+clip.w)break;
        const auto& row=*rows[i];auto prefix="investment/"+std::to_string(row.id)+"/";
        std::string model="i"+row.model+"bmp";std::transform(model.begin(),model.end(),model.begin(),[](unsigned char c){return std::tolower(c);});
        if(assets.hasImage(model))image(model,p+glm::vec2(10,2),false,clip);else image("conloadoutitembmp",p,true,clip);
        if(row.id==researchSelected_)image("investhighlightbmp",p+glm::vec2(10,2),true,clip);
        image("conshadebracketsbmp",p,true,clip);
        int completed=std::clamp(int(180*row.progress/100),0,180),paid=std::clamp(180*row.bought/100,0,180);
        bool obsolete=completed==0 && !row.canBuy;if(obsolete)completed=paid=180;
        auto shade=[&](float start,float width,glm::vec4 color){if(width<=0)return;OriginalControl c;c.kind=OriginalControl::Kind::Rectangle;c.name=prefix+"shade";c.bounds={p+glm::vec2(8+start,2),width,59};c.color=color;c.clip=clip;view_.artwork.push_back(std::move(c));};
        // Native quads take straight RGB; WinTrek's source was premultiplied.
        shade(0,completed,obsolete?glm::vec4(1,0,0,.5f):glm::vec4(0,1,0,.5f));shade(completed,paid-completed,{1,1,0,.5f});
        auto shadowText=[&](glm::vec2 offset,std::string value,const char* font="smallFont"){
            text(prefix+"shadow",p+offset+glm::vec2(1),value,font,{0,0,33.f/256,1},clip);
            text(prefix+"text",p+offset,std::move(value),font,glm::vec4(1),clip);
        };
        std::string title=row.name;
        if(row.type!=OT_development){title="Build "+title+" ("+std::to_string(row.count);if(row.type==OT_droneType)title+="/"+std::to_string(row.limit);title+=")";}
        shadowText({147,1},"$ "+std::to_string(row.bought>0 && row.bought<100?row.price-row.invested:row.price));
        if(!row.status.empty())shadowText({11,42},row.status);
        shadowText({11,1},std::move(title));
        if(row.prerequisites.empty()){
            if(!row.canBuy)shadowText({16,10},"obsolete","smallBoldFont");
            else if(row.progress>0 && row.progress<100){char time[32];snprintf(time,sizeof(time),"%dm:%02ds",row.timeRemaining/60,row.timeRemaining%60);shadowText({16,19},time);}
        }else {for(size_t j=0;j<std::min(size_t(5),row.prerequisites.size());++j)shadowText({16,10+float(j)*9},row.prerequisites[j]);if(row.prerequisites.size()>5)shadowText({156,46},"......");}
        view_.items.push_back({{p,203,72},row.name,{}, {MenuOp::ResearchSelect,row.id},true,row.id==researchSelected_,prefix+"select"});
    }
    auto closeHeight=button("btnclosepanebmp",{clip.x+clip.z,clip.y},{MenuOp::Resume});
    auto arrow=assets.image("scrollupbmp",true);researchList_.bar={clip.x+clip.z,clip.y+closeHeight,arrow->width,clip.w-closeHeight-8};scrollbar(researchList_,-2);
    image("conpartinfobmp",infoAt,true);
    auto hover=std::find_if(rows.begin(),rows.end(),[&](const auto* row){return row->id==researchHover_;});
    if(hover!=rows.end()){
        const auto& row=**hover;glm::vec4 infoClip{infoAt,info->width,info->height};auto grey=glm::vec4(180.f/255,180.f/255,180.f/255,1);
        text("investmentInfoName",infoAt+glm::vec2(15,10),row.name,"smallBoldFont",{64.f/255,70.f/255,1,1},infoClip);
        text("investmentInfoCost",infoAt+glm::vec2(13,27),"Cost:           $"+std::to_string(row.price),"smallFont",grey,infoClip);
        text("investmentInfoTime",infoAt+glm::vec2(13,39),"Time:          "+std::to_string(row.timeToBuild)+" Sec.","smallFont",grey,infoClip);
        auto font=assets.font("smallFont");std::string remaining=row.description+" ";float lineY=61;
        while(!remaining.empty()){
            size_t n=0;float width=0;while(n<remaining.size() && width+font->glyphs[uint8_t(remaining[n])].z<=info->width-26){width+=font->glyphs[uint8_t(remaining[n])].z;++n;}
            auto word=n;while(word>0 && remaining[word]!=' ' && remaining[word-1]!=' ')--word;
            if(word)n=word;
            if(!n)n=1;else {text("investmentInfoDescription",infoAt+glm::vec2(13,lineY),remaining.substr(0,n),"smallFont",grey,infoClip);lineY+=12;}
            remaining.erase(0,n);
        }
    }
    // The credit image, labels, fonts and clipping are already authored in
    // moneyPane; its composition offset is part of the MDL group.
    for(auto& control:composition)if(control.kind!=OriginalControl::Kind::Slot){
        control.translate(anchor);
        view_.artwork.push_back(std::move(control));
    }
}
}
