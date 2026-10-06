#include <json.hpp>
#include <fstream>
#include <thread>
#include <csignal>
// Test-only hostile scanner, never installed in the application.
int main(int argc,char** argv){if(argc!=3)return 2;nlohmann::json q;std::ifstream(argv[2])>>q;
 if(q.at("candidate").get<std::string>().find("crash")!=std::string::npos){
  std::ofstream out(q.at("response_path").get<std::string>());out<<nlohmann::json{{"protocol",q.at("protocol")},{"job_id",q.at("job_id")},{"action",q.at("action")},{"status","succeeded"},{"plugins",nlohmann::json::array()}}.dump();out.close();
#ifdef _WIN32
 std::raise(SIGABRT);
#else
 std::raise(SIGKILL);
#endif
 }else for(;;)std::this_thread::sleep_for(std::chrono::milliseconds(20));return 0;
}
