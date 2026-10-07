#pragma once
#include <nativedaw/v2/CommandQueue.h>
namespace ndaw::v2 {
// L4 JSON-RPC lifecycle/typed tools. Called only from a non-audio transport thread.
// Owns no Edit, engine, local grants or UI confirmation resolver.
class McpSession {
public:
    McpSession(CommandQueue::Client,Json registry);
    ~McpSession();
    std::vector<Json> receive(const std::string& line);
    std::vector<Json> ready();
    void close();
    bool initialized()const{return phase==Ready;}
    static Json tools(const Json& registry);
    static constexpr size_t maximumMessageBytes=256*1024,maximumInFlight=8;
private:
    enum Phase {Fresh,Negotiated,Ready,Closed};Phase phase=Fresh;
    CommandQueue::Client client;Json definitions;
    std::map<std::string,std::string> queryMethods;
    struct Pending {Json id;CommandQueue::Ticket ticket;};
    std::map<std::string,Pending> pending;
    static Json error(const Json& id,int code,const std::string& message);
    static Json result(const Json& id,Json);
    static Json toolResult(const Json& id,Json);
};
}
