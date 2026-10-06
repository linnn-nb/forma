#pragma once
#include <json.hpp>
#include <set>
#include <string>
#include <limits>
namespace ndaw::v2 {
enum class Permission { ReadOnly, Preview, ScopedLowRisk };
// Supplied by the trusted local UI/SDK, never accepted from a tool request.
struct Scope {
    Permission mode=Permission::Preview;
    std::set<std::string> targets,commands;
    int64_t begin=0,end=std::numeric_limits<int64_t>::max();
    void validate() const;
    nlohmann::json json() const;
    static std::set<std::string> defaultCommands();
};
}
