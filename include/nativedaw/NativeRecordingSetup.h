// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Recording.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

namespace ndaw {
class NativeRecordingSetup final : public juce::Component {
public:
    explicit NativeRecordingSetup(std::function<void(Json)> execute):execute_(std::move(execute)) {
        setSize(560,350);
        mode_.addItem("Normal recording",1);mode_.addItem("Loop recording | new Playlist per take",2);mode_.addItem("Selection Punch | preserve original Playlist",3);
        mode_.onChange=[this]{const bool range=mode_.getSelectedId()!=1,punch=mode_.getSelectedId()==3;begin_.setEnabled(range);end_.setEnabled(range);selection_.setEnabled(range);preRoll_.setEnabled(punch);postRoll_.setEnabled(punch);};
        begin_.setTitle("Record range start in samples");end_.setTitle("Record range end in samples");preRoll_.setTitle("Pre-roll in samples");postRoll_.setTitle("Post-roll in samples");
        for(auto* field:{&begin_,&end_,&preRoll_,&postRoll_})field->setInputRestrictions(18,"0123456789");
        startLabel_.setText("Range start / samples",juce::dontSendNotification);endLabel_.setText("Range end / samples",juce::dontSendNotification);
        preLabel_.setText("Pre-roll / samples",juce::dontSendNotification);postLabel_.setText("Post-roll / samples",juce::dontSendNotification);
        selection_.setButtonText("Use edit selection");selection_.onClick=[this]{begin_.setText(juce::String(selectionBegin_));end_.setText(juce::String(selectionEnd_));};
        apply_.setButtonText("Apply recording setup");apply_.onClick=[this]{try{const bool range=mode_.getSelectedId()!=1,punch=mode_.getSelectedId()==3;
            Json op{{"command","set_record_mode"},{"mode",punch?"punch":range?"loop":"normal"},{"begin",range?begin_.getText().getLargeIntValue():0},{"end",range?end_.getText().getLargeIntValue():0}};
            if(punch){op["pre_roll"]=preRoll_.getText().getLargeIntValue();op["post_roll"]=postRoll_.getText().getLargeIntValue();}execute_(Json::array({op}));
            if(applied)applied();
        }catch(const std::exception& e){note_.setText(juce::String("Not executed: ")+e.what(),juce::dontSendNotification);}};
        for(auto* c:std::vector<juce::Component*>{&mode_,&begin_,&end_,&startLabel_,&endLabel_,&preRoll_,&postRoll_,&preLabel_,&postLabel_,&selection_,&apply_,&note_})addAndMakeVisible(*c);
    }
    void update(const Json& session,Frame begin,Frame end) {
        const auto settings=recordSettings(session);selectionBegin_=begin;selectionEnd_=end;
        begin_.setText(juce::String(settings.loop || settings.punch?settings.begin:begin));end_.setText(juce::String(settings.loop || settings.punch?settings.end:end));
        preRoll_.setText(juce::String(settings.preRoll));postRoll_.setText(juce::String(settings.postRoll));mode_.setSelectedId(settings.punch?3:settings.loop?2:1,juce::sendNotificationSync);
        note_.setText("Loop: at least one second. Punch: continuous capture keeps pre/post-roll handles; only the chosen range enters a new result Playlist. Auto monitoring follows the punch range. Original media and Playlists are retained.",juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff1b2028));}
    void resized() override {
        auto area=getLocalBounds().reduced(18);mode_.setBounds(area.removeFromTop(30));area.removeFromTop(16);
        auto labels=area.removeFromTop(22);startLabel_.setBounds(labels.removeFromLeft(252));endLabel_.setBounds(labels);
        auto fields=area.removeFromTop(30);begin_.setBounds(fields.removeFromLeft(240));fields.removeFromLeft(12);end_.setBounds(fields);
        area.removeFromTop(12);auto rollLabels=area.removeFromTop(22);preLabel_.setBounds(rollLabels.removeFromLeft(252));postLabel_.setBounds(rollLabels);
        auto rolls=area.removeFromTop(30);preRoll_.setBounds(rolls.removeFromLeft(240));rolls.removeFromLeft(12);postRoll_.setBounds(rolls);
        area.removeFromTop(12);auto buttons=area.removeFromTop(30);selection_.setBounds(buttons.removeFromLeft(210));buttons.removeFromLeft(12);apply_.setBounds(buttons);
        area.removeFromTop(12);note_.setBounds(area);
    }
    std::function<void()> applied;
private:
    std::function<void(Json)> execute_;juce::ComboBox mode_;juce::TextEditor begin_,end_,preRoll_,postRoll_;
    juce::Label startLabel_,endLabel_,preLabel_,postLabel_,note_;juce::TextButton selection_,apply_;Frame selectionBegin_=0,selectionEnd_=0;
};
class NativeRecordingWindow final : public juce::DocumentWindow {
public:
    explicit NativeRecordingWindow(std::function<void(Json)> execute):DocumentWindow("Recording setup",juce::Colour(0xff1b2028),closeButton) {
        setUsingNativeTitleBar(true);editor_=new NativeRecordingSetup(std::move(execute));setContentOwned(editor_,true);centreWithSize(560,350);
        editor_->applied=[this]{setVisible(false);};
    }
    NativeRecordingSetup& editor(){return *editor_;}
    void closeButtonPressed() override {setVisible(false);}
private:NativeRecordingSetup* editor_{};
};
}
