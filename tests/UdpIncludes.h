#ifndef UDPINCLUDES_H
#define UDPINCLUDES_H

#ifdef _WIN32
#  include <Winsock2.h>
#  include <Windows.h>
   typedef SOCKET sock_t;
   typedef const char *optval_p;
   typedef DWORD timeout_t;
   typedef int socklen_t;
#elif defined(__APPLE__)
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/ip.h>
#  include <unistd.h>
typedef int sock_t;
typedef const void *optval_p;
typedef struct timeval timeout_t;
#  define INVALID_SOCKET -1
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/ip.h>
#  include <linux/limits.h>
#  include <unistd.h>
   typedef int sock_t;
   typedef const void *optval_p;
   typedef struct timeval timeout_t;
#  define INVALID_SOCKET -1
#endif

#ifdef _WIN32
#  define CHECK_RETURN(errorCondition, msg)                         \
    if(errorCondition)                                              \
    {                                                               \
        std::cout << "Error: " << (msg)                             \
                  << " in line " << __LINE__                        \
                  << ", errno="  << WSAGetLastError() << std::endl; \
        exit(-1);                                                   \
    }
#else
#  define CHECK_RETURN(errorCondition, msg)               \
    if(errorCondition)                                    \
    {                                                     \
        std::cout << "Error: " << (msg)                   \
                  << " in line " << __LINE__              \
                  << ", errno="  << errno << std::endl;   \
        exit(-1);                                         \
    }
#endif

#endif /* UDPINCLUDES_H */
