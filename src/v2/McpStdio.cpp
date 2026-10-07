// MCP stdio transport for the running desktop application. This process owns
// no Engine/Edit and cannot grant permissions or acknowledge GUI cards.
#include "McpSocket.h"
#include <iostream>
#if !defined(_WIN32)
#include <json.hpp>
#include <chrono>
#include <csignal>
using Json=nlohmann::json;
namespace {
using Clock=std::chrono::steady_clock;
constexpr size_t messageLimit=256*1024,outputLimit=8*1024*1024,inputQueueLimit=1024*1024;
struct PipeMode {
    int fd,original;
    explicit PipeMode(int f):fd(f),original(fcntl(fd,F_GETFL)){
        ndaw::v2::ipc::require(original>=0&&fcntl(fd,F_SETFL,original|O_NONBLOCK)==0,"stdio nonblocking mode");
    }
    ~PipeMode(){fcntl(fd,F_SETFL,original);}
};
bool transient(){return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR;}
void bridge(const std::string& path){
    using namespace ndaw::v2::ipc;
    ownSocket(path);
    Fd socket(::socket(AF_UNIX,SOCK_STREAM,0));require(socket.value>=0,"socket create");nonblocking(socket.value);
    auto endpoint=address(path);
    if(::connect(socket.value,reinterpret_cast<sockaddr*>(&endpoint),sizeof(endpoint))!=0){
        require(errno==EINPROGRESS||errno==EAGAIN,"connect to running Forma Studio");
        pollfd p{socket.value,POLLOUT,0};require(poll(&p,1,3000)>0,"MCP connection timeout");
        int error=0;socklen_t length=sizeof(error);require(getsockopt(socket.value,SOL_SOCKET,SO_ERROR,&error,&length)==0&&error==0,"MCP connection failed");
    }
    require(ownPeer(socket.value),"MCP peer must be the current user");
    std::signal(SIGPIPE,SIG_IGN);
    PipeMode input(STDIN_FILENO),output(STDOUT_FILENO);
    std::string inputFrame,toSocket,socketFrame,toOutput;
    auto inputStarted=Clock::now(),outputStarted=inputStarted;
    for(;;){
        pollfd fds[]={{STDIN_FILENO,POLLIN,0},{socket.value,short(POLLIN|(toSocket.empty()?0:POLLOUT)),0},{STDOUT_FILENO,short(toOutput.empty()?0:POLLOUT),0}};
        int n=poll(fds,3,20);if(n<0){if(errno==EINTR)continue;throw std::runtime_error("MCP stdio poll failed");}
        auto now=Clock::now();char data[8192];
        if(fds[0].revents&(POLLIN|POLLHUP)){
            auto count=::read(STDIN_FILENO,data,sizeof(data));
            if(count==0){if(!inputFrame.empty())throw std::runtime_error("stdin ended within an MCP message");return;}
            if(count<0){if(!transient())throw std::runtime_error("stdin read failed");}
            else {
                if(inputFrame.empty())inputStarted=now;inputFrame.append(data,size_t(count));size_t end;
                while((end=inputFrame.find('\n'))!=std::string::npos){
                    if(end>messageLimit||toSocket.size()+end+1>inputQueueLimit)throw std::runtime_error("MCP input budget exceeded");
                    toSocket+=inputFrame.substr(0,end+1);inputFrame.erase(0,end+1);inputStarted=now;
                }
                if(inputFrame.size()>messageLimit)throw std::runtime_error("MCP message exceeds 256 KiB");
            }
        }
        if(fds[1].revents&(POLLIN|POLLHUP)){
            auto count=::recv(socket.value,data,sizeof(data),0);
            if(count==0)throw std::runtime_error("Forma Studio MCP connection closed; reconnect and query before replanning");
            if(count<0){if(!transient())throw std::runtime_error("MCP socket read failed");}
            else {
                socketFrame.append(data,size_t(count));size_t end;
                while((end=socketFrame.find('\n'))!=std::string::npos){
                    if(end>outputLimit||toOutput.size()+end+1>outputLimit)throw std::runtime_error("MCP output budget exceeded");
                    auto line=socketFrame.substr(0,end);
                    auto envelope=Json::parse(line);
                    if(!envelope.is_object()||envelope.value("jsonrpc",Json(nullptr))!="2.0")throw std::runtime_error("invalid MCP server response");
                    if(toOutput.empty())outputStarted=now;toOutput+=line+"\n";socketFrame.erase(0,end+1);
                }
                if(socketFrame.size()>outputLimit)throw std::runtime_error("MCP response exceeds 8 MiB");
            }
        }
        if(!toSocket.empty()&&(fds[1].revents&POLLOUT)){
            auto count=send(socket.value,toSocket.data(),toSocket.size());
            if(count>0)toSocket.erase(0,size_t(count));else if(count<0&&!transient())throw std::runtime_error("MCP socket write failed");
        }
        if(!toOutput.empty()&&(fds[2].revents&POLLOUT)){
            auto count=::write(STDOUT_FILENO,toOutput.data(),toOutput.size());
            if(count>0)toOutput.erase(0,size_t(count));else if(count<0&&!transient())throw std::runtime_error("stdout write failed");
        }
        if(fds[0].revents&(POLLERR|POLLNVAL)||fds[1].revents&(POLLERR|POLLNVAL)||fds[2].revents&(POLLERR|POLLNVAL))throw std::runtime_error("MCP transport unavailable");
        if(!inputFrame.empty()&&now-inputStarted>std::chrono::seconds(5))throw std::runtime_error("MCP partial input timeout");
        if(!toOutput.empty()&&now-outputStarted>std::chrono::seconds(5))throw std::runtime_error("MCP stdout backpressure timeout");
    }
}
}
#endif
int main(int argc,char** argv){
    try {
#if !defined(_WIN32)
        std::string path=ndaw::v2::ipc::defaultPath();
        if(argc==2&&std::string(argv[1])=="--help"){
            std::cerr<<"Forma Studio MCP stdio bridge\nUsage: forma-mcp [--socket PATH]\nOpen the desktop application and select Command > MCP read-only or preview.\nDefault endpoint: "<<path<<"\n";return 0;
        }
        if(argc==3&&std::string(argv[1])=="--socket")path=argv[2];
        else if(argc!=1)throw std::runtime_error("usage: forma-mcp [--socket PATH]");
        bridge(path);return 0;
#else
        throw std::runtime_error("Windows MCP local transport is not implemented");
#endif
    } catch(const std::exception& e){std::cerr<<"forma-mcp: "<<e.what()<<std::endl;return 1;}
}
