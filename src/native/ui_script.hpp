#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Native {
// The original MDL menu definitions are declarative input to the native UI.
// Parsing is a load-time operation; the Vulkan frame consumes compiled quads.
struct UiExpression {
    enum class Kind { Number,String,Reference,Call,List,Tuple,Operator };
    Kind kind{};
    std::string name;
    double number{};
    std::vector<std::shared_ptr<const UiExpression>> arguments;
};
struct UiScript {
    std::vector<std::string> imports;
    std::unordered_map<std::string,std::shared_ptr<const UiExpression>> definitions;
    static UiScript read(const std::filesystem::path& path);
    static UiScript parse(std::string_view source,std::string_view name);
};
}
