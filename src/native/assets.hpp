#pragma once
#include <glm/glm.hpp>
#include <filesystem>
#include <memory>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <array>
#include <span>

namespace Native {
class SystemFonts;
enum class ImageFormat { RgbaSrgb, Coverage };
struct ImageData {
    uint32_t width{}, height{};
    std::vector<uint8_t> pixels;
    std::string name;
    bool mipmaps{true};
    ImageFormat format{ImageFormat::RgbaSrgb};
    size_t byteSize() const { return size_t(width)*height*(format==ImageFormat::Coverage?1:4); }
};
struct BitmapFont {
    std::shared_ptr<ImageData> coverage;
    std::array<glm::vec4,256> glyphs{}; // atlas pixel x/y and original advance/height
    uint32_t height{};
};
struct Vertex { glm::vec3 position, normal; glm::vec2 uv; };
static_assert(sizeof(Vertex) == 32);
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::shared_ptr<ImageData> image;
    float lodMin{-std::numeric_limits<float>::infinity()},lodMax{std::numeric_limits<float>::infinity()};
};
struct ModelFrame {std::string name;glm::vec3 position,forward,up;};
struct ModelLight {glm::vec4 color{};glm::vec3 position{};float period{},phase{},rampUp{},hold{},rampDown{};};
static_assert(sizeof(ModelLight)==48);
struct ModelData {
    std::vector<std::shared_ptr<MeshData>> meshes;std::vector<ModelFrame> frames;std::vector<ModelLight> lights;
    // Geo::FindTexture supplies ThingGeo's initial texture state. Bare meshes
    // (including authored asteroid LODs) inherit it unless a skin is supplied.
    std::shared_ptr<ImageData> defaultImage;
    float radius{1};
};

// Names in MDL and IGC are case insensitive, even on a case sensitive host.
class Assets {
public:
    explicit Assets(std::filesystem::path root);
    ~Assets();
    const std::filesystem::path& root() const { return root_; }
    std::filesystem::path find(const std::string& name) const;
    bool hasImage(const std::string& name)const;
    // CfgGetCoreName: optional [Cores] names in the original allegiance.txt.
    std::string coreName(std::string_view file)const;
    std::vector<std::string> coreCatalog()const;
    bool officialCore(std::string_view file)const;
    bool officialServer(std::string_view name,std::string_view address)const;
    std::shared_ptr<ModelData> model(const std::string& name);
    // Like Modeler::LoadSurface(name, true), additionally make exact black
    // transparent. Keep this variant separate from model/unkeyed consumers.
    std::shared_ptr<ImageData> image(const std::string& name,bool colorKey=false);
    std::shared_ptr<const BitmapFont> font(const std::string& symbol,int height=0,bool bold=false);
    SystemFonts& systemFonts();
    std::span<const std::shared_ptr<ImageData>> animation(const std::string& name);
    const std::unordered_map<std::string,size_t>& functions()const{return functions_;}
    static std::filesystem::path steamArtwork();
private:
    std::unique_ptr<SystemFonts> systemFonts_;
    struct Reader;
    struct Value;
    using ValuePtr = std::shared_ptr<Value>;
    using NameSpace = std::unordered_map<std::string,ValuePtr>;
    const NameSpace& load(const std::string& name);
    std::unordered_map<std::string,size_t> functions_;
    std::filesystem::path root_;
    std::unordered_map<std::string,std::filesystem::path> files_;
    std::unordered_map<std::string,std::string> coreNames_;
    std::unordered_map<std::string,bool> officialCores_;
    std::unordered_map<std::string,std::string> officialServers_;
    std::unordered_map<std::string,NameSpace> namespaces_;
    std::unordered_map<std::string,std::shared_ptr<ModelData>> models_;
    std::unordered_map<std::string,std::shared_ptr<ImageData>> images_;
    std::unordered_map<std::string,std::shared_ptr<ImageData>> keyedImages_;
};
std::shared_ptr<ImageData> decodeImage(const std::filesystem::path& path);
}
