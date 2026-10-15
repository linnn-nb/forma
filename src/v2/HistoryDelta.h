// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>

namespace ndaw::v2::history_delta
{
using Path = Json;
struct DuplicateIdentity : std::runtime_error
{
    using std::runtime_error::runtime_error;
};
inline void require(bool ok, const char* reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
inline std::string identity(const juce::ValueTree& n)
{
    for (const char* p : {"id", "ndaw_id", "plan_id"})
        if (n.hasProperty(p))
            return n.getType().toString().toStdString() + ":" + p + ":" + n[p].toString().toStdString();
    if (n.hasType("BASE"))
        return "BASE:" + n["plugin"].toString().toStdString() + ":" + n["parameter"].toString().toStdString();
    if (n.hasType(te::IDs::AUTOMATIONCURVE))
    {
        const auto p = n.hasProperty(te::IDs::paramID) ? n[te::IDs::paramID] : n[te::IDs::name];
        if (p.toString().isNotEmpty())
            return "CURVE:" + p.toString().toStdString();
    }
    return {};
}
inline std::vector<Path> locators(juce::ValueTree parent)
{
    std::vector<Path> result;
    std::map<std::string, int> ordinals;
    std::set<std::string> keys;
    for (auto child : parent)
    {
        const auto type = child.getType().toString().toStdString(), key = identity(child);
        if (!key.empty() && !keys.insert(key).second)
            throw DuplicateIdentity("duplicate persistent node identity in " +
                                    parent.getType().toString().toStdString() + ": " + key);
        result.push_back({{"type", type}, {"key", key}, {"ordinal", key.empty() ? ordinals[type]++ : 0}});
    }
    return result;
}
inline void validateLocator(const Path& part)
{
    require(part.is_object() && part.size() == 3 && part.at("type").is_string() && part.at("key").is_string() &&
                part.at("ordinal").is_number_integer(),
            "invalid history node locator");
}
inline int find(juce::ValueTree parent, const Path& part)
{
    validateLocator(part);
    int ordinal = 0;
    for (int i = 0; i < parent.getNumChildren(); ++i)
    {
        const auto child = parent.getChild(i);
        if (child.getType().toString().toStdString() != part["type"].get<std::string>())
            continue;
        const auto key = identity(child);
        if (!part["key"].get<std::string>().empty())
        {
            if (key == part["key"].get<std::string>())
                return i;
        }
        else if (key.empty() && ordinal++ == part["ordinal"].get<int>())
            return i;
    }
    return -1;
}
inline juce::ValueTree resolve(juce::ValueTree root, const Path& path)
{
    require(path.is_array() && path.size() <= 128, "invalid persistent history path");
    for (const auto& part : path)
    {
        const auto i = find(root, part);
        if (i < 0)
            return {};
        root = root.getChild(i);
    }
    return root;
}
// One detached replay only. Native reconciliation may mutate other parents, so
// that path deliberately keeps the original lookup. Anonymous ordinal locators
// also retain their original semantics; only stable keyed siblings are indexed.
class ReplayLookup
{
public:
    struct Limits
    {
        size_t parents = 64, nodes = 65536, keyBytes = 8 * 1024 * 1024;
    };
    explicit ReplayLookup(bool active) : enabled(active) {}
    ReplayLookup(bool active, Limits budget) : enabled(active), limits(budget) {}
    void clear()
    {
        entries.clear();
        nodes = bytes = 0;
    }
    size_t cachedNodes() const
    {
        return nodes;
    }
    size_t cachedKeyBytes() const
    {
        return bytes;
    }
    size_t cachedParents() const
    {
        return entries.size();
    }
    juce::ValueTree child(juce::ValueTree parent, const Path& part)
    {
        validateLocator(part);
        auto linear = [&]
        {
            const auto at = find(parent, part);
            return at < 0 ? juce::ValueTree{} : parent.getChild(at);
        };
        if (!enabled || part["key"].get_ref<const std::string&>().empty() || parent.getNumChildren() < 32)
            return linear();
        auto found = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) { return e.parent == parent; });
        if (found == entries.end())
        {
            const auto count = size_t(parent.getNumChildren());
            if (entries.size() >= limits.parents || count > limits.nodes - nodes)
                return linear();
            Entry entry;
            entry.parent = parent;
            for (auto node : parent)
            {
                auto type = node.getType().toString().toStdString(), key = identity(node);
                const auto available = limits.keyBytes - bytes - entry.bytes;
                if (type.size() > available || key.size() > available - type.size())
                    return linear();
                entry.bytes += type.size() + key.size();
                if (!key.empty())
                    entry.keyed[{std::move(type), std::move(key)}].push_back(node);
            }
            bytes += entry.bytes;
            nodes += count;
            entries.push_back(std::move(entry));
            found = std::prev(entries.end());
        }
        const auto match = found->keyed.find({part["type"].get<std::string>(), part["key"].get<std::string>()});
        if (match != found->keyed.end())
            for (auto node : match->second)
                if (node.getParent() == parent && identity(node) == part["key"].get<std::string>())
                    return node; // Preserve first-live-match behavior, including duplicate keys.
        return {};
    }
    juce::ValueTree resolve(juce::ValueTree root, const Path& path)
    {
        require(path.is_array() && path.size() <= 128, "invalid persistent history path");
        for (const auto& part : path)
        {
            root = child(root, part);
            if (!root.isValid())
                return {};
        }
        return root;
    }

private:
    struct Entry
    {
        juce::ValueTree parent;
        std::map<std::pair<std::string, std::string>, std::vector<juce::ValueTree>> keyed;
        size_t bytes = 0;
    };
    bool enabled;
    Limits limits;
    size_t nodes = 0, bytes = 0;
    std::vector<Entry> entries;
};
inline Path address(juce::ValueTree node)
{
    Path result = Json::array();
    for (auto parent = node.getParent(); parent.isValid(); parent = node.getParent())
    {
        result.insert(result.begin(), locators(parent).at(size_t(parent.indexOf(node))));
        node = parent;
    }
    return result;
}
inline bool prefix(const Path& a, const Path& b)
{
    return a.size() <= b.size() && std::equal(a.begin(), a.end(), b.begin());
}
using Normalise = std::function<juce::ValueTree(juce::ValueTree)>;
inline juce::ValueTree image(juce::ValueTree n, const Normalise& normalise)
{
    auto value = (normalise ? normalise(n) : n).createCopy();
    if (normalise)
        for (int i = value.getNumChildren(); --i >= 0;)
        {
            auto child = image(value.getChild(i), normalise);
            value.removeChild(i, nullptr);
            value.addChild(child, i, nullptr);
        }
    return value;
}
inline juce::ValueTree difference(const juce::ValueTree& before, juce::ValueTree after,
                                  const Path& path = Json::array(), bool recursive = true, Normalise normalise = {})
{
    if (normalise)
        after = normalise(after);
    require(before.isValid() && after.hasType(before.getType()), "history scope identity changed");
    if (!normalise && before.isEquivalentTo(after))
        return juce::ValueTree("DELTA");
    juce::ValueTree result("DELTA"), change("CHANGE"), old("BEFORE"), next("AFTER");
    change.setProperty("path", juce::String(path.dump()), nullptr);
    std::set<juce::Identifier> names;
    for (const auto node : {before, after})
        for (int i = 0; i < node.getNumProperties(); ++i)
            names.insert(node.getPropertyName(i));
    Json changed = Json::array();
    for (const auto& name : names)
        if (before.hasProperty(name) != after.hasProperty(name) || before[name] != after[name])
        {
            changed.push_back(name.toString().toStdString());
            if (before.hasProperty(name))
                old.setProperty(name, before[name], nullptr);
            if (after.hasProperty(name))
                next.setProperty(name, after[name], nullptr);
        }
    if (!changed.empty())
    {
        change.setProperty("properties", juce::String(changed.dump()), nullptr);
        change.addChild(old, -1, nullptr);
        change.addChild(next, -1, nullptr);
    }
    if (!recursive)
    {
        if (!changed.empty())
            result.addChild(change, -1, nullptr);
        return result;
    }
    const auto a = locators(before), b = locators(after);
    std::map<std::string, int> ai, bi;
    for (size_t i = 0; i < a.size(); ++i)
        ai[a[i].dump()] = int(i);
    for (size_t i = 0; i < b.size(); ++i)
        bi[b[i].dump()] = int(i);
    Json commonA = Json::array(), commonB = Json::array();
    for (size_t i = 0; i < a.size(); ++i)
        if (!bi.contains(a[i].dump()))
        {
            juce::ValueTree removed("REMOVE");
            removed.setProperty("locator", juce::String(a[i].dump()), nullptr);
            removed.setProperty("index", int(i), nullptr);
            removed.addChild(before.getChild(int(i)).createCopy(), -1, nullptr);
            change.addChild(removed, -1, nullptr);
        }
        else
            commonA.push_back(a[i]);
    for (size_t i = 0; i < b.size(); ++i)
        if (!ai.contains(b[i].dump()))
        {
            juce::ValueTree added("ADD");
            added.setProperty("locator", juce::String(b[i].dump()), nullptr);
            added.setProperty("index", int(i), nullptr);
            added.addChild(image(after.getChild(int(i)), normalise), -1, nullptr);
            change.addChild(added, -1, nullptr);
        }
        else
            commonB.push_back(b[i]);
    if (commonA != commonB)
    {
        change.setProperty("order_before", juce::String(Json(a).dump()), nullptr);
        change.setProperty("order_after", juce::String(Json(b).dump()), nullptr);
    }
    if (change.getNumProperties() > 1 || change.getNumChildren() > 0)
        result.addChild(change, -1, nullptr);
    for (size_t i = 0; i < a.size(); ++i)
        if (auto match = bi.find(a[i].dump()); match != bi.end())
        {
            auto childPath = path;
            childPath.push_back(a[i]);
            auto children =
                difference(before.getChild(int(i)), after.getChild(match->second), childPath, true, normalise);
            for (auto child : children)
                result.addChild(child.createCopy(), -1, nullptr);
        }
    return result;
}
using Reconcile = std::function<juce::ValueTree(juce::ValueTree, const juce::ValueTree&)>;
inline juce::ValueTree inverse(const juce::ValueTree& delta)
{
    juce::ValueTree result("DELTA");
    for (int i = delta.getNumChildren(); --i >= 0;)
    {
        auto old = delta.getChild(i);
        juce::ValueTree next("CHANGE");
        next.copyPropertiesFrom(old, nullptr);
        if (old.hasProperty("order_before"))
        {
            next.setProperty("order_before", old["order_after"], nullptr);
            next.setProperty("order_after", old["order_before"], nullptr);
        }
        for (auto child : old)
        {
            juce::ValueTree value(child.hasType("BEFORE")   ? "AFTER"
                                  : child.hasType("AFTER")  ? "BEFORE"
                                  : child.hasType("ADD")    ? "REMOVE"
                                  : child.hasType("REMOVE") ? "ADD"
                                                            : child.getType().toString());
            value.copyPropertiesAndChildrenFrom(child, nullptr);
            next.addChild(value, -1, nullptr);
        }
        result.addChild(next, -1, nullptr);
    }
    return result;
}
inline void apply(juce::ValueTree root, const juce::ValueTree& delta, bool forward, Reconcile reconcile = {})
{
    require(delta.hasType("DELTA"), "invalid history delta");
    struct Transfer
    {
        juce::ValueTree parent;
        Path locator;
    };
    std::vector<Transfer> transfers;
    ReplayLookup lookup(!reconcile);
    for (int step = 0; step < delta.getNumChildren(); ++step)
    {
        const auto change = delta.getChild(forward ? step : delta.getNumChildren() - step - 1);
        require(change.hasType("CHANGE"), "invalid history delta change");
        auto target = lookup.resolve(root, Json::parse(change["path"].toString().toStdString()));
        require(target.isValid(), "persistent history target unavailable");
        if (change.hasProperty("properties"))
        {
            const auto oldIdentity = reconcile ? std::string{} : identity(target);
            const auto values = change.getChildWithName(forward ? "AFTER" : "BEFORE");
            require(values.isValid(), "persistent property image missing");
            for (const auto& name : Json::parse(change["properties"].toString().toStdString()))
            {
                require(name.is_string(), "invalid persistent property name");
                const auto key = juce::Identifier(name.get<std::string>());
                if (values.hasProperty(key))
                    target.setProperty(key, values[key], nullptr);
                else
                    target.removeProperty(key, nullptr);
            }
            if (!reconcile && identity(target) != oldIdentity)
                lookup.clear();
        }
        const auto removeType = juce::Identifier(forward ? "REMOVE" : "ADD");
        const auto addType = juce::Identifier(forward ? "ADD" : "REMOVE");
        for (int i = change.getNumChildren(); --i >= 0;)
            if (auto child = change.getChild(i); child.hasType(removeType))
            {
                const auto locator = Json::parse(child["locator"].toString().toStdString());
                const auto value = lookup.child(target, locator);
                if (value.isValid())
                {
                    const int recorded = int(child["index"]);
                    const int at =
                        recorded >= 0 && recorded < target.getNumChildren() && target.getChild(recorded) == value
                            ? recorded
                            : target.indexOf(value);
                    target.removeChild(at, nullptr);
                }
                else
                {
                    // Native reconciliation can transfer this same stable node
                    // to its new parent before the delta reaches its old-parent
                    // removal. Only that exact recorded transfer consumes it.
                    auto moved = std::find_if(transfers.begin(), transfers.end(), [&](const Transfer& t)
                                              { return t.parent == target && t.locator == locator; });
                    require(moved != transfers.end(), "persistent removed child unavailable");
                    transfers.erase(moved);
                }
            }
        // Additions can precede an existing duplicate key or create a formerly
        // missing path. Removals need no rebuild: cached handles are checked for
        // their actual parent, and anonymous ordinals always use linear lookup.
        if (change.getChildWithName(addType).isValid())
            lookup.clear();
        for (auto child : change)
            if (child.hasType(addType))
            {
                require(child.getNumChildren() == 1, "persistent child image missing");
                auto value = reconcile ? reconcile(target, child.getChild(0)) : child.getChild(0).createCopy();
                if (auto parent = value.getParent(); parent.isValid())
                {
                    const auto locations = locators(parent);
                    transfers.push_back({parent, locations[size_t(parent.indexOf(value))]});
                    parent.removeChild(value, nullptr);
                }
                target.addChild(value, int(child["index"]), nullptr);
            }
        if (change.hasProperty("order_before"))
        {
            lookup.clear();
            const auto order = Json::parse(change[forward ? "order_after" : "order_before"].toString().toStdString());
            require(order.is_array() && int(order.size()) == target.getNumChildren(), "invalid history child order");
            std::vector<juce::ValueTree> values;
            for (const auto& part : order)
            {
                const auto value = lookup.child(target, part);
                require(value.isValid(), "persistent ordered child unavailable");
                values.push_back(value);
            }
            for (int i = 0; i < int(values.size()); ++i)
                target.moveChild(target.indexOf(values[size_t(i)]), i, nullptr);
            lookup.clear();
        }
    }
}
} // namespace ndaw::v2::history_delta
