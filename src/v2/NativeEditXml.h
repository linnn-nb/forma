#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <iomanip>
#include <locale>
#include <sstream>
#include <cmath>
#include <limits>
namespace ndaw::v2
{
// XML attributes load as strings anyway. Preserve every finite native double's
// round-trip decimal representation, including sub-sample automation geometry.
// JUCE var::toString uses shorter display formatting. Never mutate the Edit tree.
inline std::unique_ptr<juce::XmlElement> preciseEditXml(const juce::ValueTree& state)
{
    auto xml = state.createXml();
    if (!xml)
        return nullptr;
    auto preserve = [&](auto&& self, const juce::ValueTree& native, juce::XmlElement& node) -> void
    {
        for (int i = 0; i < native.getNumProperties(); ++i)
        {
            const auto key = native.getPropertyName(i);
            const auto value = native[key];
            if (value.isDouble() && std::isfinite(double(value)))
            {
                // Floating to_chars needs macOS 13.3; keep the existing deployment target.
                std::ostringstream text;
                text.imbue(std::locale::classic());
                text << std::setprecision(std::numeric_limits<double>::max_digits10) << double(value);
                node.setAttribute(key, juce::String::fromUTF8(text.str().c_str()));
            }
        }
        int i = 0;
        for (auto* child = node.getFirstChildElement(); child; child = child->getNextElement())
            self(self, native.getChild(i++), *child);
    };
    preserve(preserve, state, *xml);
    return xml;
}
} // namespace ndaw::v2
