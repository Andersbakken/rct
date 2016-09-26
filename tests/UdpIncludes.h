#ifdef _WIN32
#  include <Windows.h>
#  include <Winsock2.h>
   typedef SOCKET sock_t;
   typedef const char *optval_p;
   typedef DWORD timeout_t;
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/ip.h>
#  include <linux/limits.h>
#  include <unistd.h>
   typedef int sock_t;
   typedef const void *optval_p;
   typedef struct timeval timeout_t;

#  define CHECK_RETURN(errorCondition, msg)               \
    if(errorCondition)                                    \
    {                                                     \
        std::cout << "Error: " << (msg)                   \
                  << " in line " << __LINE__              \
                  << ", errno="  << errno << std::endl;   \
        exit(-1);                                         \
    }
#endif
