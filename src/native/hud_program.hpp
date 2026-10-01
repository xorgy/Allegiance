#pragma once
#include "original_ui.hpp"
#include <algorithm>
#include <array>
#include <initializer_list>

namespace Native {
// These are presentation values, sampled from the existing client state. The
// compiled program stores indices, never performs name lookup during a frame.
enum class HudNumber {
    Hull,Shield,Energy,Fuel,Ammo,Throttle,Speed,Experience,Signature,Flash,
    Artifact,Flag,Detected,VectorLock,Autopilot,Cloak,Cloaking,Pod,Endurance,Ripcord,RipcordLeft,ShowCenter,ShowTarget,
    TargetHull,TargetShield,TargetSpeed,TargetRange,TargetExperience,TargetType,TargetExists,InRange,
    StationHull,StationShield,Lives,LivesOn,Kills,KillsOn,CountdownOn,BoardState,Ghost,ChatLines,
    PartRange,PartDamage,PartRate,PartCount,PartReady,PartMounted,PartArmed,PartSelected,PartActive,PartEmpty,
    PartAfterburnSpeed,PartAfterburnTime,PartShieldStrength,PartRechargeTime,PartCloakTime,Observers,InventoryWidth,InventoryHeight,UiTime,Count
};
enum class HudString {Ship,Player,Target,TargetType,TargetSector,Station,StationSector,Sector,Part,PartKey,PartLocation,Gunner,QueuedOrder,AcceptedOrder,Critical0,Critical1,Critical2,Critical3,Tip0,Tip1,Tip2,Vote,Count};
enum class HudColor {Player,Target,Side,TargetSide,Station,Shadow,Count};
struct HudValues {
    std::array<float,size_t(HudNumber::Count)> numbers=[]{std::array<float,size_t(HudNumber::Count)> v{};v[size_t(HudNumber::ShowCenter)]=v[size_t(HudNumber::ShowTarget)]=1;return v;}();
    std::array<std::string_view,size_t(HudString::Count)> strings{};
    std::array<glm::vec4,size_t(HudColor::Count)> colors{};
    float& operator[](HudNumber n){return numbers[size_t(n)];}
    std::string_view& operator[](HudString s){return strings[size_t(s)];}
    glm::vec4& operator[](HudColor c){return colors[size_t(c)];}
};
struct HudFont {
    std::string symbol;int height{};bool bold{};
    bool operator==(const HudFont&)const=default;
};
struct HudDraw {
    enum class Kind {Image,Text,Preview,Inventory,Rectangle};
    Kind kind{};HudString textBinding{HudString::Count};glm::vec4 bounds{},uv{0,0,1,1},color{1},clip{};
    std::shared_ptr<ImageData> image;std::string text;size_t font{};int justify{};bool additive{},singleLine{},clipped{};
};
struct HudOverlay {
    std::string root;
    glm::vec2 off{},on{}; // Native top-down coordinates, including screen anchor.
    glm::vec2 anchor{}; // Screen-relative anchor weights in native coordinates.
    float duration{};
    uint32_t mask{};
    bool undetectable{};
};
struct HudOverlayMotion {
    bool initialized{},shown{};double started{};
    // A newly attached view starts settled. Subsequent mode/toggle changes
    // use WinTrek's linear endpoint-to-endpoint slide, including reversal.
    bool update(bool requested,double now,float duration) {
        if(!initialized){initialized=true;shown=requested;started=now-duration;}
        else if(shown!=requested){shown=requested;started=now;}
        return shown || (duration>0 && now-started<duration);
    }
    glm::vec2 point(const HudOverlay& spec,double now)const {
        float phase=spec.duration>0?float(std::clamp((now-started)/spec.duration,0.,1.)):1;
        return shown?glm::mix(spec.off,spec.on,phase):glm::mix(spec.on,spec.off,phase);
    }
};
// Flattens authored image expressions into indexed arithmetic and draw records
// once. Evaluation reuses all storage; no MDL traversal, asset lookup, legacy
// Image/Pane objects, or per-frame GPU resource creation.
class HudProgram {
public:
    HudProgram(OriginalUi& ui,std::string_view root,int style,std::string_view script="dialog",bool pane=false);
    HudProgram(Assets& assets,const UiScript& linked,std::string_view root,int style,bool pane=false);
    ~HudProgram();
    HudProgram(HudProgram&&)noexcept;
    HudProgram& operator=(HudProgram&&)noexcept;
    std::span<const HudDraw> evaluate(const HudValues& values);
    std::span<const HudFont> fonts()const;
    std::span<const std::shared_ptr<ImageData>> images()const;
    size_t instructionCount()const;
    glm::vec2 paneSize()const;
private:
    friend class HudOverlayTable;
    friend class HudScalar;
    struct Impl;std::unique_ptr<Impl> impl_;
};
// Display-only hosts share the same painter conversion. Scene previews and
// dynamic inventory hosts are handled by their dedicated native renderers.
std::optional<OriginalControl> authoredHudControl(const HudProgram& program,const HudDraw& draw,std::string_view name,glm::vec2 origin={});
// Reuses the indexed numeric compiler for authored UI animation expressions.
// Evaluation never traverses MDL or allocates temporary expression objects.
class HudScalar {
public:
    HudScalar(OriginalUi& ui,std::string_view script,std::string_view expression);
    ~HudScalar();
    float evaluate(const HudValues& values);
private:
    std::unique_ptr<HudProgram::Impl> impl_;
};
// The same numeric compiler resolves dialog.mdl's complete overlay table once.
// Consumers retain indices; frame evaluation has no string or AST lookups.
class HudOverlayTable {
public:
    HudOverlayTable(OriginalUi& ui,int style,std::span<const std::string_view> roots={});
    HudOverlayTable(Assets& assets,const UiScript& script,int style,std::span<const std::string_view> roots={});
    ~HudOverlayTable();
    size_t find(std::string_view root)const;
    std::span<const HudOverlay> evaluate(const HudValues& values,glm::vec2 extent);
private:
    std::unique_ptr<HudProgram::Impl> impl_;
};
// Layout adapters bind their original host image names once. The table and its
// results remain resident; consumers do not traverse MDL to place a panel.
class UiOverlayLayout {
public:
    UiOverlayLayout(OriginalUi& ui,std::initializer_list<std::string_view> roots,int style=0):table_(ui,style,{roots.begin(),roots.size()}) {
        auto entries=table_.evaluate({},{});
        for(auto root:roots){auto index=table_.find(root);indices_.push_back(index);durations_.push_back(index<entries.size()?entries[index].duration:0);}
    }
    const HudOverlay* get(size_t binding,const HudValues& values,glm::vec2 extent) {
        auto entries=table_.evaluate(values,extent);auto index=indices_.at(binding);return index<entries.size()?&entries[index]:nullptr;
    }
    float duration(size_t binding)const{return durations_.at(binding);}
    size_t index(size_t binding)const{return indices_.at(binding);}
private:
    HudOverlayTable table_;std::vector<size_t> indices_;std::vector<float> durations_;
};
}
