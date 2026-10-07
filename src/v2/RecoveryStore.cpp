#include "RecoveryStore.h"
#include <algorithm>
#include <fstream>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ndaw::v2::recovery {
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
bool identity(const std::string& id){return id.size()==32&&id.find_first_not_of("0123456789abcdef")==std::string::npos;}
bool hash(const std::string& h){return h.size()==64&&h.find_first_not_of("0123456789abcdef")==std::string::npos;}
std::string digest(const std::string& s){return juce::SHA256(s.data(),s.size()).toHexString().toStdString();}
fs::path root(const juce::File& directory){
    const fs::path path(directory.getFullPathName().toStdString());
    require(!fs::is_symlink(fs::symlink_status(path)),"recovery directory must not be a symlink");
    const bool created=fs::create_directories(path);
#ifndef _WIN32
    if(created)require(::chmod(path.c_str(),0700)==0,"recovery directory permissions failed");
    struct stat status{};require(::lstat(path.c_str(),&status)==0&&S_ISDIR(status.st_mode)&&status.st_uid==geteuid()&&(status.st_mode&0777)==0700,"recovery directory must be owned with mode 0700");
#endif
    require(fs::is_directory(path),"recovery directory unavailable");return path;
}
std::string text(const fs::path& path,int64_t limit){
    require(fs::is_regular_file(fs::symlink_status(path)),"recovery file is missing or is not a regular file");
    const auto size=fs::file_size(path);require(size>0&&size<=uint64_t(limit),"recovery file exceeds size budget");
    std::ifstream stream(path,std::ios::binary);require(bool(stream),"recovery file cannot be read");
    std::string data(size,'\0');stream.read(data.data(),std::streamsize(size));
    require(stream.gcount()==std::streamsize(size)&&stream.peek()==std::char_traits<char>::eof()&&!stream.bad(),"recovery file changed during read");return data;
}
Json manifest(const fs::path& file){
    auto meta=Json::parse(text(file,256*1024));
    require(meta.is_object()&&meta.value("schema",0)==1,"unsupported recovery manifest");
    const auto id=meta.at("id").get<std::string>();require(identity(id)&&file.filename()==id+".json","invalid recovery identity");
    require(hash(meta.at("sha256")),"invalid recovery checksum");
    require(meta.at("bytes").is_number_integer()&&meta["bytes"].get<int64_t>()>0&&meta["bytes"].get<int64_t>()<=maximumBytes,"invalid recovery payload size");
    require(meta.at("revision").is_number_integer()&&meta["revision"].get<int64_t>()>=0&&meta.at("created_utc").is_string()&&meta.at("session_token").is_string(),"invalid recovery provenance");
    return meta;
}
}
Json list(const juce::File& directory){
    const auto path=root(directory);Json entries=Json::array();int64_t bytes=0;size_t visited=0;
    for(const auto& item:fs::directory_iterator(path)){
        require(++visited<=maximumSnapshots*3+16,"recovery catalog entry budget exceeded");
        if(fs::is_regular_file(item.symlink_status())){require(item.file_size()<=uint64_t(maximumTotalBytes),"recovery disk budget exceeded");bytes+=int64_t(item.file_size());require(bytes<=maximumTotalBytes,"recovery disk budget exceeded");}
        if(item.path().extension()!=".json"||item.path().filename()=="preferences.json")continue;
        try{auto row=manifest(item.path());row["status"]="unchecked";entries.push_back(std::move(row));}
        catch(const std::exception& e){entries.push_back({{"id",item.path().stem().string()},{"status","invalid"},{"error",e.what()}});}
        require(entries.size()<=maximumSnapshots,"recovery snapshot count budget exceeded");
    }
    std::sort(entries.begin(),entries.end(),[](const Json& a,const Json& b){return a.value("created_utc",std::string{})>b.value("created_utc",std::string{});});
    return {{"entries",entries},{"bytes",bytes},{"maximum_snapshots",maximumSnapshots},{"maximum_bytes",maximumTotalBytes}};
}
Json write(const juce::File& directory,Snapshot snapshot){
    const auto path=root(directory);const auto catalog=list(directory);
    require(catalog["entries"].size()<maximumSnapshots,"recovery snapshot capacity reached; archive copies explicitly");
    require(snapshot.state.hasType(te::IDs::EDIT),"snapshot is not an Edit");
    const auto xml=snapshot.state.toXmlString().toStdString();require(!xml.empty()&&xml.size()<=uint64_t(maximumBytes),"snapshot exceeds 64 MiB budget");
    require(catalog["bytes"].get<int64_t>()+int64_t(xml.size())+256*1024<=maximumTotalBytes,"recovery disk budget reached; archive copies explicitly");
    auto meta=std::move(snapshot.metadata);const auto id=meta.at("id").get<std::string>();require(identity(id),"invalid snapshot identity");
    meta["schema"]=1;meta["bytes"]=xml.size();meta["sha256"]=digest(xml);meta["created_utc"]=juce::Time::getCurrentTime().toISO8601(true).toStdString();
    meta["media_copied"]=false;meta["undo_restored"]=false;
    atomicWrite(path/(id+".tracktionedit"),xml,false);
    // The manifest is the commit point. A crash before this leaves an orphan,
    // which is never offered as a complete recovery snapshot.
    atomicWrite(path/(id+".json"),meta.dump(2),false);
    return meta;
}
Loaded read(const juce::File& directory,const std::string& id,const std::string& expectedHash){
    require(identity(id)&&hash(expectedHash),"invalid recovery selection");const auto path=root(directory);
    auto meta=manifest(path/(id+".json"));require(meta["sha256"]==expectedHash,"recovery selection changed; refresh the preview");
    const auto xml=text(path/(id+".tracktionedit"),maximumBytes);
    require(xml.size()==meta["bytes"].get<uint64_t>()&&digest(xml)==expectedHash,"recovery payload checksum mismatch");
    auto document=juce::XmlDocument::parse(juce::String::fromUTF8(xml.data(),int(xml.size())));
    require(document&&document->hasTagName("EDIT"),"recovery payload is not valid Edit XML");
    auto state=juce::ValueTree::fromXml(*document);const auto native=state.getChildWithName("NATIVEDAW");
    require(int64_t(native.getProperty("revision",-1))==meta["revision"].get<int64_t>(),"recovery revision does not match manifest");
    return {state,meta};
}
Json settings(const juce::File& directory){
    const auto path=root(directory)/"preferences.json";
    if(!fs::exists(fs::symlink_status(path)))return {{"enabled",true},{"interval_seconds",60}};
    auto value=Json::parse(text(path,256*1024));
    require(value.is_object()&&value.value("schema",0)==1&&value.at("enabled").is_boolean()&&value.at("interval_seconds").is_number_integer(),"invalid recovery preferences");
    const auto interval=value.at("interval_seconds").get<int64_t>();require(interval>=10&&interval<=600,"invalid recovery interval");return value;
}
void settings(const juce::File& directory,const Json& value){
    const auto path=root(directory)/"preferences.json";
    require(!fs::is_symlink(fs::symlink_status(path)),"recovery preferences must not be a symlink");
    auto persisted=value;persisted["schema"]=1;atomicWrite(path,persisted.dump(2),true);
}
}
