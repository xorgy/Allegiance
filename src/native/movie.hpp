#pragma once
#include "assets.hpp"
#include <memory>
#include <string_view>
namespace Native {
class StartupMovies {
    unsigned remaining_{};
public:
    StartupMovies(bool credits=false,bool intro=false):remaining_(unsigned(credits)|(unsigned(intro)<<1)){}
    std::string_view next(){if(remaining_&1){remaining_&=~1u;return "intro_microsoft.avi";}if(remaining_&2){remaining_&=~2u;return "intro_movie.avi";}return {};}
};
// Bounded background decode. OpenAL consumes PCM on the main thread and its
// sample clock drives video selection; Vulkan receives only changed frames.
class Movie {
public:
    explicit Movie(const std::filesystem::path& file);
    ~Movie();
    glm::ivec2 size() const;
    std::shared_ptr<ImageData> advance();
    bool finished() const;
    void paused(bool value);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
