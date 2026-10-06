// SPDX-License-Identifier: AGPL-3.0-only
// Test-only Mach-O interposer. Counters record calls, not inferred safety of
// arbitrary third-party internals. Use pthread TSD: C++ dyld TLV lazy allocation
// would recursively invoke an interposed malloc on a thread's first access.
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <malloc/malloc.h>
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>

namespace {
pthread_key_t auditKey{};
std::atomic<bool> auditReady{false};
std::atomic<std::uint64_t> counts[6]{};
struct Key {Key(){if(pthread_key_create(&auditKey,nullptr)==0)auditReady.store(true);}} key;
std::uintptr_t depth() noexcept {return reinterpret_cast<std::uintptr_t>(pthread_getspecific(auditKey));}
void hit(unsigned category) noexcept {if(auditReady.load(std::memory_order_relaxed) && depth())counts[category].fetch_add(1,std::memory_order_relaxed);}
}
extern "C" void ndaw_rt_audit_enter() noexcept {if(auditReady.load()){pthread_setspecific(auditKey,reinterpret_cast<void*>(depth()+1));counts[5].fetch_add(1,std::memory_order_relaxed);}}
extern "C" void ndaw_rt_audit_leave() noexcept {if(auditReady.load())pthread_setspecific(auditKey,reinterpret_cast<void*>(depth()-1));}
extern "C" void ndaw_rt_audit_read(std::uint64_t* values) noexcept {for(unsigned i=0;i<6;++i)values[i]=counts[i].load();}
#define INTERPOSE(original,replacement) \
    __attribute__((used)) static const struct {const void* replacementAddress;const void* originalAddress;} \
    interpose_##original __attribute__((section("__DATA,__interpose")))={reinterpret_cast<const void*>(replacement),reinterpret_cast<const void*>(original)};
#define WRAP(result,name,category,args,call) \
    static result audit_##name args {hit(category);return name call;} INTERPOSE(name,audit_##name)
