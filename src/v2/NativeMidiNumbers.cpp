// SPDX-License-Identifier: AGPL-3.0-only
#include "NativeMidiNumbers.h"
#include <json.hpp>
#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace ndaw::v2::native_midi_numbers
{
namespace
{
enum class Kind
{
    other,
    edit,
    track,
    lists,
    list,
    clip,
    sequence,
    note,
    control
};
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
Kind childKind(Kind parent, const juce::String& name)
{
    if ((parent == Kind::edit || parent == Kind::track) && (name == "TRACK" || name == "FOLDERTRACK"))
        return Kind::track;
    if (parent == Kind::track && name == "NDAW_PLAYLISTS")
        return Kind::lists;
    if (parent == Kind::lists && name == "PLAYLIST")
        return Kind::list;
    if ((parent == Kind::track || parent == Kind::list) && name == "MIDICLIP")
        return Kind::clip;
    if (parent == Kind::clip && name == "SEQUENCE")
        return Kind::sequence;
    if (parent == Kind::sequence && name == "NOTE")
        return Kind::note;
    if (parent == Kind::sequence && name == "CONTROL")
        return Kind::control;
    return Kind::other;
}
double number(const juce::var& value)
{
    double result = 0;
    if (value.isString())
    {
        const auto text = value.toString();
        require(text.length() <= 512, "MIDI timing numeric text exceeds 512-character budget");
        std::istringstream input(text.toStdString());
        input.imbue(std::locale::classic());
        input >> result;
        // libc++ num_get reports ERANGE for a correctly rounded, nonzero
        // subnormal. Overflow, malformed input and underflow-to-zero still fail.
        require(!input.bad() && (!input.fail() || std::fpclassify(result) == FP_SUBNORMAL),
                "invalid MIDI timing number");
        if (input.fail())
            input.clear();
        input >> std::ws;
        require(input.eof(), "invalid MIDI timing number suffix");
    }
    else
    {
        require(value.isDouble() || value.isInt() || value.isInt64(), "invalid MIDI timing numeric type");
        result = double(value);
    }
    require(std::isfinite(result), "nonfinite MIDI timing number");
    return result;
}
void fields(juce::ValueTree node, Kind kind)
{
    for (const auto* key : {"b", "l"})
        if ((kind == Kind::note || juce::String(key) == "b") && node.hasProperty(key))
            node.setProperty(key, number(node[key]), nullptr);
}
void visit(juce::ValueTree node, Kind kind)
{
    if (kind == Kind::other)
        return;
    if (kind == Kind::note || kind == Kind::control)
    {
        fields(node, kind);
        return; // MPE children, opaque origin records and other attributes are data.
    }
    for (auto child : node)
        visit(child, childKind(kind, child.getType().toString()));
}
} // namespace
void prepareEdit(juce::ValueTree edit)
{
    require(edit.hasType("EDIT"), "MIDI numeric decode requires a detached Edit");
    visit(edit, Kind::edit);
}
void prepareDelta(juce::ValueTree delta)
{
    require(delta.hasType("DELTA"), "MIDI numeric decode requires a history delta");
    for (auto change : delta)
    {
        require(change.hasType("CHANGE"), "invalid MIDI numeric history change");
        const auto path = nlohmann::json::parse(change["path"].toString().toStdString(), nullptr, false);
        require(path.is_array() && path.size() <= 128, "invalid MIDI numeric history path");
        auto kind = Kind::edit;
        for (const auto& part : path)
        {
            require(part.is_object() && part.contains("type") && part.at("type").is_string(),
                    "invalid MIDI numeric path type");
            kind = childKind(kind, juce::String(part.at("type").get<std::string>()));
            if (kind == Kind::other)
                break;
        }
        if (kind == Kind::other)
            continue;
        for (auto image : change)
        {
            if ((kind == Kind::note || kind == Kind::control) && (image.hasType("BEFORE") || image.hasType("AFTER")))
                fields(image, kind);
            if (image.hasType("ADD") || image.hasType("REMOVE"))
            {
                require(image.getNumChildren() == 1, "invalid MIDI numeric child image");
                auto child = image.getChild(0);
                visit(child, childKind(kind, child.getType().toString()));
            }
        }
    }
}
} // namespace ndaw::v2::native_midi_numbers
