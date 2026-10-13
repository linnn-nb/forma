// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include "HistoryDelta.h"

namespace ndaw::v2
{
// Local editing history, never an Agent receipt or executable plan.
// Detached states are serialized only by an explicit user save. Native UndoManager
// owns the restored actions; the running Edit and stable node identities stay intact.
class PersistentHistory final : private juce::ValueTree::Listener
{
public:
    struct Archive
    {
        std::vector<std::string> ids;
        std::vector<juce::ValueTree> deltas;
        juce::ValueTree current;
        size_t cursor = 0;
        uint64_t highestID = 0;
        size_t discarded = 0;
    };
    explicit PersistentHistory(Commands&);
    ~PersistentHistory();
    void checkpoint();
    void appended();
    void aborted(); // called after native transaction rollback; retains the prior journal and Redo
    void moved(bool forward);
    void parameterTouched(te::AutomatableParameter&);
    Json status() const;
    juce::ValueTree save();
    static Archive read(juce::ValueTree, const juce::File&); // validates before touching the current Edit
    void install(Archive);

private:
    friend class PerformanceTestAccess;
    struct Action;
    struct Step
    {
        juce::ValueTree delta;
        juce::String data;
        std::string hash;
        std::set<std::string> references;
    };
    struct Block
    {
        juce::var value;
        juce::String data;
    };
    juce::ValueTree capture();
    juce::ValueTree normalise(juce::ValueTree);
    juce::ValueTree collect(bool publish = true);
    void refreshBases();
    void restore(const juce::ValueTree&, bool);
    void applyBases(const juce::ValueTree&, bool changedExternal = false);
    void index(juce::ValueTree);
    void patch(juce::ValueTree, const juce::ValueTree&);
    void mark(juce::ValueTree, bool);
    void notePlugins(juce::ValueTree);
    Step encode(juce::ValueTree);
    void rebuildUndo();
    void trim();
    void pruneBlocks();
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override;
    Commands& owner;
    juce::ValueTree observed, mirror, bases;
    std::vector<Step> steps;
    std::map<std::string, std::pair<history_delta::Path, bool>> dirty;
    std::set<std::string> dirtyPlugins;
    std::map<std::string, juce::ValueTree> baseNodes;
    std::map<std::string, juce::ValueTree> nodes;
    std::map<std::string, Block> blocks;
    size_t discarded = 0;
    bool replaying = false;
    bool journalApplied = false;
    bool saving = false;
};
} // namespace ndaw::v2