WRAP(void*,malloc,0,(size_t n),(n))
WRAP(void*,calloc,0,(size_t n,size_t s),(n,s))
WRAP(void*,realloc,0,(void* p,size_t n),(p,n))
WRAP(void*,reallocf,0,(void* p,size_t n),(p,n))
WRAP(void*,valloc,0,(size_t n),(n))
WRAP(void*,aligned_alloc,0,(size_t a,size_t n),(a,n))
WRAP(int,posix_memalign,0,(void** p,size_t a,size_t n),(p,a,n))
WRAP(char*,strdup,0,(const char* s),(s))
WRAP(void,free,1,(void* p),(p))
WRAP(void*,malloc_zone_malloc,0,(malloc_zone_t* z,size_t n),(z,n))
WRAP(void*,malloc_zone_calloc,0,(malloc_zone_t* z,size_t n,size_t s),(z,n,s))
WRAP(void*,malloc_zone_realloc,0,(malloc_zone_t* z,void* p,size_t n),(z,p,n))
WRAP(void,malloc_zone_free,1,(malloc_zone_t* z,void* p),(z,p))
WRAP(int,pthread_mutex_lock,2,(pthread_mutex_t* p),(p))
WRAP(int,pthread_rwlock_rdlock,2,(pthread_rwlock_t* p),(p))
WRAP(int,pthread_rwlock_wrlock,2,(pthread_rwlock_t* p),(p))
WRAP(int,pthread_cond_wait,2,(pthread_cond_t* c,pthread_mutex_t* p),(c,p))
WRAP(int,pthread_cond_timedwait,2,(pthread_cond_t* c,pthread_mutex_t* p,const timespec* t),(c,p,t))
WRAP(int,pthread_join,2,(pthread_t t,void** r),(t,r))
WRAP(long,dispatch_semaphore_wait,2,(dispatch_semaphore_t s,dispatch_time_t t),(s,t))
WRAP(long,dispatch_group_wait,2,(dispatch_group_t s,dispatch_time_t t),(s,t))
WRAP(int,nanosleep,2,(const timespec* t,timespec* r),(t,r))
WRAP(int,usleep,2,(useconds_t n),(n))
WRAP(unsigned,sleep,2,(unsigned n),(n))
static int audit_open(const char* p,int flags,...) {
    hit(3);if(flags&O_CREAT){va_list a;va_start(a,flags);auto mode=va_arg(a,int);va_end(a);return open(p,flags,mode);}return open(p,flags);
} INTERPOSE(open,audit_open)
WRAP(int,close,3,(int fd),(fd))
WRAP(ssize_t,read,3,(int fd,void* p,size_t n),(fd,p,n))
WRAP(ssize_t,write,3,(int fd,const void* p,size_t n),(fd,p,n))
WRAP(ssize_t,pread,3,(int fd,void* p,size_t n,off_t o),(fd,p,n,o))
WRAP(ssize_t,pwrite,3,(int fd,const void* p,size_t n,off_t o),(fd,p,n,o))
WRAP(off_t,lseek,3,(int fd,off_t o,int w),(fd,o,w))
WRAP(int,fsync,3,(int fd),(fd))
WRAP(FILE*,fopen,3,(const char* p,const char* mode),(p,mode))
WRAP(int,fclose,3,(FILE* f),(f))
WRAP(size_t,fread,3,(void* p,size_t s,size_t n,FILE* f),(p,s,n,f))
WRAP(size_t,fwrite,3,(const void* p,size_t s,size_t n,FILE* f),(p,s,n,f))
WRAP(int,connect,3,(int s,const sockaddr* a,socklen_t n),(s,a,n))
WRAP(ssize_t,send,3,(int s,const void* p,size_t n,int flags),(s,p,n,flags))
WRAP(ssize_t,recv,3,(int s,void* p,size_t n,int flags),(s,p,n,flags))
WRAP(ssize_t,sendto,3,(int s,const void* p,size_t n,int flags,const sockaddr* a,socklen_t l),(s,p,n,flags,a,l))
WRAP(ssize_t,recvfrom,3,(int s,void* p,size_t n,int flags,sockaddr* a,socklen_t* l),(s,p,n,flags,a,l))
WRAP(OSStatus,AudioObjectGetPropertyData,4,(AudioObjectID o,const AudioObjectPropertyAddress* a,UInt32 n,const void* q,UInt32* s,void* p),(o,a,n,q,s,p))
WRAP(OSStatus,AudioObjectGetPropertyDataSize,4,(AudioObjectID o,const AudioObjectPropertyAddress* a,UInt32 n,const void* q,UInt32* s),(o,a,n,q,s))
WRAP(OSStatus,AudioObjectSetPropertyData,4,(AudioObjectID o,const AudioObjectPropertyAddress* a,UInt32 n,const void* q,UInt32 s,const void* p),(o,a,n,q,s,p))
WRAP(Boolean,AudioObjectHasProperty,4,(AudioObjectID o,const AudioObjectPropertyAddress* a),(o,a))
WRAP(OSStatus,AudioObjectIsPropertySettable,4,(AudioObjectID o,const AudioObjectPropertyAddress* a,Boolean* b),(o,a,b))
WRAP(OSStatus,AudioDeviceStart,4,(AudioDeviceID d,AudioDeviceIOProcID p),(d,p))
WRAP(OSStatus,AudioDeviceStop,4,(AudioDeviceID d,AudioDeviceIOProcID p),(d,p))
WRAP(OSStatus,AudioDeviceDestroyIOProcID,4,(AudioDeviceID d,AudioDeviceIOProcID p),(d,p))
WRAP(OSStatus,AudioObjectAddPropertyListener,4,(AudioObjectID o,const AudioObjectPropertyAddress* a,AudioObjectPropertyListenerProc p,void* c),(o,a,p,c))
WRAP(OSStatus,AudioObjectRemovePropertyListener,4,(AudioObjectID o,const AudioObjectPropertyAddress* a,AudioObjectPropertyListenerProc p,void* c),(o,a,p,c))
