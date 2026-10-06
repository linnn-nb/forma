// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Plugins.h"
#include <set>
#include <cmath>

namespace ndaw {
static void valid(bool b,const char* message){if(!b)throw Error("invalid_plugin_reference",message);}
static bool hash(const Json& j){return j.is_string() && j.get<std::string>().size()==64 && j.get<std::string>().find_first_not_of("0123456789abcdef")==std::string::npos;}
std::string sessionPluginAudioToken(const Json& fx) {
    Json values=Json::array();for(const auto& p:fx.at("parameters"))if(!p.at("sdk_id").is_null())values.push_back({p.at("sdk_id"),p.at("value")});
    return digest(Json{{"instance",fx.at("id")},{"plugin",fx.at("catalog_plugin_id")},{"sdk",fx.at("description").at("description_xml")},
        {"fingerprint",fx.at("fingerprint").at("digest")},{"state",fx.at("state")},{"values",values},
        {"pipeline",fx.at("pipeline_latency_frames")},{"latency",fx.at("description").at("reported_latency_frames")},
        {"layout",fx.at("processing_layout")},{"precision",fx.at("processing_precision")}}.dump());
}
std::string sessionPluginRuntimeToken(const Json& fx) {
    auto identity=fx;identity["parameters"]=Json::array();return sessionPluginAudioToken(identity);
}
void validateSessionPlugin(const Json& fx) {
    valid(fx.at("kind")=="plugin" && fx.at("locked").is_boolean(),"Invalid plugin kind/lock");
    valid(hash(fx.at("catalog_plugin_id")) && fx.at("description").at("id")==fx.at("catalog_plugin_id"),"Invalid captured plugin identity");
    const auto& d=fx.at("description");valid(d.at("format")=="VST3" || d.at("format")=="AudioUnit","Unsupported actual plugin format");
    valid(d.at("description_xml").is_string() && d.at("version").is_string() && d.at("file_or_identifier").is_string(),"Missing actual SDK description/version/location");
    valid(d.at("scan_verified")==true && d.at("instrument")==false,"This increment admits scanned audio effects only");
    valid(d.at("reported_latency_frames").is_number_integer() && d.at("reported_latency_frames").get<Frame>()>=0 && d.at("reported_latency_frames").get<Frame>()<=1000000,"Invalid plugin latency admission");
    valid(fx.at("pipeline_latency_frames")==1024 && fx.at("processing_layout")=="stereo" && fx.at("processing_precision")=="float32","Unsupported admitted plugin processing path");
    valid(hash(fx.at("fingerprint").at("digest")),"Missing captured module fingerprint");
    const auto& state=fx.at("state");const auto path=fs::path(state.at("path").get<std::string>());
    valid(hash(state.at("sha256")) && state.at("bytes").is_number_unsigned() && state.at("bytes").get<std::size_t>()<=16*1024*1024,"Invalid opaque state identity/size");
    valid(path==fs::path(".plugin-states")/(state.at("sha256").get<std::string>()+".bin"),"Plugin state must be session-local and content addressed");
    const auto& params=fx.at("parameters");valid(params.is_array() && params.size()<=8192,"Invalid actual parameter collection");std::set<std::string> sdk;
    for(const auto& p:params) {
        const auto id=p.at("sdk_id");valid(id.is_null() || (id.is_string() && !id.get<std::string>().empty() && sdk.insert(id.get<std::string>()).second),"Invalid/duplicate actual SDK parameter ID");
        const auto expected=digest(fx.at("id").get<std::string>()+":parameter:"+(id.is_null()?"unaddressable-index:"+std::to_string(p.at("index").get<int>()):id.get<std::string>()));
        valid(p.at("id")==expected,"Stable parameter object ID is not bound to its instance/SDK ID");
        valid(p.at("value").is_number() && std::isfinite(p.at("value").get<double>()) && p.at("value").get<double>()>=0 && p.at("value").get<double>()<=1,"Invalid SDK normalized parameter value");
        valid(p.at("name").is_string() && p.at("unit_label").is_string() && p.at("normalized_range")==Json{{"min",0.0},{"max",1.0}} && p.at("model_editable")==!id.is_null(),"Missing actual parameter metadata");
    }
    const std::set<std::string> fields{"id","kind","locked","catalog_plugin_id","description","fingerprint","state","parameters","pipeline_latency_frames","processing_layout","processing_precision"};
    for(auto it=fx.begin();it!=fx.end();++it)valid(fields.contains(it.key()),"Unknown plugin instance field");
}
SessionPluginAsset sessionPluginFromCatalog(const fs::path& catalog,const std::string& pluginId,const std::string& instanceId,const std::string& stateId) {
    const auto inventory=readPluginCatalog(catalog);
    for(const auto& [key,e]:inventory.at("entries").items())if(e.at("status")=="verified" && !e.value("blacklisted",false))for(const auto& p:e.at("plugins"))if(p.at("id")==pluginId) {
        if(pluginModuleFingerprint(e.at("format"),e.at("candidate"))!=e.at("fingerprint"))throw Error("plugin_stale","Module changed; rescan before project insertion");
        if(p.at("instrument")==true)throw Error("plugin_instrument","MIDI instrument project hosting is unfinished; no silent instrument-to-effect mapping");
        auto state=p.at("opaque_state"),parameterSnapshot=p.at("parameters");
        if(!stateId.empty()) {
            if(!inventory.contains("states") || !inventory.at("states").contains(stateId) || inventory.at("states").at(stateId).at("plugin_id")!=pluginId)
                throw Error("plugin_state","State was not captured for this actual plugin");
            state=inventory.at("states").at(stateId);
            if(!state.contains("parameters"))throw Error("plugin_state","This older captured state lacks a verified parameter snapshot; reprocess/rescan before inserting it");
            parameterSnapshot=state.at("parameters");
        }
        const auto file=fs::path(state.at("path").get<std::string>());
        if(!fs::is_regular_file(file) || fs::file_size(file)>16*1024*1024 || fs::file_size(file)!=state.at("bytes").get<std::size_t>() || sha256(file)!=state.at("sha256").get<std::string>())
            throw Error("plugin_state","Captured opaque state is missing/changed; original reference preserved");
        auto description=p;description.erase("parameters");description.erase("opaque_state");
        Json parameters=Json::array();for(auto param:parameterSnapshot) {
            const auto sdk=param.at("id");param["sdk_id"]=sdk;param["id"]=digest(instanceId+":parameter:"+(sdk.is_null()?"unaddressable-index:"+std::to_string(param.at("index").get<int>()):sdk.get<std::string>()));
            param["enumerated_value"]=param.at("value");param["display_scope"]="captured SDK display at enumerated_value; current desired value requires actual processing receipt";parameters.push_back(std::move(param));
        }
        SessionPluginAsset asset;asset.stateFile=file;
        asset.instance={{"id",instanceId},{"kind","plugin"},{"locked",false},{"catalog_plugin_id",pluginId},{"description",description},{"fingerprint",e.at("fingerprint")},
            {"state",{{"path",(fs::path(".plugin-states")/(state.at("sha256").get<std::string>()+".bin")).generic_string()},{"sha256",state.at("sha256")},{"bytes",static_cast<std::uint64_t>(state.at("bytes").get<std::size_t>())}}},
            {"parameters",parameters},{"pipeline_latency_frames",1024},{"processing_layout","stereo"},{"processing_precision","float32"}};
        validateSessionPlugin(asset.instance);return asset;
    }
    throw Error("plugin_unavailable","Actual verified plugin ID was not found in the configured inventory");
}
}
