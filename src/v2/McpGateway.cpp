#include <nativedaw/v2/McpGateway.h>
#include <nativedaw/v2/McpSession.h>
#include "McpSocket.h"
#include <chrono>
#include <set>
#include <thread>
#if defined(__APPLE__)
#include <pthread.h>
#endif
namespace ndaw::v2 {
#if !defined(_WIN32)
namespace {using Clock=std::chrono::steady_clock;constexpr size_t maxOutput=8*1024*1024;}
struct McpGatewayState:std::enable_shared_from_this<McpGatewayState> {
    struct Lease {
        std::atomic<bool> abandoned{false};
        std::promise<CommandQueue::Client> promise;
        std::shared_future<CommandQueue::Client> future=promise.get_future().share();
        std::string clientID; // message thread only
    };
    struct Connection {
        ipc::Fd fd;
        std::string input,output;
        std::shared_ptr<Lease> lease=std::make_shared<Lease>();
        std::unique_ptr<McpSession> session;
        Clock::time_point began=Clock::now(),partial=began,outputStarted=began;
    };
    CommandQueue* queue;
    const Scope scope;
    const Json registry;
    std::string path;
    ipc::Fd listener;
    ino_t inode=0;dev_t device=0;
    std::atomic<bool> stopping{false},running{true},failed{false};
    std::atomic<size_t> count{0},pendingGrants{0};
    std::atomic<uint64_t> accepted{0},dropped{0};
    std::thread worker;
    std::set<std::string> localClients; // message thread only, bounded by live leases
    explicit McpGatewayState(CommandQueue& q,Scope s):queue(&q),scope(std::move(s)),registry(Commands::registry()){}
    void revokeLease(const std::shared_ptr<Lease>& lease) {
        if(lease->clientID.empty())return;
        try{queue->revoke(lease->clientID);}catch(const std::exception&){}
        localClients.erase(lease->clientID);lease->clientID.clear();
    }
    void retire(Connection& c) {
        c.lease->abandoned=true;
        if(c.session)c.session->close();
        if(c.lease->future.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
        auto weak=weak_from_this();auto lease=c.lease;
        juce::MessageManager::callAsync([weak,lease]{if(auto s=weak.lock();s&&!s->stopping.load())s->revokeLease(lease);});
    }
    bool append(Connection& c,const std::vector<Json>& responses) {
        for(const auto& response:responses){
            auto line=response.dump()+"\n";
            if(line.size()>maxOutput||c.output.size()+line.size()>maxOutput)return false;
            if(c.output.empty())c.outputStarted=Clock::now();
            c.output+=line;
        }
        return true;
    }
    void run() {
#if defined(__APPLE__)
        pthread_set_qos_class_self_np(QOS_CLASS_UTILITY,0);
#endif
        std::vector<std::unique_ptr<Connection>> clients;
        try {
            while(!stopping.load()){
                std::vector<pollfd> polls{{listener.value,POLLIN,0}};
                for(auto& c:clients)polls.push_back({c->fd.value,short(POLLIN|(c->output.empty()?0:POLLOUT)),0});
                int polled=poll(polls.data(),nfds_t(polls.size()),20);
                if(polled<0){if(errno==EINTR)continue;throw std::runtime_error("MCP socket poll failed");}
                const auto now=Clock::now();
                // Process existing clients before accepting so poll indices stay stable.
                for(size_t i=clients.size();i-->0;){
                    auto& c=*clients[i];bool live=true;
                    try {
                        if(!c.session&&c.lease->future.wait_for(std::chrono::milliseconds(0))==std::future_status::ready)
                            c.session=std::make_unique<McpSession>(c.lease->future.get(),registry);
                        if(polls[i+1].revents&(POLLERR|POLLNVAL))live=false;
                        if(live&&(polls[i+1].revents&(POLLIN|POLLHUP))){
                            char data[8192];auto n=::recv(c.fd.value,data,sizeof(data),0);
                            if(n==0)live=false;
                            else if(n<0){if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)live=false;}
                            else{if(c.input.empty())c.partial=now;c.input.append(data,size_t(n));}
                        }
                        if(live&&c.session){
                            int lines=0;size_t end;
                            while(lines++<8&&(end=c.input.find('\n'))!=std::string::npos){
                                auto line=c.input.substr(0,end);c.input.erase(0,end+1);
                                if(!append(c,c.session->receive(line))){live=false;break;}
                                c.partial=now;
                            }
                            if(!append(c,c.session->ready()))live=false;
                        }
                        if(c.input.size()>McpSession::maximumMessageBytes||(!c.input.empty()&&now-c.partial>std::chrono::seconds(5)))live=false;
                        if((!c.session||!c.session->initialized())&&now-c.began>std::chrono::seconds(5))live=false;
                        if(!c.output.empty()&&now-c.outputStarted>std::chrono::seconds(5))live=false;
                        if(live&&!c.output.empty()&&(polls[i+1].revents&POLLOUT)){
                            auto n=ipc::send(c.fd.value,c.output.data(),c.output.size());
                            if(n>0)c.output.erase(0,size_t(n));
                            else if(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)live=false;
                        }
                    } catch(const std::exception&){live=false;}
                    if(!live){retire(c);clients.erase(clients.begin()+std::ptrdiff_t(i));++dropped;}
                }
                if(polls[0].revents&POLLIN){
                    ipc::Fd fd(::accept(listener.value,nullptr,nullptr));
                    if(fd.value>=0){
                        if(clients.size()<4&&pendingGrants.load()<8&&ipc::ownPeer(fd.value)){
                            try {
                                ipc::nonblocking(fd.value);
                                auto c=std::make_unique<Connection>();c->fd=std::move(fd);
                                auto weak=weak_from_this();auto lease=c->lease;
                                ++pendingGrants;
                                bool posted=juce::MessageManager::callAsync([weak,lease]{
                                    auto s=weak.lock();if(s)--s->pendingGrants;
                                    try {
                                        if(!s||s->stopping.load()||lease->abandoned.load())throw std::runtime_error("gateway connection closed");
                                        auto client=s->queue->connect("agent:mcp:"+juce::Uuid().toString().toStdString(),s->scope);
                                        lease->clientID=client.id();s->localClients.insert(client.id());
                                        if(lease->abandoned.load())s->revokeLease(lease);
                                        lease->promise.set_value(std::move(client));
                                        if(lease->abandoned.load())s->revokeLease(lease);
                                    } catch(...){lease->promise.set_exception(std::current_exception());}
                                });
                                if(!posted){--pendingGrants;throw std::runtime_error("message dispatch unavailable");}
                                clients.push_back(std::move(c));++accepted;
                            } catch(const std::exception&){++dropped;}
                        } else ++dropped;
                    }
                }
                count.store(clients.size());
            }
        } catch(const std::exception&){failed=true;}
        for(auto& c:clients)retire(*c);
        count=0;running=false;
    }
};
#else
struct McpGatewayState{};
#endif
juce::File McpGateway::defaultEndpoint(){
#if !defined(_WIN32)
    return juce::File(ipc::defaultPath());
#else
    return {};
#endif
}
McpGateway::McpGateway(CommandQueue& q,const juce::File& endpoint,const Scope& scope){
    if(!juce::MessageManager::getInstance()->isThisTheMessageThread())throw std::runtime_error("gateway grants require message thread");
    scope.validate();
    if(scope.mode==Permission::ScopedLowRisk)throw std::runtime_error("MCP commits require GUI preview grants");
#if !defined(_WIN32)
    auto s=std::make_shared<McpGatewayState>(q,scope);s->path=endpoint.getFullPathName().toStdString();
    auto address=ipc::address(s->path);auto parent=endpoint.getParentDirectory();
    if(!parent.createDirectory())throw std::runtime_error("cannot create MCP endpoint directory");
    struct stat parentInfo{};
    ipc::require(lstat(parent.getFullPathName().toRawUTF8(),&parentInfo)==0,"endpoint directory stat");
    if(!S_ISDIR(parentInfo.st_mode)||parentInfo.st_uid!=geteuid())throw std::runtime_error("endpoint directory must be owned and not a symlink");
    ipc::require(chmod(parent.getFullPathName().toRawUTF8(),0700)==0,"private endpoint directory");
    struct stat old{};
    if(lstat(s->path.c_str(),&old)==0){
        ipc::ownSocket(s->path);ipc::Fd check(::socket(AF_UNIX,SOCK_STREAM,0));
        ipc::require(check.value>=0,"socket probe");ipc::nonblocking(check.value);
        if(::connect(check.value,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0||errno!=ECONNREFUSED)
            throw std::runtime_error("MCP endpoint active or cannot be verified stale");
        struct stat current{};ipc::require(lstat(s->path.c_str(),&current)==0&&current.st_ino==old.st_ino&&current.st_dev==old.st_dev,"stale socket identity changed");
        ipc::require(::unlink(s->path.c_str())==0,"remove owned stale socket");
    } else if(errno!=ENOENT)throw std::runtime_error("cannot inspect MCP endpoint");
    s->listener=ipc::Fd(::socket(AF_UNIX,SOCK_STREAM,0));
    ipc::require(s->listener.value>=0,"socket create");ipc::nonblocking(s->listener.value);
    ipc::require(::bind(s->listener.value,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0,"socket bind");
    try {
        ipc::require(chmod(s->path.c_str(),0600)==0,"private MCP socket");
        ipc::require(lstat(s->path.c_str(),&old)==0,"socket identity");s->inode=old.st_ino;s->device=old.st_dev;
        ipc::require(::listen(s->listener.value,4)==0,"socket listen");
        s->worker=std::thread([s]{s->run();});state=s;
    } catch(...){::unlink(s->path.c_str());throw;}
#else
    throw std::runtime_error("Windows MCP local transport is not implemented");
#endif
}
McpGateway::~McpGateway(){stop();}
void McpGateway::stop(){
    if(!state)return;
#if !defined(_WIN32)
    auto s=std::move(state);s->stopping=true;
    if(s->worker.joinable())s->worker.join();
    for(const auto& id:s->localClients)try{s->queue->revoke(id);}catch(const std::exception&){}
    s->localClients.clear();
    struct stat current{};
    if(lstat(s->path.c_str(),&current)==0&&S_ISSOCK(current.st_mode)&&current.st_uid==geteuid()&&current.st_ino==s->inode&&current.st_dev==s->device)::unlink(s->path.c_str());
#endif
}
Json McpGateway::status()const{
#if !defined(_WIN32)
    if(state)return {{"state",state->stopping.load()?"stopping":state->failed.load()||!state->running.load()?"failed":"listening"},{"endpoint",state->path},{"permission",state->scope.json()},{"clients",state->count.load()},{"pending_grants",state->pendingGrants.load()},{"accepted",state->accepted.load()},{"disconnected_or_rejected",state->dropped.load()},{"maximum_clients",4},{"transport","Unix socket, newline JSON-RPC; stdio bridge"}};
#endif
    return {{"state","disabled"}};
}
}
