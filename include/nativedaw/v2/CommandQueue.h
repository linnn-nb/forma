#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include <future>
#include <memory>
namespace ndaw::v2 {
struct QueueState;
struct QueueClientState;
struct QueueJob;
// Background-thread SDK entry. Never call from an audio callback or wait on the
// message thread. Every Edit operation is dispatched by this queue on that thread.
class CommandQueue {
public:
    class Client;
    struct Ticket {
        std::shared_future<Json> result;
        bool cancel() const;
    private:
        std::shared_ptr<QueueJob> job;
        friend class Client;
    };
    class Client {
    public:
        Ticket submit(const std::string& method,Json args=Json::object(),int timeoutMs=10000) const;
        std::string id() const;
    private:
        std::weak_ptr<QueueState> queue;
        std::shared_ptr<QueueClientState> principal;
        friend class CommandQueue;
    };
    explicit CommandQueue(Commands&);
    ~CommandQueue();
    // Trusted local UI only. Clients cannot send actor, scope, or acceptance.
    Client connect(const std::string& actor,const Scope& scope={});
    void grant(const std::string& client,const Scope&);
    void revoke(const std::string& client);
    Json pending();
    Json resolve(const std::string& confirmationID,bool accepted);
    Json status() const;
    void shutdown();
    static constexpr size_t capacity=32,maximumPayloadBytes=256*1024;
private:
    std::shared_ptr<QueueState> state;
};
}
