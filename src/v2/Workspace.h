#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include "CommandFileJob.h"
#include <nativedaw/v2/McpGateway.h>
namespace ndaw::v2 {class McpTestAccess;}

namespace ndaw::desktop {
using namespace ndaw::v2;
inline juce::String text(const char* s) {return juce::String::fromUTF8(s);}
inline juce::String text(const std::string& s) {return juce::String::fromUTF8(s.c_str());}
inline juce::Colour accent() {return juce::Colour(0xff54c7ba);}
inline juce::Colour base() {return juce::Colour(0xff191e25);}
inline Json operation(const std::string& command,Json args) {return {{"command",command},{"args",args}};}
using Writer=std::function<void(const std::string&,Json)>;

class Theme final : public juce::LookAndFeel_V4 {
public:
    Theme() {
        setColour(juce::TextButton::buttonColourId,juce::Colour(0xff303944));
        setColour(juce::TextButton::buttonOnColourId,accent().darker(0.5f));
        setColour(juce::TextButton::textColourOffId,juce::Colour(0xffdbe3ed));
        setColour(juce::TextButton::textColourOnId,juce::Colours::white);
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff252d37));
        setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff45505e));
        setColour(juce::Slider::thumbColourId,accent());
        setColour(juce::Slider::trackColourId,accent().darker(0.6f));
        setColour(juce::Slider::backgroundColourId,juce::Colour(0xff343e4a));
        setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff20262e));
        setColour(juce::Slider::textBoxTextColourId,juce::Colours::white);
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff45505e));
        setColour(juce::Label::textColourId,juce::Colour(0xffdbe3ed));
        setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff20262e));
        setColour(juce::TextEditor::textColourId,juce::Colour(0xffdbe3ed));
        setColour(juce::PopupMenu::backgroundColourId,base());
    }
    juce::Font getTextButtonFont(juce::TextButton&,int height) override {return juce::FontOptions(float(std::min(13,height-4)));}
    juce::Component* getParentComponentForMenuOptions(const juce::PopupMenu::Options& options) override {
        // Keep application menus in their owning window, including remote
        // desktop/accessibility capture. Do not embed third-party editor menus.
        if(auto* parent=options.getParentComponent())return parent;
        if(auto* target=options.getTargetComponent())return target->getTopLevelComponent();
        return nullptr;
    }
};

// Both workspaces use these controls. Every edit is submitted to the same L1 Writer.
inline juce::Colour trackColour(const Json& facts){auto colour=facts.is_object()?facts.value("colour",Json(nullptr)):Json(nullptr);return colour.is_string()?juce::Colour::fromString("ff"+text(colour.get<std::string>()).substring(1)):accent();}
class TrackControls final : public juce::Component {
public:
    TrackControls(std::string id,bool strip,Writer write,std::function<void(std::string)> select)
        : id(std::move(id)),strip(strip),write(std::move(write)),select(std::move(select)) {
        for(auto* b:{&name,&mute,&solo,&safe,&fold})addAndMakeVisible(b);
        addAndMakeVisible(gain);
        name.setComponentID("track.select:"+text(this->id));name.onClick=[this]{this->select(this->id);};
        mute.setComponentID("track.mute:"+text(this->id));solo.setComponentID("track.solo:"+text(this->id));safe.setComponentID("track.solo_safe:"+text(this->id));
        mute.setTooltip(text("静音 · Mute"));solo.setTooltip(text("独听 · Solo"));safe.setTooltip(text("Solo Safe · 其他轨道独听时仍可听；显式静音优先"));
        for(auto* b:{&mute,&solo,&safe})b->setClickingTogglesState(true);
        mute.onClick=[this]{this->write("track.mute",{{"track",this->id},{"enabled",!facts.value("mute",false)}});};
        solo.onClick=[this]{this->write("track.solo",{{"track",this->id},{"enabled",!facts.value("solo",false)}});};
        safe.onClick=[this]{this->write("track.solo_safe",{{"track",this->id},{"enabled",!facts.value("solo_safe",false)}});};
        fold.setComponentID("track.fold:"+text(this->id));fold.onClick=[this]{this->write("track.collapsed",{{"track",this->id},{"enabled",!facts.value("collapsed",false)}});};
        gain.setComponentID("track.gain:"+text(this->id));gain.setRange(-60,6,0.1);gain.setTextValueSuffix(" dB");
        gain.setSliderStyle(strip?juce::Slider::LinearVertical:juce::Slider::LinearHorizontal);
        gain.setTextBoxStyle(strip?juce::Slider::TextBoxBelow:juce::Slider::TextBoxRight,false,72,24);
        gain.onDragStart=[this]{stoppedGesture=false;if(live()){gesture=true;control("begin");}};
        gain.onDragEnd=[this]{if(stoppedGesture){stoppedGesture=false;return;}if(gesture){control("end");gesture=false;}else changeGain();};
        gain.onValueChange=[this]{if(gesture)control("value");else if(!gain.isMouseButtonDown()){if(live()){control("begin");control("value");control("end");}else changeGain();}};
        addAndMakeVisible(pan);addAndMakeVisible(panLaw);
        pan.setComponentID("track.pan:"+text(this->id));pan.setRange(-1,1,.01);pan.setDoubleClickReturnValue(true,0);
        pan.setSliderStyle(strip?juce::Slider::LinearHorizontal:juce::Slider::RotaryHorizontalVerticalDrag);
        pan.setTextBoxStyle(strip?juce::Slider::TextBoxRight:juce::Slider::TextBoxBelow,false,strip?58:76,20);
        pan.textFromValueFunction=[](double p){return std::abs(p)<=.005?juce::String("C"):juce::String(p<0?"L ":"R ")+juce::String(std::round(std::abs(p)*100),0);};
        pan.valueFromTextFunction=[](const juce::String& value){auto v=value.trim().toUpperCase();if(v=="C")return 0.;if(v.startsWith("L"))return -v.substring(1).getDoubleValue()/100.;if(v.startsWith("R"))return v.substring(1).getDoubleValue()/100.;return v.getDoubleValue()/100.;};
        pan.setTooltip(text("Pan / Balance · L100–C–R100；双击居中。音频声道增益，不交换立体声声道，不发送 MIDI CC10。"));
        pan.onDragStart=[this]{panStoppedGesture=false;if(live()){panGesture=true;panControl("begin");}};
        pan.onDragEnd=[this]{if(panStoppedGesture){panStoppedGesture=false;return;}if(panGesture){panControl("end");panGesture=false;}else changePan();};
        pan.onValueChange=[this]{if(panGesture)panControl("value");else if(!pan.isMouseButtonDown()){if(live()){panControl("begin");panControl("value");panControl("end");}else changePan();}};
        panLaw.setComponentID("track.pan_law:"+text(this->id));int lawID=1;for(const auto& law:Commands::panLawCatalog())panLaw.addItem(text(law["label"]),lawID++);
        panLaw.setTooltip(text("Pan Law · 停止时切换。Linear 两端保留声道 +6.02 dB；其他曲线两端为原增益，中心按曲线衰减。"));
        panLaw.onChange=[this]{const int i=panLaw.getSelectedId()-1;const auto laws=Commands::panLawCatalog();if(i>=0&&i<int(laws.size())&&laws[i]["id"]!=facts.value("pan_law_setting",Json(nullptr)))this->write("track.pan_law",{{"track",this->id},{"law",laws[i]["id"]}});};
    }
    void update(const Json& value,bool selected) {
        facts=value;this->selected=selected;name.setButtonText(text(facts["name"].get<std::string>()));
        name.setColour(juce::TextButton::buttonColourId,trackColour(facts).darker(selected?.45f:.75f));
        mute.setToggleState(facts["mute"],juce::dontSendNotification);solo.setToggleState(facts["solo"],juce::dontSendNotification);safe.setToggleState(facts["solo_safe"],juce::dontSendNotification);
        mute.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff965249));solo.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffa38236));
        bool hasGain=facts["capabilities"]["gain"];gain.setVisible(hasGain);gain.setEnabled(hasGain&&!facts.value("recording",false)&&(!facts.value("automation_writing",false)||live())&&!(facts.value("playing",false)&&facts.value("automation_mode",std::string("read"))=="read"&&facts.value("automation_volume_points",0)>0));if(hasGain&&!gesture&&!gain.isMouseButtonDown())gain.setValue(facts["gain_db"].get<double>(),juce::dontSendNotification);
        if(!facts.value("playing",false)&&gesture){gesture=false;stoppedGesture=true;}for(auto* b:{&mute,&solo,&safe})b->setEnabled(!facts.value("automation_writing",false)&&!facts.value("recording",false));
        bool hasPan=facts["capabilities"].value("pan",false);pan.setVisible(hasPan);panLaw.setVisible(strip&&hasPan);
        pan.setEnabled(hasPan&&!facts.value("recording",false)&&(!facts.value("automation_writing",false)||live())&&!(facts.value("playing",false)&&facts.value("automation_mode",std::string("read"))=="read"&&facts.value("automation_pan_points",0)>0));
        if(hasPan&&!panGesture&&!pan.isMouseButtonDown())pan.setValue(facts["pan"].get<double>(),juce::dontSendNotification);
        if(!facts.value("playing",false)&&panGesture){panGesture=false;panStoppedGesture=true;}
        panLaw.setEnabled(hasPan&&!facts.value("playing",false)&&!facts.value("recording",false)&&!facts.value("automation_writing",false));
        if(hasPan){int i=1,selectedLaw=0;for(const auto& law:Commands::panLawCatalog()){if(law["id"]==facts["pan_law_setting"])selectedLaw=i;++i;}panLaw.setSelectedId(selectedLaw,juce::dontSendNotification);}
        fold.setVisible(facts["capabilities"]["group"]);fold.setButtonText(facts["collapsed"].get<bool>()?">":"v");fold.setEnabled(!facts.value("playing",false));resized();repaint();
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(strip?0xff222b35:0xff252e39));g.setColour(trackColour(facts));g.fillRect(0,0,4,getHeight());
        g.setColour(juce::Colour(0xff8394a7));g.setFont(juce::FontOptions(11));
        if(strip) {
            g.drawText(text("INSERTS · 插入"),14,87,getWidth()-28,22,juce::Justification::left);
            int y=115;for(const auto& p:facts.value("plugins",Json::array())) {
                g.setColour(juce::Colour(0xff17202a));g.fillRoundedRectangle(juce::Rectangle<float>(14,float(y),float(getWidth()-28),24),3);
                g.setColour(p["bypassed"].get<bool>()?juce::Colour(0xff738396):juce::Colour(0xffbce5de));
                g.drawText(text(p["name"].get<std::string>()),20,y,getWidth()-40,24,juce::Justification::left);y+=28;
            }
            g.setColour(juce::Colour(0xffa8beca));
            g.drawText(text("OUT · ")+text(facts["output"]["name"].get<std::string>()),14,257,getWidth()-28,22,juce::Justification::left);
            g.drawText(text("SENDS · ")+juce::String(facts["sends"].size()),14,229,getWidth()-28,22,juce::Justification::left);
            if(facts.value("plugins",Json::array()).empty())g.drawText(text("无效果器"),14,115,getWidth()-28,24,juce::Justification::left);
            if(pan.isVisible())g.drawText(text("PAN / BALANCE"),14,288,getWidth()-28,19,juce::Justification::left);
            g.setColour(juce::Colour(0xff8394a7));g.drawText(text(facts.value("type",std::string("audio")))+text(facts.value("audible",false)?" · 可听":" · 已静音 / 非独听"),14,getHeight()-30,getWidth()-28,22,juce::Justification::centred);
        } else g.drawText(text(facts.value("type",std::string("audio")))+"  |  "+juce::String(facts.value("clips",Json::array()).size())+text(" 片段"),12,116,pan.isVisible()?128:getWidth()-24,19,juce::Justification::left);
    }
    void resized() override {
        int inset=strip?12:12+std::min(60,(facts.is_object()?facts.value("depth",0):0)*12);bool group=!facts.is_null()&&facts.contains("capabilities")&&facts["capabilities"]["group"].get<bool>();fold.setBounds(inset,10,22,28);name.setBounds(inset+(group?26:0),10,getWidth()-inset-12-(group?26:0),28);int width=(getWidth()-36)/3;
        mute.setBounds(12,46,width,25);solo.setBounds(18+width,46,width,25);safe.setBounds(24+2*width,46,width,25);
        const bool hasPan=pan.isVisible();const int faderTop=strip&&hasPan?378:292;
        gain.setBounds(strip?22:10,strip?faderTop:82,strip?getWidth()-44:hasPan?getWidth()-112:getWidth()-20,strip?std::max(60,getHeight()-faderTop-44):29);
        pan.setBounds(strip?12:getWidth()-96,strip?310:76,strip?getWidth()-24:84,strip?30:60);panLaw.setBounds(12,346,getWidth()-24,25);
    }
private:
    bool live() const {return facts.value("playing",false)&&facts.value("automation_writing",false)&&facts.value("automation_mode",std::string("read"))!="read";}
    void control(const char* action){Json args={{"track",id},{"parameter",facts.value("type",std::string{})=="vca"?"vca":"volume"}};if(std::string(action)=="value")args["value"]=gain.getValue();write(std::string("automation.gesture.")+action,args);}
    void changeGain() {if(std::abs(gain.getValue()-facts.value("gain_db",0.))>0.001)write("track.gain",{{"track",id},{"db",gain.getValue()}});}
    void panControl(const char* action){Json args={{"track",id},{"parameter","pan"}};if(std::string(action)=="value")args["value"]=pan.getValue();write(std::string("automation.gesture.")+action,args);}
    void changePan(){if(std::abs(pan.getValue()-facts.value("pan",0.))>.0001)write("track.pan",{{"track",id},{"value",pan.getValue()}});}
    std::string id;bool strip,selected=false,gesture=false,stoppedGesture=false,panGesture=false,panStoppedGesture=false;Writer write;std::function<void(std::string)> select;Json facts;
    juce::TextButton name,mute{"M"},solo{"S"},safe{"SAFE"},fold{"v"};juce::Slider gain,pan;juce::ComboBox panLaw;
};

