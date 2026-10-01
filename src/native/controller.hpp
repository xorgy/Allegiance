#pragma once
#include "settings.hpp"
#include "feedback.hpp"
#include "haptics.hpp"
#include <bitset>
#include <vector>
namespace Native {
struct ControllerDevice {std::string guid,name;bool gamepad{};int axes{},buttons{},hats{};};
struct ControllerState {
    std::array<float,3> axes{};
    float throttle{},headYaw{};bool useThrottle{},throttleChanged{},hatActive{};
    std::bitset<size_t(Binding::Count)> down,pressed;
};
class Controller {
public:
    explicit Controller(const ControllerSettings& settings,const std::filesystem::path& artwork={});
    ~Controller();
    Controller(const Controller&)=delete;Controller& operator=(const Controller&)=delete;
    void configure(const ControllerSettings& settings);
    bool event(const SDL_Event& event);
    SDL_Keycode menuKey(const SDL_Event& event)const;
    const ControllerState& sample(bool active);
    void reset();
    void feedback(const Feedback& events,bool active);
    void manualThrottle(){joyThrottle_=false;}
    bool resumeThrottle();
    const std::vector<ControllerDevice>& devices()const{return devices_;}
    const std::string& name()const{return name_;}
    bool gamepad()const{return pad_!=nullptr;}
    int axes()const{return axisCount_;}int buttons()const{return buttonCount_;}int hats()const{return hatCount_;}
    static int defaultButton(bool gamepad,int button);
private:
    void refresh();void close();void openFeedback();void stopFeedback();float axis(int index)const;
    void compileMap();void sampleMap(float& throttle,bool& validThrottle);
    struct AxisInput {SDL_Joystick* device{};int index{};bool hat{},invert{};float value()const;};
    struct ButtonInput {SDL_Joystick* device{};int index{};std::bitset<size_t(Binding::Count)> actions;};
    std::array<SDL_Joystick*,16> mappedDevices_{};
    std::array<AxisInput,6> mappedAxes_{};
    std::vector<ButtonInput> mappedButtons_;
    std::array<std::bitset<size_t(Binding::Count)>,4> mappedHat_{};
    std::vector<std::pair<SDL_JoystickID,std::string>> deviceKeys_;
    ControllerSettings settings_;
    SDL_Haptic* haptic_{};
    struct Force {HapticPreset preset;SDL_HapticEffectID id{-1};};
    std::array<std::vector<HapticPreset>,3> presets_;
    std::array<std::vector<Force>,3> forces_;
    uint64_t rumbleUntil_{};int rumblePriority_{-1};bool feedbackPlaying_{};
    std::vector<ControllerDevice> devices_;std::string name_;
    SDL_Gamepad* pad_{};SDL_Joystick* joystick_{};SDL_JoystickID id_{};
    int axisCount_{},buttonCount_{},hatCount_{};float inverseDeadZone_{.9f};
    ControllerState state_;
    std::bitset<size_t(Binding::Count)> previous_,suppressed_;
    bool initialized_{},joyThrottle_{};float oldThrottle_{};
};
}
