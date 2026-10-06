// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "NativeDevice.h"
#include <juce_gui_basics/juce_gui_basics.h>
namespace ndaw {
#if defined(__APPLE__)
class NativeAudioConfig final : public juce::Component {
public:
    NativeAudioConfig(Json inventory,int sessionRate,const Json& current,std::function<void(DeviceSetup)> apply)
        :inventory_(std::move(inventory)),rate_(sessionRate),apply_(std::move(apply)) {
        const char* labels[]{"Output device","Input device","Enabled input ports (1,2,...)","Output ports, L/R order (1,2)","Device buffer frames","Session sample rate"};
        for(int i=0;i<6;++i){labels_[i].setText(labels[i],juce::dontSendNotification);addAndMakeVisible(labels_[i]);}
        for(auto* c:std::initializer_list<juce::Component*>{&output_,&input_,&inputs_,&outputs_,&buffer_,&sampleRate_,&applyButton_,&status_})addAndMakeVisible(c);
        input_.addItem("Disabled",1);int outId=0,inId=1;
        for(const auto& d:inventory_) {
            if(!d.contains("uid"))continue;
            if(!d.at("output_channels").empty()){outputIds_.push_back(d.at("uid"));output_.addItem(d.at("name").get<std::string>(),++outId);
                if(d.value("default_output",false))output_.setSelectedId(outId);}
            if(!d.at("input_channels").empty()){inputIds_.push_back(d.at("uid"));input_.addItem(d.at("name").get<std::string>(),++inId);}
        }
        input_.setSelectedId(1);outputs_.setText("1,2");sampleRate_.setText(juce::String(rate_)+" Hz",juce::dontSendNotification);
        if(current.is_object()) {
            for(std::size_t i=0;i<outputIds_.size();++i)if(outputIds_[i]==current.value("output_uid",std::string{}))output_.setSelectedId(static_cast<int>(i+1));
            if(!current.value("physical_inputs",Json::array()).empty())for(std::size_t i=0;i<inputIds_.size();++i)if(inputIds_[i]==current.value("input_uid",std::string{}))input_.setSelectedId(static_cast<int>(i+2));
            inputs_.setText(portText(current.value("physical_inputs",Json::array())));outputs_.setText(portText(current.value("physical_outputs",Json::array({0,1}))));
        }
        output_.onChange=input_.onChange=[this]{refreshCapabilities();};refreshCapabilities();
        if(current.contains("configured_buffer_frames"))buffer_.setSelectedId(current.at("configured_buffer_frames"));
        applyButton_.setButtonText("Apply devices");applyButton_.onClick=[this]{
            try{DeviceSetup s;s.sampleRate=rate_;s.outputUid=selected(output_,outputIds_,1);s.outputs=ports(outputs_.getText());
                if(input_.getSelectedId()>1){s.inputUid=selected(input_,inputIds_,2);s.inputs=ports(inputs_.getText());if(s.inputs.empty())throw Error("device_config","Enable at least one actual input port");}
                s.bufferFrames=buffer_.getSelectedId();if(!s.bufferFrames)throw Error("device_config","Select an available buffer size");
                if(s.outputs.empty() || s.outputs.size()>2)throw Error("device_config","Select one or two physical output ports");
                apply_(s);status_.setText("Applying selection; the main status bar reports the result",juce::dontSendNotification);
            }catch(const std::exception& e){status_.setText(e.what(),juce::dontSendNotification);}
        };
        setSize(700,425);
    }
    void resized() override {
        auto area=getLocalBounds().reduced(20);juce::Component* fields[]{&output_,&input_,&inputs_,&outputs_,&buffer_,&sampleRate_};
        for(int i=0;i<6;++i){auto row=area.removeFromTop(43);labels_[i].setBounds(row.removeFromLeft(270));fields[i]->setBounds(row.reduced(2));}
        status_.setBounds(area.removeFromTop(90));applyButton_.setBounds(area.removeFromTop(34).removeFromRight(180));
    }
private:
    static std::string portText(const Json& ports) {std::string s;for(const auto& p:ports){if(!s.empty())s+=",";s+=std::to_string(p.get<int>()+1);}return s;}
    static std::vector<int> ports(const juce::String& text) {
        std::vector<int> values;for(auto token:juce::StringArray::fromTokens(text,",",{})) {
            token=token.trim();if(token.isEmpty() || !token.containsOnly("0123456789"))throw Error("device_channels","Use comma separated physical port numbers starting at 1");
            const auto value=std::stoll(token.toStdString());if(value<1 || value>65536)throw Error("device_channels","Physical port is out of range");values.push_back(static_cast<int>(value-1));
        }return values;
    }
    static std::string selected(const juce::ComboBox& box,const std::vector<std::string>& ids,int offset) {int i=box.getSelectedId()-offset;if(i<0 || static_cast<std::size_t>(i)>=ids.size())throw Error("device_missing","Select an actual device");return ids[i];}
    const Json* device(const std::string& uid)const {for(const auto& d:inventory_)if(d.value("uid",std::string{})==uid)return &d;return nullptr;}
    void refreshCapabilities() {
        const auto previous=buffer_.getSelectedId();buffer_.clear();inputs_.setEnabled(input_.getSelectedId()>1);
        try{const auto* out=device(selected(output_,outputIds_,1));const auto* in=input_.getSelectedId()>1?device(selected(input_,inputIds_,2)):nullptr;
            if(!out)throw Error("device_missing","No output device");
            for(const auto& n:out->at("buffer_sizes"))if(!in || (n.get<int>()>=in->at("buffer_frame_range")[0].get<double>() && n.get<int>()<=in->at("buffer_frame_range")[1].get<double>()))buffer_.addItem(std::to_string(n.get<int>()),n);
            buffer_.setSelectedId(previous?previous:256);if(!buffer_.getSelectedId() && buffer_.getNumItems())buffer_.setSelectedItemIndex(0);
            std::string text="Output ports: "+std::to_string(out->at("output_channels").size());if(in)text+=" | Input ports: "+std::to_string(in->at("input_channels").size())+"\nDifferent devices use a private aggregate with input drift compensation; physical clock alignment remains unverified.";
            status_.setText(text,juce::dontSendNotification);
        }catch(const std::exception& e){status_.setText(e.what(),juce::dontSendNotification);}
    }
    Json inventory_;int rate_;std::function<void(DeviceSetup)> apply_;std::vector<std::string> outputIds_,inputIds_;
    juce::Label labels_[6],sampleRate_,status_;juce::ComboBox output_,input_,buffer_;juce::TextEditor inputs_,outputs_;juce::TextButton applyButton_;
};
#endif
}
