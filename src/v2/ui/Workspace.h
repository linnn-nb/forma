#pragma once
#include "Theme.h"
#include "EditWindow.h"
#include "MixWindow.h"
#include "Inspector.h"
#include "TracksList.h"
#include "ClipsList.h"
#include "GroupsList.h"
#include "MixGroupEditor.h"
#include "TrackCommentsPanel.h"
#include "KeyboardSettings.h"
#include "Toolbar.h"
#include "Transport.h"
#include "Counters.h"
#include "EditingControls.h"
#include "MemoryLocationsPanel.h"
#include "SpotPlacementPanel.h"
#include "MidiDock.h"
#include "WorkspaceDivider.h"
#include "PianoPitchAxis.h"
#include "MusicEventPanel.h"
#include "RollPanel.h"
#include "FadesPanel.h"
#include "ZoomPresets.h"
#include "ZoomToggle.h"
#include "../TimelineState.h"
namespace ndaw::v2
{
class McpTestAccess;
}
namespace ndaw::desktop
{
#include "../RoutingPanel.h"
#include "../RecordingPanel.h"
#include "../AudioDevicePanel.h"
#include "../GroupingPanel.h"
#include "../AutomationPanel.h"
#include "MidiEditor.h"
#include "../ClipPanel.h"
#include "../PluginLibrary.h"
#include "../RecoveryPanel.h"
#include "../NewSessionPanel.h"
#include "../TimelinePanel.h"
#include "../AnalysisPanel.h"
#include "../ExportPanel.h"

class Workspace final : public juce::Component,
                        private juce::Timer,
                        public juce::MenuBarModel,
                        public juce::FileDragAndDropTarget,
                        public juce::ApplicationCommandTarget,
                        private juce::ChangeListener
{
    friend class ndaw::v2::AudioDeviceTestAccess;
    friend class ndaw::v2::McpTestAccess;

public:
    explicit Workspace(bool openDevice = true, std::unique_ptr<te::PropertyStorage> storage = {});
    ~Workspace() override;
    void chooseExport(bool selection);
    void showVerifiedExport(bool selection = false);
    void showAnalysis();
    Json queryAnalysis();
    void showTimelineRange();
    void showMemoryLocations(const std::string& markerID = {});
    void showSpotPlacement(const std::string& clipID);
    void showNewSession();
    Json query() const;
    Json queryAudioDevices() const;
    Json queryRecovery() const;
    void showRecovery();
    void closeAudioSettings();
    void showAudioSettings();

    void showPluginLibrary();
    Json queryPluginEditors() const;
    Json queryPluginLibrary() const;
    bool selectLibraryPlugin(const std::string& id);
    void prepareExternalPlugin(const std::string& descriptor);
    void insertExternalPlugin(const std::string& descriptor);
    Json queryAutomation(const std::string& target) const;
    void openSession(const juce::File& f);
    // Startup receives a fresh Workspace; project files must not enter audio import.
    void openLocalFile(const juce::File& f);
    void prepareImport(const juce::File& f);
    void importAudio(const juce::File& f);
    void importAudioFiles(const juce::Array<juce::File>& files);
    void chooseAudioFiles();
    Json queryLegacyReports() const;
    void prepareLegacyImport(const juce::File& f);
    void showLegacyReport();
    void prepareMidiTransform(const std::string& cmd, Json args, uint64_t revision);
    void prepareTrackDelete(const std::string& id);
    void prepareReverbAux();
    // Local file ingress exercises the production queue; it is not an AI provider.
    Json queryCommandResult() const;
    Json queryCommandPermission() const;
    Json queryMcpStatus() const;
    Json queryCommandQueueStatus() const;
    void startMcp(Permission mode, const juce::File& endpoint = McpGateway::defaultEndpoint());
    void stopMcp();
    void showMcpInfo();

    void setCommandPermission(Permission mode, bool clipOnly = false);
    void importCommandFile(const juce::File& file);
    void cancelCurrentCommand();
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int index, const juce::String&) override;
    void menuItemSelected(int id, int) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int, int) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    juce::ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands(juce::Array<juce::CommandID>&) override;
    void getCommandInfo(juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform(const InvocationInfo&) override;
    Json queryView() const
    {
        return commands.uiState();
    }
    juce::ApplicationCommandManager& uiCommands()
    {
        return commandManager;
    }

private:
    static juce::String historyLabel(const Json& entry);
    juce::PopupMenu rulersMenu();
    void addMenuCommand(juce::PopupMenu&, int);
    juce::PopupMenu trackHeightMenu();
    juce::PopupMenu trackColourMenu();
    juce::PopupMenu zoomPresetMenu();
    void showTrackOptions(const std::string&, juce::Component&, bool);
    void setTrackHeight(const std::string&, int, const std::string&);
    void executePresentationCommand(int);
    Json recordingCommandTargets() const;
    bool canRecordingCommand(int) const;
    void executeRecordingCommand(int);
    void dispatchRecordingCommand(const std::string&, int, bool modifiers = true);
    void showTrackMonitorMenu(const std::string&, juce::Component&);
    Json recordingTargets = nullptr;
    std::string recordingAnchor;
    void executeZoomCommand(int);
    void commitZoomGesture(Json, const std::string&, uint64_t);
    void restoreZoom();
    void executeZoomToggle(int);
    void followZoomToggle();
    void showFades();
    std::unique_ptr<FadesPanel> fadesPanel;
    void showRollSettings();
    std::unique_ptr<RollPanel> rollPanel;
    void showMusicEvent(const std::string&, double, const std::string& = "");
    std::unique_ptr<MusicEventPanel> musicEventPanel;
    void showZoomTogglePreferences();
    std::unique_ptr<ZoomTogglePanel> zoomTogglePanel;
    Json cachedAutomation(const std::string&);
    void setTrackView(const std::string&, const std::string&);
    void executeAutomationViewCommand(int);
    void commitAutomationGesture(Json, uint64_t, const std::string&);
    std::string automationCacheKey;
    Json automationCache = Json::object();
    void showZoomPresetMenu(int, juce::Component&);
    ZoomPresets zoomPresets;
    void initialiseCommandManager();
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void dispatchCommand(int);
    void setView(Json, bool zoomGesture = false);
    bool midiKeyboardFocus() const
    {
        return pianoMode && (midiCommandContext || piano.editorHasFocus());
    }
    void showShortcuts();
    void showTrackComments(const std::string&);
    std::unique_ptr<TrackCommentsPanel> trackCommentsPanel;
    void showMixGroup(const std::string& = {});
    void selectMixGroup(const std::string&);
    void toggleMixGroup(const std::string&, bool);
    std::unique_ptr<MixGroupEditor> mixGroupEditor;
    void focusMixInsert(const std::string&, int, juce::Component&);
    void showMixInsertMenu(const std::string&, juce::Component::SafePointer<juce::Component>, const std::string&,
                           uint64_t, uint64_t, double);
    uint64_t insertMenuRequest = 0;
    void transferShortcuts(bool);
    std::unique_ptr<juce::XmlElement> shortcutSnapshot();
    bool restoreShortcuts(const juce::XmlElement&);
    void resetCommandClient(const Scope& scope);
    void showCommandCard(const Json& card);
    void finishCommandConfirmation(bool accepted);
    static juce::String legacyReportText(const Json& r, bool preview);
    friend class ndaw::v2::RecordingTestAccess;
    ClipWriter clipWriter();
    void selectAudioClip(const std::string& id, bool additive = false);
    void executeEditCommand(int id);
    void executeDeleteCommand();
    juce::String shufflePreviewText(const Json& preview) const;
    void executeClipboardCommand(int id);
    void executeMidiClipboardCommand(int id);
    bool executeMidiTimelineClipboardCommand(int);
    Json automationRangeTargets() const;
    bool executeAutomationClipboardCommand(int id);
    Json clipboardSelection() const;
    void finishClipboardEdit(const Json& receipt);
    Json pendingClipboard = nullptr;
    std::string pendingClipboardPlan;
    void commitTimeSelection(Json range, Json tracks, uint64_t revision, std::string session, int64_t insertion);
    Json selectedEditClips() const;
    Json selectedAudioClip() const;
    Writer writer();
    void message(const juce::String& s);
    void invoke(std::function<void()> f);
    void write(const std::string& cmd, Json args);
    void select(std::string id);
    juce::String trackName(const std::string& id) const;
    Json selectedTrack() const;
    Json selectedProcessor() const;
    void refreshInspector();
    void refresh();
    void syncCommandCards();
    void timerCallback() override;
    void choose(bool writing, std::function<void(const juce::File&)> action,
                const juce::String& filter = "*.wav;*.aiff;*.flac")
    {
        chooser = std::make_unique<juce::FileChooser>(
            writing ? text("保存到新文件（现有文件不会被覆盖）") : text("选择本地文件"), juce::File{}, filter);
        chooser->launchAsync((writing ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode) |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [safe = juce::Component::SafePointer<Workspace>(this), action](const auto& c)
                             {
                                 if (safe && c.getResult() != juce::File{})
                                     action(c.getResult());
                             });
    }
    void chooseRecordingDirectory();
    std::string lastAudioReceipt;
    uint64_t audioSettingsEpoch = 0, lastMeterRequest = 0;
    std::unique_ptr<AudioDevicePanel> audioSettings;
    std::unique_ptr<PluginLibrary> pluginLibrary;
    std::string pluginLibraryTrack, pluginLibrarySession;
    std::unique_ptr<ExportPanel> exportPanel;
    std::unique_ptr<AnalysisPanel> analysisPanel;
    double analysisRefresh = 0;
    std::unique_ptr<TimelinePanel> timelinePanel;
    std::unique_ptr<MemoryLocationsPanel> memoryLocationsPanel;
    std::unique_ptr<SpotPlacementPanel> spotPlacementPanel;
    juce::TextButton rangeButton{text("定位 / 选区…")};
    bool newSessionRequested = false;
    std::unique_ptr<NewSessionPanel> newSessionPanel;
    std::unique_ptr<RecoveryPanel> recoveryPanel;
    std::string workspaceSession;
    juce::Label recoveryIndicator;
    bool programDraft = false;
    int programPublishedIndex = -1;
    std::string programTarget;
    bool updatingTransportControls = false;
    Toolbar toolbar;
    Transport transport;
    Counters counters;
    EditingControls editingControls;
    SelectionModel selection;
    EditingModel editing;
    Theme theme;
    juce::ApplicationCommandManager commandManager;
    std::map<int, std::function<void()>> commandActions;
    std::unique_ptr<KeyboardSettings> keyboardSettings;
    bool loadingKeymap = false;
    std::string lastKeymapSession;
    GroupsList groupsList{[this](const auto& id) { selectMixGroup(id); },
                          [this](const auto& id, bool enabled) { toggleMixGroup(id, enabled); },
                          [this](const auto& id)
                          {
                              if (id.empty())
                                  commandManager.invokeDirectly(149, false);
                              else
                                  showMixGroup(id);
                          }};
    TracksList tracksList{[this](std::string id) { select(id); }};
    ClipsList clipsList{[this](std::string id)
                        { selectAudioClip(id, juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown()); }};
    juce::TextButton zoomIn{"+"}, zoomOut{text("−")}, zoomFit{text("全工程")}, scrollLeft{text("‹")},
        scrollRight{text("›")}, shortcutsButton{text("键位…")};

    Commands commands;
    CommandQueue commandQueue{commands};
    std::unique_ptr<McpGateway> mcp;
    juce::File mcpEndpoint;
    juce::ThreadPool commandFiles{1};
    CommandQueue::Client commandClient;
    Scope commandScope;
    Json lastCommandResult = nullptr;
    std::string pendingConfirmation;
    bool commandFileBusy = false;
    juce::File recordDirectory;
    Json facts = Json::object(), deviceFacts = Json::object(), pending = nullptr;
    std::string selected, selectedClip, lastPluginIDs, lastMusicMap;
    int pluginSelection = 0;
    bool clipFXInspector = false, reportShowing = false, mix = false, recordInspector = false, routingInspector = false,
         groupInspector = false, autoInspector = false, pianoMode = false;
    juce::String sessionName = "Untitled";
    Waveforms waves;
    EditWindow editArea;
    MixWindow mixArea;
    InspectorParameters parameters;
    RoutingPanel routing;
    GroupingPanel grouping;
    RecordingPanel recording;
    AutomationPanel automation;
    ClipPanel clipPanel;
    PianoRoll piano;
    MidiDockDivider midiDivider;
    int midiHeightPreview = -1;
    WorkspaceDivider trackListDivider{false}, inspectorDivider{true};
    int tracksWidthPreview = -1, inspectorWidthPreview = -1;
    int inspectorWidth() const;
    bool midiCommandContext = false;
    juce::Viewport editView, mixView, parameterView, routingView, groupView, autoView, recordView;
    juce::MenuBarComponent menu{this};
    juce::TextButton newTrack{text("新增轨道")}, importButton{text("导入音频")}, openButton{text("打开工程")},
        saveButton{text("另存工程")}, exportButton{text("导出 WAV")}, editButton{"EDIT"}, mixButton{"MIX"},
        returnButton{"|<"}, stopButton{text("停止")}, playButton{text("播放")}, recordButton{text("● 录音")},
        followEditButton{text("自动化↔")}, rollButton{text("预后卷")}, metronomeButton{text("节拍器")},
        loopButton{text("循环")}, markerButton{text("Marker +")}, locationsButton{text("位置…")}, undoButton{"Undo"},
        redoButton{"Redo"}, insertButton{text("插入")}, bypassButton{text("旁通")}, editorButton{text("插件窗口")},
        removeButton{text("移除")}, stateRetryButton{text("重试读取")}, stateRestoreButton{text("还原已知状态")},
        programButton{text("切换 Program")}, acceptButton{text("接受计划")}, rejectButton{text("取消")};
    juce::TextButton insertTab{text("插入 / 参数")}, routingTab{text("I/O / 发送")}, groupTab{text("组织")},
        autoTab{text("自动化")}, recordTab{text("录音")};
    juce::TextButton audioSettingsButton{text("音频设置…")}, commandButton{text("命令 · 预览")},
        pianoButton{text("钢琴卷帘")}, applyMusic{text("应用")};
    juce::TextEditor bpm, programIndex;
    juce::ComboBox trackType, meter, pluginType, pluginChoice, countInMode;
    juce::Label counter, device, status, musicPosition, stateStatus;
    juce::TextEditor previewText;
    juce::TooltipWindow tooltips{this, 650};
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace ndaw::desktop