class Waveforms final : private juce::ChangeListener {
public:
    explicit Waveforms(std::function<void()> redraw):redraw(std::move(redraw)){formats.registerBasicFormats();}
    ~Waveforms() override {for(auto& [id,t]:thumbs)t->removeChangeListener(this);}
    void update(const Json& facts) {
        std::set<std::string> live;for(const auto& track:facts["tracks"])for(const auto& clip:track["clips"]) {
            if(clip.contains("path"))live.insert(clip["id"]);
            if(!clip.contains("path"))continue;auto id=clip["id"].get<std::string>();if(thumbs.contains(id))continue;
            auto t=std::make_unique<juce::AudioThumbnail>(512,formats,cache);t->addChangeListener(this);
            t->setSource(new juce::FileInputSource(juce::File(text(clip["path"].get<std::string>()))));thumbs[id]=std::move(t);
        }
        for(auto it=thumbs.begin();it!=thumbs.end();)if(!live.contains(it->first)){it->second->removeChangeListener(this);it=thumbs.erase(it);}else ++it;
    }
    void draw(juce::Graphics& g,const Json& clip,juce::Rectangle<int> area,double seconds) {
        auto id=clip["id"].get<std::string>();if(thumbs.contains(id)){double offset=clip.value("source_offset_seconds",0.),speed=clip.value("speed_ratio",1.),gain=std::pow(10.,clip.value("gain_db",0.)/20);thumbs.at(id)->drawChannels(g,area,offset,offset+seconds*speed,float(gain));}
    }
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override {redraw();}
    std::function<void()> redraw;juce::AudioFormatManager formats;juce::AudioThumbnailCache cache{64};
    std::map<std::string,std::unique_ptr<juce::AudioThumbnail>> thumbs;
};

class EditArea final : public juce::Component {
public:
    EditArea(Writer write,std::function<void(std::string)> select,std::function<void(int64_t)> seek,Waveforms& waves,std::function<void(std::string)> openMidi,
             std::function<void(std::string)> selectClip={},std::function<void(const std::string&,Json,uint64_t)> clipWrite={})
        :write(std::move(write)),select(std::move(select)),seek(std::move(seek)),waves(waves),openMidi(std::move(openMidi)),selectClip(std::move(selectClip)),clipWrite(std::move(clipWrite)){setComponentID("edit.timeline");}
    void update(const Json& value,const std::string& selection,const Json& grid,const std::string& clipSelection={}) {
        facts=value;facts["tracks"]=Json::array();for(const auto& t:value["tracks"])if(!t.value("edit_hidden",false))facts["tracks"].push_back(t);selected=selection;selectedClip=clipSelection;this->grid=grid;std::vector<std::string> ids;for(const auto& t:facts["tracks"])ids.push_back(t["id"]);
        if(ids!=trackIDs){trackIDs=ids;controls.clear();for(auto& id:ids){auto c=std::make_unique<TrackControls>(id,false,write,select);addAndMakeVisible(*c);controls.push_back(std::move(c));}}
        for(size_t i=0;i<controls.size();++i){auto item=facts["tracks"][i];item["playing"]=facts["playing"];item["automation_writing"]=!facts["automation_capture"].is_null();item["recording"]=!facts["recording_capture"].is_null();controls[i]->update(item,trackIDs[i]==selected);}
        std::set<std::string> live;
        for(const auto& t:facts["tracks"])for(const auto& c:t["clips"]){auto id=c["id"].get<std::string>();live.insert(id);if(!headers.contains(id)){auto b=std::make_unique<juce::TextButton>();b->setComponentID("clip.select:"+text(id));b->onClick=[this,id,owner=t["id"].get<std::string>()]{select(owner);if(selectClip)selectClip(id);};b->setTooltip(text("选择片段 · 波形中拖动移动，左右边缘拖动修剪"));addAndMakeVisible(*b);headers[id]=std::move(b);}auto& b=*headers.at(id);b.setButtonText(text(c["name"].get<std::string>())+(c.value("locked",false)?text("  🔒"):text("")));b.setColour(juce::TextButton::buttonColourId,trackColour(t).darker(id==selectedClip?.25f:.6f));}
        for(auto it=headers.begin();it!=headers.end();)if(!live.contains(it->first))it=headers.erase(it);else++it;
        resized();repaint();
    }
    int visibleRows() const{return int(trackIDs.size());}
    double duration() const{return std::max(10.,facts.value("length_samples",int64_t(0))/48000.*1.05);}
    juce::Rectangle<int> clipRect(const Json& c,int row)const{double d=duration(),w=getWidth()-262;return {250+int(c["start_samples"].get<int64_t>()/48000./d*w),44+row*144,std::max(3,int(c["length_samples"].get<int64_t>()/48000./d*w)),118};}
    void paint(juce::Graphics& g)override{
        g.fillAll(base());g.setColour(juce::Colour(0xff29333f));g.fillRect(0,0,getWidth(),32);g.setFont(juce::FontOptions(11));g.setColour(juce::Colour(0xffb9c8d8));g.drawText(text("TRACKS / 轨道"),12,0,210,32,juce::Justification::left);
        for(size_t i=0;i<facts.value("tracks",Json::array()).size();++i){g.setColour(juce::Colour(facts["tracks"][i]["id"]==selected?0xff21313d:0xff1c2530));g.fillRect(250,32+int(i)*144,getWidth()-250,143);}
        for(const auto& line:grid){int x=250+int(line["samples"].get<int64_t>()/48000./duration()*(getWidth()-262));bool bar=line["bar_line"];g.setColour(juce::Colour(bar?0xff536575:0xff303b49));g.drawVerticalLine(x,32,float(getHeight()));if(bar){g.setColour(juce::Colour(0xffacbbcc));g.drawText(juce::String(line["bar"].get<int>())+" |",x+4,0,50,30,juce::Justification::left);}}
        if(auto range=facts.value("time_selection",Json(nullptr));!range.is_null()){const auto left=250+int(range["start_samples"].get<int64_t>()/48000./duration()*(getWidth()-262)),right=250+int(range["end_samples"].get<int64_t>()/48000./duration()*(getWidth()-262));g.setColour(accent().withAlpha(.14f));g.fillRect(left,0,std::max(1,right-left),getHeight());g.setColour(accent());g.drawVerticalLine(left,0,float(getHeight()));g.drawVerticalLine(right,0,float(getHeight()));}
        for(size_t i=0;i<facts.value("tracks",Json::array()).size();++i){int y=32+int(i)*144;const auto& t=facts["tracks"][i];g.setColour(juce::Colour(0xff33404e));g.drawHorizontalLine(y+143,0,float(getWidth()));
            for(const auto& c:t["clips"]){auto rect=clipRect(c,int(i));double start=c["start_samples"].get<int64_t>()/48000.,length=c["length_samples"].get<int64_t>()/48000.;
                g.setColour(trackColour(t).darker(t["audible"].get<bool>()?.45f:.8f));g.fillRoundedRectangle(rect.toFloat(),3);g.setColour(t["audible"].get<bool>()?juce::Colour(0xffa2dcd6):juce::Colour(0xff738995));
                if(c.value("kind",std::string{})=="midi")for(const auto& n:c["notes"]){const double p=(n["position_samples"].get<int64_t>()/48000.-start)/length,l=n["length_samples"].get<int64_t>()/48000./length;g.fillRect(rect.getX()+4+int(p*(rect.getWidth()-8)),rect.getY()+28+int((127-n["pitch"].get<int>())/127.*72),std::max(2,int(l*(rect.getWidth()-8))),3);}
                else {waves.draw(g,c,rect.reduced(0,25),length);g.setColour(juce::Colour(0xffe6e1b2));double in=c.value("fade_in_samples",int64_t(0))/48000./length,out=c.value("fade_out_samples",int64_t(0))/48000./length;drawFade(g,rect,in,c.value("fade_in_curve",std::string("linear")),true);drawFade(g,rect,out,c.value("fade_out_curve",std::string("linear")),false);}
                if(c["id"]==selectedClip){g.setColour(accent());g.drawRoundedRectangle(rect.toFloat(),3,2);g.fillRect(rect.getX(),rect.getY()+25,3,rect.getHeight()-25);g.fillRect(rect.getRight()-3,rect.getY()+25,3,rect.getHeight()-25);}
            }
        }
        if(!drag.is_null()&&dragged){auto preview=drag["clip"];preview["start_samples"]=dragStart;preview["length_samples"]=dragEnd-dragStart;g.setColour(accent().withAlpha(.25f));g.fillRect(clipRect(preview,drag["row"]));}
        int x=250+int(facts.value("position_samples",int64_t(0))/48000./duration()*(getWidth()-262));g.setColour(juce::Colour(0xffedca72));g.drawVerticalLine(x,0,float(getHeight()));
        if(facts.value("tracks",Json::array()).empty()){g.setColour(juce::Colour(0xffb2c3d4));g.setFont(juce::FontOptions(18));g.drawText(text("导入音频，开始制作"),270,90,getWidth()-290,32,juce::Justification::centred);g.setFont(juce::FontOptions(13));g.drawText(text("⌘I 导入 · 空格播放 / 停止 · ⌘Z 撤销"),270,130,getWidth()-290,26,juce::Justification::centred);}
    }
    void resized()override{for(size_t i=0;i<controls.size();++i)controls[i]->setBounds(0,32+int(i)*144,242,143);for(size_t i=0;i<facts.value("tracks",Json::array()).size();++i)for(const auto& c:facts["tracks"][i]["clips"])if(headers.contains(c["id"]))headers.at(c["id"])->setBounds(clipRect(c,int(i)).reduced(3).removeFromTop(20));}
    void mouseDown(const juce::MouseEvent& e)override{
        drag=nullptr;dragged=false;if(e.x<250)return;int row=(e.y-32)/144;auto sample=sampleAt(e.x);
        if(e.y>=32&&row>=0&&row<int(trackIDs.size())){select(trackIDs[row]);for(const auto c:facts["tracks"][row]["clips"])if(clipRect(c,row).contains(e.getPosition())){
            if(selectClip)selectClip(c["id"]);if(c["kind"]=="audio"&&c.value("editable_audio",false)&&!c.value("locked",false)&&!facts.value("playing",false)){
                auto r=clipRect(c,row);drag={{"clip",c},{"revision",facts["revision"]},{"row",row},{"mode",e.x<r.getX()+8?"left":e.x>=r.getRight()-8?"right":"move"}};dragX=e.x;dragStart=c["start_samples"];dragEnd=dragStart+c["length_samples"].get<int64_t>();dragScale=duration()*48000/(getWidth()-262);
            }break;
        }}seek(sample);
    }
    void mouseDrag(const juce::MouseEvent& e)override{
        if(drag.is_null())return;int64_t delta=std::llround((e.x-dragX)*dragScale);const auto& c=drag["clip"];int64_t start=c["start_samples"],length=c["length_samples"],offset=c["source_offset_samples"],source=std::llround(c["source_frames"].get<int64_t>()/c["source_sample_rate"].get<double>()*48000);std::string mode=drag["mode"];
        if(mode=="move"){dragStart=std::max(int64_t(0),start+delta);dragEnd=dragStart+length;}
        else if(mode=="left"){dragStart=std::clamp(start+delta,std::max(int64_t(0),start-offset),start+length-1);dragEnd=start+length;}
        else {dragStart=start;dragEnd=std::clamp(start+length+delta,start+1,start+source-offset);}dragged=std::abs(e.x-dragX)>=3;repaint();
    }
    void mouseUp(const juce::MouseEvent&)override{
        if(drag.is_null())return;auto captured=drag;drag=nullptr;repaint();if(!dragged||!clipWrite)return;std::string mode=captured["mode"];
        if(mode=="move")clipWrite("clip.move",{{"clip",captured["clip"]["id"]},{"position_samples",dragStart}},captured["revision"]);
        else clipWrite("clip.trim",{{"clip",captured["clip"]["id"]},{"start_samples",dragStart},{"end_samples",dragEnd}},captured["revision"]);
    }
    void mouseDoubleClick(const juce::MouseEvent& e)override{if(e.x<250||e.y<32)return;int row=(e.y-32)/144;if(row>=int(trackIDs.size()))return;for(const auto& c:facts["tracks"][row]["clips"])if(c["kind"]=="midi"&&clipRect(c,row).contains(e.getPosition())){openMidi(c["id"]);return;}}
private:
    static void drawFade(juce::Graphics& g,juce::Rectangle<int> r,double fraction,const std::string& type,bool in){if(fraction<=0)return;juce::Path p;for(int i=0;i<=32;++i){double a=i/32.,theta=a*juce::MathConstants<double>::halfPi,gain=type=="convex"?std::sin(theta):type=="concave"?1-std::cos(theta):type=="s_curve"?(1-a)*(1-std::cos(theta))+a*std::sin(theta):a;float x=float(in?r.getX()+a*fraction*r.getWidth():r.getRight()-a*fraction*r.getWidth()),y=float(r.getBottom()-4-gain*(r.getHeight()-29));if(i==0)p.startNewSubPath(x,y);else p.lineTo(x,y);}g.strokePath(p,juce::PathStrokeType(1));}
    int64_t sampleAt(int x)const{return std::llround(std::max(0.,(x-250)/double(getWidth()-262)*duration()*48000));}
    Writer write;std::function<void(std::string)> select;std::function<void(int64_t)> seek;Waveforms& waves;std::function<void(std::string)> openMidi,selectClip;std::function<void(const std::string&,Json,uint64_t)> clipWrite;Json facts=Json::object(),grid=Json::array(),drag=nullptr;std::string selected,selectedClip;
    int dragX=0;int64_t dragStart=0,dragEnd=0;double dragScale=0;bool dragged=false;std::vector<std::string> trackIDs;std::vector<std::unique_ptr<TrackControls>> controls;std::map<std::string,std::unique_ptr<juce::TextButton>> headers;
};

