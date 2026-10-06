// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/RtAudit.h"
#include <CoreAudio/CoreAudio.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
int main() {
    pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
    void* (*volatile allocate)(std::size_t)=std::malloc;void (*volatile release)(void*)=std::free;
    {
        ndaw::RtAuditScope scope;
        auto* p=allocate(64);release(p);pthread_mutex_lock(&lock);pthread_mutex_unlock(&lock);
        int fd=open("/dev/null",O_WRONLY);const char b=0;if(fd>=0){write(fd,&b,1);close(fd);}
        AudioObjectPropertyAddress a{kAudioHardwarePropertyDefaultOutputDevice,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};
        AudioObjectHasProperty(kAudioObjectSystemObject,&a);
    }
    const auto counts=ndaw::rtAuditMetrics();bool ok=true;
    for(const char* key:{"allocation","free","blocking_lock_wait","file_network","device_property_control","scopes"})ok&=counts.at(key).get<std::uint64_t>()>0;
    std::cout<<ndaw::Json{{"status",ok?"passed":"failed"},{"kind","deliberately unsafe audit negative control; never production callback"},{"counts",counts}}.dump(2)<<'\n';
    pthread_mutex_destroy(&lock);return ok?0:1;
}
