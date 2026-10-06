// SPDX-License-Identifier: AGPL-3.0-only
// Test-only protocol/fault control. Never a production processor or package.
#include "nativedaw/PluginIPC.h"
#include <thread>
#include <cstdlib>
using namespace ndaw;
int main(int argc,char** argv) {
    if(argc!=3 || std::string(argv[1])!="--request")return 2;
    try {const auto request=readJson(argv[2]);const auto job=fs::path(argv[2]).parent_path();PluginMapping map(request.at("ipc_path"),false);auto& shared=map.data();
        const auto mode=request.at("plugin").at("name").get<std::string>();Json parameters=Json::array();
        for(const auto& p:request.at("parameters")){const auto i=parameters.size();parameters.push_back({{"id",p.at("id")},{"index",i},{"value",p.at("normalized")}});shared.requestedValues[i].store(p.at("normalized").get<float>());shared.actualValues[i].store(p.at("normalized").get<float>());}
        if(mode=="_test_prepare_hold"){atomicWrite(job/"held.json",Json{{"pid",pluginOwnerPid()},{"owner_pid",request.at("owner_pid")},{"test_worker",true}}.dump(),false);std::this_thread::sleep_for(std::chrono::seconds(60));}
        atomicWrite(job/"ready.json",Json{{"protocol",pluginProtocol},{"job_id",request.at("job_id")},{"status","prepared"},{"plugin_id",request.at("plugin").at("id")},
            {"worker_pid",pluginOwnerPid()},
            {"control_stream_protocol",pluginStreamProtocol},{"initial_control_token",pluginControlText(request.at("control_token").get<PluginControlToken>())},
            {"reported_latency_frames",request.at("expected_latency_frames")},{"parameters",parameters},{"test_worker",true}}.dump(),false);shared.status.store(1,std::memory_order_release);
        std::array<std::array<float,32>,2> delay{};std::size_t cursor=0;std::uint64_t epoch=UINT64_MAX;
        while(!shared.stop.load(std::memory_order_acquire)) {
            if(mode!="_test_heartbeat")shared.heartbeat.fetch_add(1);
            PluginSlot* slot=nullptr;for(auto& candidate:shared.slots)if(candidate.state.load(std::memory_order_acquire)==1 && (!slot || candidate.sequence<slot->sequence))slot=&candidate;
            if(!slot || mode=="_test_heartbeat"){std::this_thread::sleep_for(std::chrono::microseconds(50));continue;}
            std::uint32_t ready=1;if(!slot->state.compare_exchange_strong(ready,2,std::memory_order_acq_rel))continue;
            if(slot->epoch!=shared.epoch.load()){slot->state.store(3,std::memory_order_release);continue;}
            if(mode=="_test_crash")std::abort();if(mode=="_test_late")std::this_thread::sleep_for(std::chrono::seconds(2));
            if(epoch!=slot->epoch){delay={};cursor=0;epoch=slot->epoch;}
            for(std::uint32_t i=0;i<slot->controlCount && i<pluginParameterLimit;++i){const auto v=slot->controls[i];if(v.index<parameters.size()){parameters[v.index]["value"]=v.normalized;shared.requestedValues[v.index].store(v.normalized);shared.actualValues[v.index].store(v.normalized);}}
            slot->appliedControl=slot->requestedControl;
            if(mode=="_test_control_mismatch")slot->appliedControl.back()^=1;
            for(int i=0;i<pluginQuantum;++i){for(int ch=0;ch<2;++ch){auto v=slot->input[ch][i];if(mode=="_test_latency32"){const auto before=delay[ch][cursor];delay[ch][cursor]=v;v=before;}slot->output[ch][i]=v;}cursor=(cursor+1)%32;}
            if(mode=="_test_nan")slot->output[0][0]=std::numeric_limits<float>::quiet_NaN();
            if(mode=="_test_stale")++slot->sequence;
            shared.processed.fetch_add(1);slot->state.store(3,std::memory_order_release);
        }
        atomicWrite(job/"response.json",Json{{"protocol",pluginProtocol},{"job_id",request.at("job_id")},{"action","stream"},{"status","succeeded"},{"stream",{{"parameters",parameters}}},{"test_worker",true}}.dump(),false);
        if(mode=="_test_after_response_crash")std::abort();return 0;
    }catch(...){return 3;}
}
