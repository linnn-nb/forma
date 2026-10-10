// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>

namespace ndaw::v2
{
// Local editing history, never an Agent receipt or executable plan.
// Detached states are serialized only by an explicit user save. Native UndoManager
// owns the restored actions; the running Edit and stable node identities stay intact.
class PersistentHistory final
{
public:
    struct Archive
    {
        std::vector<std::string> ids;
        std::vector<juce::ValueTree> states;
        size_t cursor = 0;
        uint64_t highestID = 0;
    };
    explicit PersistentHistory(Commands&);
    void checkpoint();
    void appended();
    juce::ValueTree save();
    static Archive read(juce::ValueTree, const juce::File&); // validates before touching the current Edit
    void install(Archive);

private:
    struct Action;
    juce::ValueTree capture(bool saved = false);
    void restore(const juce::ValueTree&);
    void applyBases(const juce::ValueTree&);
    void index(juce::ValueTree);
    void patch(juce::ValueTree, const juce::ValueTree&);
    Commands& owner;
    std::vector<juce::ValueTree> states;
    std::map<std::string, juce::ValueTree> nodes;
};
} // namespace ndaw::v2
