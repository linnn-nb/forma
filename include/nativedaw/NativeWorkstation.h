// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Audio.h"
#include "EditorGeometry.h"
#include "EditSelection.h"
#include "TrackGroups.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace ndaw::desktop {
inline const juce::Colour background{0xff202329}, panel{0xff2a2e35}, raised{0xff343a43}, line{0xff454c57},
    text{0xffe4e7ec}, muted{0xff9fa9b8}, accent{0xff79b5d6}, amber{0xffe7bb6c};
inline juce::Colour trackColour(const Json& track) {
    if(track.at("kind")=="master")return juce::Colour(0xffd7b66d);
    if(track.at("kind")=="aux")return juce::Colour(0xffa293c8);
    constexpr std::uint32_t colours[]{0xff79b5c5,0xff8ead83,0xffb49bc7,0xffce9a7a,0xff839ecc,0xffc7b775};
    return juce::Colour(colours[juce::String(track.at("id").get<std::string>()).hashCode64()%6ull]);
}
inline void label(juce::Graphics& g,const juce::String& value,juce::Rectangle<int> rect,float size=11,juce::Colour colour=muted,juce::Justification align=juce::Justification::centredLeft) {
    g.setFont(juce::FontOptions(size));g.setColour(colour);g.drawText(value,rect,align,true);
}
inline juce::String clock(Frame frame,int rate) {
    const auto ms=static_cast<std::int64_t>(std::llround(static_cast<double>(frame)*1000/rate));
    return juce::String::formatted("%02lld:%02lld:%02lld.%03lld",static_cast<long long>(ms/3600000),static_cast<long long>((ms/60000)%60),static_cast<long long>((ms/1000)%60),static_cast<long long>(ms%1000));
}
class WorkstationLook final : public juce::LookAndFeel_V4 {
public:
    WorkstationLook() {
        setColour(juce::ResizableWindow::backgroundColourId,background);
        setColour(juce::TextButton::buttonColourId,raised);setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff48718a));
        setColour(juce::TextButton::textColourOffId,text);setColour(juce::TextButton::textColourOnId,juce::Colours::white);
        setColour(juce::ComboBox::backgroundColourId,background);setColour(juce::ComboBox::outlineColourId,line);setColour(juce::ComboBox::textColourId,text);
        setColour(juce::TextEditor::backgroundColourId,background);setColour(juce::TextEditor::textColourId,text);setColour(juce::TextEditor::outlineColourId,line);
        setColour(juce::Label::textColourId,text);setColour(juce::ListBox::backgroundColourId,panel);
        setColour(juce::PopupMenu::backgroundColourId,panel);setColour(juce::PopupMenu::textColourId,text);setColour(juce::PopupMenu::highlightedBackgroundColourId,raised);
        setColour(juce::Slider::thumbColourId,accent);setColour(juce::Slider::trackColourId,accent);setColour(juce::Slider::backgroundColourId,background);
        setColour(juce::Slider::textBoxTextColourId,text);setColour(juce::Slider::textBoxBackgroundColourId,background);setColour(juce::Slider::textBoxOutlineColourId,line);
        setColour(juce::ScrollBar::thumbColourId,juce::Colour(0xff59616e));
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& c,bool over,bool down) override {
        auto r=b.getLocalBounds().toFloat().reduced(.5f);g.setColour(c.withMultipliedBrightness(down?.8f:over?1.15f:1));g.fillRoundedRectangle(r,3);
        g.setColour(line);g.drawRoundedRectangle(r,3,1);
    }
    juce::Font getTextButtonFont(juce::TextButton&,int height) override {return juce::Font(juce::FontOptions(std::min(13.f,height*.45f)));}
    void drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,float position,float minimum,float maximum,const juce::Slider::SliderStyle style,juce::Slider& slider) override {
        if(style!=juce::Slider::LinearVertical){juce::LookAndFeel_V4::drawLinearSlider(g,x,y,w,h,position,minimum,maximum,style,slider);return;}
        const float centre=x+w*.5f;g.setColour(juce::Colour(0xff17191e));g.fillRoundedRectangle(centre-3,static_cast<float>(y),6,static_cast<float>(h),2);
        g.setColour(line);for(int i=0;i<=10;++i){float yy=y+h*i/10.f;g.drawLine(centre-15,yy,centre-6,yy);g.drawLine(centre+6,yy,centre+15,yy);}
        juce::Rectangle<float> cap(centre-13,position-7,26,14);g.setColour(slider.isEnabled()?juce::Colour(0xffb6bdc7):line);g.fillRoundedRectangle(cap,2);
        g.setColour(juce::Colour(0xff535c6a));g.drawLine(centre-10,position,centre+10,position,1.5f);
    }
};
class SamplePeakMeter final : public juce::Component {
public:
    SamplePeakMeter(){setTitle("Actual post-fader sample peak");setDescription("Measured audio sample peak; not LUFS or true peak.");}
    void measured(double linear,std::uint64_t blocks) {
        if(blocks!=blocks_){peak_=std::isfinite(linear)?std::max(0.0,linear):0;blocks_=blocks;}else peak_=0;
        hold_=std::max(peak_,hold_*.95);setDescription("Post-fader sample peak "+(peak_>0?juce::String(20*std::log10(peak_),1)+" dBFS":"silence")+"; processed blocks "+juce::String(static_cast<juce::int64>(blocks_))+". Not LUFS or true peak.");repaint();
    }
    void paint(juce::Graphics& g) override {
        auto r=getLocalBounds().reduced(2);g.setColour(juce::Colour(0xff14171b));g.fillRect(r);
        for(int i=0;i<45;++i) {
            const double db=-90+2.*i, threshold=std::pow(10.,db/20.);
            g.setColour(peak_>=threshold?(i>=42?juce::Colour(0xffdca365):juce::Colour(0xff83b69c)):juce::Colour(0xff283830));
            const int segment=std::max(1,r.getHeight()/45);g.fillRect(r.getX()+1,r.getBottom()-(i+1)*segment,r.getWidth()-2,std::max(1,segment-1));
        }
        if(hold_>0){g.setColour(hold_>=1?juce::Colour(0xffed6e6e):text);const auto db=20*std::log10(hold_);int yy=r.getBottom()-static_cast<int>(juce::jlimit(0.,1.,(db+90)/90)*r.getHeight());g.fillRect(r.getX(),yy,r.getWidth(),2);}
    }
    juce::String reading() const {return peak_>0?juce::String(20*std::log10(peak_),1)+" dBFS":"-inf dBFS";}
private:double peak_=0,hold_=0;std::uint64_t blocks_=0;
};
class Timeline final : public juce::Component,private juce::ChangeListener {
public:
    enum class Tool { Select, Grab, Trim };
    explicit Timeline(AudioEngine& e):engine_(e),cache_(64){setTitle("Audio edit timeline");setWantsKeyboardFocus(true);}
    ~Timeline() override {for(auto& [id,t]:thumbnails_)t->removeChangeListener(this);}
    std::function<void(std::string,std::string,Frame,Frame)> select;
    std::function<void(EditSelection,std::uint64_t)> selectionChanged;
    std::function<void(Json,std::uint64_t,std::string)> executePinned;
    std::function<void(std::string,std::string,Frame,Frame,std::uint64_t)> auditionSource;
    std::function<void(std::string,std::string,std::string,Frame,Frame,std::uint64_t)> sourceChosen;
    std::function<void(Json)> execute;std::function<void()> contextAI;
    EditorGeometry geometry;
    Tool tool=Tool::Select;
    int rowHeight=116;
    void update(Json session,fs::path root) {
        if(root_!=root){thumbnails_.clear();expanded_.clear();sourceClip_.clear();sourcePlaylist_.clear();dragging_=false;}
        session_=std::move(session);root_=std::move(root);geometry.sampleRate=session_.at("sample_rate");
        rebuildRows();selection_.retain(session_);
        for(const auto& source:session_.at("sources")) {
            auto id=source.at("id").get<std::string>();
            if(!thumbnails_.contains(id)) {
                if(formats_.getNumKnownFormats()==0)formats_.registerBasicFormats();
                auto thumbnail=std::make_unique<juce::AudioThumbnail>(256,formats_,cache_);thumbnail->addChangeListener(this);
                auto path=mediaPath(root_,source);if(fs::exists(path))thumbnail->setSource(new juce::FileInputSource(juce::File(juce::String(path.string()))));
                thumbnails_[id]=std::move(thumbnail);
            }
        }
        extent();repaint();
    }
    void extent(int minimumWidth=0,int minimumHeight=0) {
        rebuildRows();
        minWidth_=minimumWidth>0?minimumWidth:minWidth_;minHeight_=minimumHeight>0?minimumHeight:minHeight_;
        Frame length=session_.is_null()?geometry.sampleRate*15:std::max<Frame>(geometry.sampleRate*15,sessionLength(session_)+geometry.sampleRate*3);
        for(const auto& row:rows_)if(row.source)for(const auto& clip:playlist(session_.at("tracks")[row.trackIndex],row.playlistId).at("clips"))length=std::max(length,clip.at("start").get<Frame>()+clip.at("length").get<Frame>()+geometry.sampleRate*3);
        geometry.pixelsPerSecond=std::min(geometry.pixelsPerSecond,2000000.*geometry.sampleRate/length);
        setSize(std::max(minWidth_,static_cast<int>(std::ceil(geometry.pixelAt(length)))),std::max(minHeight_,rows_.empty()?0:rows_.back().y+rows_.back().height));
    }
    void selected(const std::string& track,const std::string& clip,Frame begin,Frame end){EditSelection s;s.single(track,clip,begin,end);selected(s);}
    void selected(const EditSelection& s){selection_=s;selectedTrack_=s.focusTrack;selectedClip_=s.focusClip;begin_=s.begin;end_=s.end;repaint();}
    void toggleSourceLanes(const std::string& id){if(expanded_.contains(id))expanded_.erase(id);else expanded_.insert(id);rebuildRows();extent();repaint();}
    std::map<std::string,int> trackHeights() const {std::map<std::string,int> result;for(const auto& row:rows_)result[row.trackId]+=row.height;return result;}
    void paint(juce::Graphics& g) override {
        g.fillAll(background);if(session_.is_null())return;auto visible=g.getClipBounds();
        for(std::size_t i=0;i<rows_.size();++i){
            const auto& row=rows_[i];int y=row.y;if(y+row.height<visible.getY() || y>visible.getBottom())continue;
            g.setColour(selection_.tracks.contains(row.trackId)?juce::Colour(0xff29333e):(i%2?juce::Colour(0xff242930):juce::Colour(0xff22262d)));g.fillRect(visible.getX(),y,visible.getWidth(),row.height);
            g.setColour(line.withAlpha(.5f));g.drawHorizontalLine(y+row.height-1,static_cast<float>(visible.getX()),static_cast<float>(visible.getRight()));
        }
        const auto step=majorStep();for(double second=std::floor(visible.getX()/geometry.pixelsPerSecond/step)*step;second*geometry.pixelsPerSecond<visible.getRight();second+=step){
            int x=static_cast<int>(second*geometry.pixelsPerSecond);g.setColour(line.withAlpha(.45f));g.drawVerticalLine(x,0.f,static_cast<float>(getHeight()));
        }
        if(geometry.gridFrames>0 && geometry.pixelAt(geometry.gridFrames)>=8){double grid=geometry.pixelAt(geometry.gridFrames);for(double x=std::floor(visible.getX()/grid)*grid;x<visible.getRight();x+=grid){g.setColour(line.withAlpha(.2f));g.drawVerticalLine(static_cast<int>(x),0.f,static_cast<float>(getHeight()));}}
        for(const auto& row:rows_){const auto& t=session_.at("tracks")[row.trackIndex];int y=row.y;
            if(y+row.height<visible.getY() || y>visible.getBottom())continue;
            const auto& clips=row.source?playlist(t,row.playlistId).at("clips"):activeClips(t);
            for(const auto& clip:clips){
                auto c=clip;if(dragging_ && !row.source && effective_.contains(c.at("id"))){const Frame delta=previewStart_-original_.at("start").get<Frame>();if(tool==Tool::Grab)c["start"]=c.at("start").get<Frame>()+delta;else if(tool==Tool::Trim){const Frame d=trimLeft_?previewOffset_-original_.at("source_start").get<Frame>():previewLength_-original_.at("length").get<Frame>();try{if(trimLeft_)c=remapClipBounds(c,c.at("start").get<Frame>()+d,c.at("source_start").get<Frame>()+d,c.at("length").get<Frame>()-d);else c=remapClipBounds(c,c.at("start"),c.at("source_start"),c.at("length").get<Frame>()+d);}catch(const std::exception&){c=clip;}}}
                auto r=clipRect(c,y,row.height);if(!r.intersects(visible))continue;auto colour=trackColour(t);bool active=row.source?c.at("id")==sourceClip_:selection_.clips.contains(c.at("id")) || effective_.contains(c.at("id"));
                g.setColour(colour.withMultipliedBrightness(active?.76f:.53f));g.fillRect(r);
                g.setColour(colour.withMultipliedBrightness(active?1.05f:.78f));g.fillRect(r.withHeight(21));
                label(g,juce::String(c.at("name").get<std::string>()),r.reduced(5,0).withHeight(21),11,juce::Colour(0xff152129));
                auto wave=r.withTrimmedTop(23).reduced(3,2);auto it=thumbnails_.find(c.at("source_id").get<std::string>());
                if(it!=thumbnails_.end() && it->second->getTotalLength()>0){g.setColour(colour.brighter(.3f));double first=c.at("source_start").get<double>()/geometry.sampleRate;it->second->drawChannels(g,wave,first,first+c.at("length").get<double>()/geometry.sampleRate,1.f);}
                else label(g,"Media unavailable / reading waveform",wave,11,muted);
                g.setColour(active?accent:colour);g.drawRect(r,active?2:1);
                if(c.at("fade_in").get<Frame>() || c.at("fade_out").get<Frame>() || c.contains("gain_envelope")) {
                    GainEnvelope envelope(c);juce::Path shape;const auto length=c.at("length").get<Frame>();
                    const int first=std::max(r.getX(),visible.getX()),last=std::min(r.getRight(),visible.getRight());
                    for(int x=first;x<=last;++x){auto local=std::min<Frame>(length-1,static_cast<Frame>(double(x-r.getX())*length/std::max(1,r.getWidth())));float yy=static_cast<float>(r.getBottom()-(r.getHeight()-22)*envelope.at(local));if(x==first)shape.startNewSubPath(static_cast<float>(x),yy);else shape.lineTo(static_cast<float>(x),yy);}
                    g.setColour(text.withAlpha(.8f));g.strokePath(shape,juce::PathStrokeType(1.f));
                }
            }
            if(row.source){if(sourcePlaylist_==row.playlistId && end_>begin_){g.setColour(amber.withAlpha(.15f));g.fillRect(static_cast<int>(geometry.pixelAt(begin_)),y,std::max(1,static_cast<int>(geometry.pixelAt(end_-begin_))),row.height);}}
        }
        if(end_>begin_){g.setColour(accent.withAlpha(.12f));g.fillRect(static_cast<int>(geometry.pixelAt(begin_)),0,std::max(1,static_cast<int>(geometry.pixelAt(end_-begin_))),getHeight());}
        const auto position=engine_.state()==PlaybackState::Playing || engine_.state()==PlaybackState::Priming?engine_.presentationPosition():begin_;
        g.setColour(amber);g.drawVerticalLine(static_cast<int>(geometry.pixelAt(position)),0.f,static_cast<float>(getHeight()));
        if(session_.at("sources").empty())label(g,"Import audio to begin, or arm an audio track to record",visible.withHeight(42).withY(std::min(getHeight()-42,static_cast<int>(session_.at("tracks").size())*rowHeight+18)),13,muted,juce::Justification::centred);
    }
    double majorStep() const {const double raw=85/geometry.pixelsPerSecond;const double choices[]{.01,.02,.05,.1,.2,.5,1,2,5,10,15,30,60,120,300,600,1800,3600};for(double s:choices)if(s>=raw)return s;return 7200;}
    void mouseDown(const juce::MouseEvent& e) override {
        if(session_.is_null())return;grabKeyboardFocus();auto* row=rowAt(e.y);if(!row)return;
        gestureRevision_=session_.at("revision");const auto& t=session_.at("tracks")[row->trackIndex];mouseTrack_=row->trackId;mouseX_=e.x;additive_=e.mods.isShiftDown() || e.mods.isCommandDown();isolated_=e.mods.isCtrlDown();beforeGesture_=selection_;dragging_=false;effective_.clear();
        const auto& clips=row->source?playlist(t,row->playlistId).at("clips"):activeClips(t);std::string hit;
        for(const auto& c:clips)if(clipRect(c,row->y,row->height).contains(e.getPosition())){hit=c.at("id");original_=c;break;}
        begin_=geometry.snap(geometry.frameAt(e.x));end_=begin_;sourcePlaylist_=row->source?row->playlistId:"";sourceClip_=row->source?hit:"";
        if(row->source) {selectedTrack_=row->trackId;selectedClip_.clear();if(!hit.empty()){begin_=original_.at("start");end_=begin_+original_.at("length").get<Frame>();notifySource();}}
        else {
            if(!hit.empty() && selection_.clips.contains(hit) && tool!=Tool::Select && !additive_){selection_.focusTrack=row->trackId;selection_.focusClip=hit;selection_.begin=begin_;selection_.end=end_;}
            else {selection_.single(row->trackId,hit,begin_,end_,additive_);if(additive_ && beforeGesture_.clips.contains(hit)){selection_.clips.erase(hit);selection_.focusClip=selection_.clips.empty()?"":*selection_.clips.begin();}}
            selectedTrack_=selection_.focusTrack;selectedClip_=selection_.focusClip;notifySelection();
            if(!hit.empty() && !selection_.clips.empty())try{auto list=resolveEditClips(session_,{selection_.clips.begin(),selection_.clips.end()},!isolated_,tool==Tool::Grab);effective_={list.begin(),list.end()};}catch(const std::exception& err){setDescription(std::string("Group preview unavailable: ")+err.what());}
        }
        trimLeft_=!hit.empty() && e.x<geometry.pixelAt(original_.at("start").get<Frame>()+original_.at("length").get<Frame>()/2);repaint();
        if(e.mods.isPopupMenu()) {
            juce::PopupMenu menu;if(row->source){menu.addItem(4,"Audition source selection",!hit.empty());menu.addItem(5,"Copy source selection to target",!hit.empty());}
            else {menu.addItem(1,"Split selected clips at cursor",!selection_.clips.empty());menu.addItem(2,"Delete selected clips",!selection_.clips.empty());}
            menu.addSeparator();menu.addItem(3,"Ask AI about selection...");auto safe=juce::Component::SafePointer<Timeline>(this);
            menu.showMenuAsync(juce::PopupMenu::Options{},[safe,sessionId=session_.at("id")](int n){if(!safe || n<=0)return;if(safe->session_.at("id")!=sessionId){safe->setDescription("Not executed: session changed after context menu opened");return;}try{
                if(n==3){if(safe->contextAI)safe->contextAI();}
                else if(n==4){if(safe->auditionSource)safe->auditionSource(safe->selectedTrack_,safe->sourcePlaylist_,safe->begin_,safe->end_,safe->gestureRevision_);}
                else if(n==5){const auto* row=safe->rowForSource();if(!row)return;const auto& t=safe->session_.at("tracks")[row->trackIndex];Json op{{"command","copy_range_to_playlist"},{"track_id",t.at("id")},{"playlist_id",t.at("target_playlist_id")},{"source_playlist_id",safe->sourcePlaylist_},{"clip_id",safe->sourceClip_},{"begin",safe->begin_},{"end",safe->end_},{"fade_in",0},{"fade_out",0}};safe->submit(Json::array({op}));}
                else {auto op=safe->selection_.operation(n==1?"split":"delete");if(n==1)op["position"]=safe->begin_;safe->submit(Json::array({op}));}
            }catch(const std::exception& err){safe->setDescription(std::string("Not executed: ")+err.what());}});
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(e.mods.isPopupMenu())return;
        if(!sourcePlaylist_.empty()){if(!sourceClip_.empty()){Frame a=geometry.snap(geometry.frameAt(mouseX_)),b=geometry.snap(geometry.frameAt(e.x)),start=original_.at("start"),end=start+original_.at("length").get<Frame>();begin_=std::clamp(std::min(a,b),start,end);end_=std::clamp(std::max(a,b),begin_,end);notifySource();repaint();}return;}
        if(tool==Tool::Select || selectedClip_.empty()) {
            auto* row=rowAt(e.y);if(!row || row->source)return;selection_=beforeGesture_;selection_.range(session_,mouseTrack_,row->trackId,geometry.snap(geometry.frameAt(mouseX_)),geometry.snap(geometry.frameAt(e.x)),additive_);selectedTrack_=selection_.focusTrack;selectedClip_=selection_.focusClip;begin_=selection_.begin;end_=selection_.end;notifySelection();repaint();return;
        }
        if(!e.mouseWasDraggedSinceMouseDown())return;dragging_=true;
        previewStart_=original_.at("start");previewOffset_=original_.at("source_start");previewLength_=original_.at("length");
        if(tool==Tool::Grab)previewStart_=geometry.snap(std::max<Frame>(0,previewStart_+static_cast<Frame>(std::llround((e.x-mouseX_)*geometry.sampleRate/geometry.pixelsPerSecond))));
        else {auto ops=EditorGeometry::trim(original_,geometry.snap(geometry.frameAt(e.x)),trimLeft_,sourceFrames());for(const auto& op:ops){if(op.at("command")=="move_clip")previewStart_=op.at("position");else{previewOffset_=op.at("source_start");previewLength_=op.at("length");}}}
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if(dragging_)try {
            if(tool==Tool::Grab && previewStart_!=original_.at("start")){auto op=selection_.operation("move");op["delta"]=previewStart_-original_.at("start").get<Frame>();if(isolated_)op["group_behavior"]="individual";submit(Json::array({op}));}
            else if(tool==Tool::Trim && (previewOffset_!=original_.at("source_start") || previewLength_!=original_.at("length"))){auto op=selection_.operation("trim");op["edge"]=trimLeft_?"left":"right";op["delta"]=trimLeft_?previewOffset_-original_.at("source_start").get<Frame>():previewLength_-original_.at("length").get<Frame>();if(isolated_)op["group_behavior"]="individual";submit(Json::array({op}));}
        }catch(const std::exception& e){setDescription(std::string("Not executed: ")+e.what());}dragging_=false;effective_.clear();repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override {if(!sourceClip_.empty()){notifySource();return;}if(!selectedClip_.empty()){selection_.begin=original_.at("start");selection_.end=selection_.begin+original_.at("length").get<Frame>();begin_=selection_.begin;end_=selection_.end;notifySelection();repaint();}}
private:
    struct Row {std::string trackId,playlistId;std::size_t trackIndex{};int y{},height{};bool source{};};
    void rebuildRows(){rows_.clear();if(session_.is_null())return;int y=0;std::size_t index=0;for(const auto& t:session_.at("tracks")){const std::string id=t.at("id");rows_.push_back({id,t.at("kind")=="audio"?t.at("active_playlist_id").get<std::string>():"",index,y,rowHeight,false});y+=rowHeight;
        if(expanded_.contains(id) && t.at("kind")=="audio")for(const auto& p:t.at("playlists"))if(p.at("id")!=t.at("active_playlist_id")){rows_.push_back({id,p.at("id"),index,y,84,true});y+=84;}++index;}}
    const Row* rowAt(int y) const {for(const auto& row:rows_)if(y>=row.y && y<row.y+row.height)return &row;return nullptr;}
    const Row* rowForSource() const {for(const auto& row:rows_)if(row.source && row.trackId==selectedTrack_ && row.playlistId==sourcePlaylist_)return &row;return nullptr;}
    void notifySelection(){if(selectionChanged)selectionChanged(selection_,gestureRevision_);else if(select)select(selection_.focusTrack,selection_.focusClip,selection_.begin,selection_.end);}
    void notifySource(){if(sourceChosen)sourceChosen(selectedTrack_,sourcePlaylist_,sourceClip_,begin_,end_,gestureRevision_);}
    void submit(Json ops){if(executePinned)executePinned(std::move(ops),gestureRevision_,session_.at("id"));else if(execute)execute(std::move(ops));}
    juce::Rectangle<int> clipRect(const Json& c,int y,int height) const{return {static_cast<int>(geometry.pixelAt(c.at("start").get<Frame>())),y+4,std::max(2,static_cast<int>(geometry.pixelAt(c.at("length").get<Frame>()))),height-8};}
    Frame sourceFrames() const {for(const auto& s:session_.at("sources"))if(s.at("id")==original_.at("source_id"))return s.at("frames");return original_.at("source_start").get<Frame>()+original_.at("length").get<Frame>();}
    void changeListenerCallback(juce::ChangeBroadcaster*) override {repaint();}
    AudioEngine& engine_;Json session_,original_;fs::path root_;juce::AudioFormatManager formats_;juce::AudioThumbnailCache cache_;
    std::map<std::string,std::unique_ptr<juce::AudioThumbnail>> thumbnails_;std::string selectedTrack_,selectedClip_;
    EditSelection selection_,beforeGesture_;std::set<std::string> expanded_,effective_;std::vector<Row> rows_;std::string mouseTrack_,sourcePlaylist_,sourceClip_;std::uint64_t gestureRevision_{};bool additive_=false,isolated_=false;
    Frame begin_=0,end_=0,previewStart_=0,previewOffset_=0,previewLength_=0;int mouseX_=0,minWidth_=800,minHeight_=500;bool dragging_=false,trimLeft_=false;
};
class TimelineRuler final : public juce::Component {
public:
    explicit TimelineRuler(Timeline& t):timeline_(t){setTitle("Timeline rulers");}
    int scroll=0;Frame begin=0,end=0;bool samples=false;RecordSettings recording;Json markers=Json::array();std::function<void(Frame)> seek;
    void paint(juce::Graphics& g) override {
        g.fillAll(panel);g.setColour(line);g.drawHorizontalLine(24,0,static_cast<float>(getWidth()));g.drawHorizontalLine(48,0,static_cast<float>(getWidth()));
        const auto& geometry=timeline_.geometry;const auto step=timeline_.majorStep();
        for(double second=std::floor(scroll/geometry.pixelsPerSecond/step)*step;second*geometry.pixelsPerSecond-scroll<getWidth();second+=step){int x=static_cast<int>(second*geometry.pixelsPerSecond)-scroll;
            label(g,samples?juce::String(static_cast<juce::int64>(std::llround(second*geometry.sampleRate))):clock(static_cast<Frame>(second*geometry.sampleRate),geometry.sampleRate),{x+5,0,100,22},10,text);
            g.setColour(line);g.drawVerticalLine(x,17,48);}
        for(const auto& m:markers){int x=static_cast<int>(geometry.pixelAt(m.at("position").get<Frame>()))-scroll;g.setColour(amber);g.fillRect(x,27,2,18);label(g,juce::String(m.at("name").get<std::string>()),{x+5,25,160,22},11,amber);}
        if(end>begin){g.setColour(accent.withAlpha(.3f));g.fillRect(static_cast<int>(geometry.pixelAt(begin))-scroll,0,std::max(1,static_cast<int>(geometry.pixelAt(end-begin))),24);}
        g.setColour(amber);g.drawVerticalLine(static_cast<int>(geometry.pixelAt(begin))-scroll,0,static_cast<float>(getHeight()));
        if(recording.loop || recording.punch) {
            auto pixel=[&](Frame frame){return geometry.pixelAt(frame)-scroll;};
            auto band=[&](Frame a,Frame b,juce::Colour colour){
                const auto x=juce::jlimit(0.,static_cast<double>(getWidth()),pixel(a)),right=juce::jlimit(0.,static_cast<double>(getWidth()),pixel(b));
                if(right>x){g.setColour(colour);g.fillRect(static_cast<float>(x),52.f,static_cast<float>(right-x),22.f);}
            };
            if(recording.punch){band(recording.captureBegin(),recording.begin,amber.withAlpha(.15f));band(recording.end,recording.captureEnd(),amber.withAlpha(.15f));}
            band(recording.begin,recording.end,recording.punch?juce::Colour(0xff94565e):accent.withAlpha(.3f));
            for(Frame frame:{recording.captureBegin(),recording.begin,recording.end,recording.captureEnd()}) {
                const auto x=pixel(frame);if(x<0 || x>=getWidth())continue;
                g.setColour(recording.punch?juce::Colour(0xffed9ba3):accent);g.drawVerticalLine(static_cast<int>(x),50,75);
            }
            const auto x=juce::jlimit(4.,static_cast<double>(std::max(4,getWidth()-130)),pixel(recording.begin)+5);
            label(g,recording.punch?"PUNCH  /  rolls retained":"LOOP RECORD",{static_cast<int>(x),52,std::max(0,getWidth()-static_cast<int>(x)-4),22},10,text);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {if(seek)seek(timeline_.geometry.snap(timeline_.geometry.frameAt(e.x+scroll)));}
private:Timeline& timeline_;
};
class SyncedViewport final : public juce::Viewport {
public:std::function<void(juce::Rectangle<int>)> changed;
private:void visibleAreaChanged(const juce::Rectangle<int>& r) override {if(changed)changed(r);}
};
// Both compact Edit headers and full Mix strips operate the same domain commands.
class ChannelStrip final : public juce::Component {
public:
    explicit ChannelStrip(bool compact):compact_(compact) {
        setWantsKeyboardFocus(true);
        for(auto* b:{&name_,&mute_,&arm_,&io_,&inserts_,&sends_,&playlist_})addAndMakeVisible(*b);
        mute_.setButtonText("M");mute_.setClickingTogglesState(true);mute_.setTooltip("Mute this track");
        arm_.setButtonText("R");arm_.setClickingTogglesState(true);arm_.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff924b50));arm_.setTooltip("Arm this audio track for recording");
        addAndMakeVisible(volume_);volume_.setRange(-120,24,.1);volume_.setSkewFactor(.36);volume_.setTextValueSuffix(" dB");volume_.setSliderStyle(compact?juce::Slider::LinearHorizontal:juce::Slider::LinearVertical);volume_.setTextBoxStyle(compact?juce::Slider::TextBoxRight:juce::Slider::TextBoxBelow,false,compact?65:100,22);
        addAndMakeVisible(pan_);pan_.setRange(-1,1,.01);pan_.setSliderStyle(juce::Slider::LinearHorizontal);pan_.setTextBoxStyle(juce::Slider::TextBoxRight,false,compact?46:52,20);
        addAndMakeVisible(meter_);addAndMakeVisible(meterLabel_);meterLabel_.setJustificationType(juce::Justification::centred);meterLabel_.setFont(juce::FontOptions(10));meterLabel_.setTitle("Post-fader sample peak dBFS");addAndMakeVisible(monitor_);monitor_.addItem("Monitor off",1);monitor_.addItem("Input",2);monitor_.addItem("Auto",3);
        name_.onClick=[this]{grabKeyboardFocus();if(selectModified)selectModified(id(),juce::ModifierKeys::getCurrentModifiers());else if(select)select(id());};
        playlist_.setButtonText("PL");playlist_.onClick=[this]{if(track_.is_null() || track_.at("kind")!="audio")return;juce::PopupMenu menu;std::vector<std::string> ids;for(const auto& p:track_.at("playlists")){ids.push_back(p.at("id"));menu.addItem(static_cast<int>(ids.size()),p.at("name").get<std::string>()+(p.at("id")==track_.at("target_playlist_id")?" [target]":""),true,p.at("id")==track_.at("active_playlist_id"));}menu.addSeparator();menu.addItem(static_cast<int>(ids.size())+1,"Playlist lanes and Comp...");menu.addItem(static_cast<int>(ids.size())+2,"Show / hide source lanes in Edit");auto safe=juce::Component::SafePointer<ChannelStrip>(this);menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&playlist_),[safe,ids](int result){if(!safe || result<=0)return;if(result==static_cast<int>(ids.size())+1){if(safe->playlists)safe->playlists(safe->id());}else if(result==static_cast<int>(ids.size())+2){if(safe->toggleLanes)safe->toggleLanes(safe->id());}else if(result<=static_cast<int>(ids.size()))safe->send({{"command","select_playlist"},{"track_id",safe->id()},{"playlist_id",ids.at(result-1)}});});};
        mute_.onClick=[this]{send({{"command","set_track_mute"},{"track_id",id()},{"muted",mute_.getToggleState()}});};
        arm_.onClick=[this]{send({{"command","set_track_arm"},{"track_id",id()},{"record_armed",arm_.getToggleState()}});};
        monitor_.onChange=[this]{send({{"command","set_track_monitor"},{"track_id",id()},{"monitor_mode",monitor_.getSelectedId()==2?"input":monitor_.getSelectedId()==3?"auto":"off"}});};
        io_.onClick=[this]{if(routing)routing(id());};inserts_.onClick=io_.onClick;sends_.onClick=io_.onClick;
        auto connect=[this](juce::Slider& s,const char* command,const char* field){auto* slider=&s;s.onDragStart=[this]{dragging_=true;isolated_=juce::ModifierKeys::getCurrentModifiers().isCtrlDown();};auto commit=[this,slider,command,field]{send({{"command",command},{"track_id",id()},{field,slider->getValue()}});};s.onDragEnd=[this,commit]{dragging_=false;commit();isolated_=false;};s.onValueChange=[this,commit]{if(!dragging_ && !updating_)commit();};};
        connect(volume_,"set_track_gain","gain_db");connect(pan_,"set_track_pan","pan");
    }
    std::function<void(Json)> execute;std::function<void(std::string)> select,routing,playlists;
    std::function<void(std::string,juce::ModifierKeys)> selectModified;std::function<void(std::string)> toggleLanes;
    void update(const Json& track,const Json& session,bool selected) {
        track_=track;selected_=selected;updating_=true;const auto name=juce::String(track.at("name").get<std::string>());
        name_.setButtonText(name);name_.setTooltip(name);name_.setToggleState(selected,juce::dontSendNotification);
        if(!dragging_){volume_.setValue(track.at("gain_db").get<double>(),juce::dontSendNotification);pan_.setValue(track.at("pan").get<double>(),juce::dontSendNotification);}
        volume_.setTitle(name+" volume");pan_.setTitle(name+" pan / balance");mute_.setTitle(name+" mute");arm_.setTitle(name+" record arm");
        if(track.contains("group_values")){const auto& v=track.at("group_values");volume_.setTooltip("Actual level; retained linked value "+juce::String(v.value("gain_db",track.at("gain_db").get<double>()),1)+" dB. Control-drag isolates.");pan_.setTooltip("Actual pan; retained linked value "+juce::String(v.value("pan",track.at("pan").get<double>()),2)+". Control-drag isolates.");}
        mute_.setToggleState(track.at("muted"),juce::dontSendNotification);arm_.setToggleState(track.at("record_armed"),juce::dontSendNotification);
        const bool audio=track.at("kind")=="audio",master=track.at("kind")=="master",locked=track.at("locked");
        mute_.setEnabled(!master && !locked);arm_.setEnabled(audio && !locked);pan_.setEnabled(!master && !locked);volume_.setEnabled(!locked);monitor_.setEnabled(audio && !locked);playlist_.setVisible(audio);playlist_.setEnabled(audio && !locked);playlist_.setTitle(name+" Playlist selection");if(audio)playlist_.setTooltip("Playback Playlist: "+playlist(track,track.at("active_playlist_id")).at("name").get<std::string>());
        auto mode=track.at("monitor_mode").get<std::string>();monitor_.setSelectedId(mode=="input"?2:mode=="auto"?3:1,juce::dontSendNotification);
        juce::String route="Hardware output";if(!master)for(const auto& b:session.at("buses"))if(b.at("id")==track.at("output").at("target_bus_id"))route=b.at("name").get<std::string>();
        io_.setButtonText(route);io_.setTooltip("Edit input and output routing for "+name);
        juce::String insert=compact_?"Inserts":"No inserts";if(!track.at("processors").empty()){const auto& p=track.at("processors")[0];insert=p.at("kind")=="plugin"?p.at("description").at("name").get<std::string>():"Lookahead limiter";if(track.at("processors").size()>1)insert+=" +"+juce::String(static_cast<int>(track.at("processors").size()-1));}
        inserts_.setButtonText(insert);inserts_.setTooltip("Open actual insert chain for "+name);
        sends_.setButtonText(track.at("sends").empty()?"Add send...":juce::String(static_cast<int>(track.at("sends").size()))+" sends");sends_.setEnabled(!master);
        updating_=false;repaint();
    }
    std::string id() const{return track_.is_null()?"":track_.at("id").get<std::string>();}
    void measured(const Json& metrics) {bool found=false;for(const auto& m:metrics.at("routing").at("meters"))if(m.at("id")==id()){meter_.measured(m.at("post_fader_sample_peak"),m.at("processed_blocks"));found=true;break;}if(!found)meter_.measured(0,0);meterLabel_.setText(meter_.reading(),juce::dontSendNotification);}
    void paint(juce::Graphics& g) override {
        g.fillAll(selected_?juce::Colour(0xff343e4b):panel);g.setColour(line);g.drawRect(getLocalBounds());if(track_.is_null())return;
        g.setColour(trackColour(track_));g.fillRect(0,0,compact_?4:getWidth(),compact_?getHeight():4);
        if(!compact_){label(g,"INSERTS",{10,44,getWidth()-20,18},10);label(g,"SENDS",{10,111,getWidth()-20,18},10);label(g,"I / O",{10,180,getWidth()-20,18},10);label(g,track_.at("kind")=="master"?"MASTER":track_.at("kind")=="aux"?"AUX":"AUDIO",{10,getHeight()-33,getWidth()-20,22},10,trackColour(track_),juce::Justification::centred);}
    }
    void resized() override {
        const int w=getWidth();meterLabel_.setVisible(!compact_);if(compact_){name_.setBounds(10,6,w-102,24);playlist_.setBounds(w-87,6,54,24);meter_.setBounds(w-23,7,14,getHeight()-15);mute_.setBounds(10,36,26,22);arm_.setBounds(40,36,26,22);monitor_.setBounds(72,36,w-105,22);volume_.setBounds(10,63,w-42,22);pan_.setBounds(10,89,92,20);io_.setBounds(108,88,w-141,22);inserts_.setVisible(false);sends_.setVisible(false);}
        else {name_.setBounds(8,12,w-62,26);playlist_.setBounds(w-50,12,42,26);inserts_.setBounds(10,65,w-20,30);sends_.setBounds(10,132,w-20,30);io_.setBounds(10,200,w-20,28);monitor_.setBounds(10,235,w-20,23);pan_.setBounds(10,273,w-20,23);mute_.setBounds(14,310,40,25);arm_.setBounds(62,310,40,25);volume_.setBounds(14,350,w-54,std::max(100,getHeight()-410));meter_.setBounds(w-33,350,20,std::max(75,getHeight()-422));meterLabel_.setBounds(10,getHeight()-58,w-20,18);}
    }
private:
    void send(Json op){if(!track_.is_null() && execute){if((isolated_ || juce::ModifierKeys::getCurrentModifiers().isCtrlDown()) && op.at("command").get<std::string>().starts_with("set_track") && op.at("command")!="set_track_input")op["group_behavior"]="individual";execute(Json::array({op}));}}
    bool compact_,selected_=false,dragging_=false,updating_=false,isolated_=false;Json track_;juce::TextButton name_,mute_,arm_,io_,inserts_,sends_,playlist_;juce::Slider volume_,pan_;juce::ComboBox monitor_;SamplePeakMeter meter_;juce::Label meterLabel_;
};
class ChannelBank final : public juce::Component {
public:
    explicit ChannelBank(bool compact):compact_(compact){}
    std::function<void(Json)> execute;std::function<void(std::string)> select,routing,playlists;
    std::function<void(std::string,juce::ModifierKeys)> selectModified;std::function<void(std::string)> toggleLanes;
    void update(const Json& session,const std::string& selected,const std::set<std::string>& selectedTracks={}){session_=session;selected_=selected;
        std::set<std::string> ids;for(const auto& track:session.at("tracks")){auto id=track.at("id").get<std::string>();ids.insert(id);if(!strips_.contains(id)){auto c=std::make_unique<ChannelStrip>(compact_);c->execute=[this](Json ops){if(execute)execute(std::move(ops));};c->select=[this](std::string id){if(select)select(id);};c->selectModified=[this](std::string id,juce::ModifierKeys mods){if(selectModified)selectModified(id,mods);else if(select)select(id);};c->toggleLanes=[this](std::string id){if(toggleLanes)toggleLanes(id);};c->routing=[this](std::string id){if(routing)routing(id);};c->playlists=[this](std::string id){if(playlists)playlists(id);};addAndMakeVisible(*c);strips_[id]=std::move(c);}strips_[id]->update(track,session,selectedTracks.empty()?selected==id:selectedTracks.contains(id));}
        for(auto i=strips_.begin();i!=strips_.end();)if(!ids.contains(i->first))i=strips_.erase(i);else ++i;layout();
    }
    void extent(int width,int height,int rowHeight=116){minimumWidth_=width;minimumHeight_=height;rowHeight_=rowHeight;layout();}
    void setRowHeights(std::map<std::string,int> heights){heights_=std::move(heights);layout();}
    void measured(const Json& metrics){for(auto& [id,c]:strips_)c->measured(metrics);}
    void paint(juce::Graphics& g) override {g.fillAll(panel);if(!compact_ || session_.is_null())return;int y=0;for(const auto& t:session_.at("tracks")){int h=height(t.at("id"));if(h>rowHeight_){int py=y+rowHeight_;for(const auto& p:t.at("playlists"))if(p.at("id")!=t.at("active_playlist_id")){label(g,"SOURCE PLAYLIST",{12,py+9,getWidth()-24,20},9,amber);label(g,p.at("name").get<std::string>(),{12,py+31,getWidth()-24,25},11,text);label(g,p.at("id")==t.at("target_playlist_id")?"Target  |  right-click waveform":"Right-click waveform to audition/copy",{12,py+58,getWidth()-24,19},9);py+=84;}}y+=h;}}
private:
    int height(const std::string& id) const {auto it=heights_.find(id);return it==heights_.end()?rowHeight_:it->second;}
    void layout(){int count=session_.is_null()?0:static_cast<int>(session_.at("tracks").size()),total=0;if(!session_.is_null())for(const auto& t:session_.at("tracks"))total+=height(t.at("id"));setSize(compact_?minimumWidth_:std::max(minimumWidth_,count*142),compact_?std::max(minimumHeight_,total):minimumHeight_);
        if(session_.is_null())return;int i=0,y=0;for(const auto& t:session_.at("tracks")){auto& c=*strips_.at(t.at("id").get<std::string>());c.setBounds(compact_?0:i*142,compact_?y:0,compact_?getWidth():142,compact_?rowHeight_:std::max(520,getHeight()));y+=height(t.at("id"));++i;}}
    bool compact_;int minimumWidth_=220,minimumHeight_=600,rowHeight_=116;Json session_;std::string selected_;std::map<std::string,int> heights_;std::map<std::string,std::unique_ptr<ChannelStrip>> strips_;
};
class SessionList final : public juce::Component,private juce::ListBoxModel {
public:
    explicit SessionList(bool clips):clips_(clips),list_("",this){addAndMakeVisible(list_);list_.setRowHeight(clips?46:31);list_.setOutlineThickness(0);list_.setMultipleSelectionEnabled(true);setTitle(clips?"Session clips":"Track list");}
    std::function<void(std::string,std::string,Frame,Frame)> select;
    std::function<void(EditSelection)> selectionChanged;
    void update(const Json& s,const std::string& selected,const std::set<std::string>& selectedIds={}){updating_=true;items_=Json::array();for(const auto& t:s.at("tracks")){if(clips_){for(const auto& c:activeClips(t))items_.push_back({{"name",c.at("name")},{"track",t.at("id")},{"clip",c.at("id")},{"position",c.at("start")},{"end",c.at("start").get<Frame>()+c.at("length").get<Frame>()},{"colour",trackColour(t).getARGB()},{"length",c.at("length")}});}else items_.push_back({{"name",t.at("name")},{"track",t.at("id")},{"clip",""},{"position",0},{"end",0},{"colour",trackColour(t).getARGB()},{"kind",t.at("kind")}});}
        rate_=s.at("sample_rate");list_.updateContent();juce::SparseSet<int> rows;for(std::size_t i=0;i<items_.size();++i){const std::string id=items_[i].at(clips_?"clip":"track");if(selectedIds.empty()?id==selected:selectedIds.contains(id))rows.addRange({static_cast<int>(i),static_cast<int>(i)+1});}list_.setSelectedRows(rows,juce::dontSendNotification);updating_=false;repaint();}
    void paint(juce::Graphics& g) override {g.fillAll(panel);label(g,clips_?"CLIPS":"TRACKS",{12,4,getWidth()-24,26},11,text);g.setColour(line);g.drawHorizontalLine(32,0,static_cast<float>(getWidth()));if(items_.empty())label(g,clips_?"No audio clips":"No tracks",{12,45,getWidth()-24,28},11);}
    void resized() override{list_.setBounds(0,34,getWidth(),std::max(0,getHeight()-34));}
private:
    int getNumRows() override{return static_cast<int>(items_.size());}
    juce::String getNameForRow(int row) override {return row>=0 && row<getNumRows()?juce::String(items_[row].at("name").get<std::string>()):juce::String{};}
    void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool selected) override {if(row<0 || row>=getNumRows())return;const auto& item=items_[row];if(selected){g.setColour(raised);g.fillAll();}g.setColour(juce::Colour(item.at("colour").get<std::uint32_t>()));g.fillRect(7,7,3,h-14);label(g,juce::String(item.at("name").get<std::string>()),{17,2,w-22,clips_?22:h-4},12,text);if(clips_)label(g,juce::String(item.at("length").get<double>()/rate_,2)+" s",{17,24,w-22,18},10);}
    void listBoxItemClicked(int row,const juce::MouseEvent&) override {choose(row);}
    void listBoxItemDoubleClicked(int row,const juce::MouseEvent&) override{choose(row);}
    void selectedRowsChanged(int row) override{if(!updating_)choose(row);}
    void choose(int row){if(updating_ || row<0 || row>=getNumRows())return;list_.grabKeyboardFocus();const auto& item=items_[row];
        if(selectionChanged){EditSelection s;s.focusTrack=item.at("track");s.focusClip=item.at("clip");s.begin=INT64_MAX;for(int i=0;i<getNumRows();++i)if(list_.isRowSelected(i)){const auto& entry=items_[i];s.tracks.insert(entry.at("track"));if(clips_)s.clips.insert(entry.at("clip"));s.begin=std::min(s.begin,entry.at("position").get<Frame>());s.end=std::max(s.end,entry.at("end").get<Frame>());}if(s.begin==INT64_MAX)s.begin=0;selectionChanged(std::move(s));}
        else if(select)select(item.at("track"),item.at("clip"),item.at("position"),item.at("end"));}
    bool clips_,updating_=false;int rate_=48000;Json items_=Json::array();juce::ListBox list_;
};
class CounterPanel final : public juce::Component {
public:
    CounterPanel(){setTitle("Transport counters");}
    Frame position=0,begin=0,end=0;int rate=48000;bool playing=false,recording=false;juce::String sessionName,device,activity;
    void paint(juce::Graphics& g) override {g.setColour(juce::Colour(0xff171b20));g.fillRoundedRectangle(getLocalBounds().toFloat(),4);auto left=getLocalBounds().removeFromLeft(std::min(290,getWidth()/2));
        label(g,activity.isNotEmpty()?activity:recording?"RECORDING":playing?"PLAYING":"MAIN COUNTER",left.reduced(12,0).withHeight(24),10,recording?juce::Colour(0xffed8f8f):muted);
        g.setFont(juce::FontOptions(27).withStyle("Bold"));g.setColour(amber);g.drawText(clock(position,rate),left.reduced(12,0).withTrimmedTop(22).withHeight(37),juce::Justification::centredLeft);
        auto right=getLocalBounds().withTrimmedLeft(left.getWidth()+12);label(g,sessionName,right.withHeight(24),12,text);label(g,juce::String(rate/1000.,1)+" kHz  |  "+device,right.withTrimmedTop(25).withHeight(24),11);}
};
}
