#include "ui_script.hpp"
#include "assets.hpp"
#include "original_ui.hpp"
#include "hud_program.hpp"
#include "../ClientCore/HudColors.h"
#include "../ClientCore/AuthoredText.h"
#include "test_reference/image_text.hpp"
#include "test_reference/edit_text.hpp"
#include "test_reference/edit_input.hpp"
#include "test_reference/chat_text.hpp"
#include "../ClientCore/TrainingPresentation.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <fstream>
#include <stdexcept>
int main(int argc,char** argv) {
    try {
        Native::Assets assets(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());
        for(auto name:{"introscreen","trainingscreen","hangar","gamescreen","teamscreen","newgamescreen","loadoutpane","partinfo","hullinfo","inputmappane","gamestartingscreen","creditspane","creditstext","teleportpane"}) {
            auto script=Native::UiScript::read(assets.find(std::string(name)+".mdl"));
            if(script.definitions.empty())throw std::runtime_error(std::string(name)+": empty original UI");
            printf("%s: %zu definitions, %zu imports\n",name,script.definitions.size(),script.imports.size());
        }
        auto expression=Native::UiScript::parse("/* comment */ use \"font\"; offset = Point(-1, 2 + 3*4); text = \"a\\nb\";","parser check");
        if(expression.imports!=std::vector<std::string>{"font"} || expression.definitions.at("text")->name!="a\nb")throw std::runtime_error("MDL string/import semantics");
        bool failed=false;try {Native::UiScript::parse("x = Point(1,);","invalid check");}catch(const std::exception&){failed=true;}
        if(!failed)throw std::runtime_error("Malformed MDL accepted");
        Native::OriginalUi ui(assets);
        {
            size_t cases=0,unsupported=0;
            for(auto name:{"smallFont","smallBoldFont","medVerdana","largeFont"}){
                auto font=assets.font(name);ReferenceChatText::Font original;for(int i=0;i<256;++i)original.advances[i]=font->glyphs[i].z;
                for(std::string_view value:{"","A","AB","ABC","AB  CD","   ABC"," A BC","ABCDEF","ABCD\nEF","AAAA BBBB CCCC DDDD","A very long token: ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789","A description of this development that spans several original chat lines.","A\x81 FF800040B\x82 C"})for(int width=1;width<=450;++width){
                    std::vector<std::string> expected;try{expected=ReferenceChatText::wrap(std::string(value),width,original);}catch(const std::runtime_error&){++unsupported;continue;}
                    std::vector<std::string> actual;Allegiance::ChatTextLines(value,width-6,[&](unsigned char c){return font->glyphs[c].z;},[&](size_t offset,size_t length,bool continued){actual.push_back((continued?"  ":"")+std::string(value.substr(offset,length)));});
                    if(actual!=expected)throw std::runtime_error("Classic chat differs from frozen original ChatListItem wrapping");++cases;
                }
            }
            for(int width=-10;width<=10;++width)for(std::string_view value:{"","A","AB", "MMMMMMMM"}){size_t rows=0;Allegiance::ChatTextLines(value,width,[](unsigned char){return 10.f;},[&](size_t,size_t,bool){if(++rows>value.size()+3)throw std::runtime_error("Narrow original chat fails to make bounded progress");});}
            printf("Original chat: %zu frozen-source comparisons across four resident fonts, widths, spaces/hard tokens/newlines and color bytes; %zu legacy short-width assertion cases excluded and native bounded fallback checked\n",cases,unsupported);
        }
        {
            for(auto name:{"smallFont","medVerdana","largeFont"}){auto font=assets.font(name);ReferenceImageText::Font widths;for(int i=0;i<256;++i)widths.advances[i]=font->glyphs[i].z;
                ReferenceImageText::StringImage original{&widths};auto advance=[&](unsigned char ch){return widths.advances[ch];};
                for(std::string_view value:{"", "\n", "\n\nAB\n", "AB  CD", "   ABC", " A BC", "ABCDEF", "ABCD\nEF", "AAAA BBBB CCCC DDDD", "A description of this development that spans several original small-font lines."})for(int width=-1;width<=180;++width){
                    auto remaining=value;ReferenceImageText::ZString legacy{std::string(value)};size_t lines=0;
                    while(!remaining.empty()){
                        auto expected=original.BreakLine(legacy,width).text;auto line=Allegiance::ImageLine(remaining,width,advance);
                        if(remaining.substr(0,line.length)!=expected || remaining.substr(line.consumed)!=legacy.text)throw std::runtime_error("Native wrapping differs from frozen original StringImage");
                        remaining.remove_prefix(line.consumed);++lines;
                    }
                    if(!legacy.text.empty() || lines!=Allegiance::ImageTextLines(value,width,advance))throw std::runtime_error("StringImage measured lines differ from its original draw policy");
                }
            }
            auto advance=[](unsigned char ch){return ch=='M'?10.f:1.f;};
            if(Allegiance::ClipPaneText("MMMMiiii",12,0,advance)!="M" || Allegiance::ClipPaneText("MMMMiiii",12,1,advance)!="M" ||
               Allegiance::ClipPaneText("MMMMiiii",12,2,advance)!="iiii" || Allegiance::ClipPaneText("MMMMiiii",12,3,advance)!="iiii" ||
               !Allegiance::ClipPaneText("MMMMiiii",0,0,advance).empty())throw std::runtime_error("StringPane prefix/suffix/zero-width clipping differs");
            puts("Original text policy: three resident fonts, 5,460 frozen-source wrapping cases, actual height, separator/newline/hard-break quirks and both-ended clipping passed");
        }

        {
            size_t cases=0;
            for(auto name:{"smallFont","largeFont","medVerdana"}){auto font=assets.font(name);ReferenceEditText::Font widths;for(int i=0;i<256;++i)widths.advances[i]=font->glyphs[i].z;
                for(std::string_view value:{"","MMMMiiii","Long name with spaces and punctuation.","0123456789abcdefghijklmnopqrstuvwxyz"})for(int width=-1;width<=180;++width)for(bool focus:{false,true})for(bool readOnly:{false,true})for(bool password:{false,true}){
                    ReferenceEditText::EditPane pane;pane.m_str=std::string(value);pane.m_type=password?ReferenceEditText::EditPane::Password:ReferenceEditText::EditPane::Normal;pane.m_bFocus=focus;pane.m_bReadOnly=readOnly;pane.m_xsizeCursor=int(widths.advances['o']);pane.width=width;pane.m_pfont=&widths;pane.UpdateStrings();
                    auto advance=[&](unsigned char c){return widths.advances[password?'*':c];};auto native=Allegiance::ClipEditText(value,width,focus,readOnly,advance,pane.m_xsizeCursor);auto actual=password?std::string(native.size(),'*'):std::string(native);
                    if(actual!=pane.m_strClipped.text)throw std::runtime_error("Native edit clipping differs from frozen original EditPane");++cases;
                }
            }
            for(auto sample:{std::pair{10.,true},std::pair{10.25,true},std::pair{10.5,false},std::pair{10.75,false},std::pair{11.,true},std::pair{11.5,false},std::pair{9.75,true}})if(Allegiance::EditCursorVisible(sample.first,10)!=sample.second)throw std::runtime_error("Original end cursor blink differs at the half-second boundary");
            printf("Original EditPane: %zu frozen-source clipping cases across three fonts, focus/read-only/password states, cursor reservation and one-second blink passed\n",cases);
        }

        {
            size_t cases=0;
            for(std::string_view input:{"","ABC DEF 0123456789","123a4b5 67","! hello commander","ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"})for(int capacity=0;capacity<=32;++capacity)for(int batch=1;batch<=15;++batch)for(bool numeric:{false,true})for(bool readOnly:{false,true}){
                ReferenceEditInput::EditPane original;original.m_maxLength=capacity;original.m_type=numeric?ReferenceEditInput::EditPane::Numeric:ReferenceEditInput::EditPane::Normal;original.m_bReadOnly=readOnly;std::string text;
                for(size_t at=0;at<input.size();at+=batch){auto event=input.substr(at,batch);for(unsigned char c:event)original.OnChar(nullptr,{c});
                    if(!readOnly)Allegiance::AppendEditInput(text,event,capacity,[&](unsigned char c){return !numeric || (c>='0' && c<='9');});
                    if(text!=original.m_str.text)throw std::runtime_error("Batched native edit input differs from original individual character events");
                }++cases;
            }
            std::string text="A";Allegiance::AppendEditInput(text,"éB",2);if(text!="A")throw std::runtime_error("Native byte limit splits a UTF-8 input character");
            Allegiance::AppendEditInput(text,"éB",4);if(text!="AéB")throw std::runtime_error("Fitting native UTF-8 text is rejected");Allegiance::EraseEditCharacter(text);Allegiance::EraseEditCharacter(text);if(text!="A")throw std::runtime_error("Native Backspace leaves a UTF-8 continuation byte");
            text.clear();Allegiance::AppendEditInput(text,std::string_view("A\nB\x7f" "C",5),10);if(text!="ABC")throw std::runtime_error("Filtered characters reject the valid remainder of a text event");
            printf("Original edit input: %zu frozen-source normal/numeric/readonly cases across capacities and SDL batch sizes; native UTF-8 capacity/backspace and character filtering passed\n",cases);
        }
        {
            const std::string value="A\x81 FF800040B\x82 C";auto remaining=std::string_view(value);Allegiance::InlineTextColor code;
            remaining.remove_prefix(1);if(!Allegiance::ConsumeTextColor(remaining,code) || !code.valid || code.reset || code.rgba!=std::array<uint8_t,4>{255,128,0,64} || remaining.front()!='B')throw std::runtime_error("Original inline color bytes, case or separator consumption differ");
            remaining.remove_prefix(1);if(!Allegiance::ConsumeTextColor(remaining,code) || !code.valid || !code.reset || remaining!="C")throw std::runtime_error("Original color end does not consume its separator and restore the base color");
            for(size_t length=1;length<10;++length){auto truncated=std::string_view(value).substr(1,length);if(!Allegiance::ConsumeTextColor(truncated,code) || code.valid || !truncated.empty())throw std::runtime_error("Truncated inline color code reads beyond the string or draws its bytes");}
            remaining="\x81 BADVALUE!A";if(!Allegiance::ConsumeTextColor(remaining,code) || code.valid || remaining!="!A")throw std::runtime_error("Invalid color code consumption changes its base color or consumes following text");
            for(auto name:{"smallFont","largeFont","medVerdana"}){auto font=assets.font(name);auto advance=[&](unsigned char c){return font->glyphs[c].z;};if(Allegiance::TextExtent(value,advance)!=advance('A')+advance('B')+advance('C'))throw std::runtime_error("Inline controls consume displayed glyph width");ReferenceImageText::Font original;for(int i=0;i<256;++i)original.advances[i]=advance(i);for(int width=-1;width<=180;++width)for(bool left:{false,true})if(Allegiance::FitText(value,width,left,advance)!=size_t(original.GetMaxTextLength({value},width,left)))throw std::runtime_error("Inline color support changes original byte-based fitting");}
            puts("Original inline color: RRGGBBAA, uppercase hex, reset/base retention, invalid and truncated controls, zero displayed width and unchanged byte fitting across three fonts passed");
        }
        {
            auto control=[](const auto& controls,std::string_view name)->const Native::OriginalControl&{
                auto found=std::find_if(controls.begin(),controls.end(),[&](const auto& c){return c.name==name;});
                if(found==controls.end())throw std::runtime_error("Authored panel control missing: "+std::string(name));return *found;
            };
            if(ui.integers("teleportpane","teleportPaneDestinationColumns")!=std::vector<int>{90,176,268,324} ||
               ui.integers("teampane","teamPanePlayerColumns")!=std::vector<int>{118,167,167,167,167,167} ||
               ui.integers("teampane","eTeamPanePlayerColumns")!=std::vector<int>{129,158,202,294,333,405,457,490} ||
               ui.integers("gamescreen","gameColumns")!=std::vector<int>{116,237,283,348,399,470,521,654})throw std::runtime_error("Authored integer column arrays differ");
            auto compact=ui.layout("teampane","teamPane"),expanded=ui.layout("teampane","expandedTeamPane");
            if(control(compact,"teamPaneTeamComboPane").bounds!=glm::vec4(50,10,125,12) || control(compact,"teamPaneTeamComboPane").faceSize!=glm::vec2(109,12) ||
               control(expanded,"eTeamPanePlayerListPane").bounds!=glm::vec4(180,153,446,180))throw std::runtime_error("Authored combo/list slots differ");
            auto investment=ui.imageLayout("dialog","MergedInvestmentsPaneImage",{{"Money",1234}}, {},{{"InvestmentsPane",{442,450}},{"InvestmentsInfoPane",{214,110}}});
            if(control(investment,"InvestmentsPane").bounds!=glm::vec4(0,-450,442,450) ||
               control(investment,"InvestmentsInfoPane").bounds!=glm::vec4(17,-232,214,110))throw std::runtime_error("Authored dynamic purchase/info composition differs");
            auto credit=std::find_if(investment.begin(),investment.end(),[](const auto& c){return c.text=="1234 cr";});
            if(credit==investment.end() || credit->justify!=3 || credit->bounds.x!=91)throw std::runtime_error("StringPane clip-right override or authored credit offset differs");
            for(auto extent:{glm::vec2(200,300),glm::vec2(300,600),glm::vec2(400,180)}){
                auto state=ui.imageLayout("dialog","gameStateImage",{}, {},{{"GameStateImage",extent}});
                if(control(state,"GameStateImage").bounds!=glm::vec4(0,-extent.y*.5f,extent) ||
                   control(state,"gameStateCloseButton").bounds!=glm::vec4(extent.x-23,4-extent.y*.5f,16,16))throw std::runtime_error("Original nested right/top/left/center image justification differs");
            }
            // An independent description changes the PaneImage key flag, not
            // its native host. Restore the hint after each composed image.
            auto directory=std::filesystem::temp_directory_path()/("allegiance-mdl-key-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(directory);struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}} cleanup{directory};
            std::filesystem::copy_file(assets.find("font.mdl"),directory/"font.mdl");
            // Opaque black and RGB(32,64,96), independent of installed art.
            constexpr unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,1,8,2,0,0,0,123,64,232,221,0,0,0,15,73,68,65,84,120,156,99,96,96,96,80,112,72,0,0,1,71,0,193,170,187,204,14,0,0,0,0,73,69,78,68,174,66,96,130};
            {std::ofstream file(directory/"fixture.png",std::ios::binary);file.write(reinterpret_cast<const char*>(png),sizeof(png));}
            {std::ofstream file(directory/"keytest.mdl");file<<R"(content=BlackPane(ImagePane(ImportImageFromFile("fixture.png",false),[])); opaque=PaneImage(content,false,false); keyed=PaneImage(content,false,true);)";}
            {std::ofstream file(directory/"cliptest.mdl");file<<R"(edit=EditPane(Point(10,1),smallFont); outside=EditPane(Point(10,1),smallFont); inner=ImagePane(ImportImageFromFile("fixture.png",false),[(edit,Point(0,0)),(outside,Point(10,0))]); content=BlackPane(ImagePane(ImportImageFromFile("fixture.png",false),[(inner,Point(1,0))])); image=PaneImage(content,false,false); centered=JustifyImage(image,JustifyCenter); tinted=BlendColorImage(Color(0,0,.5,.5),image); host=ImagePane(image,[]); animated=AnimatedImagePane(image);)";}
            {std::ofstream file(directory/"compiledlayout.mdl");file<<R"(
raw=ImportImageFromFile("fixture.png",true);
label=StringPane("MMMM",Color(.2,.4,.6),Point(10,1),JustifyLeft,smallFont);
inner=ImagePane(raw,[(label,Point(0,0))]);
outer=ImagePane(raw,[(inner,Point(1,0))]);
composed=PaneImage(BlackPane(outer),false,true);
clipped=ClipImage(composed,Rect(1,0,Add(1,Lives),1));
centered=JustifyImage(clipped,JustifyCenter);
rightTop=JustifyImage(composed,JustifyRight|JustifyTop);
leftBottom=JustifyImage(TranslateImage(composed,Point(4,6)),JustifyLeft|JustifyBottom);
centerX=JustifyImage(composed,JustifyXCenter);
centerY=JustifyImage(composed,JustifyYCenter);
tinted=BlendColorImage(Color(0,0,Multiply(Lives,.5),.5),clipped);
host=ImagePane(clipped,[(label,Point(0,0))]);
group=GroupImage([TranslateImage(StringImage(JustifyLeft,20,smallFont,Color(1,1,1),GetPartName(part,OnEveryFrame)),Point(5,1)),TranslateImage(raw,Point(-2,0))]);
groupcenter=JustifyImage(group,JustifyCenter);
empty=BlendColorImage(Color(0,0,1,.5),SwitchImage(Lives,raw,[(0,emptyImage)]));
gauge=JustifyImage(GaugeImage(JustifyLeft,raw,.5),JustifyRight);
)";}
            Native::Assets alternateAssets(directory);Native::OriginalUi alternate(alternateAssets);
            auto keyed=alternate.imageLayout("keytest","keyed"),opaque=alternate.imageLayout("keytest","opaque");
            auto rectangle=[](const auto& controls)->const Native::OriginalControl&{auto found=std::find_if(controls.begin(),controls.end(),[](const auto& c){return c.kind==Native::OriginalControl::Kind::Rectangle;});if(found==controls.end())throw std::runtime_error("BlackPane fill missing");return *found;};
            if(rectangle(keyed).color.a!=0 || rectangle(opaque).color.a!=1)throw std::runtime_error("PaneImage color-key hint does not control BlackPane or leaks to the next image");
            auto image=std::find_if(keyed.begin(),keyed.end(),[](const auto& c){return bool(c.image);});if(image==keyed.end())throw std::runtime_error("Keyed composite image missing");size_t black=0;
            for(size_t i=0;i<image->image->pixels.size();i+=4)if(!(image->image->pixels[i]|image->image->pixels[i+1]|image->image->pixels[i+2])){if(image->image->pixels[i+3])throw std::runtime_error("PaneImage hint does not key its imported surface");++black;}
            if(black!=1 || image->image->pixels[4]!=32 || image->image->pixels[5]!=64 || image->image->pixels[6]!=96 || image->image->pixels[7]!=255)throw std::runtime_error("Color key alters nonblack pixels or misses opaque black");
            auto rawImage=std::find_if(opaque.begin(),opaque.end(),[](const auto& c){return bool(c.image);});if(rawImage==opaque.end() || rawImage->image->pixels[3]!=255)throw std::runtime_error("PaneImage key hint persists after returning to an opaque image");
            for(auto source:{std::pair{"teampane","expandedTeamPane"},std::pair{"teleportpane","TeleportPane"}}){
                auto panel=ui.layout(source.first,source.second,{}, {},true);size_t black=0;for(auto& c:panel)if(c.image)for(size_t i=0;i<c.image->pixels.size();i+=4)if(!(c.image->pixels[i]|c.image->pixels[i+1]|c.image->pixels[i+2])){if(c.image->pixels[i+3])throw std::runtime_error("Original console host ignores its global color-key hint");++black;}
                if(!black)throw std::runtime_error("Original console color-key fixture contains no black pixels");
            }
            if(!alternateAssets.hasImage("fixture.png") || !alternateAssets.hasImage("FIXTURE.PNG") || !alternateAssets.hasImage("keytest") || !alternateAssets.hasImage("KEYTEST#image") || alternateAssets.hasImage("missing") || alternateAssets.hasImage("missing.png"))throw std::runtime_error("UI image existence check disagrees with filename/namespace resolution");
            {
            auto nested=alternate.layout("cliptest","content");auto& edit=control(nested,"edit");auto& outside=control(nested,"outside");
            if(nested.front().bounds!=glm::vec4(0,0,2,1) || edit.bounds!=glm::vec4(1,0,10,1) || edit.visibleBounds()!=glm::vec4(1,0,1,1) || !outside.clip || outside.clip->z!=0)throw std::runtime_error("Nested ImagePane/BlackPane clips or fixed parent extents differ");
            if(alternate.imageSize("cliptest","image")!=glm::vec2(2,1))throw std::runtime_error("PaneImage extent includes children outside its clipped parent");
            if(alternate.imageLayout("cliptest","centered").front().bounds!=glm::vec4(-1,-.5,2,1) || alternate.imageLayout("cliptest","tinted").back().bounds!=glm::vec4(0,-1,2,1) || alternate.layout("cliptest","host").front().bounds!=glm::vec4(0,0,2,1) || alternate.layout("cliptest","animated").front().bounds!=glm::vec4(0,0,2,1))throw std::runtime_error("Clipped children shift JustifyImage/BlendColorImage or composed/animated host extents");
            auto shifted=edit;shifted.translate({70,80});auto label=Native::widgetText(shifted,"M");if(shifted.visibleBounds()!=glm::vec4(71,80,1,1) || label.clip!=shifted.clip)throw std::runtime_error("Control movement or widget substitution loses inherited clipping");
            auto created=ui.layout("gamescreen","createDialog");auto& root=created.front();auto& nameField=control(created,"createEditPane");if(!nameField.clip || nameField.clip->x+nameField.clip->z>root.bounds.x+root.bounds.z || nameField.bounds.z!=450)throw std::runtime_error("Create Game changes the authored edit dimensions or permits drawing past its parent");
            puts("Authored nested clipping: fixed ImagePane/BlackPane/PaneImage extent, partial/empty intersections, translated widgets and original oversized Create Game field passed");
            }
            {
                size_t cases=0;
                for(auto root:{"composed","clipped","centered","rightTop","leftBottom","centerX","centerY","tinted","groupcenter","empty","outer","host"}){
                    bool pane=std::string_view(root)=="outer" || std::string_view(root)=="host";
                    Native::HudProgram program(alternate,root,0,"compiledlayout",pane);
                    for(float lives:{0.f,.5f,1.f,2.f})for(auto name:{"Short","a considerably longer label"}){
                        Native::HudValues values;values[Native::HudNumber::Lives]=lives;values[Native::HudString::Part]=name;
                        auto draws=program.evaluate(values);Native::UiNumbers numbers{{"Lives",lives}};Native::UiStrings strings{{"GetPartName",name}};
                        auto expected=pane?alternate.layout("compiledlayout",root,numbers,strings,true):alternate.imageLayout("compiledlayout",root,numbers,strings);
                        if(draws.size()!=expected.size())throw std::runtime_error("Compiled layout draw count differs: "+std::string(root));
                        for(size_t i=0;i<draws.size();++i){auto d=draws[i];auto& e=expected[i];auto footprint=d.bounds;
                            if(d.kind==Native::HudDraw::Kind::Text && !d.singleLine){auto& font=program.fonts()[d.font];auto metrics=alternateAssets.font(font.symbol,font.height,font.bold);footprint.w=metrics->height*Allegiance::ImageTextLines(d.text,int(footprint.z),[&](unsigned char ch){return metrics->glyphs[ch].z;});}
                            if(d.clipped){auto lo=glm::max(glm::vec2(footprint),glm::vec2(d.clip));auto hi=glm::max(lo,glm::min(glm::vec2(footprint)+glm::vec2(footprint.z,footprint.w),glm::vec2(d.clip)+glm::vec2(d.clip.z,d.clip.w)));footprint={lo,hi-lo};}
                            auto visible=e.visibleBounds();if(glm::any(glm::greaterThan(glm::abs(footprint-visible),glm::vec4(.001f)))){fprintf(stderr,"%s/%zu: compiled=(%.2f %.2f %.2f %.2f) expected=(%.2f %.2f %.2f %.2f)\n",root,i,footprint.x,footprint.y,footprint.z,footprint.w,visible.x,visible.y,visible.z,visible.w);throw std::runtime_error("Compiled layout geometry/clip differs: "+std::string(root)+" item "+std::to_string(i));}
                            if(d.color!=e.color || d.text!=e.text)throw std::runtime_error("Compiled layout tint/string differs: "+std::string(root));
                            if(d.kind==Native::HudDraw::Kind::Image && e.bounds.z>0 && e.bounds.w>0){
                                auto size=glm::vec2(e.bounds.z,e.bounds.w),uvSize=glm::vec2(e.uv.z-e.uv.x,e.uv.w-e.uv.y);
                                auto first=glm::vec2(e.uv)+(glm::vec2(visible)-glm::vec2(e.bounds))/size*uvSize;
                                auto last=first+glm::vec2(visible.z,visible.w)/size*uvSize;
                                if(d.image!=e.image || glm::any(glm::greaterThan(glm::abs(d.uv-glm::vec4(first,last)),glm::vec4(.001f))))throw std::runtime_error("Compiled layout clipped texture coordinates differ: "+std::string(root));
                            }
                            if((d.kind==Native::HudDraw::Kind::Rectangle && e.kind!=Native::OriginalControl::Kind::Rectangle) || (d.kind==Native::HudDraw::Kind::Image && e.kind!=Native::OriginalControl::Kind::Image) || (d.kind==Native::HudDraw::Kind::Text && e.kind!=Native::OriginalControl::Kind::Text))throw std::runtime_error("Compiled layout painter kind differs");
                        }
                        ++cases;
                    }
                }
                Native::HudProgram gauge(alternate,"gauge",0,"compiledlayout");auto draw=gauge.evaluate({});if(draw.size()!=1 || draw[0].bounds!=glm::vec4(-2,-1,1,1))throw std::runtime_error("Justified gauge uses its cropped rather than declared image extent");
                printf("Compiled MDL layouts: %zu nested pane/clip/tint/justification/string-state comparisons, empty switches and full gauge extents passed\n",cases);
            }
            Native::OriginalLoadoutSlot different;different.prefix="i";
            if(ui.loadoutImage(different,"ACS38")!=assets.image("iACS38bmp",true))throw std::runtime_error("Loadout ignores an alternate authored image prefix");
            different.prefix="missing-";
            if(ui.loadoutImage(different,"acs38")!=assets.image("lacs38bmp",true))throw std::runtime_error("Loadout image prefix fallback differs");
            auto games=ui.layout("gamescreen");auto columns=ui.integers("gamescreen","gameColumns"),header=ui.integers("gamescreen","gameHeaderColumns");
            for(size_t i=0;i<columns.size();++i){
                auto& list=control(games,"hover/gameListPane/"+std::to_string(i));auto& head=control(games,"hover/gameListHeader/"+std::to_string(i));
                int left=i?columns[i-1]:0,headerLeft=i?header[i-1]:0;
                if(list.kind!=Native::OriginalControl::Kind::Hover || list.bounds!=glm::vec4(2+left,90,columns[i]-left,388) ||
                   head.bounds.x!=55+headerLeft || head.bounds.z!=header[i]-headerLeft || head.hoverTarget!="gameListHeader"+std::to_string(i) || list.hoverTarget!="gameListPane" || list.help.empty() || list.help!=head.help)throw std::runtime_error("MDL column hover IDs, boundaries or text differ");
            }
            {std::ofstream file(directory/"fonttest.mdl");file<<R"(labelFont=Win32Font("Verdana",18,true,0); wrapped=StringImage(JustifyLeft,40,labelFont,Color(1,1,1),"WW WW WW"); screen=StringPane("MMMM",Color(1,1,1),Point(100,18),JustifyLeft,labelFont); large=StringImage(JustifyLeft,1000,Win32Font("Verdana",48,false,0),Color(1,1,1),"W"); paragraph=StringImage(JustifyLeft,wrapWidth,labelFont,Color(1,1,1),"MMMM MMMM MMMM MMMM"); combo=StringColorComboPane(Point(31,18),Point(51,22),labelFont,Color(.2,.4,.6)); edit=EditPane(Point(71,24),labelFont); clipped=StringPane("MMMMiiii",Color(1,1,1),Point(31,18),JustifyCenter,labelFont,true);)";}
            {std::ofstream file(directory/"hovertest.mdl");file<<R"(labelFont=Win32Font("Verdana",18,true,0); firstSite=HoverSite(0); secondSite=HoverSite(0); firstHelp=SwitchString(HoverSiteGetID(firstSite),"",[(7,"FIRST")]); secondHelp=SwitchString(HoverSiteGetID(secondSite),"",[(7,"SECOND")]); firstSlot=ListPane(Point(50,20)); secondSlot=ListPane(Point(50,20)); firstText=StringPane(firstHelp,Color(1,1,1),Point(50,18),JustifyLeft,labelFont); secondText=StringPane(secondHelp,Color(1,1,1),Point(50,18),JustifyLeft,labelFont); screen=ScreenPane(ImportImageFromFile("fixture.png",false),[(HoverPane(firstSite,7,firstSlot),Point(0,0)),(HoverPane(secondSite,7,secondSlot),Point(60,0)),(firstText,Point(0,30)),(secondText,Point(60,30))]);)";}
            {std::ofstream file(directory/"imagelisttest.mdl");file<<R"(use "fonttest"; cueAlias=cue; bitmap=ImportImageFromFile("fixture.png",true); label=TranslateImage(StringImage(JustifyRight,40,labelFont,Color(.1,.3,.5,.7),"Authored paragraph"),Point(9,13)); entries=[(GroupImage([label,BlendImage(bitmap,BlendModeAdd)]),cueAlias),(bitmap,-1)]; missing=[(bitmap,missingCue)]; malformed=[bitmap]; clipped=[(ClipImage(TranslateImage(BlendImage(bitmap,BlendModeAdd),Point(3,7)),Rect(3,7,4,8)),4)];)";}
            Native::Assets fontAssets(directory);Native::OriginalUi fontUi(fontAssets);
            auto images=fontUi.imageList("imagelisttest","entries",{{"cue",1234}});
            if(images.size()!=2 || images[0].value!=1234 || images[1].value!=-1 || images[0].artwork.size()!=2 || !images[0].artwork[0].additive ||
               images[0].artwork[0].image->pixels[3]!=0 || images[1].artwork[0].additive)throw std::runtime_error("Authored image list loses pair values, imported aliases, layers or independent alpha/additive flags");
            auto& labelImage=images[0].artwork[1];
            if(labelImage.font!="Verdana" || labelImage.fontHeight!=18 || !labelImage.fontBold || labelImage.justify!=2 || labelImage.color!=glm::vec4(.1,.3,.5,.7) || labelImage.bounds.x!=9 || labelImage.bounds.y!=-13 || labelImage.bounds.z!=40)
                throw std::runtime_error("Authored image list substitutes imported font, color, justification, width or coordinates");
            auto clippedImage=fontUi.imageList("imagelisttest","clipped").front().artwork.front();
            if(!clippedImage.additive || clippedImage.visibleBounds()!=glm::vec4(3,-8,1,1))throw std::runtime_error("Authored image list ignores composed image clipping");
            for(auto root:{"missing","malformed"}){bool rejected=false;try{fontUi.imageList("imagelisttest",root);}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Malformed image list or unresolved cue accepted");}
            puts("Authored image lists: linked font/cue aliases, layer order, independent color key/additive flags, color/justification/translation/clipping and strict missing/malformed values passed");
            auto fontPane=fontUi.layout("fonttest");auto fontImage=fontUi.imageLayout("fonttest","wrapped");auto largeImage=fontUi.imageLayout("fonttest","large");
            auto metrics=fontAssets.font("Verdana",18,true),largeMetrics=fontAssets.font("Verdana",48,false);
            if(fontPane.size()!=1 || fontPane[0].font!="Verdana" || fontPane[0].fontHeight!=18 || !fontPane[0].fontBold || fontImage.size()!=1 ||
               fontImage[0].font!="Verdana" || fontImage[0].fontHeight!=18 || !fontImage[0].fontBold || metrics->height!=18 ||
               metrics!=fontAssets.font("Verdana",18,true) || metrics->coverage->format!=Native::ImageFormat::Coverage ||
               metrics==fontAssets.font("Verdana",18,false) || metrics->glyphs['W'].z<=0)throw std::runtime_error("Authored system fonts lose their family/cell height/weight or resident cache");
            ReferenceImageText::Font widths;for(int i=0;i<256;++i)widths.advances[i]=metrics->glyphs[i].z;ReferenceImageText::StringImage legacy{&widths};ReferenceImageText::ZString remaining{fontImage[0].text};int lines=0;while(!remaining.text.empty()){legacy.BreakLine(remaining,40);++lines;}
            auto paragraph=fontUi.imageLayout("fonttest","paragraph",{{"wrapWidth",int(metrics->glyphs['M'].z*7)}});auto combo=fontUi.layout("fonttest","combo").front(),edit=fontUi.layout("fonttest","edit").front();auto label=Native::widgetText(combo,"ABCD");
            if(paragraph.front().bounds.w!=18*4 || combo.bounds!=glm::vec4(0,0,51,22) || combo.faceSize!=glm::vec2(31,18) || combo.widget!=Native::OriginalControl::Widget::ColorCombo || combo.font!="Verdana" || !combo.fontBold || combo.fontHeight!=18 || combo.color!=glm::vec4(.2,.4,.6,1) || label.bounds!=glm::vec4(0,0,31,18) || label.textMode!=Native::OriginalControl::TextMode::Pane || edit.widget!=Native::OriginalControl::Widget::Edit || edit.fontHeight!=18)throw std::runtime_error("MDL word-wrapped image height or separate combo face/control style is discarded");
            if(fontImage[0].bounds.w!=lines*18 || largeImage[0].bounds.w!=48 || largeMetrics->glyphs['W'].z<=32 || largeMetrics->coverage->width<1024)throw std::runtime_error("System-font layout metrics disagree with rasterization or large glyphs are truncated");
            Native::HudProgram clipped(fontAssets,fontUi.script("fonttest"),"clipped",0,true);if(clipped.evaluate({}).front().justify!=3)throw std::runtime_error("Compiled HUD StringPane ignores its sixth clip-right override");
            Native::HudProgram program(fontAssets,fontUi.script("fonttest"),"wrapped",0);auto draws=program.evaluate({});auto spec=program.fonts()[draws[0].font];
            if(fontAssets.font(spec.symbol,spec.height,spec.bold)!=metrics)throw std::runtime_error("HUD and menu fonts are independently rasterized");
            auto scoped=fontUi.layout("hovertest");auto& firstHover=control(scoped,"hover/firstSlot");auto& secondHover=control(scoped,"hover/secondSlot");
            if(firstHover.help!="FIRST" || secondHover.help!="SECOND" || firstHover.helpSite==secondHover.helpSite || firstHover.helpSite!=control(scoped,"firstText").textSite || secondHover.helpSite!=control(scoped,"secondText").textSite)throw std::runtime_error("Hover controls assume a global string table or cross namespace/site bindings");
            puts("Asset UI: namespace/file presence, eight list/header hover regions, system-font menu/image/HUD metrics, shared cache and large glyphs passed");
            puts("Original panel adapters: four authored column tables, combo/list slots, dynamic research composition, credit clipping, three Game State sizes and independent color-key flags passed");
        }
        static_assert(Allegiance::HudShadowArgb==0x9d376f33u);
        {
            Native::HudOverlayTable overlays(ui,0);Native::HudValues values;values[Native::HudNumber::ChatLines]=7;
            auto entries=overlays.evaluate(values,{1920,1080});
            struct Expected {const char* name;glm::vec2 on,off;float duration;uint32_t mask;bool transparent;};
            const Expected expected[]={
                {"MergedInventoryPaneImage",{1884,1040},{1884,1230},.5f,8192,true},
                {"SectorImage",{49,1044},{49,1267},.5f,33536,false},
                {"CenterHudDisplayImage",{640,780},{640,-460},.5f,48,true},
                {"SoftCenterHudDisplayImage",{728,119},{640,-1000},.5f,48,true},
                {"SoftCenterHudDisplayParciStyleImage",{36,900},{0,1267},.5f,48,true},
                {"TargetDisplayImage",{640,1055},{640,1263},.5f,65584,true},
                {"GHLargeTargetView",{1690,330},{1690,-50},.75f,65590,true},
                {"CommandModeIcons",{34,405},{-200,405},.5f,2,true},
                {"cortCommandModeInfo",{36,420},{-200,420},.5f,2,true},
                {"cortCommandModeInfoBase",{36,200},{36,200},0,69,true},
                {"CommandPaneImage",{825,1040},{825,1270},.5f,6,false},
                {"LocationInCockpitImage",{1740,37},{1740,37},0,8,true},
                {"LocationInHangarImage",{1740,37},{1740,37},0,65,true},
                {"LookingAtImage",{1740,37},{1740,37},0,4,true},
                {"EjectPodImage",{874,627},{874,627},.5f,8,true},
                {"LivesImage",{40,97},{45,97},.5f,8,true},
                {"KillsImage",{40,177},{45,177},.5f,8,true},
                {"CountdownImage",{40,137},{45,137},.5f,8,true},
                {"RipcordCountdownImage",{885,525},{885,525},.5f,58,true},
                {"DeathBannerPaneImage",{855,525},{855,525},.5f,65613,true}};
            for(const auto& e:expected){auto index=overlays.find(e.name);if(index>=entries.size())throw std::runtime_error("Missing authored overlay");const auto& actual=entries[index];
                if(actual.on!=e.on || actual.off!=e.off || actual.duration!=e.duration || actual.mask!=e.mask || actual.undetectable!=e.transparent)throw std::runtime_error("Original overlay metadata differs: "+std::string(e.name));
            }
            std::unique_ptr<Native::HudOverlayTable> alternate;
            {auto source=Native::UiScript::parse("dialogs=[(Custom,SideRight,Point(-12,7),Point(4+StyleHUD,-12*NumChatLines),.25,0x2|0x8,true)];","alternate overlay set");
                alternate=std::make_unique<Native::HudOverlayTable>(assets,source,3);}
            auto changed=alternate->evaluate(values,{801,601});
            if(changed.size()!=1 || changed[0].on!=glm::vec2(808,384.5f) || changed[0].off!=glm::vec2(789,293.5f) || changed[0].mask!=10 || alternate->find("MergedInventoryPaneImage")!=size_t(-1))throw std::runtime_error("Alternate overlay asset description or namespace lifetime differs");
            values[Native::HudNumber::ChatLines]=4;changed=alternate->evaluate(values,{801,601});
            if(changed[0].on!=glm::vec2(808,348.5f))throw std::runtime_error("Dynamic overlay point expression was frozen");
            Native::HudOverlayMotion motion;const auto& spec=changed[0];
            if(!motion.update(true,1,spec.duration) || motion.point(spec,1)!=spec.on)throw std::runtime_error("Attached overlay is not settled");
            if(!motion.update(false,2,spec.duration) || motion.point(spec,2)!=spec.on || motion.point(spec,2.125)!=glm::mix(spec.on,spec.off,.5f) || motion.update(false,2.25,spec.duration))throw std::runtime_error("Original overlay exit slide or completion differs");
            if(!motion.update(true,3,spec.duration) || motion.point(spec,3)!=spec.off || motion.point(spec,3.125)!=glm::mix(spec.off,spec.on,.5f))throw std::runtime_error("Original overlay entrance slide differs");
            motion.update(false,3.125,spec.duration);if(motion.point(spec,3.125)!=spec.on)throw std::runtime_error("WinTrek endpoint reversal semantics differ");
            Native::HudOverlay immediate;immediate.on={2,3};immediate.off={4,5};Native::HudOverlayMotion zero;
            zero.update(false,0,0);if(!zero.update(true,1,0) || zero.point(immediate,1)!=immediate.on || zero.update(false,2,0))throw std::runtime_error("Zero-duration overlay does not change immediately");
            printf("Original overlay table: %zu entries, 20 independent placement/mode/duration/transparency comparisons, alternate assets and linear/reversed/zero-duration slides passed\n",entries.size());
        }
        {
            using N=Native::HudNumber;using S=Native::HudString;
            Native::HudValues v;v[S::Part]="Gun";v[S::PartKey]="1.";v[S::Gunner]="Gunner";
            v[N::PartRange]=123.9f;v[N::PartDamage]=7.1f;v[N::PartRate]=9.8f;v[N::PartCount]=1.25f;v[N::Observers]=3;
            v[N::PartMounted]=.5f;v[N::PartArmed]=.75f;
            size_t cases=0;
            for(int style=0;style<6;++style)for(auto script:{"invdefault","invsweapon","invsmissile","invsmine","invchaff","invshield","invcloak","invafterburn",
                "invemptyweapon","invemptymissile","invemptychaff","invemptyshield","invemptycloak","invemptyafterburner","invemptymine","invemptycargo","invobserver","invturret","invemptyturret"}) {
                Native::HudProgram program(ui,"InventoryLine",style,script,true);
                for(int state=0;state<4;++state){v[N::PartReady]=state;v[N::PartSelected]=v[N::PartActive]=v[N::PartEmpty]=state%2;
                    auto draws=program.evaluate(v);if(draws.empty())throw std::runtime_error("Compiled inventory lost its row background");
                    for(const auto& d:draws)if(d.kind==Native::HudDraw::Kind::Text && (!d.singleLine || d.bounds.w!=12 || program.fonts()[d.font].symbol!="smallFont"))throw std::runtime_error("MDL StringPane lost its original font or single-line clipping");
                    if(std::string_view(script)=="invsweapon" && state==1 && std::none_of(draws.begin(),draws.end(),[](const auto& d){return d.kind==Native::HudDraw::Kind::Image && d.bounds==glm::vec4(130,5,32,13);}))throw std::runtime_error("GaugePane is not integer-cropped and clipped by its ImagePane");
                    if(std::string_view(script)=="invsweapon" && (state==0 || state==3) && std::none_of(draws.begin(),draws.end(),[](const auto& d){return d.text=="124m / 7 X 10" && d.bounds==glm::vec4(130,5,83,12);}))throw std::runtime_error("Weapon inventory statistics are not authored by MDL");
                    if(std::string_view(script)=="invdefault" && state==0 && std::none_of(draws.begin(),draws.end(),[](const auto& d){return d.text=="1.2";}))throw std::runtime_error("Original two-significant-digit cargo formatter differs");
                    ++cases;
                }
            }
            std::unique_ptr<Native::HudProgram> changed;
            {
                auto source=Native::UiScript::parse("InventoryLine = ImagePane(ImportImage(\"inventorylinebkgndbmp\", false), [(StringPane(ConcatinatedString(partkey, GetPartName(part, OnEveryFrame)), Color(.2,.4,.6), Point(72,12)), Point(47,1))]);","alternate inventory MDL");
                changed=std::make_unique<Native::HudProgram>(assets,source,"InventoryLine",0,true);
            } // compiled records must outlive their source namespace
            auto draws=changed->evaluate(v);if(draws.size()!=2 || draws[1].text!="1.Gun" || draws[1].bounds!=glm::vec4(47,1,72,12) || draws[1].color!=glm::vec4(.2f,.4f,.6f,1))throw std::runtime_error("Alternative asset-set MDL changes require new C++ or retain dead source references");
            printf("Original inventory MDL: %zu style/row/readiness cases, exact statistics/formatting/clipping and independent alternate asset descriptions passed\n",cases);
        }
        {
            size_t instructions=0;bool failed=false;
            for(int style=0;style<6;++style)for(auto root:{"CenterHudDisplayImage","SoftCenterHudDisplayImage","SoftCenterHudDisplayParciStyleImage","TargetDisplayImage","GHLargeTargetView","GHMsgDetectedView","CommandModeIcons","cortCommandModeInfo","cortCommandModeInfoBase","EjectPodImage","DeathBannerPaneImage","RipcordCountdownImage","LivesImage","KillsImage","CountdownImage","LocationInCockpitImage","LocationInHangarImage","LookingAtImage"}) {
                try {Native::HudProgram program(ui,root,style);instructions+=program.instructionCount();Native::HudValues v;
                    v[Native::HudNumber::TargetExists]=1;v[Native::HudString::Target]="Target";
                    v[Native::HudString::Sector]="Viewed sector";v[Native::HudString::StationSector]="Docked sector";
                    for(auto& color:v.colors)color=glm::vec4(1);
                    auto draws=program.evaluate(v);for(auto& d:draws)if(!std::isfinite(d.bounds.x) || !std::isfinite(d.bounds.y))throw std::runtime_error("Nonfinite HUD bounds");
                    std::string_view surface(root);
                    if(surface=="LocationInHangarImage" || surface=="LocationInCockpitImage" || surface=="LookingAtImage"){
                        auto expected=surface=="LocationInHangarImage"?"Docked sector":"Viewed sector";
                        if(std::none_of(draws.begin(),draws.end(),[&](const auto& d){return d.kind==Native::HudDraw::Kind::Text && d.text==expected;}))throw std::runtime_error("Authored location label loses its station/view-sector data binding");
                    }
                }catch(const std::exception& e){fprintf(stderr,"HUD style %d root %s: %s\n",style,root,e.what());failed=true;}
            }
            if(failed)throw std::runtime_error("Authored HUD compilation failed");
            printf("Original HUD: all six authored styles / 18 surfaces, %zu resident instructions checked\n",instructions);
            Native::HudProgram classic(ui,"CenterHudDisplayImage",0);Native::HudValues v;
            v[Native::HudNumber::Hull]=.501f;v[Native::HudNumber::Shield]=.75f;v[Native::HudNumber::Speed]=123.9f;
            v[Native::HudNumber::Throttle]=.5f;v[Native::HudNumber::Detected]=1;
            for(auto& color:v.colors)color=glm::vec4(.2f,.3f,.4f,.75f);
            auto draws=classic.evaluate(v);
            auto find=[&](std::string_view text)->const Native::HudDraw&{for(const auto& d:draws)if(d.text==text)return d;throw std::runtime_error("Authored HUD caption missing: "+std::string(text));};
            auto& speed=find("124 mps");auto& armor=find("ARMOR");auto& shield=find("SHIELD");
            if(speed.bounds!=glm::vec4(281,-152,80,0) || classic.fonts()[speed.font].symbol!="lgBoldVerdana" ||
               armor.bounds!=glm::vec4(205,-243,100,0) || shield.bounds!=glm::vec4(340,-243,100,0) ||
               classic.fonts()[armor.font].symbol!="smVerdana" || armor.color!=glm::vec4(.2f,.3f,.4f,.75f))throw std::runtime_error("Original HUD anchors, fonts or color bindings differ");
            auto hull=assets.image("hullgaugebmp",true),throttle=assets.image("thrustgauge2bmp",true);
            auto image=[&](const auto& asset)->const Native::HudDraw&{for(auto& d:draws)if(d.image==asset)return d;throw std::runtime_error("HUD gauge missing");};
            if(image(hull).bounds!=glm::vec4(242,-248,48,41) || image(throttle).bounds!=glm::vec4(176,-364,95,54))throw std::runtime_error("Original gauge clipping is not integer cropped at its authored anchor");
            v[Native::HudNumber::Artifact]=1;v[Native::HudNumber::Flag]=1;v[Native::HudNumber::Detected]=0;draws=classic.evaluate(v);
            if(std::none_of(draws.begin(),draws.end(),[&](const auto& d){return d.image==assets.image("captureflaglbmp",true);}) ||
               std::none_of(draws.begin(),draws.end(),[&](const auto& d){return d.image==assets.image("captureflagrbmp",true);}) ||
               std::none_of(draws.begin(),draws.end(),[&](const auto& d){return d.image==assets.image("radonbmp",true);}))throw std::runtime_error("Carried objectives or undetected indicator missing");
            Native::HudProgram target(ui,"TargetDisplayImage",0);v[Native::HudNumber::TargetExists]=1;v[Native::HudNumber::TargetType]=1;
            v[Native::HudString::Target]="Outpost";v[Native::HudString::TargetType]="Garrison";auto td=target.evaluate(v);
            if(std::none_of(td.begin(),td.end(),[](auto& d){return d.text=="Outpost: Garrison" && d.bounds==glm::vec4(240,-125,160,0);}) ||
               std::any_of(td.begin(),td.end(),[&](auto& d){return d.image==assets.image("targetexperiencebmp",true);}))throw std::runtime_error("Target name anchor/wrapping or ship-only experience display differs");
            v[Native::HudNumber::ShowCenter]=0;v[Native::HudNumber::ShowTarget]=0;
            if(!classic.evaluate(v).empty() || !target.evaluate(v).empty())throw std::runtime_error("MDL VisibleImage options are not applied immediately");
            puts("Original HUD data: gauge crop/UV direction, authored font/color/label anchors, flag/artifact/eye and ship-only target experience checked");
            for(int style=0;style<6;++style) {
                Native::HudProgram shell(ui,"MergedInventoryPaneImage",style);v[Native::HudNumber::InventoryWidth]=214;
                for(unsigned count=0;count<17;++count){v[Native::HudNumber::InventoryHeight]=float(36+18*count);auto records=shell.evaluate(v);
                    auto host=std::find_if(records.begin(),records.end(),[](const auto& d){return d.kind==Native::HudDraw::Kind::Inventory;});
                    if(host==records.end() || host->bounds!=glm::vec4(style==3?-220.f:-214.f,-39.f-18*count,214,36+18*count))throw std::runtime_error("MDL inventory grouping, column size, translation or right justification differs");}
            }
        }
        {
            // Time resident programs only: compilation, font loading and
            // checks below are outside each measured sample. No timing gate.
            using N=Native::HudNumber;using S=Native::HudString;
            for(int style=0;style<6;++style){
                std::vector<Native::HudProgram> programs;
                for(auto root:{"CenterHudDisplayImage","TargetDisplayImage","LocationInCockpitImage","RipcordCountdownImage","CommandModeIcons"})programs.emplace_back(ui,root,style);
                for(auto source:{"invsweapon","invshield"})programs.emplace_back(ui,"InventoryLine",style,source,true);
                Native::HudValues values;values[N::TargetExists]=values[N::Ripcord]=1;
                values[S::Sector]="Sector";values[S::Part]="Weapon";values[S::PartKey]="1.";
                for(auto& color:values.colors)color=glm::vec4(1);
                std::vector<const Native::HudDraw*> storage;for(auto& program:programs)storage.push_back(program.evaluate(values).data());
                std::vector<double> timings;timings.reserve(2000);
                for(unsigned frame=0;frame<2032;++frame){
                    float fraction=(frame%101)/100.f;values[N::Hull]=values[N::Shield]=values[N::PartMounted]=fraction;
                    values[N::Speed]=float(frame%300);values[N::TargetRange]=float(frame*7%3000);
                    values[N::PartReady]=float(frame%4);values[N::PartActive]=float(frame%2);
                    values[S::Target]=frame%2?"Outpost":"A longer target name";
                    auto started=std::chrono::steady_clock::now();
                    for(auto& program:programs)program.evaluate(values);
                    auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count();
                    for(size_t i=0;i<programs.size();++i){auto records=programs[i].evaluate(values);
                        if(records.data()!=storage[i])throw std::runtime_error("Resident HUD output storage moved during evaluation");
                        for(auto& draw:records)if(draw.text.capacity()<512)throw std::runtime_error("Resident HUD lost reserved text storage");
                    }
                    if(frame>=32)timings.push_back(elapsed);
                }
                std::sort(timings.begin(),timings.end());
                printf("Resident HUD style %d: seven programs, 2000 samples median %.2f us / p99 %.2f us; stable output/text storage\n",style,timings[1000],timings[1980]);
            }
        }
        if(ui.number("dialog","ShowScrollbarOnCockpitChat")!=1 || ui.number("dialog","CriticalTipImageYOffset",{{"NumChatLines",7}})!=-134 ||
           ui.number("dialog","VoteTextImageYOffset",{{"NumChatLines",6}})!=-157)
            throw std::runtime_error("Authored console scalar references/arithmetic differ");
        {
            // Independent authored cursor anchors, including the asymmetric
            // command icons. Image coordinates grow upward around the pointer.
            struct Cursor {const char* root;const char* image;int x,y;};
            const Cursor cursors[]={
                {"default","cursorbmp",7,7},{"drop","dropcursorbmp",7,7},{"drag","dragcursorbmp",7,7},
                {"select","tgcursorbmp",7,7},{"illegal","illegalcursorbmp",7,7},{"wait","waitcursorbmp",7,7},
                {"attack","ackillbmp",7,7},{"capture","accptbmp",5,2},{"defend","acdefendbmp",7,7},
                {"pickup","acpickupbmp",7,3},{"goto","acgotobmp",11,10},{"repair","acrepairbmp",7,7},
                {"join","acjoinbmp",7,7},{"mine","acminebmp",7,7},{"build","acbuildbmp",7,7}};
            for(auto c:cursors) {
                auto image=assets.image(c.image,true),raw=assets.image(c.image,false);auto art=ui.imageLayout("cursor",c.root);
                if(art.size()!=1 || art[0].image!=image || art[0].bounds!=glm::vec4(-c.x,c.y-int(image->height),image->width,image->height))
                    throw std::runtime_error(std::string(c.root)+": original cursor hotspot or artwork changed");
                size_t transparent=0,visible=0;
                for(size_t i=0;i<raw->pixels.size();i+=4)if(!(raw->pixels[i]|raw->pixels[i+1]|raw->pixels[i+2])) {
                    if(image->pixels[i+3])throw std::runtime_error("Cursor black cutout is opaque");++transparent;
                }else ++visible;
                if(!transparent || !visible || c.x>=int(image->width) || int(image->height)-c.y<0)throw std::runtime_error("Invalid cursor fixture");
            }
            auto throttle=Allegiance::TrainingCalloutFor(ThrottleTrainingOverlay,0,0,800,600);
            auto speed=Allegiance::TrainingCalloutFor(SpeedTrainingOverlay,0,0,1280,720);
            auto text=Allegiance::TrainingTextFor(throttle,98,17);
            if(throttle.ax!=273.5f || throttle.ay!=393.5f || throttle.bx!=223.5f || throttle.by!=443.5f ||
               text.x!=125.5f || text.y!=424.5f || text.secondaryX!=-14.5f || text.secondaryY!=403.5f ||
               speed.ax!=615 || speed.ay!=254.5f || speed.bx!=515 || speed.by!=154.5f)
                throw std::runtime_error("Training callout view anchors or original secondary caption moved");
            for(int i=SpeedTrainingOverlay;i<=CloakTrainingOverlay;++i)
                if(!*Allegiance::TrainingCalloutFor(TrainingOverlay(i),0,0,800,600).description)throw std::runtime_error("Authored training cue missing");
            puts("Original cursors: all 15 keyed images/hotspots; training callout captions and view anchors checked");
        }
        {
            auto controls=ui.layout("teleportpane","TeleportPane");
            auto find=[&](std::string_view name)->const Native::OriginalControl&{for(const auto& c:controls)if(c.name==name)return c;throw std::runtime_error("Teleport script lost control: "+std::string(name));};
            if(find("teleportTeleportButtonPane").bounds.x!=12 || find("teleportTeleportButtonPane").bounds.y!=184 ||
               find("teleportBackButtonPane").bounds.x!=312 || find("teleportBackButtonPane").bounds.y!=3)
                throw std::runtime_error("Original teleport buttons moved");
            std::array<float,5> edges{2,92,178,270,326};
            for(int i=0;i<4;++i){const auto& c=find("teleportPaneDestinationListHeader"+std::to_string(i));
                if(c.kind!=Native::OriginalControl::Kind::Button || c.bounds.x!=edges[i] || c.bounds.z!=edges[i+1]-edges[i] || c.bounds.y!=23)
                    throw std::runtime_error("Original teleport header columns changed");}
            puts("Original teleport script: button artwork, header columns and authored positions checked");
        }
        {
            auto controls=ui.layout("newgamescreen","screen",{{"CanChooseMaxPlayers",0},{"IsCustomMap",0}});
            auto preview=std::find_if(controls.begin(),controls.end(),[](const auto& c){return c.name=="mapPreviewPane";});
            if(preview==controls.end() || !preview->image || preview->bounds.x!=635 || preview->bounds.y!=128)throw std::runtime_error("Authored game setup map preview is missing or misplaced");
            auto lives=ui.choices("newgamescreen","LivesNames",{"LivesValues"});
            auto connectivity=ui.choices("newgamescreen","ConnectivityNames",{"ConnectivityValues"});
            auto resources=ui.choices("newgamescreen","ResourcesNames",{"ResourcesNeutralMinable","ResourcesPlayerMinable","ResourcesNeutralSpecial","ResourcesPlayerSpecial"});
            if(lives.size()!=7 || lives[0].values!=std::vector<double>{32767} || lives[1].label!="1" || lives[1].values!=std::vector<double>{0} || lives.back().values!=std::vector<double>{9})throw std::runtime_error("Original lives death-count convention changed");
            if(connectivity.size()!=3 || connectivity[0].values!=std::vector<double>{7} || connectivity[1].values!=std::vector<double>{2} || connectivity[2].values!=std::vector<double>{0} || resources.size()!=8 || resources[0].values.size()!=4)throw std::runtime_error("Authored game setup choice arrays are not aligned");
            printf("Original game setup: authored preview, lives, connectivity and four-column resources checked\n");
        }
        for(auto name:{"introscreen","trainingscreen","hangar"}) {
            auto controls=ui.layout(name);
            size_t buttons=0;for(auto& c:controls)buttons+=c.kind==Native::OriginalControl::Kind::Button;
            size_t expected=std::string_view(name)=="introscreen"?11:std::string_view(name)=="trainingscreen"?9:8;
            if(buttons!=expected)throw std::runtime_error(std::string(name)+": lost authored control");
            printf("%s: %zu compiled controls, %zu buttons\n",name,controls.size(),buttons);
        }
        auto frames=assets.animation("animlaunch");
        if(frames.size()<2 || frames.front()->pixels==frames.back()->pixels)throw std::runtime_error("Original launch animation lost its XOR frames");
        printf("Original launch button: %zu decoded frames\n",frames.size());
        for(auto entry:{std::pair{"gamescreen","screen"},std::pair{"teamscreen","screen"},std::pair{"inputmappane","inputMapPane"}}) {
            auto controls=ui.layout(entry.first,entry.second);size_t buttons=0;for(auto& c:controls)buttons+=c.kind==Native::OriginalControl::Kind::Button;
            if(buttons<3)throw std::runtime_error(std::string(entry.first)+": lost original buttons");
            printf("%s: %zu compiled controls, %zu buttons\n",entry.first,controls.size(),buttons);
        }
        auto loadout=ui.layout("loadoutpane","loadoutPane",{{"money",1234},{"hullMaxSpeed",.5},{"hullMaxSpeedNumber",90}},
            {{"titleString","NAMESPACE CHECK"},{"hullDescriptionString","Hull description"},{"civImage","beltersloadoutpanebmp"},{"civHullImage","beltershullinfobmp"}});
        bool title=false,description=false,gauge=false,background=false,defaults=false;
        for(auto& c:loadout) {
            if(c.kind==Native::OriginalControl::Kind::Text && c.text=="NAMESPACE CHECK")title=c.bounds.z==175;
            if(c.kind==Native::OriginalControl::Kind::Text && c.text=="Hull description")description=c.bounds.x==212 && c.bounds.y==118 && c.bounds.z==260 && c.bounds.w<=110;
            if(c.image && c.bounds.x==295 && c.bounds.z==std::floor(c.image->width*.5f))gauge=c.uv.z==c.bounds.z/c.image->width;
            if(c.name=="loadoutPane" && c.image)background=c.bounds.z==800 && c.bounds.w==600;
            if(c.name=="defaultButtonPane")defaults=c.bounds.x==482 && c.bounds.y==318;
        }
        if(!title || !description || !gauge || !background || !defaults)throw std::runtime_error("Imported loadout namespaces, signed coordinates, image composition or gauge crop differ");
        printf("Original loadout: %zu controls, scoped imports and composite/gauge coordinates checked\n",loadout.size());
        Native::UiNumbers partTypes{{"equipmentTypeWeapon",1},{"equipmentTypeShield",4},{"equipmentTypeMagazine",2},{"equipmentTypeDispenser",3},
            {"equipmentTypeCloak",5},{"equipmentTypeAfterburner",7},{"equipmentTypeChaff",0},{"equipmentTypeMine",-4}};
        for(auto [type,name]:{std::pair{1,"weapon"},std::pair{4,"shield"},std::pair{2,"magazine"},std::pair{3,"probe"},
                             std::pair{5,"cloak"},std::pair{7,"afterburner"},std::pair{0,"chaff"},std::pair{-4,"mine"}}) {
            partTypes["equipmentType"]=type;partTypes["weaponRate"]=.5;
            auto controls=ui.layout("partinfo","partInfoPane",partTypes);auto background=assets.image(std::string("partinfo")+name+"bmp",false);
            auto it=std::find_if(controls.begin(),controls.end(),[&](const auto& c){return c.image==background;});
            if(it==controls.end() || it->bounds!=glm::vec4(454,-3,background->width,background->height) || !it->clip || *it->clip!=glm::vec4(0,0,800,66))
                throw std::runtime_error(std::string(name)+": original part-stat labels, ImportImageLR coordinates or pane clipping lost");
            if(type==1){auto gauge=assets.image("loadoutgaugebmp",false);auto row=std::find_if(controls.begin(),controls.end(),[&](const auto& c){return c.image==gauge;});
                if(row==controls.end() || it>=row || row->bounds.z!=std::floor(gauge->width*.5f))throw std::runtime_error("Part-stat label background covers the gauge");}
        }
        printf("Original part statistics: all eight label backgrounds, image origins, clipping and gauge ordering checked\n");
        auto slots=ui.loadoutSlots(414);bool cargo=false,weapon=false;
        for(auto& slot:slots){if(!slot.empty || slot.prefix.empty())throw std::runtime_error("Original equipment slot lost its artwork");cargo|=slot.type==-1 && slot.mount==4;weapon|=slot.type==1 && slot.mount==0;}
        if(!cargo || !weapon)throw std::runtime_error("Original loadout slot types or mounts differ");
        auto credits=ui.imageLayout("creditstext");auto extent=ui.imageSize("creditstext");bool author=false;
        for(auto& c:credits)author|=c.text.find("Rick Rashid")!=std::string::npos;
        if(!author || extent.y<1000 || extent.x<440)throw std::runtime_error("Original authored credits or image extent missing");
    }catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}
}
