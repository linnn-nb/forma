#pragma once
#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <cstdlib>
#include <stdexcept>
namespace ndaw::v2::ipc {
inline void require(bool ok,const char* why){if(!ok)throw std::runtime_error(std::string(why)+": "+std::strerror(errno));}
struct Fd {
    int value=-1;Fd()=default;explicit Fd(int f):value(f){}~Fd(){if(value>=0)::close(value);}
    Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;
    Fd(Fd&& other)noexcept:value(other.value){other.value=-1;}
    Fd& operator=(Fd&& other)noexcept{if(value>=0)::close(value);value=other.value;other.value=-1;return *this;}
};
inline std::string defaultPath(){const char* home=std::getenv("HOME");if(!home||!*home)throw std::runtime_error("HOME is required for the default MCP endpoint");
#if defined(__APPLE__)
    return std::string(home)+"/Library/Application Support/NativeDAW/v2/agent.sock";
#else
    return std::string(home)+"/.config/NativeDAW/v2/agent.sock";
#endif
}
inline sockaddr_un address(const std::string& path){sockaddr_un a{};a.sun_family=AF_UNIX;if(path.empty()||path.size()>=sizeof(a.sun_path))throw std::runtime_error("Unix socket path is empty or too long");std::memcpy(a.sun_path,path.c_str(),path.size()+1);return a;}
inline void nonblocking(int fd){require(fcntl(fd,F_SETFD,FD_CLOEXEC)==0,"socket close-on-exec");int flags=fcntl(fd,F_GETFL);require(flags>=0&&fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0,"nonblocking socket");
#if defined(__APPLE__)
    int yes=1;require(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes))==0,"socket no-SIGPIPE");
#endif
}
inline bool ownPeer(int fd){
#if defined(__APPLE__)
    uid_t uid;gid_t gid;return getpeereid(fd,&uid,&gid)==0&&uid==geteuid();
#else
    struct ucred c{};socklen_t n=sizeof(c);return getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&c,&n)==0&&c.uid==geteuid();
#endif
}
inline ssize_t send(int fd,const char* data,size_t n){
#if defined(MSG_NOSIGNAL)
    return ::send(fd,data,n,MSG_NOSIGNAL);
#else
    return ::send(fd,data,n,0);
#endif
}
inline void ownSocket(const std::string& path){struct stat s{};require(lstat(path.c_str(),&s)==0,"socket endpoint missing");if(!S_ISSOCK(s.st_mode)||s.st_uid!=geteuid()||(s.st_mode&0777)!=0600)throw std::runtime_error("endpoint must be an owned non-symlink Unix socket with mode 0600");}
}
#endif
