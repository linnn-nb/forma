#include "Workspace.h"
namespace ndaw::desktop
{
void Workspace::executeMidiClipboardCommand(int id)
{
    invoke(
        [&]
        {
            if (workspaceSession != commands.sessionToken() ||
                facts["revision"] != commands.querySummary()["revision"] || !pending.is_null())
                throw std::runtime_error(
                    "refresh changed project or resolve pending preview before MIDI clipboard editing");
            if (editing.mode == "shuffle" && id != editCommand::copy)
                throw std::runtime_error("MIDI note clipboard Shuffle is not yet supported; choose Slip or Grid");
            auto target = piano.viewedClip();
            auto targetOwner = selected;
            Json buffer;
            const bool capture = id == editCommand::copy || id == editCommand::cut || id == editCommand::duplicate;
            Json selectedNotes;
            if (capture)
            {
                const auto selected = piano.timingSelection();
                if (selected.is_null() || selected["revision"] != facts["revision"])
                    throw std::runtime_error("select actual editable MIDI notes first");
                selectedNotes = selected["note_ids"];
                buffer =
                    commands.prepareMidiNoteClipboard(target["id"], selectedNotes, workspaceSession, facts["revision"]);
                if (id == editCommand::copy)
                {
                    commands.acceptClipboard(buffer["id"]);
                    message(text("已复制所选 MIDI 音符 · 工程与 Undo 保持"));
                    return;
                }
            }
            else
                buffer = commands.clipboard();
            if (buffer.is_null() || buffer.value("kind", std::string{}) != "midi_notes")
                throw std::runtime_error("MIDI note clipboard is empty");
            if (id == editCommand::pasteOriginal)
            {
                target = nullptr;
                const auto current = commands.query();
                for (const auto& track : current["tracks"])
                    for (const auto& clip : track["clips"])
                        if (clip["id"] == buffer["source_clip"])
                        {
                            target = clip;
                            targetOwner = track["id"].get<std::string>();
                        }
            }
            if (target.is_null())
                throw std::runtime_error("MIDI paste destination or original clip no longer exists");
            Json args{{"clip", target["id"]}};
            const bool cutting = id == editCommand::cut;
            if (cutting)
                args["note_ids"] = selectedNotes;
            else
            {
                args["clipboard"] = buffer["id"];
                args["mode"] = id == editCommand::duplicate ? "merge" : "replace";
                args["placement"] = id == editCommand::duplicate       ? "after"
                                    : id == editCommand::pasteOriginal ? "original"
                                                                       : "cursor";
                args["position_samples"] =
                    args["placement"] == "cursor"
                        ? (selection.range.is_null() ? facts["position_samples"] : selection.range["start_samples"])
                        : Json(0);
            }
            const auto receipt = commands.commit(commands.makePlan(
                "human", Json::array({operation(cutting ? "midi.notes.erase" : "midi.notes.paste", args)})));
            if (receipt.value("state", std::string{}) != "committed")
                throw std::runtime_error("MIDI clipboard edit did not commit");
            if (capture)
                commands.acceptClipboard(buffer["id"]);
            Json ids = Json::array();
            for (const auto& object : receipt["objects"])
                if (object["kind"] == "midi_note")
                    ids.push_back(object["id"]);
            selection.chooseNotes(target, targetOwner, ids);
            selected = targetOwner;
            selectedClip.clear();
            commands.updateUiState({{"object_selection", selection.objects},
                                    {"selection_tracks", selection.tracks},
                                    {"midi_clip", target["id"]}},
                                   workspaceSession);
            midiCommandContext = true;
            message(text(cutting ? "MIDI 音符已剪切 · 一次 Undo · 控制器保持"
                                 : "MIDI 音符已粘贴 · 一次 Undo · 原生音符属性保留"));
        });
}
} // namespace ndaw::desktop
