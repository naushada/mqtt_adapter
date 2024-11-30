#ifndef __adapter_h__
#define __adapter_h__

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <getopt.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <netdb.h>
#include <stdarg.h> 
#include <sys/timerfd.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <libgen.h>

typedef union {
    unsigned int num;
    unsigned char ch[16];
}itoa_t;

typedef enum {
    SERVICE_TYPE_SIGNAL,
    SERVICE_TYPE_TELEMETRY,
    SERVICE_TYPE_NOTIFY_TELEMETRY_DATA,
    SERVICE_TYPE_INVALID
}service_type_t;

static int32_t startAndConnectTCPClient(const char* host, uint16_t port);
static int32_t createAndRegisterSignal(int32_t epollFd, struct epoll_event *evt);
static int32_t registerToepoll(int32_t epollFd, int32_t fd, service_type_t svt, struct epoll_event *evt);
static int32_t removeFromepoll(int32_t epollFd, int32_t fd, service_type_t svt, struct epoll_event *evt);
static int32_t modifyEpoll(int32_t epollFd, int32_t fd, service_type_t svt, int32_t mask, struct epoll_event *evt);
static int32_t getHandle(service_type_t svt, struct epoll_event *evt[], int32_t evtCount);
static int32_t handleIO(int32_t channel, service_type_t svc, struct epoll_event *evtlist[], int32_t evtCount);
static int32_t getLength(int32_t channel);
static char* getData(int32_t channel, int32_t length);
static int32_t sentToPeer(int32_t channel, const char* data, int32_t length);

#endif /*__adapter_h__*/