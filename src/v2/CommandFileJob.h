#pragma once
#include <nativedaw/v2/CommandQueue.h>
#include <juce_cryptography/juce_cryptography.h>
namespace ndaw::v2 {
// Optional local SDK ingress. File contents are data, not a model/system prompt.
class CommandFileJob final:public juce::ThreadPoolJob {
public:
    CommandFileJob(juce::File source,CommandQueue::Client client,uint64_t revision,std::function<void(Json)> completion)
        :ThreadPoolJob("NativeDAW command file"),source(std::move(source)),client(std::move(client)),revision(revision),completion(std::move(completion)){}
    JobStatus runJob() override {
        Json result;
        try{
            if(shouldExit())throw std::runtime_error("command file task cancelled");
            if(!source.existsAsFile()||source.getSize()<1||source.getSize()>int64_t(CommandQueue::maximumPayloadBytes))throw std::runtime_error("command file must be 1..256 KiB");
            juce::MemoryBlock bytes;if(!source.loadFileAsData(bytes)||bytes.getSize()>CommandQueue::maximumPayloadBytes)throw std::runtime_error("command file read failed or exceeds limit");
            std::string raw(static_cast<const char*>(bytes.getData()),bytes.getSize());auto args=Json::parse(raw);
            if(!args.is_object()||!args.contains("operations")||args.size()>(args.contains("base_revision")?2:1))throw std::runtime_error("command file accepts only operations and optional base_revision");
            if(!args.contains("base_revision"))args["base_revision"]=revision;
            if(shouldExit())throw std::runtime_error("command file task cancelled");
            auto planned=wait(client.submit("plan",args));
            if(planned["status"]!="planned")result=planned;
            else {
                result=wait(client.submit("commit",{{"plan_id",planned["plan"]["plan_id"]}}));
                result["plan"]=planned["plan"];if(!result.contains("preview"))result["preview"]=planned["preview"];
            }
            result["source"]={{"kind","local_command_file"},{"path",source.getFullPathName().toStdString()},{"sha256",juce::SHA256(bytes.getData(),bytes.getSize()).toHexString().toStdString()}};
        }catch(const std::exception& e){result={{"status",shouldExit()?"cancelled":"failed"},{"error",e.what()}};}
        auto callback=completion;juce::MessageManager::callAsync([callback,result]{callback(result);});return jobHasFinished;
    }
private:
    Json wait(const CommandQueue::Ticket& ticket){
        while(ticket.result.wait_for(std::chrono::milliseconds(10))!=std::future_status::ready){
            if(shouldExit()&&ticket.cancel())return {{"status","cancelled"},{"error","command task cancelled before execution"}};
        }
        return ticket.result.get();
    }
    juce::File source;CommandQueue::Client client;uint64_t revision;std::function<void(Json)> completion;
};
}