class MixArea final : public juce::Component {
public:
    MixArea(Writer write,std::function<void(std::string)> select):write(std::move(write)),select(std::move(select)){
        setComponentID("mix.channels");reset.setComponentID("mix.output.reset");reset.setButtonText(text("复位峰值 / OVER"));
        reset.setTooltip(text("清除输出峰值保持与超过 0 dBFS 的标记。下一音频回调确认；持续过载会重新亮起。"));
        reset.onClick=[this]{this->write("audio.meters.reset",Json::object());};addAndMakeVisible(reset);
    }
    void update(const Json& facts,const std::string& selected,const Json& device) {
        std::vector<std::string> ids;for(const auto& t:facts["tracks"])ids.push_back(t["id"]);
        if(ids!=trackIDs) {trackIDs=ids;controls.clear();for(auto& id:ids){auto c=std::make_unique<TrackControls>(id,true,write,select);addAndMakeVisible(*c);controls.push_back(std::move(c));}resized();}
        for(size_t i=0;i<controls.size();++i){auto item=facts["tracks"][i];item["playing"]=facts["playing"];item["automation_writing"]=!facts["automation_capture"].is_null();item["recording"]=!facts["recording_capture"].is_null();controls[i]->update(item,trackIDs[i]==selected);}
        meters=device.value("output_meters",Json::object());physical=device.value("active_output_channels",Json::array());
        auto frames=meters.value("frames",uint64_t(0));const auto now=juce::Time::getMillisecondCounterHiRes();
        if(frames!=lastFrames||meters.value("generation",uint64_t(0))!=lastGeneration){lastFrames=frames;lastGeneration=meters.value("generation",uint64_t(0));lastAdvance=now;}
        const auto maxAge=std::max(250.,3000.*device.value("buffer_frames",0)/std::max(1.,device.value("sample_rate",48000.)));
        available=device.value("available",false)&&device.value("driver_running",false)&&meters.value("available",false)&&now-lastAdvance<=maxAge;
        reset.setEnabled(available&&!meters.value("reset_pending",false));repaint();
    }
    void resized()override {for(size_t i=0;i<controls.size();++i)controls[i]->setBounds(int(i)*180+8,8,170,getHeight()-16);reset.setBounds(masterX()+12,93,126,24);}
    void paint(juce::Graphics& g)override {
        g.fillAll(base());const int x=masterX();g.setColour(juce::Colour(0xff23333c));g.fillRect(x,8,150,getHeight()-16);g.setColour(accent());g.fillRect(x,8,150,4);
        g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(13,juce::Font::bold));g.drawText("OUTPUT",x+12,22,126,30,juce::Justification::centred);
        g.setFont(juce::FontOptions(11));g.setColour(juce::Colour(0xffb8cbd7));g.drawText(text("限幅前 · Sample Peak"),x+8,61,134,24,juce::Justification::centred);
        const int top=148,bottom=std::max(top+50,getHeight()-108),height=bottom-top;
        g.setFont(juce::FontOptions(10));for(int d=0;d>=-60;d-=12){g.setColour(juce::Colour(0xff8394a7));g.drawText(juce::String(d),x+9,top+int(-d/60.*height)-8,33,16,juce::Justification::right);}
        const auto channels=meters.value("channels",Json::array());const int count=std::min(2,int(physical.size()));
        for(int i=0;i<2;++i){const int bx=x+57+i*35;g.setColour(juce::Colour(0xff111921));g.fillRect(bx,top,19,height);
            const int index=i<count?physical[i].get<int>():-1;const bool present=available&&index>=0&&index<int(channels.size());
            const auto c=present?channels[index]:Json::object();const double display=c.value("display_peak",0.),hold=c.value("hold",0.);
            g.setColour(c.value("over",false)?juce::Colour(0xffe0756b):accent());int h=levelHeight(display,height);g.fillRect(bx,bottom-h,19,h);
            if(present&&hold>0){g.setColour(juce::Colour(0xffedca72));g.fillRect(bx,bottom-levelHeight(hold,height),19,2);}
            g.setColour(juce::Colour(0xffd0dce5));auto label=index<0?juce::String("—"):physical==Json::array({0,1})?juce::String(i==0?"L":"R"):juce::String(index+1);
            g.drawText(label,bx-3,122,25,22,juce::Justification::centred);g.drawText(present?dbText(hold):juce::String("—"),bx-17,bottom+11,53,20,juce::Justification::centred);
            if(c.value("over",false)){g.setColour(juce::Colour(0xffe0756b));g.drawText("OVER",bx-13,bottom+32,45,20,juce::Justification::centred);}
        }
        g.setColour(juce::Colour(0xffb8cbd7));g.drawText(text("保持 · dBFS"),x+8,bottom+53,134,19,juce::Justification::centred);
        auto state=!available?text("等待真实输出回调"):meters.value("reset_pending",false)?text("复位等待回调确认"):physical.size()>2?text("其余声道可通过查询查看"):text("设备输出 / 非 True Peak");
        g.setFont(juce::FontOptions(10));g.drawText(state,x+5,getHeight()-31,140,18,juce::Justification::centred);
    }
private:
    int masterX()const{return int(controls.size())*180+16;}
    static int levelHeight(double level,int height){return int(std::clamp(((level>0?20*std::log10(level):-100)+60)/60.,0.,1.)*height);}
    static juce::String dbText(double level){return level>0?juce::String(20*std::log10(level),1):text("−∞");}
    Writer write;std::function<void(std::string)> select;std::vector<std::string> trackIDs;std::vector<std::unique_ptr<TrackControls>> controls;
    juce::TextButton reset;Json meters=Json::object(),physical=Json::array();bool available=false;
    uint64_t lastFrames=0,lastGeneration=0;double lastAdvance=0;
};

class ParameterRows final : public juce::Component {
public:
    explicit ParameterRows(Writer writer):write(std::move(writer)){
        for(auto* c:std::initializer_list<juce::Component*>{&previous,&next,&pageLabel})addAndMakeVisible(c);
        previous.setComponentID("plugin.parameters.previous");next.setComponentID("plugin.parameters.next");pageLabel.setComponentID("plugin.parameters.page");
        previous.onClick=[this]{if(page>0){--page;refreshPage();}};next.onClick=[this]{++page;refreshPage();};
    }
    void update(const Json& plugin,bool playing,int width,const std::string& track={},bool writing=false,const std::string& mode="read",bool nativeActive=false) {
        lastPlugin=plugin;lastPlaying=playing;lastWidth=width;lastTrack=track;lastWriting=writing;lastMode=mode;lastNativeActive=nativeActive;
        stopped=!playing;trackID=track;liveWrite=playing&&writing&&mode!="read";
        std::string signature=plugin.is_null()?"":plugin["id"].get<std::string>();
        if(signature!=pluginID){page=0;pluginID=signature;}
        int count=plugin.is_null()?0:int(plugin["parameters"].size());int pages=std::max(1,(count+63)/64);page=std::clamp(page,0,pages-1);
        paged=count>64;auto signatureWithPage=signature+":"+std::to_string(page);
        if(signatureWithPage!=rowSignature) {
            rowSignature=signatureWithPage;rows.clear();
            if(!plugin.is_null()) {
                if(plugin.contains("delay_time_ms"))addRow({{"id","@delay_time"},{"name","Time · ms"},{"minimum",1},{"maximum",2000},{"interval",1}});
                for(int i=page*64;i<std::min(count,(page+1)*64);++i)addRow(plugin["parameters"][i]);
            }
        }
        std::map<std::string,const Json*> values;if(!plugin.is_null())for(const auto& p:plugin["parameters"])values[p["id"]]=&p;
        if(!plugin.is_null())for(auto& row:rows) {
            Json p;if(row->id=="@delay_time")p={{"value",plugin["delay_time_ms"]},{"display",std::to_string(plugin["delay_time_ms"].get<int>())+" ms"}};
            else if(values.contains(row->id))p=*values.at(row->id);
            if(p.is_null())continue;row->value=playing?p.value("current_value",p["value"].get<double>()):p["value"].get<double>();row->display.setText(text(p["display"].get<std::string>()),juce::dontSendNotification);
            if(!row->gesture&&!row->slider.isMouseButtonDown())row->slider.setValue(row->value,juce::dontSendNotification);row->slider.setEnabled(!playing||(liveWrite&&row->id!="@delay_time"));if(row->gesture&&((row->automationGesture&&!playing)||(!row->automationGesture&&!nativeActive))){row->gesture=false;row->stoppedGesture=true;}
        }
        for(auto* c:std::initializer_list<juce::Component*>{&previous,&next,&pageLabel})c->setVisible(paged);
        previous.setEnabled(page>0&&!nativeActive&&!playing);next.setEnabled(page+1<pages&&!nativeActive&&!playing);pageLabel.setText(juce::String(page+1)+"/"+juce::String(pages)+" \u00b7 "+juce::String(count),juce::dontSendNotification);
        setSize(width,std::max(40,int(rows.size())*70+(paged?36:0)));resized();
    }
    void resized() override {previous.setBounds(8,2,66,28);next.setBounds(getWidth()-74,2,66,28);pageLabel.setBounds(80,2,getWidth()-160,28);int y=paged?36:0;for(auto& row:rows){row->name.setBounds(10,y+4,getWidth()-125,22);row->display.setBounds(getWidth()-115,y+4,105,22);row->slider.setBounds(8,y+28,getWidth()-16,30);y+=70;}}
private:
    void refreshPage(){rowSignature.clear();update(lastPlugin,lastPlaying,lastWidth,lastTrack,lastWriting,lastMode,lastNativeActive);}
    struct Row {std::string id;double value=0;bool gesture=false,stoppedGesture=false,automationGesture=false;juce::Label name,display;juce::Slider slider;};
    void control(Row& row,const char* action){if(!row.automationGesture){Json args={{"plugin",pluginID},{"parameter",row.id}};if(std::string(action)=="value")args["value"]=row.slider.getValue();write(std::string("parameter.gesture.")+action,args);return;}Json args={{"track",trackID},{"parameter",pluginID+"::"+row.id}};if(std::string(action)=="value")args["value"]=row.slider.getValue();write(std::string("automation.gesture.")+action,args);}
    void addRow(const Json& p) {
        auto row=std::make_unique<Row>();row->id=p["id"];row->name.setText(text(p["name"].get<std::string>()),juce::dontSendNotification);row->name.setFont(juce::FontOptions(12));
        row->display.setFont(juce::FontOptions(12));row->display.setJustificationType(juce::Justification::right);
        row->slider.setSliderStyle(juce::Slider::LinearHorizontal);row->slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,76,24);
        row->slider.setRange(p["minimum"],p["maximum"],p.value("interval",0.));
        row->slider.setNumDecimalPlacesToDisplay(p["maximum"].get<double>()>1000?0:3);
        double lo=p["minimum"],hi=p["maximum"];if(lo>0 && hi/lo>50)row->slider.setSkewFactorFromMidPoint(std::sqrt(lo*hi));
        row->slider.setTooltip(text("参数 ID: ")+text(row->id)+text(" · 拖动实时调参，一次撤销整个手势；播放时按自动化模式录写"));row->slider.setComponentID("plugin.parameter:"+text(row->id));
        auto* ptr=row.get();auto change=[this,ptr]{if(std::abs(ptr->slider.getValue()-ptr->value)<1e-6)return;
            if(ptr->id=="@delay_time")write("plugin.delay_time",{{"plugin",pluginID},{"ms",std::llround(ptr->slider.getValue())}});
            else write("parameter.gesture.value",{{"plugin",pluginID},{"parameter",ptr->id},{"value",ptr->slider.getValue()}});};
        row->slider.onDragStart=[this,ptr]{ptr->stoppedGesture=false;if((liveWrite||stopped)&&ptr->id!="@delay_time"){ptr->automationGesture=liveWrite;ptr->gesture=true;control(*ptr,"begin");}};
        row->slider.onDragEnd=[this,ptr,change]{if(ptr->stoppedGesture){ptr->stoppedGesture=false;return;}if(ptr->gesture){control(*ptr,"end");ptr->gesture=false;}else change();};
        row->slider.onValueChange=[this,ptr,change]{if(ptr->gesture)control(*ptr,"value");else if(!ptr->slider.isMouseButtonDown()){if(liveWrite&&ptr->id!="@delay_time"){ptr->automationGesture=true;control(*ptr,"begin");control(*ptr,"value");control(*ptr,"end");}else change();}};
        for(auto* c:std::initializer_list<juce::Component*>{&row->name,&row->display,&row->slider})addAndMakeVisible(c);rows.push_back(std::move(row));
    }
    Writer write;std::string pluginID,trackID,rowSignature;bool liveWrite=false,stopped=true,paged=false;int page=0;
    Json lastPlugin=nullptr;bool lastPlaying=false,lastWriting=false,lastNativeActive=false;int lastWidth=300;std::string lastTrack,lastMode;
    juce::TextButton previous{text("\u4e0a\u4e00\u9875")},next{text("\u4e0b\u4e00\u9875")};juce::Label pageLabel;std::vector<std::unique_ptr<Row>> rows;
};

#include "RoutingPanel.h"
#include "RecordingPanel.h"
#include "AudioDevicePanel.h"
#include "GroupingPanel.h"
#include "AutomationPanel.h"
#include "PianoRoll.h"
#include "ClipPanel.h"
#include "PluginLibrary.h"
#include "RecoveryPanel.h"
#include "NewSessionPanel.h"
#include "TimelinePanel.h"

