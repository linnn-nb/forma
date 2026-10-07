#pragma once
#include <nativedaw/v2/CommandQueue.h>
namespace ndaw::v2 {
struct McpGatewayState;
// Trusted message-thread owner configures local permissions. Network workers
// have only opaque queue clients. No TCP listener or second audio engine.
class McpGateway {
public:
    McpGateway(CommandQueue&,const juce::File& endpoint,const Scope&);
    ~McpGateway();
    void stop();
    Json status()const;
    static juce::File defaultEndpoint();
private:std::shared_ptr<McpGatewayState> state;
};
}
