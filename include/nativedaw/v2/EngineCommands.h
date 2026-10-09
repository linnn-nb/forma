#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <json.hpp>
#include <nativedaw/v2/Scope.h>
#include <nativedaw/v2/PluginScanning.h>
#include <map>
namespace ndaw::v2
{
namespace te = tracktion::engine;
using Json = nlohmann::json;
// L1 owns all mutable Edit access. Callers receive facts, never mutable objects.
class OutputProbe;
class RecordingTestAccess;
class TransportTestAccess;
class AudioDeviceTestAccess;
class PluginEditorWindows;
class NativePluginStates;
class SessionRecovery;
class MasterAnalysis;
class ScrubPlayback;
struct SelectionOutputGateState;
class Commands : private juce::Timer, private te::ParameterChangeHandler::UserChangeListener
{
public:
    explicit Commands(bool openDevice = true, std::unique_ptr<te::PropertyStorage> storage = {});
    ~Commands();
    Json query() const;
    Json querySummary(const std::string& selectedTrack = {}, const std::string& selectedClip = {}) const;
    Json queryObjects(const Json&) const;
    static constexpr size_t maximumQueryPageBytes = 256 * 1024;
    static Json registry();
    static Json processorCatalog();
    static Json panLawCatalog();
    Json refreshPluginInventory(const juce::File& directory = juce::File{});
    Json pluginInventory() const;
    Json makePlan(const std::string& actor, Json operations) const;
    Json preview(const Json&) const;
    Json commit(const Json&, bool accepted = false, const Scope& scope = {});
    Json review(const Json&, const Scope&) const;
    std::string sessionToken() const;
    Json transactionStatus(const std::string&) const;
    // A live receipt is authoritative. Saved markers are untrusted history and
    // never grant permission, restore Undo or authorize replay after reopening.
    Json requestRecovery(const std::string& requestKey) const;
    static constexpr size_t maximumRequestRecords = 4096;
    Json undo(const std::string& expectedPlan = {});
    Json redo();
    Json render(const juce::File&, int64_t start, int64_t end);
    Json timelineRange() const;
    // Local L5 view preferences. Persisted under Edit/NATIVEDAW/UI, outside
    // project transactions, revision and the Agent command registry.
    Json uiState() const;
    Json updateUiState(const Json&, const std::string& expectedSession);
    // Local clipboard stores frozen native state in L1, never caller-supplied XML.
    // Copy itself is outside Edit history; cut/paste use ordinary editing Plans.
    Json prepareClipboard(const Json& clips, const Json& tracks, int64_t start, int64_t end,
                          const std::string& expectedSession, uint64_t expectedRevision);
    void acceptClipboard(const std::string& buffer);
    Json clipboard() const;
    Json exportRequest(bool selection) const;
    Json renderRequest(const juce::File&, const Json&);
    Json save(const juce::File&);
    Json recoveryStatus() const;
    Json recoveryControl(const std::string&, const Json&);
    void open(const juce::File&);
    void play();
    void stop();
    void seek(int64_t);
    // Local audition; optional selection edits use the same human Plan/Undo path.
    Json scrub(const std::string& action, const Json& args = Json::object());
    Json scrubStatus() const;
    Json scrubPreferences() const;
    Json setScrubPreferences(const Json&);
    Json deviceStatus() const;
    Json outputMeters() const;
    Json outputMeterControl(const std::string&, const Json&);
    Json audioDevices(bool rescan = false) const;
    Json audioCapabilities(const Json&) const;
    Json audioDeviceControl(const Json&);
    static std::string inputPermission();
    static void requestInputPermission(std::function<void(bool)>);
    Json configureInput(const std::string& deviceName);
    Json record(const juce::File& directory);
    Json configureMidiDevice(const std::string& direction, const std::string& device, bool enabled);
    Json midiKeyboard(const std::string& track, int pitch, int velocity, bool noteOn);
    Json automationQuery(const std::string& track) const;
    Json automationCurveSamples(const std::string& track, const std::string& parameter, int64_t end,
                                int count = 256) const;
    Json automationCurveRange(const std::string& track, const std::string& parameter, int64_t start, int64_t end,
                              int count = 256) const;
    Json automationControl(const std::string& command, const Json& args);
    Json parameterControl(const std::string& command, const Json& args);
    Json pluginEditorControl(const std::string& command, const Json& args);
    Json pluginEditorQuery() const;
    Json nativeStateControl(const std::string&, const Json&);

