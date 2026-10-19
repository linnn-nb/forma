// SPDX-License-Identifier: AGPL-3.0-only
// Linked by qualify_project_reader.py against unchanged historical production
// ProjectDocument.cpp. This qualifies that format reader, not an older whole DAW.
#include "ProjectDocument.h"
#include <iostream>
int main(int argc, char** argv)
{
    try
    {
        if (argc != 2 && argc != 4)
            throw std::runtime_error("expected project path or --legacy-copy input output");
        if (argc == 4)
        {
            if (std::string(argv[1]) != "--legacy-copy")
                throw std::runtime_error("unknown mode");
            const auto input = juce::parseXML(juce::File(juce::String::fromUTF8(argv[2])));
            const auto* payload = input ? input->getChildByName("EDIT") : nullptr;
            if (!payload)
                throw std::runtime_error("missing Edit payload for owned writer fixture");
            auto native = juce::ValueTree::fromXml(*payload);
            ndaw::v2::project_document::Requirements requirements;
            ndaw::v2::project_document::collect(native, requirements);
            auto legacy = ndaw::v2::project_document::write(native, requirements);
            const juce::File output(juce::String::fromUTF8(argv[3]));
            if (output.exists() || !legacy->writeTo(output))
                throw std::runtime_error("legacy writer output exists or write failed");
            return 0;
        }
        const auto xml = juce::parseXML(juce::File(juce::String::fromUTF8(argv[1])));
        if (!xml)
            throw std::runtime_error("XML parse failed");
        auto doc = ndaw::v2::project_document::read(*xml);
        if (!doc.edit.hasType("EDIT"))
            throw std::runtime_error("no native Edit");
        std::cout << "accepted by historical production format reader\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
