#pragma once

class SpectrumView final : public juce::Component {
public:
    static constexpr int preferredHeight=356;
    SpectrumView(){
        setComponentID("analysis.spectrum");
        for(auto* child:std::initializer_list<juce::Component*>{&bin,&maximum,&band,&readout,&bandReadout})addAndMakeVisible(child);
        bin.setComponentID("analysis.spectrum.bin");bin.setSliderStyle(juce::Slider::LinearHorizontal);bin.setTextBoxStyle(juce::Slider::TextBoxRight,false,68,24);bin.setRange(0,2048,1);bin.setTooltip(text("实际 FFT bin ID；频率由真实采样率与 FFT 大小计算。"));bin.onValueChange=[this]{refresh();};
        maximum.setComponentID("analysis.spectrum.maximum");maximum.onClick=[this]{if(data.value("dominant_bin",Json(nullptr)).is_number())bin.setValue(data["dominant_bin"].get<int>(),juce::sendNotificationSync);};
        band.setComponentID("analysis.spectrum.band");band.onChange=[this]{refresh();};
        readout.setComponentID("analysis.spectrum.readout");bandReadout.setComponentID("analysis.spectrum.band_readout");for(auto* label:{&readout,&bandReadout})label->setFont(juce::FontOptions(12));
    }
    void bind(Json value,bool valid){
        data=std::move(value);current=valid;const auto previous=band.getSelectedId();band.clear(juce::dontSendNotification);
        int i=1;for(const auto& b:data.at("bands"))band.addItem(juce::String(b["lower_hz"].get<double>(),0)+" – "+juce::String(b["upper_hz"].get<double>(),0)+" Hz",i++);
        band.setSelectedId(i>1?std::clamp(previous,1,i-1):0,juce::dontSendNotification);const bool measured=data["status"]=="measured";bin.setEnabled(measured);band.setEnabled(measured);maximum.setEnabled(measured&&!data["dominant_bin"].is_null());refresh();
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(juce::Colour(0xff202730));g.setColour(juce::Colour(0xffdbe3ed));g.setFont(juce::FontOptions(12));
        g.drawText(text("频谱概要 · 声道功率平均 · ")+(current?text("当前证据"):text("历史快照，须重测")),12,2,getWidth()-24,24,juce::Justification::left);
        const auto plot=graph();for(int db:{0,-30,-60,-90,-120}){const auto y=yPosition(db,plot);g.setColour(juce::Colour(0xff38414d));g.drawHorizontalLine(int(y),plot.getX(),plot.getRight());g.setColour(juce::Colour(0xff9baebf));g.drawText(juce::String(db),0,int(y)-7,42,14,juce::Justification::right);}
        if(!data.is_object()||data["status"]!="measured"){g.setColour(juce::Colour(0xffc6d5e5));g.drawText(text("范围不足 4096 帧，没有频谱测量。"),plot.toNearestInt(),juce::Justification::centred);return;}
        const double rate=data["sample_rate"];for(double f:{20.,80.,250.,1000.,2000.,6000.,20000.})if(f<=rate/2){const auto x=xPosition(f,plot);g.setColour(juce::Colour(0xff38414d));g.drawVerticalLine(int(x),plot.getY(),plot.getBottom());g.setColour(juce::Colour(0xff9baebf));g.drawText(juce::String(f,0),int(x)-22,int(plot.getBottom()+3),44,16,juce::Justification::centred);}
        juce::Path path;bool previous=false;const double delta=data["bin_width_hz"];const auto& values=data["bin_power"];
        for(size_t k=0;k<values.size();++k){const double power=values[k];if(power<=0){previous=false;continue;}const float x=xPosition(k*delta,plot),y=yPosition(10*std::log10(power),plot);if(previous)path.lineTo(x,y);else path.startNewSubPath(x,y);previous=true;}
        g.setColour(juce::Colour(0xff56c9b9));g.strokePath(path,juce::PathStrokeType(1.4f));const float x=xPosition(bin.getValue()*delta,plot);g.setColour(juce::Colour(0xffe2c177));g.drawVerticalLine(int(x),plot.getY(),plot.getBottom());
        g.setColour(juce::Colour(0xff9baebf));g.drawText(text("显示 −120…0 dBFS/bin；超界值见读数。DC 在左端；连线仅辅助阅读，不是时间定位。"),12,getHeight()-24,getWidth()-24,20,juce::Justification::left);
    }
    void resized()override{
        bin.setBounds(12,210,std::max(80,getWidth()-204),28);maximum.setBounds(getWidth()-178,210,166,28);readout.setBounds(12,240,getWidth()-24,28);band.setBounds(12,278,194,28);bandReadout.setBounds(216,272,std::max(80,getWidth()-228),48);
    }
    void mouseDown(const juce::MouseEvent& event)override{if(data.is_object()&&data["status"]=="measured"&&graph().contains(event.position)){const auto p=graph();const double frequency=std::exp(std::clamp(double((event.position.x-p.getX())/p.getWidth()),0.,1.)*std::log(data["sample_rate"].get<double>()/2+1))-1;bin.setValue(std::llround(frequency/data["bin_width_hz"].get<double>()),juce::sendNotificationSync);}}
private:
    juce::Rectangle<float> graph()const{return {48.f,32.f,float(std::max(100,getWidth()-64)),148.f};}
    float xPosition(double frequency,juce::Rectangle<float> plot)const{return plot.getX()+float(std::log(frequency+1)/std::log(data["sample_rate"].get<double>()/2+1))*plot.getWidth();}
    static float yPosition(double level,juce::Rectangle<float> plot){return plot.getBottom()-float(std::clamp(level,-120.,0.)+120)/120*plot.getHeight();}
    static juce::String powerText(const Json& power){return power.is_number()?(power.get<double>()>0?juce::String(10*std::log10(power.get<double>()),3)+" dBFS":text("−∞ / 数字静音")):text("证据不足");}
    void refresh(){
        if(!data.is_object()||data["status"]!="measured"){readout.setText(text("不足一个完整 FFT 窗口；未补零或返回假频谱。"),juce::dontSendNotification);bandReadout.setText({},juce::dontSendNotification);repaint();return;}
        const auto k=size_t(bin.getValue());readout.setText("Bin "+juce::String(int(k))+" · "+juce::String(k*data["bin_width_hz"].get<double>(),3)+" Hz · "+powerText(data["bin_power"][k])+" / bin · "+juce::String(data["window_count"].get<int64_t>())+text(" 个实际完整窗"),juce::dontSendNotification);
        const int index=band.getSelectedId()-1;if(index>=0&&index<int(data["bands"].size())){const auto& b=data["bands"][size_t(index)];juce::String detail=powerText(b["power"])+text(" · 占总窗功率 ")+(b["fraction"].is_number()?juce::String(100*b["fraction"].get<double>(),3)+"%":text("静音，无占比"));for(size_t c=0;c<b["channel_power"].size();++c)detail+="\nCh "+juce::String(int(c)+1)+" "+powerText(b["channel_power"][c]);bandReadout.setText(detail,juce::dontSendNotification);}repaint();
    }
    Json data=nullptr;bool current=false;juce::Slider bin;juce::TextButton maximum{text("最高功率频点")};juce::ComboBox band;juce::Label readout,bandReadout;
};