    int64_t sampleAtBeat(double) const;
    int64_t sampleAtBarBeat(int bar, double beat) const;
    int64_t snapToGrid(int64_t sample, double division) const;
    int64_t offsetByBeats(int64_t sample, double beats) const;
    Json timelinePosition(int64_t samples) const; // Local read API; no new MCP tool.
    std::string formatRollDuration(int64_t duration, int64_t anchor, bool pre, const std::string& unit, int fps) const;
    int64_t parseRollDuration(const std::string& input, int64_t anchor, bool pre, const std::string& unit,
                              int fps) const;
    Json musicalGrid(int64_t start, int64_t end, double division = 1) const;
    static Json analyse(const juce::File&);
    static std::string mediaHash(const juce::File&);
    Json analysisControl(const std::string&, const Json&, const std::string& actor = "human");
    Json analysisStatus();
    Json legacyReports() const;

private:
    std::shared_ptr<ScrubPlayback> scrubPlayback;
    std::unique_ptr<juce::ThreadPool> scrubDecoder;
    void activateScrub();
    void advanceScrub();
    Json finishScrubSelection(const std::string& reason);
    Json lastScrubStatus = {{"active", false}};
    void stopScrub(const std::string& reason = "stopped");
    friend class MasterAnalysis;
    std::unique_ptr<MasterAnalysis> masterAnalysis;
    static void registerAnalysisCommands(Json&);
    friend class SessionRecovery;
    std::unique_ptr<SessionRecovery> recovery;
    std::pair<juce::ValueTree, Json> recoverySnapshot();
    void restoreRecoveryState(juce::ValueTree, const std::string&, uint64_t);
    void createNewSession(const std::string&, const std::string&, uint64_t);
    void adoptEdit(std::unique_ptr<te::Edit>);
    static void registerRecoveryCommands(Json&);
    friend class NativePluginStates;
    std::unique_ptr<NativePluginStates> nativeStates;
    void captureNativeStates() const;
    void nativeStateOwned(bool);
    void checkThread() const;
    te::AudioTrack* track(const std::string&) const;
    te::Track* domainTrack(const std::string&) const;
    static void registerHierarchyCommands(Json&);
    static void registerMixGroupCommands(Json&);
    Json mixGroupsQuery() const;
    Json validateMixGroupPlan(const Json&) const;
    Json expandMixGroupFlags(const Json&) const;
    void executeMixGroupOperation(const std::string&, const Json&);
    static void registerPanCommands(Json&);
    Json panQuery(te::AudioTrack&) const;
    Json validatePanPlan(const Json&) const;
    void executePanOperation(const std::string&, const Json&);
    Json hierarchyQuery(te::Track&) const;
    Json validateHierarchyPlan(const Json&) const;
    void executeHierarchyOperation(const std::string&, const Json&);
    te::Plugin* processor(const std::string&) const;
    Json externalDescriptor(const std::string&) const;
    bool mayLoadExternal(te::ExternalPlugin&) const;
    void closePluginEditors(bool all = false);
    bool nativePluginEditorOpen(const std::string&) const;
    std::unique_ptr<PluginEditorWindows> pluginEditors;
    void validateExternalRuntime() const;
    void synchroniseExternalParameters(te::ExternalPlugin* only = nullptr);
    bool reconcileExternalPreparation(te::AutomatableParameter&);
    Json externalInventory = Json::object();
    std::map<std::string, double> externalPreparedRates;
    struct ExternalParameterLayout
    {
        std::vector<juce::AudioProcessorParameter*> identities;
        std::string signature;
    };
    std::map<std::string, ExternalParameterLayout> externalParameterLayouts;
    bool externalLayoutChanged(te::ExternalPlugin&);
    static void registerProcessorCommands(Json&);
    Json processorQuery(te::AudioTrack&) const;
    Json processorQuery(te::PluginList&) const;
    static bool commandProcessor(const te::Plugin&);
    Json parameterQuery(te::Plugin&, te::AutomatableParameter&) const;
    Json processorSummary(te::Plugin&) const;
    void validateProcessorOperation(const std::string&, const Json&) const;
    void executeProcessorOperation(const std::string&, const Json&, Json&);
    static void registerRoutingCommands(Json&);
    Json routingQuery(te::AudioTrack&) const;
    Json outputQuery(te::AudioTrack&) const;
    Json sendQuery(te::AudioTrack&, te::AuxSendPlugin&) const;
    void validateRoutingPlan(const Json&, const Json&) const;
    void createAux(te::AudioTrack&, Json&);
    void captureRoutingAssignments();
    void restoreRoutingAssignments();
    void executeRoutingOperation(const std::string&, const Json&, Json&);
    te::AuxSendPlugin* send(const std::string&) const;
    void setParameterValue(te::Plugin&, te::AutomatableParameter&, float,
                           juce::NotificationType = juce::dontSendNotification);
    struct ParameterWriteGuard
    {
        explicit ParameterWriteGuard(Commands& c) : owner(c)
        {
            ++owner.ownedParameterWrites;
            owner.nativeStateOwned(true);
        }
        ~ParameterWriteGuard()
        {
            owner.nativeStateOwned(false);
            --owner.ownedParameterWrites;
        }
        Commands& owner;
    };
    int ownedParameterWrites = 0;
    bool requestParameterChange(te::AutomatableParameter&, float, juce::NotificationType) override;
    bool requestParameterGesture(te::AutomatableParameter&, bool) override;
    static void registerParameterCommands(Json&);
    void beginParameterCapture();
    void finishParameterCapture(bool interrupted = false);
    void endParameterGestures();
    Json parameterCapture = nullptr, lastParameterCapture = nullptr, parameterFailure = nullptr;
    std::map<std::string, te::AutomatableParameter::Ptr> parameterGestures;
    std::string parameterSource = "sdk-parameter";
    bool parameterTransactionStarted = false;
    friend class ParameterTestAccess;
    static void registerTimelineCommands(Json&);
    Json validateTimelinePlan(const Json&) const;
    void executeTimelineOperation(const std::string&, const Json&);
    static void registerMarkerCommands(Json&);
    te::MarkerClip* marker(const std::string&) const;
    Json markerQuery() const;
    Json validateMarkerPlan(const Json&) const;
    void executeMarkerOperation(const std::string&, const Json&, Json&);
    int64_t markerPositionForCurrentTransport() const;
    static void registerMusicCommands(Json&);
    std::string trackType(te::AudioTrack&) const;
    void createMusicTrack(te::AudioTrack&, const std::string&, Json&);
    te::MidiClip* midiClip(const std::string&) const;
    Json musicQuery() const;
    Json midiQuery(te::MidiClip&) const;
    void initialiseMusicIDs(juce::UndoManager* = nullptr);
    Json validateMusicPlan(const Json&) const;
    void executeMusicOperation(const std::string&, const Json&, Json&, std::map<std::string, std::string>&);
    static void registerTransportCommands(Json&);
    Json validateTransportPlan(const Json&) const;
    void executeTransportOperation(const std::string&, const Json&);
    Json transportSettingsQuery() const;
    void restoreTransportSettings();
    bool beginRollPlayback();
    void releaseRollGraph();
    void stopTransport(bool preserveRollBoundary);
    std::shared_ptr<SelectionOutputGateState> rollGate;
    void advanceRollPlayback();
    Json rollPlayback = nullptr;
    uint64_t rollProgressFrames = 0;
    double rollProgressTime = 0;
    static void registerAutomationCommands(Json&);
    Json automationLaneQuery(te::AutomatableParameter&) const;
    Json automationPointQuery(te::AutomatableParameter&, int) const;
    te::AutomatableParameter* automationParameter(const std::string& track, const std::string& parameter) const;
    void validateAutomationPlan(const Json&) const;
    void executeAutomationOperation(const std::string&, const Json&, Json&);
    void initialiseAutomationIDs(juce::UndoManager* = nullptr);
    void beginAutomationCapture();
    void finishAutomationCapture();
    Json capture = nullptr, lastCapture = nullptr;
    std::map<std::string, te::AutomatableParameter::Ptr> gestures;
    std::set<std::string> deferredWriteGestures;
    std::map<std::string, std::pair<juce::ValueTree, float>> touchCurves;
    friend class AudioDeviceTestAccess;
    friend class RecordingTestAccess;
    friend class TransportTestAccess;
    static void registerRecordingCommands(Json&);
    void validateRecordingPlan(const Json&) const;
    void executeRecordingOperation(const std::string&, const Json&);
    Json recordingQuery(te::AudioTrack&) const;
    std::string inputAvailability(const te::InputDevice*) const;
    Json recordingReadiness() const;
    void restoreInputAssignments();
    void finishRecordingCapture(bool unexpected = false);
    static void registerAudioDeviceCommands(Json&);
    bool nativeDeviceManagerStarted = false;
    Json audioConfiguration = nullptr;
    bool audioConfigurationPending() const
    {
        return !audioConfiguration.is_null() && audioConfiguration.value("state", std::string{}) == "preparing";
    }
    void finishAudioConfiguration();
    uint64_t audioDeviceGeneration() const;
    juce::AudioDeviceManager::AudioDeviceSetup audioDesired, audioPrevious;
    double audioConfigurationStarted = 0, audioConfigurationStable = 0;
    uint64_t audioConfigurationGeneration = 0, audioConfigurationFrames = 0;
    Json midiDevices() const;
    void releaseMidiKeys();
    void finishMidiConfiguration();
    Json midiConfiguration = nullptr;
    double midiConfigurationStarted = 0;
    struct HeldKey
    {
        std::shared_ptr<te::MidiInputDevice> device;
        int pitch;
    };
    std::vector<HeldKey> heldMidiKeys;
    struct MidiRecordSettings
    {
        std::shared_ptr<te::MidiInputDevice> device;
        bool merge, replace, expression;
        te::QuantisationType quantisation;
    };
    std::vector<MidiRecordSettings> midiRecordSettings;
    void timerCallback() override;
    Json recordingCapture = nullptr, lastRecording = nullptr;
    static constexpr double recordingStallBudgetMs = 500;
    double recordingLastProgress = 0;
    uint64_t recordingProgressFrames = 0;
    juce::File recordingDirectory;
    std::string recordingError;
    te::WaveAudioClip* audioClip(const std::string&) const;
    struct ClipboardEntry
    {
        juce::ValueTree state;
        Json facts;
    };
    struct ClipboardBuffer
    {
        Json manifest;
        std::map<std::string, ClipboardEntry> entries;
    };
    std::optional<ClipboardBuffer> activeClipboard, stagedClipboard;
    const ClipboardEntry* clipboardEntry(const std::string&) const;
    static void registerClipCommands(Json&);
    Json audioClipQuery(te::WaveAudioClip&) const;
    Json validateClipPlan(const Json&) const;
    void executeClipOperation(const std::string&, const Json&, Json&, std::map<std::string, std::string>&);
    static void registerLegacyCommands(Json&);
    static void registerQueryCommands(Json&);
    Json prepareLegacy(const juce::File&) const;
    Json validateLegacyOperation(const Json&) const;
    void executeLegacyOperation(const Json&, Json&);
    void performTrackGain(te::Track&, float);
    void performTrackFlag(te::Track&, const std::string&, bool);
    Json assessScope(const Json&, const Scope&, const Json&) const;
    void bumpRevision();
    Json requestAudit() const;
    void validateRequestCommit(const Json&) const;
    void storeRequestAudit(const Json&, const std::string&);
    void updateRequestAudit(const std::string& planID, const std::string&);
    te::Engine engine;
    OutputProbe* outputProbe = nullptr;
    std::unique_ptr<te::Edit> edit;
    // The command layer owns transaction boundaries for the entire Edit lifetime.
    std::unique_ptr<te::Edit::UndoTransactionInhibitor> undoBoundaryInhibitor;
    juce::ValueTree metadata;
    std::string sessionID = juce::Uuid().toString().toStdString();
    uint64_t revision = 0;
    struct Receipt
    {
        std::string fingerprint;
        Json result;
    };
    std::map<std::string, Receipt> receipts;
    std::vector<std::string> history;
    size_t historyCursor = 0;
    static constexpr double timelineRate = 48000;
};
} // namespace ndaw::v2
