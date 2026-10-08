#include <nativedaw/v2/RequestIdentity.h>

namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
bool hash(const std::string& s)
{
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
} // namespace
Json Commands::requestAudit() const
{
    int roots = 0;
    for (auto child : metadata)
        if (child.hasType("REQUEST_AUDIT"))
            ++roots;
    require(roots <= 1, "duplicate request audit trees; manual recovery required");
    auto tree = metadata.getChildWithName("REQUEST_AUDIT");
    Json result = Json::array();
    if (!tree.isValid())
        return result;
    const auto schema = tree.getProperty("schema", 0);
    // Tracktion Edit XML round-trips ValueTree attributes as strings. Accept
    // the canonical spelling too, without accepting coercions such as "1x".
    require(((schema.isInt() || schema.isInt64()) && int(schema) == 1) ||
                (schema.isString() && schema.toString() == "1"),
            "unsupported request audit schema; manual recovery required");
    require(size_t(tree.getNumChildren()) <= maximumRequestRecords,
            "request audit exceeds resource budget; manual recovery required");
    std::set<std::string> keys;
    for (auto row : tree)
    {
        require(row.hasType("REQUEST"), "invalid request audit row; manual recovery required");
        Json record;
        for (const char* name :
             {"request_key", "request_fingerprint", "request_scope_hash", "plan_id", "actor", "state"})
        {
            auto value = row.getProperty(name);
            require(value.isString(), "invalid request audit property; manual recovery required");
            record[name] = value.toString().toStdString();
        }
        const auto key = requestKey(record["request_key"]);
        require(keys.insert(key).second, "duplicate request audit key; manual recovery required");
        require(hash(record["request_fingerprint"]) && hash(record["request_scope_hash"]),
                "invalid request audit hash; manual recovery required");
        require(!record["plan_id"].get<std::string>().empty() && record["plan_id"].get<std::string>().size() <= 256 &&
                    record["actor"].get<std::string>().size() <= 256,
                "invalid request audit identity; manual recovery required");
        require(record["state"] == "committed" || record["state"] == "undone",
                "invalid request audit state; manual recovery required");
        result.push_back(std::move(record));
    }
    return result;
}
Json Commands::requestRecovery(const std::string& key) const
{
    checkThread();
    requestKey(key);
    captureNativeStates();
    for (const auto& [_, receipt] : receipts)
    {
        auto plan = Json::parse(receipt.fingerprint);
        if (plan.value("request_key", std::string{}) == key)
        {
            auto actual = transactionStatus(plan["plan_id"]);
            return {{"state", actual["state"]}, {"plan", plan}, {"receipt", actual}, {"trusted_current_run", true}};
        }
    }
    for (const auto& record : requestAudit())
        if (record["request_key"] == key)
            return {{"state", "recovery_requires_review"},
                    {"historical_marker", record},
                    {"receipt", nullptr},
                    {"trusted_current_run", false}};
    return {{"state", "unknown"}, {"receipt", nullptr}, {"trusted_current_run", false}};
}
void Commands::validateRequestCommit(const Json& plan) const
{
    if (!plan.contains("request_key") && !plan.contains("request_fingerprint") && !plan.contains("request_scope_hash"))
        return;
    const auto key = requestKey(plan.at("request_key"));
    require(hash(plan.at("request_fingerprint")) && hash(plan.at("request_scope_hash")),
            "invalid host request fingerprint");
    require(requestRecovery(key)["state"] == "unknown",
            "request key already recorded; recover or review instead of replaying");
    require(requestAudit().size() < maximumRequestRecords,
            "request audit capacity reached; start a new session explicitly");
}
void Commands::storeRequestAudit(const Json& plan, const std::string& state)
{
    if (!plan.contains("request_key"))
        return;
    // Deliberately outside Undo: an undone intent must not become reusable. This
    // is saved with Edit, not a WAL or a substitute for a current execution receipt.
    auto tree = metadata.getOrCreateChildWithName("REQUEST_AUDIT", nullptr);
    tree.setProperty("schema", 1, nullptr);
    juce::ValueTree row("REQUEST");
    for (const char* name : {"request_key", "request_fingerprint", "request_scope_hash", "plan_id", "actor"})
        row.setProperty(name, juce::String(plan.at(name).get<std::string>()), nullptr);
    row.setProperty("state", juce::String(state), nullptr);
    tree.addChild(row, -1, nullptr);
}
void Commands::updateRequestAudit(const std::string& id, const std::string& state)
{
    auto tree = metadata.getChildWithName("REQUEST_AUDIT");
    for (auto row : tree)
        if (row.getProperty("plan_id").toString() == juce::String(id))
            row.setProperty("state", juce::String(state), nullptr);
}
} // namespace ndaw::v2
