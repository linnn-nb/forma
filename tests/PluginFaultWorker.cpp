// SPDX-License-Identifier: AGPL-3.0-only
// TEST ONLY. No plugin SDK, audio substitute or model response. Never package.
#include <json.hpp>
#include <filesystem>
#include <fstream>
#include <thread>
#include <csignal>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
using Json=nlohmann::json;
static Json read(const std::filesystem::path& p){std::ifstream f(p);Json j;f>>j;return j;}
int main(int argc,char** argv){
    if(argc!=3 || std::string(argv[1])!="--request")return 2;
    const auto request=read(argv[2]);const auto output=request.at("response_path").get<std::string>();
    const auto mode=request.contains("test_mode")?request.at("test_mode").get<std::string>():
        request.contains("candidate")?read(request.at("candidate").get<std::string>()).at("mode").get<std::string>():"normal";
    auto crash=[]{
#ifdef _WIN32
        TerminateProcess(GetCurrentProcess(),0xc0000005);
#else
        ::kill(::getpid(),SIGKILL);
#endif
    };
    if(mode=="crash")crash();
    if(mode=="hang"){for(;;)std::this_thread::sleep_for(std::chrono::seconds(1));}
    if(mode=="memory"){std::vector<char> storage(64*1024*1024);volatile char* pages=storage.data();
        for(std::size_t i=0;i<storage.size();i+=4096)pages[i]=1;
        for(;;)std::this_thread::sleep_for(std::chrono::seconds(1));}
    if(mode=="oversized"){std::ofstream f(output);f<<std::string(request.at("limits").at("response_bytes").get<std::size_t>()+1,'x');f.close();
        for(;;)std::this_thread::sleep_for(std::chrono::seconds(1));}
    Json response{{"protocol",request.at("protocol")},{"job_id",request.at("job_id")},{"action",request.at("action")},{"status","succeeded"},
        {"test_fixture",true},{"plugins",Json::array({{{"id","supervisor-fixture-only"},{"parameters",Json::array()},{"production_plugin",false}}})},
        {"candidates",Json::array()}};
    if(mode=="stale")response["job_id"]="stale-job";
    if(mode=="protocol")response["protocol"]=999;
    if(mode=="failed"){response["status"]="failed";response["code"]="fixture_failure";response["message"]="Intentional worker failure";}
    {std::ofstream f(output);f<<(mode=="malformed"?"{malformed":response.dump());f.flush();if(!f)return 4;}
    if(mode=="crash_after_response")crash();
    return mode=="nonzero_after_response"?7:0;
}
