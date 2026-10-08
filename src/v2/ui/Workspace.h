#pragma once
#include "Theme.h"
#include "EditWindow.h"
#include "MixWindow.h"
#include "Inspector.h"
#include "TracksList.h"
#include "ClipsList.h"
#include "KeyboardSettings.h"
#include "Toolbar.h"
#include "Transport.h"
#include "Counters.h"
#include "EditingControls.h"
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
#include "../PianoRoll.h"
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
    Json queryAutomation(const std::string& target) const;
    void openSession(const juce::File& f);
    void prepareImport(const juce::File& f);
    void importAudio(const juce::File& f);
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
    void initialiseCommandManager();
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void dispatchCommand(int);
    void setView(Json);
    void showShortcuts();
    void focusMixInsert(const std::string&, int);
    void transferShortcuts(bool);
    void resetCommandClient(const Scope& scope);
    void showCommandCard(const Json& card);
    void finishCommandConfirmation(bool accepted);
    static juce::String legacyReportText(const Json& r, bool preview);
    friend class ndaw::v2::RecordingTestAccess;
    ClipWriter clipWriter();
    void selectAudioClip(const std::string& id, bool additive = false);
    void executeEditCommand(int id);
    void executeClipboardCommand(int id);
    Json clipboardSelection() const;
    void finishClipboardEdit(const Json& receipt);
    Json pendingClipboard = nullptr;
    std::string pendingClipboardPlan;
    void commitTimeSelection(Json range, Json tracks, uint64_t revision);
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
    juce::TextButton rangeButton{text("定位 / 选区…")};
    bool newSessionRequested = false;
    std::unique_ptr<NewSessionPanel> newSessionPanel;
    std::unique_ptr<RecoveryPanel> recoveryPanel;
    std::string workspaceSession;
    juce::Label recoveryIndicator;
    bool programDraft = false;
    std::string programTarget;
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
    TracksList tracksList{[this](std::string id) { select(id); }};
    ClipsList clipsList{[this](std::string id) { selectAudioClip(id); }};
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
    juce::Viewport editView, mixView, parameterView, routingView, groupView, autoView, recordView;
    juce::MenuBarComponent menu{this};
    juce::TextButton newTrack{text("新增轨道")}, importButton{text("导入音频")}, openButton{text("打开工程")},
        saveButton{text("另存工程")}, exportButton{text("导出 WAV")}, editButton{"EDIT"}, mixButton{"MIX"},
        returnButton{"|<"}, stopButton{text("停止")}, playButton{text("播放")}, recordButton{text("● 录音")},
        undoButton{"Undo"}, redoButton{"Redo"}, insertButton{text("插入")}, bypassButton{text("旁通")},
        editorButton{text("插件窗口")}, removeButton{text("移除")}, stateRetryButton{text("重试读取")},
        stateRestoreButton{text("还原已知状态")}, programButton{text("切换 Program")}, acceptButton{text("接受计划")},
        rejectButton{text("取消")};
    juce::TextButton insertTab{text("插入 / 参数")}, routingTab{text("I/O / 发送")}, groupTab{text("组织")},
        autoTab{text("自动化")}, recordTab{text("录音")};
    juce::TextButton audioSettingsButton{text("音频设置…")}, commandButton{text("命令 · 预览")},
        pianoButton{text("钢琴卷帘")}, applyMusic{text("应用")};
    juce::TextEditor bpm, programIndex;
    juce::ComboBox trackType, meter, pluginType, pluginChoice;
    juce::Label counter, device, status, musicPosition, stateStatus;
    juce::TextEditor previewText;
    juce::TooltipWindow tooltips{this, 650};
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace ndaw::desktop
