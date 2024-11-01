#ifndef __adapter_c__
#define __adapter_c__

#include "adapter.h"

static int32_t getHandle(service_type_t svt, struct epoll_event *evt[], int32_t evtCount) {
    int32_t idx;
    int32_t channel;
    service_type_t svc;

    for(idx = 0; idx < evtCount; ++idx) {
        int64_t elm = evt[idx]->data.u64;
        channel = (elm >> 32) & 0xFFFFFFFFU;
        svc = (elm >> 24) & 0xFFU;
        //printf("%s:%d channel:%d svc:%d\n", __FUNCTION__, __LINE__, channel, (int32_t)svc);

        if(svc == svt) {
            return(channel);
        }
    }
    return(-1);
}

static int32_t registerToepoll(int32_t epollFd, int32_t fd, service_type_t svt, struct epoll_event *evt) {
    evt->events = EPOLLHUP | EPOLLIN;
    evt->data.u64 = ((((uint64_t)(fd & 0xFFFFFFFFU)) << 32U) |
                        (uint64_t)((svt & 0xFFU) << 24U));
    int32_t ret = epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, evt);
    /*printf("%s:%d Fd:%d svc:%d Registered to epoll\n", __FUNCTION__, __LINE__, fd, (int32_t)svt);*/
    return(ret);
}

static int32_t removeFromepoll(int32_t epollFd, int32_t fd, service_type_t svt, struct epoll_event *evt) {
    return(0);
}

static int32_t modifyEpoll(int32_t epollFd, int32_t fd, service_type_t svt, int32_t mask, struct epoll_event *evt) {
    return(0);
}

static int32_t handleIO(int32_t channel, service_type_t svc, struct epoll_event *evtlist[], int32_t evtCount) {
    switch (svc)
    {
    case SERVICE_TYPE_TELEMETRY:
    {
        char *buff = NULL;
        ssize_t len;
        int32_t offset = 0;
        char onebyte;
        char length_str[256];
        memset(length_str, 0, sizeof(length_str));

        while(1) {
            len = read(channel, (void *)&onebyte, 1);
            if(len > 0 && '{' == onebyte) 
                break;
            length_str[offset++] = onebyte;
        }

        uint32_t content_length = atoi(length_str);
        offset = 0;
        buff = (char *)malloc(content_length);
        if(NULL != buff) {
            memset(buff, 0, content_length);
            buff[offset] = '{';

            while(offset != (content_length-1)) {
                len = read(channel, (void *)&buff[1+offset], ((content_length-1) - offset));
                offset += len;
                if(len < 0) break;
            }

            char cl[256];
            memset(cl, 0, sizeof(cl));
            ssize_t cnt = snprintf(cl, sizeof(cl)-1, "Content-Length: %d\r\n", content_length);
            const char* header[] = {
                "POST /api/v1/telemetry/data HTTP/1.1\r\n",
                "Host: 0.0.0.0:28989\r\n",
                "Connection: Keep-Alive\r\n",
                "Content-Type: application/vnd.api+json\r\n",
                cl,
                "\r\n",
                NULL
            };

            int32_t handle = getHandle(SERVICE_TYPE_NOTIFY_TELEMETRY_DATA, evtlist, evtCount);
            offset = 0;
            char* hdr = (char *)malloc(1024);
            if(NULL != hdr) {
                memset(hdr, 0, 1024);
                ssize_t ccount = 0;
                int32_t index = 0;
                int32_t ret = 0;

                for(offset = 0; header[index]; index++) {
                    ccount = sprintf((hdr + offset), "%s", header[index]);
                    offset += ccount;
                }

                if(handle > 0) {
                    /*sending header*/
                    len = offset;
                    offset = 0;
                    while(offset != len) {
                        ret = send(handle, hdr+offset, len-offset, 0);
                        if(ret > 0) {
                            offset += ret;
                            continue;
                        }
                        fprintf(stderr, "%s:%d send to channel:%d failed\n", __FUNCTION__, __LINE__, channel);
                        break;
                    }
                    free(hdr);
                    fprintf(stderr, "%s:%d sent bytes(header):%d\n", __FUNCTION__, __LINE__, offset);
                    /*send payload/contents*/
                    offset = 0;
                    while(offset != content_length) {
                        ret = send(handle, (void *)&buff[offset], content_length-offset, 0);
                        if(ret > 0) {
                            offset += ret;
                            continue;
                        }
                        fprintf(stderr, "%s:%d send to channel:%d failed\n", __FUNCTION__, __LINE__, channel);
                        break;
                    }
                    free(buff);
                    fprintf(stderr, "%s:%d sent bytes(body):%d\n", __FUNCTION__, __LINE__, offset);
                }
            }
        } else {
            /* TODO: Error Handling */
        }
    }
        break;
    case SERVICE_TYPE_SIGNAL:
    {
        char buffer[128];
        recv(channel, (void*)buffer, sizeof(buffer) - 1, 0);
        /* break the infinite loop */
        exit(0);
    }
        break;
    case SERVICE_TYPE_NOTIFY_TELEMETRY_DATA:
    {
        //printf("%s:%d Not Expecting Request/Response\n", __FUNCTION__, __LINE__);
        char buffer[1024];
        ssize_t len =  recv(channel, (void*)buffer, sizeof(buffer), 0);
        if(len > 0) {
            buffer[len] = '\0';
            fprintf(stderr, "%s:%d From Telemetry server:%s", __FUNCTION__, __LINE__, buffer);
        }
    }
        break;
    default:
        break;
    }
    return(0);
}

