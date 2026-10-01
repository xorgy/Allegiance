#pragma once
#include "assets.hpp"
#include "ui_script.hpp"
#include <glm/glm.hpp>
#include <optional>
#include "../ClientCore/SoundIds.h"

namespace Native {
struct OriginalHoverCondition {
    std::string site;
    std::optional<int> value; // No value means the authored SwitchString default.
    std::shared_ptr<const std::vector<int>> cases;
    int initialValue{};
};
struct OriginalControl {
    enum class Kind { Image,Button,Text,Slot,Rectangle,Hover };
    enum class TextMode {Native,Image,Pane,Edit};
    enum class Widget {None,Edit,StringCombo,ColorCombo,ImageCombo,List,ScrollBar,Page,Chat};
    Kind kind{};
    std::string name,text,font;
    glm::vec4 bounds{},color{1};
    std::shared_ptr<ImageData> image;
    uint32_t faces{};
    int justify{};
    std::vector<std::shared_ptr<ImageData>> animation;
    float duration{};
    glm::vec4 uv{0,0,1,1};
    std::optional<glm::vec4> clip;
    std::string help;
    std::string helpSite,textSite,hoverTarget;
    bool singleLine{},additive{};
    TextMode textMode{};
    int fontHeight{};bool fontBold{};
    bool clipCharacters{true}; // Raw original DrawString painters clip glyphs at their surface rectangle.
    Widget widget{};glm::vec2 faceSize{};
    uint8_t checkedState{}; // Original Button3State: 0 normal, 1 checked, 2 checked2.
    bool editFocused{},editReadOnly{},editPassword{};double editLastTyped{};
    std::string scrollBar;int chatFont{1};bool showScrollBar{};
    int helpID{};
    std::vector<OriginalHoverCondition> hoverConditions;
    bool hoverBlocked{};
    bool pressAction{}; // FrameImageButtonPane fires on down without capture.
    SoundID clickSound{-1},hoverSound{-1},selectSound{-1};
    // Surface::DrawString paints one unwrapped line and clips individual glyphs
    // at the surface rectangle. StringPane/StringImage retain their own rules.
    void surfaceText(){textMode=TextMode::Native;singleLine=true;clipCharacters=false;}
    // PaneButtonFacePane uses separate full-size up/down images, rather than
    // a bitmap strip. It has no hover or disabled face.
    std::shared_ptr<ImageData> downImage;
    bool toggleOnRelease{}; // ButtonPane's authored toggle flag, independent of its bitmap faces.
    void translate(glm::vec2 offset){bounds.x+=offset.x;bounds.y+=offset.y;if(clip){clip->x+=offset.x;clip->y+=offset.y;}}
    void intersectClip(glm::vec4 rectangle){
        if(clip){auto lo=glm::max(glm::vec2(*clip),glm::vec2(rectangle)),hi=glm::min(glm::vec2(*clip)+glm::vec2(clip->z,clip->w),glm::vec2(rectangle)+glm::vec2(rectangle.z,rectangle.w));clip=glm::vec4(lo,glm::max(hi-lo,glm::vec2(0)));}
        else clip=rectangle;
    }
    glm::vec4 visibleBounds()const{
        if(!clip)return bounds;auto lo=glm::max(glm::vec2(bounds),glm::vec2(*clip)),hi=glm::min(glm::vec2(bounds)+glm::vec2(bounds.z,bounds.w),glm::vec2(*clip)+glm::vec2(clip->z,clip->w));return {lo,glm::max(hi-lo,glm::vec2(0))};
    }
};
inline OriginalControl widgetText(const OriginalControl& face,std::string text) {
    auto control=face;control.kind=OriginalControl::Kind::Text;control.name.clear();control.image.reset();control.animation.clear();
    // ImageComboFacePane::SetString is intentionally a no-op: its bitmap
    // already contains the button label. Text/color faces display selection.
    control.text=face.widget==OriginalControl::Widget::ImageCombo?std::string{}:std::move(text);control.textMode=OriginalControl::TextMode::Pane;control.singleLine=true;
    if(face.widget==OriginalControl::Widget::StringCombo || face.widget==OriginalControl::Widget::ColorCombo){control.bounds.z=face.faceSize.x;control.bounds.w=face.faceSize.y;}
    control.intersectClip(control.bounds);return control;
}
using UiNumbers=std::unordered_map<std::string,double>;
using UiStrings=std::unordered_map<std::string,std::string>;
using UiSlots=std::unordered_map<std::string,glm::vec2>;
struct OriginalLoadoutSlot {
    int type{},mount{};std::string prefix;glm::vec2 position{};
    std::shared_ptr<ImageData> empty;
};
struct OriginalChoice {std::string label;std::vector<double> values;};
struct OriginalImageItem {std::vector<OriginalControl> artwork;double value{};};
// Original menu descriptions are compiled at load/rebuild time into native
// controls. They do not instantiate the legacy Pane/Image rendering engine.
class OriginalUi {
public:
    explicit OriginalUi(Assets& assets):assets_(assets){}
    Assets& assets(){return assets_;}
    // Read-only, linked authored definitions for other load-time native compilers.
    const UiScript& script(const std::string& name){return linkedScript(name);}
    std::vector<OriginalControl> layout(const std::string& script,const std::string& root="screen",const UiNumbers& numbers={},const UiStrings& strings={},bool colorKeyHint=false,const UiSlots& slots={});
    std::vector<OriginalLoadoutSlot> loadoutSlots(int hull);
    std::vector<OriginalControl> loadoutOverlay(int hull);
    std::shared_ptr<ImageData> loadoutImage(const OriginalLoadoutSlot& slot,const std::string& model);
    std::vector<OriginalControl> imageLayout(const std::string& script,const std::string& root="image",const UiNumbers& numbers={},const UiStrings& strings={},const UiSlots& slots={});
    std::vector<OriginalImageItem> imageList(const std::string& script,const std::string& root,const UiNumbers& numbers={});
    glm::vec2 imageSize(const std::string& script,const std::string& root="image");
    glm::vec2 paneSize(const std::string& script,const std::string& root);
    std::string string(const std::string& script,const std::string& name);
    double number(const std::string& script,const std::string& name,const UiNumbers& numbers={});
    glm::vec2 point(const std::string& script,const std::string& name);
    OriginalControl font(const std::string& script,const std::string& name,int argument=-1);
    glm::vec4 color(const std::string& script,const std::string& name);
    std::vector<int> integers(const std::string& script,const std::string& name,const UiNumbers& numbers={});
    std::vector<OriginalChoice> choices(const std::string& script,const std::string& names,std::initializer_list<const char*> tables);
private:
    const UiScript& linkedScript(const std::string& name);
    Assets& assets_;
    std::unordered_map<std::string,UiScript> scripts_;
};
}