class Workspace final : public juce::Component,private juce::Timer,public juce::MenuBarModel,public juce::FileDragAndDropTarget {
    friend class ndaw::v2::AudioDeviceTestAccess;
    friend class ndaw::v2::McpTestAccess;
public:
    explicit Workspace(bool openDevice=true,std::unique_ptr<te::PropertyStorage> storage={}):commands(openDevice,std::move(storage)),waves([this]{editArea.repaint();}),
        editArea(writer(),[this](auto id){select(id);},[this](auto sample){invoke([&]{commands.seek(sample);});},waves,[this](auto id){piano.showClip(id);pianoMode=true;mix=false;refresh();},[this](auto id){selectAudioClip(id);},clipWriter()),
        mixArea(writer(),[this](auto id){select(id);}),parameters(writer()),routing(writer(),[this]{prepareReverbAux();}),grouping(writer(),[this](const auto& id){prepareTrackDelete(id);}),recording(writer(),[this](auto name){message(text("正在核验 macOS 麦克风权限；若出现系统提示，请选择是否允许"));Commands::requestInputPermission([safe=juce::Component::SafePointer<Workspace>(this),name](bool granted){if(!safe)return;safe->invoke([&]{if(!granted)throw std::runtime_error("microphone permission denied; audio input is unavailable");auto receipt=safe->commands.configureInput(name);safe->message(receipt["state"]=="verified"?text("实际音频输入已核验 · 监听默认关闭"):receipt["state"]=="preparing"?text("正在准备实际音频输入 · 等待回调回执"):text("音频输入配置失败 · 查看设备回执"));});});},[this]{chooseRecordingDirectory();},[this](auto direction,auto device,bool enabled){invoke([&]{commands.configureMidiDevice(direction,device,enabled);message(text("正在配置 MIDI 端口 · 等待原生设备回执"));});},[this](auto track,int pitch,int velocity,bool on){invoke([&]{commands.midiKeyboard(track,pitch,velocity,on);});}),automation([this](const auto& cmd,Json args,uint64_t revision){invoke([&]{auto plan=commands.makePlan("human",Json::array({operation(cmd,args)}));plan["base_revision"]=revision;commands.commit(plan);message(text("自动化编辑已提交 · 可撤销"));});},writer(),[this](const auto& track,const auto& parameter,int64_t end){return commands.automationCurveSamples(track,parameter,end);}),
        clipPanel(clipWriter()),piano([this](const auto& cmd,Json args,uint64_t revision){invoke([&]{auto plan=commands.makePlan("human",Json::array({operation(cmd,args)}));plan["base_revision"]=revision;commands.commit(plan);message(text("MIDI 编辑已提交 · 可撤销"));});},
            [this](double beat){return commands.sampleAtBeat(beat);},[this](int64_t start,int64_t end,double snap){return commands.musicalGrid(start,end,snap);},
            [this](const auto& cmd,Json args,uint64_t revision){prepareMidiTransform(cmd,args,revision);}) {
        commandClient=commandQueue.connect("agent:command-file",commandScope);
        commandButton.setComponentID("command.menu");commandButton.onClick=[this]{getMenuForIndex(3,{}).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(commandButton).withParentComponent(this),[safe=juce::Component::SafePointer<Workspace>(this)](int id){if(safe&&id)safe->menuItemSelected(id,3);});};
        setLookAndFeel(&theme);setWantsKeyboardFocus(true);clipPanel.onClose=[this]{selectedClip.clear();refresh();};clipPanel.onError=[this](auto error){message(text("未执行：")+text(error));};
        piano.onError=[this](const auto& error){message(text("未执行：")+text(error));};
        for(auto* c:std::initializer_list<juce::Component*>{&menu,&editView,&mixView,&newTrack,&importButton,&openButton,&saveButton,&exportButton,&editButton,&mixButton,&returnButton,&stopButton,&playButton,&undoButton,&redoButton,&counter,&device,&status,&pluginType,&insertButton,&pluginChoice,&bypassButton,&editorButton,&removeButton,&stateStatus,&stateRetryButton,&stateRestoreButton,&programIndex,&programButton,&parameterView,&previewText,&acceptButton,&rejectButton,&routingView,&insertTab,&routingTab,&groupTab,&groupView,&autoTab,&autoView,&recordView,&recordTab,&recordButton,&piano,&pianoButton,&trackType,&bpm,&meter,&applyMusic,&musicPosition,&clipPanel,&commandButton,&audioSettingsButton,&rangeButton})addAndMakeVisible(c);
        editView.setViewedComponent(&editArea,false);mixView.setViewedComponent(&mixArea,false);parameterView.setViewedComponent(&parameters,false);routingView.setViewedComponent(&routing,false);routingView.setScrollBarsShown(true,false);groupView.setViewedComponent(&grouping,false);groupView.setScrollBarsShown(true,false);autoView.setViewedComponent(&automation,false);autoView.setScrollBarsShown(true,false);recordView.setViewedComponent(&recording,false);recordView.setScrollBarsShown(true,false);
        editView.setScrollBarsShown(true,false);mixView.setScrollBarsShown(false,true);parameterView.setScrollBarsShown(true,false);
        for(const auto& p:Commands::processorCatalog())pluginType.addItem(text(p["name"].get<std::string>()),pluginType.getNumItems()+1);pluginType.setSelectedId(1,juce::dontSendNotification);
        counter.setFont(juce::FontOptions(25,juce::Font::bold));device.setFont(juce::FontOptions(11));status.setFont(juce::FontOptions(12));
        previewText.setComponentID("legacy.report");previewText.setMultiLine(true);previewText.setReadOnly(true);previewText.setFont(juce::FontOptions(13));previewText.setText(text("所有修改通过统一命令层提交。"));
        trackType.addItem(text("音频"),1);trackType.addItem("MIDI",2);trackType.addItem(text("乐器"),3);trackType.addItem("Aux",4);trackType.addItem("Folder",5);trackType.addItem("VCA",6);trackType.setSelectedId(1,juce::dontSendNotification);trackType.setComponentID("track.type");newTrack.setComponentID("track.create");
        newTrack.onClick=[this]{invoke([&]{const std::string type=trackType.getSelectedId()==2?"midi":trackType.getSelectedId()==3?"instrument":trackType.getSelectedId()==4?"aux":trackType.getSelectedId()==5?"folder":trackType.getSelectedId()==6?"vca":"audio";
            auto ops=Json::array({operation("track.create",{{"name",type+" "+std::to_string(facts["tracks"].size()+1)},{"type",type},{"ref","$new"}})});
            if(type=="midi"||type=="instrument"){ops.push_back(operation("midi.clip.create",{{"track","$new"},{"ref","$midi"},{"name","MIDI 01"},{"position_samples",0},{"length_samples",commands.sampleAtBeat(facts["music"]["meters"][0]["numerator"].get<int>()*4.)}}));if(type=="instrument")ops.push_back(operation("track.gain",{{"track","$new"},{"db",-12}}));pianoMode=true;mix=false;}
            if(type=="folder"||type=="vca"){groupInspector=true;routingInspector=false;autoInspector=false;recordInspector=false;pianoMode=false;}
            commands.commit(commands.makePlan("human",ops));selected.clear();message(type=="midi"?text("已建 MIDI 轨 · 未加载乐器，不会发声；可插入 FourOsc"):type=="instrument"?text("已建 FourOsc 乐器轨 · 空白四小节 MIDI 片段 · 增益 −12 dB"):text("已新增 ")+text(type)+text(" 轨道 · 可撤销"));});};
        bpm.setComponentID("music.bpm");bpm.setInputRestrictions(7,"0123456789.");bpm.setTooltip(text("起始 Tempo，20–300 BPM；停止后应用"));meter.setComponentID("music.meter");
        for(const auto& label:{"4/4","3/4","6/8","3/8","5/4","7/8"})meter.addItem(label,meter.getNumItems()+1);meter.setSelectedId(1,juce::dontSendNotification);bpm.setText("120",false);applyMusic.setComponentID("music.apply");
        applyMusic.onClick=[this]{invoke([&]{const auto parts=juce::StringArray::fromTokens(meter.getText(),"/","");if(parts.size()!=2)throw std::runtime_error("invalid meter input");commands.commit(commands.makePlan("human",Json::array({operation("tempo.set",{{"position_samples",0},{"bpm",std::stod(bpm.getText().toStdString())}}),operation("meter.set",{{"position_samples",0},{"numerator",parts[0].getIntValue()},{"denominator",parts[1].getIntValue()}})})));message(text("起始 Tempo / 拍号已提交 · 音符按节拍跟随，导入音频按采样保持"));});};
        importButton.onClick=[this]{choose(false,[this](const auto& f){prepareImport(f);});};
        openButton.onClick=[this]{choose(false,[this](const auto& f){openSession(f);},"*.tracktionedit;*.ndaw");};
        saveButton.onClick=[this]{choose(true,[this](const auto& f){invoke([&]{commands.save(f);sessionName=f.getFileName();message(text("已另存工程 · ")+sessionName);});},"*.tracktionedit");};
        exportButton.onClick=[this]{chooseExport(false);};
        rangeButton.setComponentID("timeline.range.open");rangeButton.onClick=[this]{showTimelineRange();};
        recordButton.setComponentID("transport.record");recordTab.setComponentID("inspector.recording");recordTab.onClick=[this]{recordInspector=true;autoInspector=false;groupInspector=false;routingInspector=false;refresh();};recordButton.onClick=[this]{invoke([&]{commands.record(recordDirectory);message(text("正在原生录音 · 音频写盘 / MIDI 事件捕获"));});};
        playButton.onClick=[this]{invoke([&]{commands.play();});};stopButton.onClick=[this]{invoke([&]{
            bool wasRecording=!commands.query()["recording_capture"].is_null();commands.stop();auto q=commands.query();
            if(wasRecording&&!q["last_recording"].is_null()){
                const auto& receipt=q["last_recording"];const auto state=receipt["state"].get<std::string>();
                if(state=="failed")message(text("录音失败：")+text(receipt["error"].get<std::string>()));
                else if(state=="no_events")message(text("未收到 MIDI 事件 · 未创建片段或历史"));
                else if(state=="committed")message(receipt["files"].empty()?text("MIDI 录音已停止 · 事件已校验 · 可整段撤销"):text("录音已停止 · 文件与片段已校验 · 可整段撤销"));
                else message(text("录音已停止 · 请查看录音回执"));
            }
        });};returnButton.onClick=[this]{invoke([&]{commands.seek(0);});};
        undoButton.onClick=[this]{invoke([&]{commands.undo();if(reportShowing&&pendingConfirmation.empty()){if(commands.legacyReports().empty())reportShowing=false;else showLegacyReport();}message(text("已撤销上一项事务"));});};redoButton.onClick=[this]{invoke([&]{commands.redo();if(reportShowing&&pendingConfirmation.empty())showLegacyReport();message(text("已重做上一项事务"));});};
        editButton.onClick=[this]{pianoMode=false;mix=false;resized();refresh();};mixButton.onClick=[this]{pianoMode=false;mix=true;resized();refresh();};pianoButton.onClick=[this]{pianoMode=true;mix=false;refresh();};pianoButton.setComponentID("view.piano");
        insertTab.onClick=[this]{recordInspector=false;routingInspector=false;groupInspector=false;autoInspector=false;refresh();};routingTab.onClick=[this]{recordInspector=false;routingInspector=true;groupInspector=false;autoInspector=false;refresh();};groupTab.onClick=[this]{recordInspector=false;routingInspector=false;groupInspector=true;autoInspector=false;refresh();};groupTab.setComponentID("inspector.group");autoTab.setComponentID("inspector.automation");autoTab.onClick=[this]{recordInspector=false;autoInspector=true;groupInspector=false;routingInspector=false;refresh();};
        acceptButton.onClick=[this]{invoke([&]{if(pending.is_null())return;if(!pendingConfirmation.empty()){finishCommandConfirmation(true);return;}bool legacy=pending["operations"][0]["command"]=="session.import_legacy",musical=pending["operations"][0]["command"].get<std::string>().starts_with("midi.notes.");bool external=pending["operations"][0]["command"]=="plugin.external.insert";commands.commit(pending,true);if(external){auto q=commands.query();for(const auto& t:q["tracks"])if(t["id"]==selected)pluginSelection=int(t["plugins"].size())-1;}pending=nullptr;if(!musical&&!external)selected.clear();if(legacy)showLegacyReport();message(legacy?text("旧工程已导入 · 缺失项见报告 · 一次 Undo 撤销导入"):text("计划已提交 · 一次 Undo 整体撤销"));});};
        rejectButton.onClick=[this]{if(!pendingConfirmation.empty()){invoke([&]{finishCommandConfirmation(false);});return;}bool preview=!pending.is_null();reportShowing=false;pending=nullptr;message(preview?text("已取消预览，工程未修改"):text("已关闭导入报告"));refresh();};
        insertButton.onClick=[this]{if(selected.empty())return;auto type=Commands::processorCatalog()[pluginType.getSelectedId()-1]["type"];write("plugin.insert",{{"track",selected},{"type",type}});};
        pluginChoice.onChange=[this]{pluginSelection=pluginChoice.getSelectedId()-1;refreshInspector();};
        bypassButton.onClick=[this]{auto p=selectedProcessor();if(!p.is_null())write("plugin.bypass",{{"plugin",p["id"]},{"bypassed",!p["bypassed"].get<bool>()}});};
        stateStatus.setComponentID("plugin.state.status");stateStatus.setFont(juce::FontOptions(11));
        stateRetryButton.setComponentID("plugin.state.retry");stateRestoreButton.setComponentID("plugin.state.restore_checkpoint");
        stateRetryButton.onClick=[this]{auto p=selectedProcessor();if(!p.is_null())write("plugin.state.retry",{{"plugin",p["id"]}});};
        stateRestoreButton.onClick=[this]{auto p=selectedProcessor();if(!p.is_null())write("plugin.state.restore_checkpoint",{{"plugin",p["id"]}});};
        programIndex.setComponentID("plugin.program.index");programIndex.setInputRestrictions(6,"0123456789");programIndex.setTooltip(text("插件实际 SDK Program 索引，从 0 开始；不是插件私有预设浏览器。"));
        programIndex.onTextChange=[this]{programDraft=true;};programButton.setComponentID("plugin.program");programButton.onClick=[this]{auto p=selectedProcessor();if(!p.is_null()){write("plugin.program",{{"plugin",p["id"]},{"index",programIndex.getText().getIntValue()}});programDraft=false;}};
        stateRestoreButton.setTooltip(text("还原最后已知插件状态；将丢弃无法捕获的变化，这项恢复不可撤销。"));
        editorButton.setComponentID("plugin.editor");editorButton.onClick=[this]{auto p=selectedProcessor();if(p.is_null())return;const std::string id=p["id"];bool open=false;for(const auto& e:commands.pluginEditorQuery())if(e["plugin"]==id)open=true;write(open?"plugin.editor.close":"plugin.editor.open",{{"plugin",id}});};
        removeButton.onClick=[this]{auto p=selectedProcessor();if(!p.is_null())write("plugin.remove",{{"plugin",p["id"]}});};
        for(auto pair:std::initializer_list<std::pair<juce::TextButton*,const char*>>{{&playButton,"transport.play"},{&stopButton,"transport.stop"},{&undoButton,"history.undo"},{&redoButton,"history.redo"},{&editButton,"view.edit"},{&mixButton,"view.mix"},{&insertButton,"plugin.insert"},{&acceptButton,"plan.accept"},{&rejectButton,"plan.reject"},{&insertTab,"inspector.inserts"},{&routingTab,"inspector.routing"}})pair.first->setComponentID(pair.second);
        bypassButton.setComponentID("plugin.bypass");removeButton.setComponentID("plugin.remove");status.setComponentID("workspace.status");
        pluginType.setComponentID("plugin.type");pluginChoice.setComponentID("plugin.choice");
        audioSettingsButton.setComponentID("audio.settings.open");audioSettingsButton.onClick=[this]{showAudioSettings();};
        workspaceSession=commands.sessionToken();if(openDevice)commands.recoveryControl("session.recovery.start",Json::object());
        recoveryIndicator.setComponentID("recovery.status");recoveryIndicator.setFont(juce::FontOptions(11));addAndMakeVisible(recoveryIndicator);
        setSize(1440,880);refresh();startTimerHz(20);
    }
    ~Workspace() override {stopTimer();timelinePanel.reset();newSessionPanel.reset();recoveryPanel.reset();audioSettings.reset();pluginLibrary.reset();mcp.reset();commandQueue.shutdown();commandFiles.removeAllJobs(true,2000);commands.stop();setLookAndFeel(nullptr);}
    void chooseExport(bool selection){invoke([&]{const auto request=commands.exportRequest(selection);choose(true,[this,request](const juce::File& f){invoke([&]{auto r=commands.renderRequest(f,request);message(text("已生成并校验 WAV · ")+juce::String(r["frames"].get<int64_t>())+text(" 帧 · ")+(r["lufs_i"].is_number()?juce::String(r["lufs_i"].get<double>(),2):text("静音"))+" LUFS-I");});},"*.wav");});}
    void showTimelineRange(){invoke([&]{
        if(!timelinePanel){timelinePanel=std::make_unique<TimelinePanel>([this](const auto& command,const auto& args,const auto& binding){
            auto snapshot=commands.query();if(binding["session_token"]!=commands.sessionToken()||binding["base_revision"]!=snapshot["revision"])throw std::runtime_error("project changed; read current version before applying");
            if(command=="seek")commands.seek(args.at("position_samples"));else{auto plan=commands.makePlan("human",Json::array({operation(command,args)}));plan["session_token"]=binding["session_token"];plan["base_revision"]=binding["base_revision"];commands.commit(plan);}
            refresh();return commands.query();},[this]{return commands.query();},[this]{timelinePanel->setVisible(false);grabKeyboardFocus();},[this]{chooseExport(true);});addChildComponent(*timelinePanel);}
        timelinePanel->bind(commands.query());timelinePanel->setBounds(getLocalBounds());timelinePanel->setVisible(true);timelinePanel->toFront(true);
    });}
    void showNewSession(){invoke([&]{
        commands.recoveryControl("session.recovery.start",Json::object());
        if(!newSessionPanel){newSessionPanel=std::make_unique<NewSessionPanel>(
            [this](const std::string& id,const Json& args){auto result=commands.recoveryControl(id,args);if(id=="session.new"){
                newSessionRequested=true;if(mcp)startMcp(Permission::ReadOnly,mcpEndpoint);Scope readOnly;readOnly.mode=Permission::ReadOnly;resetCommandClient(readOnly);pending=nullptr;pendingConfirmation.clear();reportShowing=false;
            }return result;},
            [this]{newSessionRequested=false;newSessionPanel->setVisible(false);grabKeyboardFocus();},[this]{saveButton.triggerClick();});addChildComponent(*newSessionPanel);}
        newSessionRequested=false;newSessionPanel->bind(commands.query(),commands.recoveryStatus());newSessionPanel->setVisible(true);newSessionPanel->setBounds(getLocalBounds());newSessionPanel->toFront(true);
    });}
    Json query() const {return commands.query();}
    Json queryAudioDevices()const{return commands.audioDevices();}
    Json queryRecovery()const{return commands.recoveryStatus();}
    void showRecovery(){invoke([&]{commands.recoveryControl("session.recovery.start",Json::object());if(!recoveryPanel){recoveryPanel=std::make_unique<RecoveryPanel>([this](const std::string& id,const Json& args){auto result=commands.recoveryControl(id,args);if(id=="session.recovery.restore"){if(mcp)startMcp(Permission::ReadOnly,mcpEndpoint);Scope readOnly;readOnly.mode=Permission::ReadOnly;resetCommandClient(readOnly);pending=nullptr;pendingConfirmation.clear();reportShowing=false;}return result;},[this]{recoveryPanel->setVisible(false);grabKeyboardFocus();});addChildComponent(*recoveryPanel);}recoveryPanel->update(commands.recoveryStatus());recoveryPanel->setVisible(true);recoveryPanel->setBounds(getLocalBounds());recoveryPanel->toFront(true);});}
    void closeAudioSettings(){++audioSettingsEpoch;if(audioSettings)audioSettings->setVisible(false);grabKeyboardFocus();}
    void showAudioSettings(){
        invoke([&]{++audioSettingsEpoch;if(!audioSettings){audioSettings=std::make_unique<AudioDevicePanel>([this](bool scan){return commands.audioDevices(scan);},[this](const Json& args){return commands.audioCapabilities(args);},
            [this](Json args){const auto epoch=audioSettingsEpoch;auto apply=[safe=juce::Component::SafePointer<Workspace>(this),args,epoch]{if(safe&&safe->audioSettingsEpoch==epoch&&safe->audioSettings->isVisible())safe->invoke([&]{try{safe->audioSettings->setBusy(false);auto receipt=safe->commands.audioDeviceControl(args);safe->audioSettings->showReceipt(receipt);safe->message(receipt["state"]=="verified"?text("实际音频设备设置已核验"):receipt["state"]=="preparing"?text("实际设备正在准备 · 等待音频回调回执"):text("音频设备配置未完成 · 查看实际回执"));}catch(const std::exception& e){safe->audioSettings->showError(e.what());throw;}});};
                if(args["input"]!=""&&Commands::inputPermission()!="authorized"&&Commands::inputPermission()!="not_required"){audioSettings->setBusy(true);audioSettings->showError("等待系统麦克风权限回执");Commands::requestInputPermission([safe=juce::Component::SafePointer<Workspace>(this),apply,epoch](bool granted){if(safe&&safe->audioSettingsEpoch==epoch&&safe->audioSettings->isVisible()){if(granted)apply();else{safe->audioSettings->setBusy(false);safe->audioSettings->showError("麦克风权限未授权；可关闭输入并配置输出");}}});}else apply();},
            [this]{closeAudioSettings();});addChildComponent(*audioSettings);}
            audioSettings->reload(false);audioSettings->setVisible(true);audioSettings->setBounds(getLocalBounds());audioSettings->toFront(true);});
    }

    void showPluginLibrary(){
        invoke([&]{
            if(!pending.is_null()||commandFileBusy)throw std::runtime_error("finish the existing preview first");
            if(!pluginLibrary)pluginLibrary=std::make_unique<PluginLibrary>(
                [this]{commands.refreshPluginInventory();refresh();},
                [this](std::string descriptor){prepareExternalPlugin(descriptor);},
                [this]{pluginLibrary->setVisible(false);});
            pluginLibraryTrack=selected;pluginLibrarySession=commands.sessionToken();
            auto t=selectedTrack();pluginLibrary->setTarget(t.is_null()?"\u672a\u9009\u62e9\u8f68\u9053":t["name"].get<std::string>(),!t.is_null()&&t["capabilities"]["audio_routing"].get<bool>()&&!facts["playing"].get<bool>());
            addAndMakeVisible(*pluginLibrary);resized();pluginLibrary->toFront(true);
        });
    }
    Json queryPluginEditors()const{return commands.pluginEditorQuery();}
    Json queryPluginLibrary()const{return pluginLibrary?pluginLibrary->query():Json(nullptr);}
    bool selectLibraryPlugin(const std::string& id){return pluginLibrary&&pluginLibrary->selectDescriptor(id);}
    void prepareExternalPlugin(const std::string& descriptor){
        if(!pending.is_null()||commandFileBusy)throw std::runtime_error("finish the current preview first");
        if(commands.sessionToken()!=pluginLibrarySession)throw std::runtime_error("session changed; reopen the plugin library");
        commands.refreshPluginInventory();auto plan=commands.makePlan("human",Json::array({operation("plugin.external.insert",{{"track",pluginLibraryTrack},{"descriptor",descriptor}})}));commands.preview(plan);pending=plan;
        reportShowing=false;selected=pluginLibraryTrack;pluginLibrary->setVisible(false);
        previewText.setText(text("\u5916\u90e8\u63d2\u4ef6 \u00b7 \u5f85\u786e\u8ba4\n\n\u76ee\u6807\u8f68\u9053\uff1a")+trackName(pluginLibraryTrack)+text("\n\u5b9e\u9645\u626b\u63cf\u63cf\u8ff0 ID\uff1a\n")+text(descriptor)+text("\n\n\u9ed8\u8ba4\u8fdb\u7a0b\u5185\u5904\u7406\uff0c\u65e0\u56fa\u5b9a IPC \u5ef6\u8fdf\u3002\n\u539f\u59cb\u63d2\u4ef6\u79c1\u6709\u72b6\u6001\u4fdd\u5b58\uff0c\u4e0d\u89e3\u91ca\u5176\u8bed\u4e49\u3002\n\u63a5\u53d7\u540e\u52a0\u8f7d\u771f\u5b9e\u5b9e\u4f8b\uff1b\u5931\u8d25\u4e0d\u4f1a\u663e\u793a\u5b8c\u6210\u3002\n\u4e00\u7b14 Undo \u79fb\u9664\u63d2\u5165\u3002\n"));
        message(text("\u63d2\u4ef6\u63d2\u5165\u9884\u89c8 \u00b7 \u5de5\u7a0b\u672a\u4fee\u6539"));refresh();
    }
    Json queryAutomation(const std::string& target) const {return commands.automationQuery(target);}
    void openSession(const juce::File& f) {if(f.hasFileExtension("ndaw")){prepareLegacyImport(f);return;}reportShowing=false;invoke([&]{commands.open(f);if(mcp){mcp.reset();Scope readonly;readonly.mode=Permission::ReadOnly;mcp=std::make_unique<McpGateway>(commandQueue,mcpEndpoint,readonly);}resetCommandClient({});selected.clear();selectedClip.clear();pending=nullptr;sessionName=f.getFileName();message(text("工程已重开 · 本轮撤销历史从此开始"));});}
    void prepareImport(const juce::File& f) {
        if(!pendingConfirmation.empty()||commandFileBusy){message(text("先接受或取消当前命令请求，再打开新的预览"));return;}
        if(f.hasFileExtension("ndaw")){prepareLegacyImport(f);return;}reportShowing=false;invoke([&]{pending=commands.makePlan("human",Json::array({operation("track.create",{{"name",f.getFileNameWithoutExtension().toStdString()},{"ref","$import"}}),
            operation("clip.import",{{"track","$import"},{"path",f.getFullPathName().toStdString()},{"position_samples",0}}),operation("track.gain",{{"track","$import"},{"db",-12}})}));
            previewText.setText(text("待确认的导入\n\n新增 Audio 轨道：")+f.getFileNameWithoutExtension()+text("\n导入：")+f.getFileName()+text("\n位置：0 samples\n轨道增益：−12 dB\n\n三项修改构成一个可撤销事务。"));message(text("待确认：导入预览；当前工程未修改"));});
    }
    Json queryLegacyReports() const {return commands.legacyReports();}
    void prepareLegacyImport(const juce::File& f) {
        if(!pendingConfirmation.empty()||commandFileBusy){message(text("先接受或取消当前命令请求，再打开新的预览"));return;}
        invoke([&]{auto plan=commands.makePlan("human",Json::array({operation("session.import_legacy",{{"path",f.getFullPathName().toStdString()}})}));auto report=commands.preview(plan)["legacy_imports"][0];pending=plan;reportShowing=true;previewText.setText(legacyReportText(report,true));message(text("旧工程导入预览 · 当前工程未修改"));});
    }
    void showLegacyReport(){invoke([&]{if(!pendingConfirmation.empty()||commandFileBusy)throw std::runtime_error("finish current command preview first");auto reports=commands.legacyReports();if(reports.empty())throw std::runtime_error("当前工程没有旧工程导入记录");reportShowing=true;previewText.setText(legacyReportText(reports.back(),false));});}
    void prepareMidiTransform(const std::string& cmd,Json args,uint64_t revision){
        if(!pendingConfirmation.empty()||commandFileBusy){message(text("先接受或取消当前命令请求，再打开新的预览"));return;}
        invoke([&]{auto plan=commands.makePlan("human",Json::array({operation(cmd,args)}));plan["base_revision"]=revision;const auto diff=commands.preview(plan)["midi_changes"][0];
            auto out=cmd=="midi.notes.quantize"?text("音符量化 · 待确认\n工程绝对节拍网格：")+juce::String(args["grid_beats"].get<double>())+text(" 拍\n强度：")+juce::String(args["strength"].get<double>()*100)+text("%\n保留节拍时长、音高和力度\n"):text("音符移调 · 待确认\n半音：")+juce::String(args["semitones"].get<int>())+text("\n起音、时长和力度保持\n");
            out+=text("\n起音选择范围：")+text(args["selection"].get<std::string>());if(args.contains("range_start_samples"))out+=text(" [")+juce::String(args["range_start_samples"].get<int64_t>())+text(", ")+juce::String(args["range_end_samples"].get<int64_t>())+text(")");
            out+=text("\n音符数：")+juce::String(int(diff["notes"].size()))+text("\n\n");
            for(const auto& n:diff["notes"]){const auto& b=n["before"];const auto& a=n["after"];out+=text(n["note"].get<std::string>())+text(" · ")+juce::String(b["pitch"].get<int>())+text(" → ")+juce::String(a["pitch"].get<int>())+text("\n采样 ")+juce::String(b["position_samples"].get<int64_t>())+text(" → ")+juce::String(a["position_samples"].get<int64_t>())+text("\n");}
            out+=text("\n接受后提交；一次 Undo 撤销全部音符变化。\n取消不会修改工程。");pending=plan;reportShowing=false;previewText.setText(out);message(text("MIDI 变换预览 · 工程未修改"));
        });
    }
    void prepareTrackDelete(const std::string& id) {
        if(!pendingConfirmation.empty()||commandFileBusy){message(text("先接受或取消当前命令请求，再打开新的预览"));return;}
        reportShowing=false;invoke([&]{pending=commands.makePlan("human",Json::array({operation("track.delete",{{"track",id},{"connections","disconnect"}})}));auto diff=commands.preview(pending)["track_changes"][0];
            auto out=text("删除轨道 · 待确认\n\n将删除这些轨道及其片段、插入和自动化：\n");
            for(const auto& t:diff["deleted_tracks"]){const auto& f=t["original_facts"];out+=text(t["name"].get<std::string>())+" · "+text(t["type"].get<std::string>())+text("\n")+juce::String(f["clips"].size())+text(" 个片段 / ")+juce::String(f["plugins"].size())+text(" 个插入\n");}
            out+=text("\n外部输出 → None（不会改接 Master）：\n");for(const auto& r:diff["disconnected_outputs"])out+=trackName(r["track"])+text(" → None\n");
            out+=text("\n移除外部发送：\n");for(const auto& r:diff["removed_incoming_sends"])out+=trackName(r["track"])+text(" → ")+trackName(r["target"])+"\n";
            out+=text("\n原始媒体文件全部保留。\n一次 Undo 恢复轨道、路由和发送。\n取消不会改变工程。");previewText.setText(out);message(text("删除范围已列出 · 接受后才修改工程"));
        });
    }
    void prepareReverbAux() {
        if(!pendingConfirmation.empty()||commandFileBusy){message(text("先接受或取消当前命令请求，再打开新的预览"));return;}
        reportShowing=false;
        invoke([&]{if(selected.empty())throw std::runtime_error("select a source track first");
            auto t=selectedTrack();pending=commands.makePlan("human",Json::array({
                operation("track.create",{{"name","Reverb · "+t["name"].get<std::string>()},{"type","aux"},{"ref","$reverb"}}),
                operation("plugin.insert",{{"track","$reverb"},{"type","reverb"},{"wet_only",true}}),
                operation("track.solo_safe",{{"track","$reverb"},{"enabled",true}}),
                operation("send.create",{{"track",selected},{"target","$reverb"},{"db",-12},{"position","post"}})}));
            previewText.setText(text("新混响 Aux · 待确认\n\n源轨道：")+text(t["name"].get<std::string>())+text("\n新建 Aux + 内置 Reverb\n纯湿：dry=0 / wet=1⁄3\nAux 启用 Solo Safe\nPost 发送：−12 dB\n原输出保持：")+text(t["output"]["name"].get<std::string>())+text("\n\n四项修改作为一笔事务，可整体撤销。"));message(text("混响 Aux 预览 · 工程未修改"));
        });
    }
    // Local file ingress exercises the production queue; it is not an AI provider.
    Json queryCommandResult()const {return lastCommandResult;}
    Json queryCommandPermission()const {return commandScope.json();}
    Json queryMcpStatus()const {return mcp?mcp->status():Json{{"state","disabled"}};}
    Json queryCommandQueueStatus()const {return commandQueue.status();}
    void startMcp(Permission mode,const juce::File& endpoint=McpGateway::defaultEndpoint()) {
        invoke([&]{if(mode==Permission::ScopedLowRisk)throw std::runtime_error("MCP requires GUI preview confirmation");mcp.reset();Scope grant;grant.mode=mode;mcpEndpoint=endpoint;mcp=std::make_unique<McpGateway>(commandQueue,endpoint,grant);syncCommandCards();message(mode==Permission::ReadOnly?text("MCP 已连接 · 只读查询；编辑请求会拒绝"):text("MCP 已连接 · 外部 Agent 的提交与撤销需本地确认"));});
    }
    void stopMcp(){invoke([&]{mcp.reset();syncCommandCards();message(text("MCP 已停止 · 未提交请求和授权已撤回，已提交编辑保留"));});}
    void showMcpInfo(){
        if(!pending.is_null())return;
        reportShowing=true;auto state=queryMcpStatus();
        auto helper=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getSiblingFile("Helpers").getChildFile("forma-mcp");
        previewText.setText(text("外部 Agent · MCP\n\n")+text(state.dump(2))+text("\n\nstdio 命令：\n")+helper.getFullPathName()+text("\n\n1. 在命令菜单选 MCP 只读或预览。\n2. MCP 客户端启动以上命令。\n3. query_session 查询实际对象，plan_edits 生成预览，commit_plan 请求确认。\n4. 本地接受后可试听，一次 Undo 撤销。\n\n权限更改或重开工程后重新连接。未提交计划不跨断开保存；不要盲目重复编辑。"));refresh();
    }

    void setCommandPermission(Permission mode,bool clipOnly=false) {
        invoke([&]{Scope grant;grant.mode=mode;
            if(mode==Permission::ScopedLowRisk){grant.commands=Scope::defaultCommands();
                if(clipOnly){auto c=pianoMode?piano.viewedClip():selectedAudioClip();if(c.is_null())throw std::runtime_error("select an existing clip before granting permission");grant.targets={c["id"]};grant.begin=c["start_samples"];grant.end=grant.begin+c["length_samples"].get<int64_t>();}
                else{if(selected.empty())throw std::runtime_error("select a track before granting permission");grant.targets={selected};}
            }
            resetCommandClient(grant);message(mode==Permission::ReadOnly?text("\u547d\u4ee4\u6743\u9650\uff1a\u53ea\u8bfb\uff1b\u7f16\u8f91\u8bf7\u6c42\u5c06\u62d2\u7edd"):mode==Permission::Preview?text("\u547d\u4ee4\u6743\u9650\uff1a\u5148\u9884\u89c8\u518d\u63d0\u4ea4"):text("\u547d\u4ee4\u6743\u9650\u5df2\u56fa\u5b9a\u5230\u5f53\u524d\u5bf9\u8c61\uff1b\u6539\u53d8\u9009\u62e9\u4e0d\u4f1a\u6269\u5927\u6388\u6743"));
        });
    }
    void importCommandFile(const juce::File& file) {
        invoke([&]{if(commandFileBusy||!pending.is_null())throw std::runtime_error("finish or cancel the existing preview first");
            commandFileBusy=true;lastCommandResult={{"status","queued"}};auto clientID=commandClient.id();
            commandFiles.addJob(new CommandFileJob(file,commandClient,facts["revision"],[safe=juce::Component::SafePointer<Workspace>(this),clientID](Json result){
                if(!safe||safe->commandClient.id()!=clientID)return;safe->commandFileBusy=false;safe->lastCommandResult=result;
                if(result["status"]=="awaiting_confirmation"){for(const auto& card:safe->commandQueue.pending())if(card["id"]==result["confirmation_id"])safe->showCommandCard(card);safe->message(text("\u672c\u5730\u547d\u4ee4\u5f85\u786e\u8ba4 \u00b7 \u5de5\u7a0b\u5c1a\u672a\u4fee\u6539"));}
                else if(result["status"]=="committed")safe->message(text("\u8303\u56f4\u5185\u547d\u4ee4\u5df2\u63d0\u4ea4 \u00b7 \u53ef\u6574\u7b14\u64a4\u9500"));
                else safe->message(text("\u547d\u4ee4\u672a\u63d0\u4ea4\uff1a")+text(result.value("error",std::string("cancelled"))));
                safe->refresh();
            }),true);message(text("\u6b63\u5728\u8bfb\u53d6\u672c\u5730\u547d\u4ee4 \u00b7 \u5728\u5f53\u524d\u6388\u6743\u8303\u56f4\u5185\u9884\u68c0"));
        });
    }
    void cancelCurrentCommand() {
        invoke([&]{resetCommandClient(commandScope);lastCommandResult={{"status","cancelled"},{"detail","pending requests and confirmations revoked; earlier completed edits remain in history"}};message(text("\u5f85\u6267\u884c\u8bf7\u6c42\u4e0e\u6388\u6743\u5df2\u64a4\u56de \u00b7 \u5df2\u63d0\u4ea4\u4e8b\u52a1\u4fdd\u7559\uff0c\u53ef\u7528 Undo \u64a4\u9500"));});
    }
    juce::StringArray getMenuBarNames() override {return {text("文件"),text("编辑"),text("视图"),text("命令")};}
    juce::PopupMenu getMenuForIndex(int index,const juce::String&) override {
        juce::PopupMenu p;p.setLookAndFeel(&theme);if(index==0){p.addItem(41,text("新建工程…   ⌘N"),!facts.value("playing",false)&&facts["parameter_capture"].is_null());p.addSeparator();p.addItem(1,text("导入音频…   ⌘I"));p.addItem(2,text("打开工程…   ⌘O"));p.addItem(3,text("另存工程…   ⌘S"));p.addItem(4,text("导出 WAV…   ⇧⌘E"));p.addItem(40,text("工程恢复副本…"));p.addSeparator();p.addItem(5,text("新增音频轨道"));p.addSeparator();p.addItem(11,text("导入旧 .ndaw 工程…"));p.addItem(12,text("查看旧工程导入报告"));}
        if(index==1){p.addItem(42,text("定位与时间选区…"));p.addSeparator();p.addItem(6,text("Undo   ⌘Z"),undoButton.isEnabled());p.addItem(7,text("Redo   ⇧⌘Z"),redoButton.isEnabled());}
        if(index==2){p.addItem(8,"Edit",true,!mix&&!pianoMode);p.addItem(9,"Mix",true,mix);p.addItem(10,text("钢琴卷帘"),true,pianoMode);p.addSeparator();p.addItem(13,text("插件库 · AU / VST3"),pending.is_null()&&!commandFileBusy);p.addItem(14,text("音频设备设置…"));}
        if(index==3){p.addItem(26,text("从本地 JSON 请求编辑…"),!commandFileBusy&&pending.is_null());p.addSeparator();p.addItem(21,text("只读分析"),true,commandScope.mode==Permission::ReadOnly);p.addItem(22,text("先预览再提交"),true,commandScope.mode==Permission::Preview);
            p.addItem(23,text("自动低风险 · 当前轨道"),!selected.empty());p.addItem(24,text("自动低风险 · 当前片段与时间"),!(pianoMode?piano.viewedClip():selectedAudioClip()).is_null());p.addSeparator();p.addItem(25,text("取消请求 / 撤回当前授权"),commandFileBusy||!pendingConfirmation.empty());
            auto m=queryMcpStatus();auto mode=m.contains("permission")?m["permission"].value("mode",std::string{}):std::string{};
            p.addSeparator();p.addItem(30,text("MCP · 只读连接"),true,mode=="read_only");p.addItem(31,text("MCP · 预览与确认提交"),true,mode=="preview");p.addItem(32,text("停止 MCP / 撤回 Agent 授权"),bool(mcp));p.addItem(33,text("MCP 配置与状态…"),pending.is_null());}
        return p;
    }
    void menuItemSelected(int id,int) override {
        if(id==42){showTimelineRange();return;}if(id==41){showNewSession();return;}if(id==40){showRecovery();return;}
        if(id==30||id==31){startMcp(id==30?Permission::ReadOnly:Permission::Preview);return;}if(id==32){stopMcp();return;}if(id==33){showMcpInfo();return;}
        if(id==14){showAudioSettings();return;}if(id==13){showPluginLibrary();return;}if(id>=21&&id<=24){setCommandPermission(id==21?Permission::ReadOnly:id==22?Permission::Preview:Permission::ScopedLowRisk,id==24);return;}
        if(id==25){cancelCurrentCommand();return;}if(id==26){choose(false,[this](const auto& f){importCommandFile(f);},"*.json");return;}if(id==11){choose(false,[this](const auto& f){prepareLegacyImport(f);},"*.ndaw");return;}if(id==12){showLegacyReport();return;}juce::TextButton* b=nullptr;switch(id){case 1:b=&importButton;break;case 2:b=&openButton;break;case 3:b=&saveButton;break;case 4:b=&exportButton;break;case 5:trackType.setSelectedId(1,juce::dontSendNotification);b=&newTrack;break;case 6:b=&undoButton;break;case 7:b=&redoButton;break;case 8:b=&editButton;break;case 9:b=&mixButton;break;case 10:b=&pianoButton;break;}if(b&&b->isEnabled())b->triggerClick();}
    bool isInterestedInFileDrag(const juce::StringArray& files) override {return files.size()==1;}
    void filesDropped(const juce::StringArray& files,int,int) override {auto file=juce::File(files[0]);if(file.hasFileExtension("json"))importCommandFile(file);else prepareImport(file);}
    bool keyPressed(const juce::KeyPress& key) override {
        if(timelinePanel&&timelinePanel->isVisible()){if(key==juce::KeyPress::escapeKey){timelinePanel->setVisible(false);grabKeyboardFocus();return true;}return false;}
        if(newSessionPanel&&newSessionPanel->isVisible()){if(key==juce::KeyPress::escapeKey){newSessionPanel->cancelAndClose();return true;}return false;}
        if(recoveryPanel&&recoveryPanel->isVisible()){if(key==juce::KeyPress::escapeKey){recoveryPanel->setVisible(false);grabKeyboardFocus();return true;}return false;}
        if(audioSettings&&audioSettings->isVisible()){if(key==juce::KeyPress::escapeKey){closeAudioSettings();return true;}return false;}
        if(key==juce::KeyPress::spaceKey){invoke([&]{if(facts["playing"].get<bool>())commands.stop();else commands.play();});return true;}
        if(key.getModifiers().isCommandDown()) {auto c=juce::CharacterFunctions::toLowerCase(key.getTextCharacter());if(c=='z')menuItemSelected(key.getModifiers().isShiftDown()?7:6,0);else if(c=='n')menuItemSelected(41,0);else if(c=='i')menuItemSelected(1,0);else if(c=='o')menuItemSelected(2,0);else if(c=='s')menuItemSelected(3,0);else if(c=='e'&&key.getModifiers().isShiftDown())menuItemSelected(4,0);else return false;return true;}return false;
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(base());g.setColour(juce::Colour(0xff222b35));g.fillRect(0,28,getWidth(),140);
        g.setColour(juce::Colour(0xff526476));g.drawHorizontalLine(167,0,float(getWidth()));g.drawHorizontalLine(getHeight()-29,0,float(getWidth()));
        const int right=getWidth()-332;g.setColour(juce::Colour(0xff202a34));g.fillRect(right,168,332,getHeight()-197);g.setColour(juce::Colour(0xffb7c7d8));g.setFont(juce::FontOptions(11));
        g.drawText(text("INSPECTOR / 轨道检查器"),right+16,175,298,25,juce::Justification::left);g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(17,juce::Font::bold));
        auto t=selectedTrack();g.drawText(t.is_null()?text("未选择轨道"):text(t["name"].get<std::string>()),right+16,206,298,28,juce::Justification::left);
        g.setFont(juce::FontOptions(11));g.setColour(juce::Colour(0xff91a3b7));
        auto p=selectedProcessor();if(p.is_null()&&!routingInspector&&!groupInspector&&!autoInspector&&!recordInspector){g.setFont(juce::FontOptions(13));g.drawText(text("选择效果器并插入\n参数取自真实 Tracktion 实例"),right+24,382,284,70,juce::Justification::centred);}
        double peak=deviceFacts.value("output_peak",0.);double db=peak>0?20*std::log10(peak):-100;
        g.setFont(juce::FontOptions(11));g.setColour(juce::Colour(0xffa8beca));g.drawText("MASTER PEAK "+(peak>0?juce::String(db,1):text("−∞"))+" dBFS",getWidth()-235,76,219,20,juce::Justification::right);
        g.setColour(juce::Colour(0xff101821));g.fillRect(getWidth()-218,103,202,6);g.setColour(db>=0?juce::Colour(0xffe0756b):accent());g.fillRect(getWidth()-218,103,int(std::clamp((db+60)/60.,0.,1.)*202),6);
    }
    void resized() override {
        if(timelinePanel)timelinePanel->setBounds(getLocalBounds());
        if(newSessionPanel)newSessionPanel->setBounds(getLocalBounds());
        if(pluginLibrary)pluginLibrary->setBounds(8,168,getWidth()-16,getHeight()-205);
        menu.setBounds(0,0,getWidth(),28);trackType.setBounds(12,39,86,28);int x=106;for(auto* b:{&newTrack,&importButton,&openButton,&saveButton,&exportButton}){b->setBounds(x,39,100,28);x+=108;}
        commandButton.setBounds(654,39,142,28);editButton.setBounds(getWidth()-310,39,92,28);mixButton.setBounds(getWidth()-210,39,92,28);pianoButton.setBounds(getWidth()-110,39,98,28);
        x=12;for(auto* b:{&returnButton,&stopButton,&playButton,&recordButton,&undoButton,&redoButton}){b->setBounds(x,83,64,29);x+=72;}
        counter.setBounds(448,76,196,39);rangeButton.setBounds(428,126,116,28);bpm.setBounds(654,83,62,28);meter.setBounds(726,83,68,28);applyMusic.setBounds(804,83,70,28);device.setBounds(12,124,285,32);audioSettingsButton.setBounds(305,126,111,28);musicPosition.setBounds(550,124,getWidth()-572,32);
        int right=getWidth()-332,areaHeight=getHeight()-197;bool clipDock=!selectedClip.empty()&&!mix&&!pianoMode;int dockHeight=clipDock?182:0;editView.setBounds(0,168,right,areaHeight-dockHeight);clipPanel.setBounds(0,getHeight()-29-dockHeight,right,dockHeight);clipPanel.setVisible(clipDock);mixView.setBounds(0,168,right,areaHeight);piano.setBounds(0,168,right,areaHeight);
        editView.setVisible(!mix&&!pianoMode);mixView.setVisible(mix&&!pianoMode);piano.setVisible(pianoMode);
        editArea.setSize(std::max(600,right-14),std::max(areaHeight-dockHeight,32+editArea.visibleRows()*144));
        mixArea.setSize(std::max(right,int(facts.value("tracks",Json::array()).size())*180+180),std::max(420,areaHeight-14));
        insertTab.setBounds(right+8,237,64,26);routingTab.setBounds(right+76,237,58,26);groupTab.setBounds(right+138,237,50,26);autoTab.setBounds(right+192,237,60,26);recordTab.setBounds(right+256,237,64,26);
        pluginType.setBounds(right+16,270,190,28);insertButton.setBounds(right+216,270,100,28);
        pluginChoice.setBounds(right+16,312,300,29);bypassButton.setBounds(right+16,352,86,26);editorButton.setBounds(right+108,352,108,26);removeButton.setBounds(right+222,352,94,26);
        const bool preview=!pending.is_null();const bool legacyView=reportShowing||(preview&&(pending["operations"][0]["command"]=="track.delete"||pending["operations"][0]["command"].get<std::string>().starts_with("midi.notes.")));int bottom=getHeight()-41-(preview?220:0);bool externalState=stateStatus.isVisible();bool recovering=stateRetryButton.isVisible();int parameterTop=externalState?(recovering?447:418):390;if(programButton.isVisible())parameterTop+=30;
        stateStatus.setBounds(right+16,385,300,26);stateRetryButton.setBounds(right+16,415,110,26);stateRestoreButton.setBounds(right+136,415,180,26);
        programIndex.setBounds(right+16,parameterTop-28,65,25);programButton.setBounds(right+91,parameterTop-28,225,25);
        parameterView.setBounds(right+8,parameterTop,316,std::max(40,bottom-parameterTop));
        parameters.setSize(300,parameters.getHeight());
        routingView.setBounds(right+6,274,320,std::max(100,bottom-274));routing.setSize(302,380);
        routingView.setVisible(routingInspector);groupView.setBounds(right+6,274,320,std::max(100,bottom-274));grouping.setSize(302,585);groupView.setVisible(groupInspector);autoView.setBounds(right+6,274,320,std::max(100,bottom-274));automation.setSize(302,650);autoView.setVisible(autoInspector);recordView.setBounds(right+6,274,320,std::max(100,bottom-274));recording.setSize(302,775);recordView.setVisible(recordInspector);for(auto* c:std::initializer_list<juce::Component*>{&pluginType,&insertButton,&pluginChoice,&bypassButton,&editorButton,&removeButton,&parameterView})c->setVisible(!routingInspector&&!groupInspector&&!autoInspector&&!recordInspector);
        previewText.setBounds(right+16,getHeight()-246,300,164);acceptButton.setBounds(right+16,getHeight()-72,142,30);rejectButton.setBounds(right+168,getHeight()-72,148,30);
        if(legacyView){previewText.setBounds(right+16,274,300,std::max(100,getHeight()-356));for(auto* c:std::initializer_list<juce::Component*>{&pluginType,&insertButton,&pluginChoice,&bypassButton,&editorButton,&removeButton,&stateStatus,&stateRetryButton,&stateRestoreButton,&programIndex,&programButton,&parameterView,&routingView,&groupView,&autoView,&recordView})c->setVisible(false);}
        if(audioSettings&&audioSettings->isVisible())audioSettings->setBounds(getLocalBounds());
        if(recoveryPanel&&recoveryPanel->isVisible())recoveryPanel->setBounds(getLocalBounds());
        previewText.setVisible(preview||legacyView);acceptButton.setVisible(preview);rejectButton.setVisible(preview||legacyView);rejectButton.setButtonText(legacyView&&!preview?text("关闭报告"):text("取消"));status.setBounds(12,getHeight()-27,getWidth()-320,24);recoveryIndicator.setBounds(getWidth()-300,getHeight()-27,288,24);
    }