int32_t createAndRegisterSignal(int32_t epollFd, struct epoll_event *evt) {
    /// @brief Adding Signal Processing.
    sigset_t mask;
    int32_t ret = -1;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigprocmask(SIG_BLOCK, &mask, NULL);
        
    int32_t sfd = signalfd(-1, &mask, SFD_NONBLOCK);
    if(sfd > 0) {
        ret = registerToepoll(epollFd, sfd, SERVICE_TYPE_SIGNAL, evt);
    }
    return(ret);
}

int32_t startAndConnectTCPClient(const char* host, uint16_t port) {
    int32_t ret = -1;
    int32_t handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(handle > 0) {

        struct addrinfo *result;
        struct sockaddr_in peerAddr;
        itoa_t itoa_data;
        itoa_data.num = port;
        int32_t s = getaddrinfo(host, /*(char *)itoa_data.ch*/"28989", NULL, &result);

        if(!s) {
            peerAddr = *((struct sockaddr_in*)(result->ai_addr));
            freeaddrinfo(result);
        }

        ret = connect(handle, (struct sockaddr *)&peerAddr, sizeof(peerAddr));
        if(ret < 0) {
            // TODO:: Add Error log
            close(handle);
            return(-1);
        }
    }
    return(handle);
}

int main(int argc, char *argv[])
{
    int32_t Fd[2];
    pid_t pid;

    ///@brief 
    pid = fork();
    if(!pid) {
        ///@brief This is a Child Process.
        char* _argv[] = {
            "/opt/app/mosquitto",
            "-c",
            "/opt/app/mosquitto.conf",
            "-p",
            "1883",
            //"-d",
            NULL
        };

        //setpgrp();
        if(execvp(_argv[0], _argv) < 0) {
            printf("%s:%d %s\n", __FUNCTION__, __LINE__,  "Spawning of mosquitto is failed");
            perror("Error:");
        }
    }

    sleep(2);
    ///@brief 
    if(pipe(Fd)) {
        fprintf(stdout,"%s:%d %s",__FUNCTION__,__LINE__, "pipe for m_Fd is failed\n");
        return(-1); 
    }

    pid = fork();
    if(!pid) {
        ///@brief This is a Child Process.
        if(Fd[0] > 0) {
            close(Fd[0]);
        }

        if((Fd[1] > 0) && (dup2(Fd[1], 1)) < 0) {
            perror("dup2: Failed to map stdout");
            return(-1);
        }

        ///@brief Alleviate the access priviledges of this process.
        //setuid(0);
        //seteuid(0);
        //setgid(0);
        char* _argv[] = {
            "/opt/app/mosquitto_sub",
            "-t",
            ///@brief Topic to be subscribed
            "Vehicle",
            ///@brief For verbose output.
            "-v",
            "-h",
            //"192.168.1.102",
            "0.0.0.0",
            "-p",
            "1883",
            "-F",
            "%l%p",
            ///@brief enable debug log
            //"-d",
            ///@brief Each Packet will have new line character in it.Enable this option if not required.
            //"-N",
            NULL
        };

        //setpgrp();
        if(execvp(_argv[0], _argv) < 0) {
            printf("%s:%d %s\n", __FUNCTION__, __LINE__,  "Spawning of mosquitto_sub is failed");
            perror("Error:");
        }
        close(Fd[1]);

    } else {
        /* This is a Parent Process */
        if(Fd[1] > 0) {
            close(Fd[1]);
        }

        int32_t ret = -1;
        int32_t epollFd = epoll_create1(EPOLL_CLOEXEC);

        if(epollFd < 0) {
            fprintf(stdout,"%s:%d %s",__FUNCTION__,__LINE__, "creation of epollFd failed\n");
            exit(1);
        }

        struct epoll_event *evtlist[5];
        struct epoll_event *signalevt = (struct epoll_event*)malloc(sizeof(struct epoll_event));
        if(NULL == signalevt) {
            return(-1);
        }
        evtlist[0] = signalevt;
        if(createAndRegisterSignal(epollFd, signalevt) < 0) {
            free(signalevt);
            exit(1);
        }

        while(1) {
            int32_t connFd = startAndConnectTCPClient("0.0.0.0", 28989);

            if(connFd < 0) {
                fprintf(stderr, "%s:%d Connect to TCP Server Failed for Vehicle Data\n", __FUNCTION__, __LINE__);
                sleep(1);
            } else {
                fprintf(stderr, "%s:%d Telemetry client Fd:%d\n", __FUNCTION__, __LINE__, connFd);
                struct epoll_event *clntevt = (struct epoll_event*)malloc(sizeof(struct epoll_event));

                if(NULL == clntevt) {
                    exit(1);
                }

                evtlist[1] = clntevt;
                ret = registerToepoll(epollFd, connFd, SERVICE_TYPE_NOTIFY_TELEMETRY_DATA , clntevt);
                fprintf(stderr, "%s:%d Connected to Telemetry server\n", __FUNCTION__, __LINE__);
                break;
            }
        }

        struct epoll_event *mqttsub = (struct epoll_event*)malloc(sizeof(struct epoll_event));
        if(NULL == mqttsub) {
            exit(1);
        }
        evtlist[2] = mqttsub;
        ret = registerToepoll(epollFd, Fd[0], SERVICE_TYPE_TELEMETRY, mqttsub);

        struct epoll_event activeEvent[5];
        while(1) {
            ret = epoll_wait(epollFd, (struct epoll_event *)activeEvent, 5, 1000);
            if(ret > 0) {
                int32_t idx;
                int32_t channel;
                service_type_t svc;

                for(idx = 0; idx < ret; ++idx) {
                    int64_t elm = activeEvent[idx].data.u64;

                    channel = (elm >> 32) & 0xFFFFFFFFU;
                    svc = (elm >> 24) & 0xFFU;
                    fprintf(stderr, "%s:%d channel:%d svc:%d\n", __FUNCTION__, __LINE__, channel, (int32_t)svc);

                    if(activeEvent[idx].events & EPOLLIN) {
                        if(handleIO(channel, svc, evtlist, 3) < 0) {
                            break;
                        }
                    }   
                }
            }
        }
    }
    return(0);
}


#endif /*__adapter_c__*/