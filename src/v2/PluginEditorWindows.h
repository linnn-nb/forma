// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <json.hpp>
namespace ndaw::v2 {
// L1 owns the native windows. The desktop receives immutable status only.
class PluginEditorWindows final {
public:
 using Json=nlohmann::json;
 PluginEditorWindows();
 ~PluginEditorWindows();
 Json open(tracktion::engine::ExternalPlugin&,std::function<void()> close);
 void close(const std::string&);
 bool showing(const std::string&)const;
 Json query()const;
 void prune(const std::function<tracktion::engine::Plugin*(const std::string&)>&);
private:
 struct Window;
 std::map<std::string,std::unique_ptr<Window>> windows;
};
}