private:
    void resetCommandClient(const Scope& scope) {
        commandQueue.revoke(commandClient.id());commandFiles.removeAllJobs(true,0);commandFileBusy=false;commandScope=scope;commandClient=commandQueue.connect("agent:command-file",commandScope);
        if(!pendingConfirmation.empty()){pending=nullptr;pendingConfirmation.clear();reportShowing=false;}lastCommandResult=nullptr;
        commandButton.setButtonText(commandScope.mode==Permission::ReadOnly?text("\u547d\u4ee4 \u00b7 \u53ea\u8bfb"):commandScope.mode==Permission::Preview?text("\u547d\u4ee4 \u00b7 \u9884\u89c8"):text("\u547d\u4ee4 \u00b7 \u8303\u56f4"));
    }
    void showCommandCard(const Json& card) {
        pending=card["plan"];pendingConfirmation=card["id"];reportShowing=true;acceptButton.setButtonText(card["kind"]=="undo"?text("确认撤销"):text("接受并提交"));
        auto out=text(card["kind"]=="undo"?"\u547d\u4ee4\u64a4\u9500 \u00b7 \u5f85\u786e\u8ba4\n\n":"外部 Agent / 命令 · 待确认\n\n")+text(card["actor"].get<std::string>())+text("\n\u5de5\u7a0b\u7248\u672c\uff1a")+text(pending["base_revision"].dump())+text("\n\n");
        if(pending["actor"]!=card["actor"])out+=text("原事务发起者：")+text(pending["actor"].get<std::string>())+text("\n当前连接仅请求撤销；原事务身份保留。\n\n");
        if(card["kind"]=="undo")out+=text("\u64a4\u9500\u8fd9\u7b14\u4e8b\u52a1\u3002\u5176\u540e\u82e5\u6709\u4eba\u5de5\u64cd\u4f5c\uff0c\u63d0\u4ea4\u65f6\u5c06\u62d2\u7edd\u8986\u76d6\u3002\n\n");
        for(const auto& op:pending["operations"]){out+=text(op["command"].get<std::string>())+"\n";
            const auto& a=op["args"];if(a.contains("track"))out+=text("\u8f68\u9053\uff1a")+trackName(a["track"])+"\n";
            out+=text(a.dump(2))+"\n\n";
        }
        out+=text("\u6743\u9650\u4e0e\u5b9e\u9645\u53d8\u66f4\n")+text(card["preview"].dump(2))+text("\n\n\u63a5\u53d7\u540e\u624d\u63d0\u4ea4\uff1b\u4e00\u9879 Plan \u4e00\u6b21 Undo\u3002\u6587\u4ef6\u6587\u672c\u53ea\u4f5c\u4e3a\u6570\u636e\u8bfb\u53d6\u3002");
        previewText.setText(out);
    }
    void finishCommandConfirmation(bool accepted) {
        auto result=commandQueue.resolve(pendingConfirmation,accepted);lastCommandResult=result;pendingConfirmation.clear();pending=nullptr;reportShowing=false;acceptButton.setButtonText(text("接受计划"));
        message(result["status"]=="committed"?text("\u547d\u4ee4\u5df2\u63d0\u4ea4 \u00b7 \u4e00\u6b21 Undo \u6574\u4f53\u64a4\u9500"):result["status"]=="undone"?text("\u547d\u4ee4\u4e8b\u52a1\u5df2\u64a4\u9500"):result["status"]=="rejected"?text("\u5df2\u62d2\u7edd\u8bf7\u6c42 \u00b7 \u5de5\u7a0b\u672a\u4fee\u6539"):text("\u547d\u4ee4\u672a\u63d0\u4ea4\uff1a")+text(result.value("error",std::string{})));
    }
    static juce::String legacyReportText(const Json& r,bool preview){
        auto out=preview?text("旧工程导入 · 待确认\n\n"):text("旧工程导入报告\n\n");out+=text(r["legacy_name"].get<std::string>())+text("\nSchema ")+juce::String(r["legacy_schema"].get<int>())+text(" → Tracktion\n新增轨道（含子混音）：")+juce::String(r["track_count"].get<int>())+text("\n因未完整映射先静音：")+juce::String(r["muted_for_incomplete_mapping"].get<int>())+text("\n\n")+text(r["message"].get<std::string>())+text("\n\n当前工程 Master / 现有轨道保持。\n源媒体不复制、不覆盖；移动媒体需重定位。\n一次 Undo 撤销整笔导入。\n\n媒体与插件状态\n");
        for(const auto& d:r["dependencies"])out+=text(d["kind"].get<std::string>())+" · "+text(d["status"].get<std::string>())+"\n"+text(d["path"].get<std::string>())+"\n\n";
        out+=text("映射调整\n");for(const auto& a:r["adjustments"])out+=text(a["field"].get<std::string>())+"\n"+text(a["reason"].get<std::string>())+"\n\n";
        out+=text("未映射字段（原值保存在工程中）\n");for(const auto& a:r["unmapped"])out+=text(a["field"].get<std::string>())+" = "+text(a["value"].dump()).substring(0,240)+"\n";
        return out;
    }
    friend class ndaw::v2::RecordingTestAccess;
    ClipWriter clipWriter(){return [this](const auto& cmd,Json args,uint64_t revision){invoke([&]{auto plan=commands.makePlan("human",Json::array({operation(cmd,args)}));plan["base_revision"]=revision;commands.commit(plan);message(text("片段编辑已提交 · 原媒体保留 · 可撤销"));});};}
    void selectAudioClip(const std::string& id){for(const auto& t:facts["tracks"])for(const auto& c:t["clips"])if(c["id"]==id){if(c["kind"]=="midi"){selectedClip.clear();return;}selected=t["id"];selectedClip=id;mix=false;pianoMode=false;refresh();return;}}
    Json selectedAudioClip()const{for(const auto& t:facts.value("tracks",Json::array()))for(const auto& c:t["clips"])if(c["id"]==selectedClip)return c;return nullptr;}
    Writer writer(){return [this](const auto& cmd,Json args){write(cmd,std::move(args));};}
    void message(const juce::String& s){status.setText(s,juce::dontSendNotification);}
    void invoke(std::function<void()> f) {try{f();refresh();}catch(const std::exception& e){message(text("操作失败：")+text(e.what()));refresh();}}
    void write(const std::string& cmd,Json args) {invoke([&]{if(cmd=="audio.meters.reset"){lastMeterRequest=commands.outputMeterControl(cmd,args)["reset_request"];message(text("峰值复位已请求 · 等待音频回调确认"));return;}if(cmd.starts_with("plugin.state.")){auto r=commands.nativeStateControl(cmd,args);message(r.value("state",std::string{})=="restored_checkpoint"?text("已还原已知插件状态 · 未捕获变化已丢弃"):r.value("state",std::string{})=="captured"?text("该插件状态读取已验证"):r.value("state",std::string{})=="pending"?text("状态变化待处理"):text("状态读取失败 · 查看插件状态说明"));return;}if(cmd.starts_with("plugin.editor.")){auto r=commands.pluginEditorControl(cmd,args);message(r["status"]=="opened"||r["status"]=="focused"?text("已打开真实插件窗口 · 公开参数修改可撤销"):text("已关闭插件窗口"));return;}if(cmd.starts_with("parameter.gesture.")){auto r=commands.parameterControl(cmd,args);message(!r.is_null()&&r["state"]=="editing"?text("人工参数手势中 · 实时调参 · 完成后整笔撤销"):!r.is_null()&&r["state"]=="no_changes"?text("参数未改变 · 没有新增历史"):text("人工参数已捕获 · 可撤销 · 旧计划失效"));return;}if(cmd.starts_with("automation.gesture.")){commands.automationControl(cmd,args);message(text("自动化录写中 · 停止后可整段撤销"));return;}commands.commit(commands.makePlan("human",Json::array({operation(cmd,args)})));
        if(cmd=="track.create")selected.clear();if(cmd=="track.collapsed")selected=args["track"];
        if(cmd=="plugin.insert"||cmd=="plugin.external.insert") {auto snapshot=commands.query();for(const auto& t:snapshot["tracks"])if(t["id"]==selected)pluginSelection=int(t["plugins"].size())-1;}
        message(text("已提交：")+text(cmd)+text(" · 可撤销"));});}
    void select(std::string id){for(const auto& t:facts["tracks"])if(t["id"]==id && t["capabilities"]["group"].get<bool>()){groupInspector=true;routingInspector=false;autoInspector=false;recordInspector=false;}if(selected!=id){selectedClip.clear();selected=std::move(id);pluginSelection=0;lastPluginIDs.clear();}refresh();}
    juce::String trackName(const std::string& id)const{for(const auto& t:facts["tracks"])if(t["id"]==id)return text(t["name"].get<std::string>());return text(id);}
    Json selectedTrack() const {for(const auto& t:facts.value("tracks",Json::array()))if(t["id"]==selected)return t;return nullptr;}
    Json selectedProcessor() const {auto t=selectedTrack();if(!t.is_null() && pluginSelection>=0 && pluginSelection<int(t["plugins"].size()))return t["plugins"][pluginSelection];return nullptr;}
    void refreshInspector() {
        auto t=selectedTrack();Json ids=Json::array();if(!t.is_null())for(const auto& p:t["plugins"])ids.push_back(p["id"]);
        if(ids.dump()!=lastPluginIDs){lastPluginIDs=ids.dump();pluginChoice.clear(juce::dontSendNotification);int i=1;if(!t.is_null())for(const auto& p:t["plugins"])pluginChoice.addItem(text(p["name"].get<std::string>()),i++);
            pluginSelection=ids.empty()?-1:std::clamp(pluginSelection,0,int(ids.size())-1);pluginChoice.setSelectedId(pluginSelection+1,juce::dontSendNotification);}
        auto p=selectedProcessor();const bool playing=facts.value("playing",false)||(facts["audio_configuration"].is_object()&&facts["audio_configuration"].value("state",std::string{})=="preparing"),parameterEditing=!facts["parameter_capture"].is_null();insertButton.setEnabled(!t.is_null()&&!playing&&!parameterEditing&&t["capabilities"]["audio_routing"].get<bool>());pluginType.setEnabled(insertButton.isEnabled());
        bypassButton.setEnabled(!p.is_null()&&!playing&&!parameterEditing);removeButton.setEnabled(!p.is_null()&&!playing&&!parameterEditing);bypassButton.setToggleState(!p.is_null()&&p["bypassed"].get<bool>(),juce::dontSendNotification);
        const auto& native=facts["native_plugin_states"];bool stateVisible=!p.is_null()&&p.contains("external")&&!routingInspector&&!groupInspector&&!autoInspector&&!recordInspector&&!reportShowing;
        bool stateFailed=false;Json stateError=nullptr;bool statePending=false;bool checkpoint=false;int nativeProgram=-1;
        if(stateVisible)for(const auto& c:native["checkpoints"])if(c["plugin"]==p["id"]){statePending=c["pending"];stateError=c["failure"];stateFailed=!stateError.is_null();checkpoint=!c["state_hash"].get<std::string>().empty();nativeProgram=c["program"];}
        stateStatus.setVisible(stateVisible);stateRetryButton.setVisible(stateFailed);stateRestoreButton.setVisible(stateFailed&&checkpoint);
        stateRetryButton.setEnabled(!playing&&!parameterEditing);stateRestoreButton.setEnabled(!playing&&!parameterEditing);
        bool programs=stateVisible&&p["external"].value("program_count",0)>0;programIndex.setVisible(programs);programButton.setVisible(programs);programIndex.setEnabled(!playing&&!parameterEditing&&!stateFailed&&!statePending);programButton.setEnabled(programIndex.isEnabled());
        if(programs&&programTarget!=p["id"].get<std::string>()){programTarget=p["id"];programDraft=false;}
        if(programs&&!programDraft)programIndex.setText(juce::String(p["external"].value("program_index",0)),false);
        programButton.setButtonText(programs?text("切换 Program · ")+text(p["external"].value("program_name",std::string{})):text("切换 Program"));
        programIndex.setTooltip(programs?text("实际索引范围 0–")+juce::String(p["external"]["program_count"].get<int>()-1)+text("；私有预设名称未解析。"):text("该插件没有 SDK Program"));
        stateStatus.setText(stateFailed?text("状态捕获失败 · 请重试或还原"):statePending?text("插件状态变化 · 停止后捕获"):checkpoint?(nativeProgram>=0?text("Program ")+text(std::to_string(nativeProgram))+text(" · 原始状态快照可用"):text("原始状态快照可用")):text("尚无可恢复的状态快照"),juce::dontSendNotification);
        stateStatus.setTooltip(stateFailed?text(stateError.value("error",std::string{})):text("公开参数、Program 索引及实际非参数通知纳入人工历史；播放时不读取私有状态。未报告的私有预设变化尚未验证。"));
        bool editorOpen=false;if(!p.is_null())for(const auto& e:commands.pluginEditorQuery())if(e["plugin"]==p["id"])editorOpen=true;
        editorButton.setEnabled(!p.is_null()&&p.contains("external")&&p["external"]["loaded"].get<bool>()&&p["external"]["has_editor"].get<bool>()&&facts["recording_capture"].is_null()&&(!parameterEditing||editorOpen));editorButton.setButtonText(editorOpen?text("关闭窗口"):text("插件窗口"));
        parameters.update(p,playing,300,selected,!facts["automation_capture"].is_null(),t.is_null()?"read":t["automation_mode"].get<std::string>(),!facts["parameter_capture"].is_null());if(autoInspector)automation.update(t.is_null()?Json(nullptr):commands.automationQuery(selected),playing,facts["position_samples"],facts["length_samples"]);routing.update(t,facts["tracks"],playing,deviceFacts.value("midi_outputs",Json::array()));grouping.update(t,facts["tracks"],playing);recording.update(t,deviceFacts,facts,recordDirectory);recordTab.setToggleState(recordInspector,juce::dontSendNotification);
        groupTab.setToggleState(groupInspector,juce::dontSendNotification);autoTab.setToggleState(autoInspector,juce::dontSendNotification);insertTab.setToggleState(!routingInspector&&!groupInspector&&!autoInspector,juce::dontSendNotification);routingTab.setToggleState(routingInspector,juce::dontSendNotification);
    }
    void refresh() {
        if(workspaceSession!=commands.sessionToken()){workspaceSession=commands.sessionToken();if(mcp)startMcp(Permission::ReadOnly,mcpEndpoint);Scope readOnly;readOnly.mode=Permission::ReadOnly;resetCommandClient(readOnly);selected.clear();selectedClip.clear();pending=nullptr;pendingConfirmation.clear();reportShowing=false;programDraft=false;if(pluginLibrary)pluginLibrary->setVisible(false);message(text("已切换工程会话 · Agent 授权回到只读"));}
        const auto recovery=commands.recoveryStatus();
        if(newSessionPanel&&newSessionPanel->isVisible()){newSessionPanel->update(commands.query(),recovery);if(newSessionRequested&&recovery.value("state",std::string{})=="created"&&recovery.value("receipt_current_session",false)){newSessionRequested=false;newSessionPanel->setVisible(false);sessionName=text(recovery["receipt"]["name"].get<std::string>());mix=false;pianoMode=false;recordInspector=false;routingInspector=false;groupInspector=false;autoInspector=false;message(text("已新建：")+sessionName+text(" · 上个工程的恢复副本已保留"));grabKeyboardFocus();}}
        if(recoveryPanel&&recoveryPanel->isVisible())recoveryPanel->update(recovery);
        if(recovery.value("available",false)){auto phase=recovery.value("state",std::string{});recoveryIndicator.setText(text(phase=="failed"?"恢复副本写入/读取失败":phase=="deferred"?"恢复副本保存延期":phase=="saved"?"恢复副本已保存":phase=="restored"?"已恢复工程 · 请另存":phase=="created"?"新工程 · 请另存":recovery.value("busy",false)?"正在处理恢复副本":recovery.value("enabled",false)?"自动恢复副本开启":"自动恢复副本关闭"),juce::dontSendNotification);}else recoveryIndicator.setText({},juce::dontSendNotification);
        const auto audio=commands.audioDevices();const auto& receipt=audio["last_configuration"];
        if(!receipt.is_null()){auto tag=receipt.value("id",std::string{})+receipt.value("state",std::string{});if(tag!=lastAudioReceipt){lastAudioReceipt=tag;if(receipt["state"]=="verified")message(text("实际音频设备与引擎准备已核验"));else if(receipt["state"]=="failed")message(text("设备配置失败：")+text(receipt.value("error",std::string{})));else if(receipt["state"]=="preparing")message(text("实际设备正在准备 · 等待音频回调回执"));}}
        facts=commands.query();if(timelinePanel&&timelinePanel->isVisible())timelinePanel->update(facts);waves.update(facts);bool found=false;for(const auto& t:facts["tracks"])found|=t["id"]==selected;
        if(!found){selected=facts["tracks"].empty()?"":facts["tracks"].back()["id"].get<std::string>();pluginSelection=0;lastPluginIDs.clear();}
        const bool playing=facts["playing"].get<bool>()||(facts["audio_configuration"].is_object()&&facts["audio_configuration"].value("state",std::string{})=="preparing"),parameterEditing=!facts["parameter_capture"].is_null();undoButton.setEnabled(facts["can_undo"].get<bool>()&&!playing&&!parameterEditing);redoButton.setEnabled(facts["can_redo"].get<bool>()&&!playing&&!parameterEditing);
        for(auto* b:{&newTrack,&importButton,&openButton,&saveButton,&exportButton,&applyMusic})b->setEnabled(!playing&&!parameterEditing);trackType.setEnabled(!playing&&!parameterEditing);bpm.setEnabled(!playing&&!parameterEditing);meter.setEnabled(!playing&&!parameterEditing);playButton.setEnabled(!playing&&!parameterEditing);
        const bool audioPending=facts["audio_configuration"].is_object()&&facts["audio_configuration"].value("state",std::string{})=="preparing";editView.setEnabled(!audioPending);mixView.setEnabled(!audioPending);recordView.setEnabled(!audioPending);
        returnButton.setEnabled(!audioPending&&!parameterEditing&&facts["automation_capture"].is_null()&&facts["recording_capture"].is_null());recordButton.setEnabled(facts["recording_readiness"]["ready"].get<bool>()&&recordDirectory.isDirectory());recordButton.setTooltip(recordDirectory.isDirectory()?recordingReadinessText(facts["recording_readiness"]):text("先选择录音目录"));recordButton.setToggleState(facts["recording"],juce::dontSendNotification);recordButton.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffbd4249));playButton.setToggleState(facts["playing"].get<bool>(),juce::dontSendNotification);editButton.setToggleState(!mix&&!pianoMode,juce::dontSendNotification);mixButton.setToggleState(mix,juce::dontSendNotification);pianoButton.setToggleState(pianoMode,juce::dontSendNotification);
        acceptButton.setEnabled(!pending.is_null()&&!playing&&!parameterEditing);rejectButton.setEnabled(!pending.is_null()||reportShowing);
        auto samples=facts["position_samples"].get<int64_t>();auto ms=samples/48;
        counter.setText(juce::String::formatted("%02lld:%02lld.%03lld",ms/60000,(ms/1000)%60,ms%1000),juce::dontSendNotification);
        deviceFacts=commands.deviceStatus();const auto& d=deviceFacts;device.setText((d.value("available",false)?text(d.value("name",std::string{}))+"\n"+juce::String(d["sample_rate"].get<double>()/1000.,1)+" kHz / "+juce::String(d["buffer_frames"].get<int>())+" frames":text("音频设备未打开"))+"  · r"+juce::String(facts["revision"].get<uint64_t>()),juce::dontSendNotification);
        if(lastMeterRequest&&d["output_meters"].value("reset_applied",uint64_t(0))>=lastMeterRequest){lastMeterRequest=0;message(text("输出峰值已复位 · 持续过载会重新标记"));}
        if(audioSettings&&audioSettings->isVisible())audioSettings->updateRuntime(commands.audioDevices());
        const auto& music=facts["music"];musicPosition.setText(text("小节 / 拍 ")+juce::String(music["bar"].get<int>())+" | "+juce::String(music["beat"].get<double>(),2)+text("    当前 ")+juce::String(music["bpm"].get<double>(),2)+" BPM   "+juce::String(music["numerator"].get<int>())+"/"+juce::String(music["denominator"].get<int>())+text("    起始 Tempo / 拍号：上方应用"),juce::dontSendNotification);
        if(music["tempos"].dump()+music["meters"].dump()!=lastMusicMap){lastMusicMap=music["tempos"].dump()+music["meters"].dump();bpm.setText(juce::String(music["tempos"][0]["bpm"].get<double>(),2),false);const auto m=juce::String(music["meters"][0]["numerator"].get<int>())+"/"+juce::String(music["meters"][0]["denominator"].get<int>());int item=0;for(int i=0;i<meter.getNumItems();++i)if(meter.getItemText(i)==m)item=meter.getItemId(i);if(!item){item=meter.getNumItems()+1;meter.addItem(m,item);}meter.setSelectedId(item,juce::dontSendNotification);}
        auto selectedAudio=selectedAudioClip();if(selectedAudio.is_null())selectedClip.clear();clipPanel.update(selectedAudio,selected,facts["revision"],facts["position_samples"],playing);
        editArea.update(facts,selected,commands.musicalGrid(0,std::llround(std::max(10.,facts["length_samples"].get<int64_t>()/48000.*1.05)*48000)),selectedClip);mixArea.update(facts,selected,d);piano.update(selectedTrack(),facts["revision"],playing,music["position_beats"]);auto selectedMidi=pianoMode?piano.viewedClip():Json(nullptr);commandQueue.setSelection(selected,!selectedMidi.is_null()?selectedMidi["id"].get<std::string>():selectedClip);refreshInspector();resized();repaint();
    }
    void syncCommandCards(){
        auto cards=commandQueue.pending();
        if(!pendingConfirmation.empty()){
            bool found=false;for(const auto& card:cards)found|=card["id"]==pendingConfirmation;
            if(!found){pending=nullptr;pendingConfirmation.clear();reportShowing=false;acceptButton.setButtonText(text("接受计划"));message(text("待确认请求已撤回 · 工程保留实际状态"));}
        }
        if(pending.is_null()&&!commandFileBusy&&!cards.empty())showCommandCard(cards[0]);
    }
    void timerCallback() override {syncCommandCards();refresh();if(!facts["last_recording"].is_null()&&facts["last_recording"]["state"]=="failed")message(text("录音失败：")+text(facts["last_recording"]["error"].get<std::string>()));}
    void choose(bool writing,std::function<void(const juce::File&)> action,const juce::String& filter="*.wav;*.aiff;*.flac") {
        chooser=std::make_unique<juce::FileChooser>(writing?text("保存到新文件（现有文件不会被覆盖）"):text("选择本地文件"),juce::File{},filter);
        chooser->launchAsync((writing?juce::FileBrowserComponent::saveMode:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles,
            [safe=juce::Component::SafePointer<Workspace>(this),action](const auto& c){if(safe&&c.getResult()!=juce::File{})action(c.getResult());});
    }
    void chooseRecordingDirectory(){chooser=std::make_unique<juce::FileChooser>(text("选择录音目录 · 只创建新录音，不覆盖已有文件"),recordDirectory,"*");chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe=juce::Component::SafePointer<Workspace>(this)](const auto& c){if(safe&&c.getResult().isDirectory()){safe->recordDirectory=c.getResult();safe->refresh();}});}
    std::string lastAudioReceipt;uint64_t audioSettingsEpoch=0,lastMeterRequest=0;std::unique_ptr<AudioDevicePanel> audioSettings;
    std::unique_ptr<PluginLibrary> pluginLibrary;std::string pluginLibraryTrack,pluginLibrarySession;
    std::unique_ptr<TimelinePanel> timelinePanel;juce::TextButton rangeButton{text("定位 / 选区…")};
    bool newSessionRequested=false;std::unique_ptr<NewSessionPanel> newSessionPanel;std::unique_ptr<RecoveryPanel> recoveryPanel;std::string workspaceSession;juce::Label recoveryIndicator;
    bool programDraft=false;std::string programTarget;
    Theme theme;Commands commands;CommandQueue commandQueue{commands};std::unique_ptr<McpGateway> mcp;juce::File mcpEndpoint;juce::ThreadPool commandFiles{1};CommandQueue::Client commandClient;Scope commandScope;Json lastCommandResult=nullptr;std::string pendingConfirmation;bool commandFileBusy=false;juce::File recordDirectory;Json facts=Json::object(),deviceFacts=Json::object(),pending=nullptr;std::string selected,selectedClip,lastPluginIDs,lastMusicMap;int pluginSelection=0;bool reportShowing=false,mix=false,recordInspector=false,routingInspector=false,groupInspector=false,autoInspector=false,pianoMode=false;juce::String sessionName="Untitled";
    Waveforms waves;EditArea editArea;MixArea mixArea;ParameterRows parameters;RoutingPanel routing;GroupingPanel grouping;RecordingPanel recording;AutomationPanel automation;ClipPanel clipPanel;PianoRoll piano;juce::Viewport editView,mixView,parameterView,routingView,groupView,autoView,recordView;
    juce::MenuBarComponent menu{this};juce::TextButton newTrack{text("新增轨道")},importButton{text("导入音频")},openButton{text("打开工程")},saveButton{text("另存工程")},exportButton{text("导出 WAV")},editButton{"EDIT"},mixButton{"MIX"},
        returnButton{"|<"},stopButton{text("停止")},playButton{text("播放")},recordButton{text("● 录音")},undoButton{"Undo"},redoButton{"Redo"},insertButton{text("插入")},bypassButton{text("旁通")},editorButton{text("插件窗口")},removeButton{text("移除")},stateRetryButton{text("重试读取")},stateRestoreButton{text("还原已知状态")},programButton{text("切换 Program")},acceptButton{text("接受计划")},rejectButton{text("取消")};
    juce::TextButton insertTab{text("插入 / 参数")},routingTab{text("I/O / 发送")},groupTab{text("组织")},autoTab{text("自动化")},recordTab{text("录音")};
    juce::TextButton audioSettingsButton{text("音频设置…")},commandButton{text("命令 · 预览")},pianoButton{text("钢琴卷帘")},applyMusic{text("应用")};juce::TextEditor bpm,programIndex;juce::ComboBox trackType,meter,pluginType,pluginChoice;juce::Label counter,device,status,musicPosition,stateStatus;juce::TextEditor previewText;juce::TooltipWindow tooltips{this,650};std::unique_ptr<juce::FileChooser> chooser;
};
}
