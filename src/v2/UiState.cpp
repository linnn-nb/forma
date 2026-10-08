#include "TimelineState.h"

namespace ndaw::v2
{
namespace
{
Json defaults()
{
    return {{"start_samples", 0},  {"span_samples", 480000}, {"first_row", 0},     {"row_height", 144},
            {"workspace", "edit"}, {"tracks_list", true},    {"clips_list", true}, {"keymap_xml", ""}};
}
void validate(const Json& value)
{
    const auto model = defaults();
    if (!value.is_object() || value.size() != model.size())
        throw std::runtime_error("invalid saved UI state");
    for (auto it = model.begin(); it != model.end(); ++it)
    {
        if (!value.contains(it.key()) || value[it.key()].type() != it.value().type())
        {
            // nlohmann distinguishes positive integer storage; accept both integer signs.
            if (!it.value().is_number_integer() || !value.contains(it.key()) || !value[it.key()].is_number_integer())
                throw std::runtime_error("invalid UI field type");
        }
    }
    const auto max = std::llround(te::Edit::maximumLength * 48000);
    for (const auto* key : {"start_samples", "span_samples", "first_row", "row_height"})
        if (value[key].is_number_unsigned() && value[key].get<uint64_t>() > uint64_t(max))
            throw std::runtime_error("UI integer overflow");
    auto start = value["start_samples"].get<int64_t>(), span = value["span_samples"].get<int64_t>();
    if (start < 0 || span < 480 || span > max || start > max - span || value["first_row"].get<int64_t>() < 0 ||
        value["first_row"].get<int64_t>() > 100000 || value["row_height"].get<int>() < 96 ||
        value["row_height"].get<int>() > 320)
        throw std::runtime_error("UI viewport out of range");
    if (value["workspace"] != "edit" && value["workspace"] != "mix" && value["workspace"] != "midi")
        throw std::runtime_error("unknown UI workspace");
    const auto xml = value["keymap_xml"].get<std::string>();
    if (xml.size() > 128 * 1024)
        throw std::runtime_error("key mappings exceed saved UI budget");
    if (!xml.empty())
    {
        auto document = juce::parseXML(juce::String::fromUTF8(xml.c_str()));
        if (!document || !document->hasTagName("KEYMAPPINGS"))
            throw std::runtime_error("invalid shortcut mapping XML");
    }
}
} // namespace
Json readUiState(const juce::ValueTree& metadata)
{
    auto result = defaults();
    auto state = metadata.getChildWithName("UI");
    if (state.isValid())
    {
        if (state.getNumProperties() != 1 || !state.hasProperty("json") || !state.getProperty("json").isString())
            throw std::runtime_error("malformed UI subtree");
        result = Json::parse(state.getProperty("json").toString().toStdString());
    }
    validate(result);
    return result;
}
Json Commands::uiState() const
{
    checkThread();
    return readUiState(metadata);
}
Json Commands::updateUiState(const Json& patch, const std::string& expectedSession)
{
    checkThread();
    if (expectedSession != sessionToken())
        throw std::runtime_error("UI session changed");
    if (!patch.is_object())
        throw std::runtime_error("UI patch must be an object");
    auto next = uiState();
    for (auto it = patch.begin(); it != patch.end(); ++it)
    {
        if (!next.contains(it.key()))
            throw std::runtime_error("unknown UI field");
        next[it.key()] = it.value();
    }
    validate(next);
    if (next != uiState())
    {
        auto state = metadata.getOrCreateChildWithName("UI", nullptr);
        state.setProperty("json", juce::String(next.dump()), nullptr);
    }
    return next;
}
} // namespace ndaw::v2
