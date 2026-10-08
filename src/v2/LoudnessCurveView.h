#pragma once

class LoudnessCurveView final : public juce::Component {
public:
    static constexpr int preferredHeight=246;
    LoudnessCurveView(){
        setComponentID("analysis.loudness_curve");
        for(auto* child:std::initializer_list<juce::Component*>{&series,&point,&readout,&locate,&maxM,&maxS})addAndMakeVisible(child);
        series.setComponentID("analysis.loudness.series");series.addItem("LUFS-M / 400 ms",1);series.addItem("LUFS-S / 3 s",2);series.setSelectedId(1,juce::dontSendNotification);
        point.setComponentID("analysis.loudness.point");point.setSliderStyle(juce::Slider::LinearHorizontal);point.setTextBoxStyle(juce::Slider::TextBoxRight,false,66,24);point.setTooltip(text("选择实际测量点；数值是从0开始的回执点ID，窗末尾每100 ms一次。"));
        readout.setComponentID("analysis.loudness.readout");readout.setFont(juce::FontOptions(12));
        locate.setComponentID("analysis.loudness.locate");maxM.setComponentID("analysis.loudness.max_m");maxS.setComponentID("analysis.loudness.max_s");
        point.onValueChange=[this]{refresh();};series.onChange=[this]{refresh();};maxM.onClick=[this]{selectMaximum(1);};maxS.onClick=[this]{selectMaximum(2);};
        locate.onClick=[this]{if(locate.isEnabled()&&onLocate)onLocate(selected());};
    }
    void bind(Json value,bool isCurrent,std::function<bool(const Json&)> allowed,std::function<void(const Json&)> action){
        curve=std::move(value);current=isCurrent;canLocate=std::move(allowed);onLocate=std::move(action);
        const int count=int(curve.at("points").size());point.setRange(0,std::max(1,count-1),1);point.setValue(std::min(int(point.getValue()),std::max(0,count-1)),juce::dontSendNotification);point.setEnabled(count>1);
        maxM.setEnabled(maximum(1)>=0);maxS.setEnabled(maximum(2)>=0);refresh();
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(juce::Colour(0xff202730));g.setColour(juce::Colour(0xffdbe3ed));g.setFont(juce::FontOptions(12));
        g.drawText(text("连续响度 · M 青 / S 黄 · 100 ms网格 · ")+(current?text("当前证据"):text("历史快照，禁止定位")),12,2,getWidth()-24,24,juce::Justification::left);
        const auto plot=graph();
        for(int lu:{0,-14,-35,-70}){const float y=plot.getBottom()-float(lu+70)/70.f*plot.getHeight();g.setColour(juce::Colour(0xff38414d));g.drawHorizontalLine(int(y),plot.getX(),plot.getRight());g.setColour(juce::Colour(0xff9baebf));g.drawText(juce::String(lu),0,int(y)-7,42,14,juce::Justification::right);}
        if(!curve.is_object())return;
        for(int column:{1,2}){
            juce::Path path;bool previous=false;
            for(const auto& row:curve["points"]){if(!row[column].is_number()){previous=false;continue;}
                const float x=plot.getX()+float(row[0].get<double>()/curve["frames"].get<double>())*plot.getWidth();
                const float y=plot.getBottom()-float(std::clamp(row[column].get<double>(),-70.,0.)+70.)/70.f*plot.getHeight();
                if(previous)path.lineTo(x,y);else path.startNewSubPath(x,y);previous=true;
                // An isolated measured point is visible too. Gaps stay gaps;
                // lines between samples are visual guides, never measurements.
                g.setColour((column==1?accent():juce::Colour(0xffecc47b)).withAlpha(current?1.f:.45f));g.fillEllipse(x-1.5f,y-1.5f,3,3);
            }
            g.setColour((column==1?accent():juce::Colour(0xffecc47b)).withAlpha(current?1.f:.45f));g.strokePath(path,juce::PathStrokeType(1.6f));
        }
        if(!curve["points"].empty()){
            const auto& row=curve["points"][size_t(int(point.getValue()))];const float x=plot.getX()+float(row[0].get<double>()/curve["frames"].get<double>())*plot.getWidth();g.setColour(juce::Colours::white.withAlpha(.65f));g.drawVerticalLine(int(x),plot.getY(),plot.getBottom());
        }
        g.setColour(juce::Colour(0xff9baebf));g.drawText(text("显示 −70…0 LUFS；超界数值见选点；null 留空，窗不足与 −∞ 分开；连线只辅助阅读。"),12,preferredHeight-25,getWidth()-24,20,juce::Justification::left);
    }
    void mouseDown(const juce::MouseEvent& e)override{
        if(!curve.is_object()||curve["points"].empty()||!graph().contains(e.position))return;
        const double end=(e.position.x-graph().getX())/graph().getWidth()*curve["frames"].get<double>();int nearest=0;double distance=INFINITY;
        for(size_t i=0;i<curve["points"].size();++i){const double d=std::abs(curve["points"][i][0].get<double>()-end);if(d<distance){distance=d;nearest=int(i);}}
        point.setValue(nearest,juce::sendNotificationSync);
    }
    void resized()override{
        point.setBounds(48,148,std::max(120,getWidth()-298),28);maxM.setBounds(getWidth()-238,148,104,28);maxS.setBounds(getWidth()-126,148,114,28);
        series.setBounds(12,184,140,28);readout.setBounds(162,178,std::max(80,getWidth()-310),42);locate.setBounds(getWidth()-138,184,126,28);
    }
private:
    juce::Rectangle<float> graph()const{return {48.f,32.f,float(std::max(1,getWidth()-64)),108.f};}
    Json selected()const{if(!curve.is_object()||curve["points"].empty())return nullptr;return analysis::loudnessPoint(curve,int64_t(point.getValue()),series.getSelectedId()==1?"momentary":"short_term");}
    int maximum(int column)const{int best=-1;double level=-INFINITY;if(curve.is_object())for(size_t i=0;i<curve["points"].size();++i){const auto& value=curve["points"][i][column];if(value.is_number()&&value.get<double>()>level){best=int(i);level=value;}}return best;}
    void selectMaximum(int column){const int best=maximum(column);if(best<0)return;series.setSelectedId(column,juce::dontSendNotification);point.setValue(best,juce::dontSendNotification);refresh();}
    void refresh(){
        const auto item=selected();bool allowed=false;juce::String description;
        if(item.is_object()){
            const auto status=item["status"].get<std::string>();description=text("点 ")+juce::String(item["point_index"].get<int64_t>())+" · "+(item["lufs"].is_number()?juce::String(item["lufs"].get<double>(),2)+" LUFS":status=="insufficient_window"?text("窗口不足"):text("−∞ LUFS"));
            if(status!="insufficient_window"){
                if(item.contains("source_start_frame"))description+=text("\n源帧 [")+juce::String(item["source_start_frame"].get<int64_t>())+", "+juce::String(item["source_end_frame"].get<int64_t>())+")";
                else description+=text("\n工程 [")+juce::String(item["start_samples"].get<int64_t>()/48000.,6)+", "+juce::String(item["end_samples"].get<int64_t>()/48000.,6)+text(" 秒)");
                allowed=current&&canLocate&&canLocate(item);
            }
        }else description=text("区间未包含完整400 ms测量窗口");
        readout.setText(description,juce::dontSendNotification);locate.setEnabled(allowed);repaint();
    }
    Json curve=nullptr;bool current=false;std::function<bool(const Json&)> canLocate;std::function<void(const Json&)> onLocate;
    juce::ComboBox series;juce::Slider point;juce::Label readout;juce::TextButton locate{text("定位所选窗尾")},maxM{text("最大 M")},maxS{text("最大 S")};
};
