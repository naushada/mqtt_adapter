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
    if(SERVICE_TYPE_NOTIFY_TELEMETRY_DATA == svt) {
        evt->events = EPOLLHUP | EPOLLOUT;
    }
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

static int32_t getLength(int32_t channel) {
    ssize_t len;
    int32_t offset = 0;
    char onebyte;
    char length_str[256];
    memset(length_str, 0, sizeof(length_str));

    while(1) {
        len = read(channel, (void *)&onebyte, 1);
        if(len > 0 && '{' == onebyte) {
            return(atoi(length_str));
        } else if( offset >= sizeof(length_str)) {
            memset(length_str, 0, sizeof(length_str));
            offset = 0;
        } else {
            length_str[offset++] = onebyte;
        }
    }
    return(0);
}

static char* getData(int32_t channel, int32_t length) {
    char *buff = NULL;
    ssize_t len;
    ssize_t offset = 0;
    buff = (char *)malloc(length);
    if(NULL != buff) {

        memset(buff, 0, length);
        buff[offset] = '{';

        while(offset != (length-1)) {
            len = read(channel, (void *)&buff[1+offset], ((length-1) - offset));
            offset += len;
            if(len < 0) break;
        }
        return(buff);
    }

    return(NULL);
}

static int32_t sentToPeer(int32_t handle, const char* data, int32_t length) {
    ssize_t offset = 0;
    int32_t ret = -1;

    while(offset != length) {
        ret = send(handle, data+offset, length-offset, 0);
        if(ret > 0) {
            offset += ret;
            continue;
        }

        fprintf(stderr, "%s:%d send to channel:%d failed\n", __FUNCTION__, __LINE__, handle);
        break;
    }
    return(offset);
}

static int32_t handleIO(int32_t channel, service_type_t svc, struct epoll_event *evtlist[], int32_t evtCount) {
    switch (svc)
    {
    case SERVICE_TYPE_TELEMETRY:
    {
        char *buff = NULL;
        ssize_t offset = 0;
        int32_t len = 0;
        uint32_t content_length = getLength(channel);
        buff = getData(channel, content_length);
        if(NULL != buff) {
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
                    len = sendToPeer(handle, hdr, offset);
                    free(hdr);
                    fprintf(stderr, "%s:%d sent bytes(header):%d\n", __FUNCTION__, __LINE__, offset);
                    /*send payload/contents*/
                    len = sendToPeer(handle, buff, content_length);
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
    int32_t handle = socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), IPPROTO_TCP);

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
            if(EINPROGRESS == errno) {
                fprintf(stderr, "%s:%d connecting to Telemetry server on port 28989 is in-progress\n", basename(__FILE__), __LINE__);
                return(handle);
            }
            // TODO:: Add Error log
            fprintf(stderr, "%s:%d Unable to connect to Telemetry Server\n", basename(__FILE__), __LINE__);
            close(handle);
            return(-1);
        }
    }
    return(handle);
}

int main(int argc, char *argv[])
{
    pid_t pid;
    int32_t mFd[2];
    if(pipe(mFd)) {
        fprintf(stderr,"%s:%d %s",__FUNCTION__,__LINE__, "pipe for Fd is failed\n");
        return(-1); 
    }

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

        if(mFd[0] > 0) {
            close(mFd[0]);
        }

        /* @brief override stderr stream of child process with mFd[1] */
        if((mFd[1] > 0) && (dup2(mFd[1], 2)) < 0) {
            perror("dup2: Failed to map stderr\n");
            return(-1);
        }

        if(execvp(_argv[0], _argv) < 0) {
            fprintf(stderr, "%s:%d %s\n", basename(__FILE__), __LINE__,  "Spawning of mosquitto broker is failed");
            perror("Error:");
        }
    }

    /* @brief Start mosquitto_sub - i.e. subscriber to broker only when broker is running */
    if(mFd[1] > 0) {
        close(mFd[1]);
    }

    ssize_t len = 0;
    char onebyte;
    char word[128];
    ssize_t offset = 0;
    memset(word, 0, sizeof(word));
    /* @brief Start receiving mosquitto_broker stderr output to parent process. once mosquitto broker running, it prints to stderr "running" */
    while(1) {
        /* let mosquitto broker settled down so that subscriber will be able to connect to broker. */
        len = read(mFd[0], (void *)&onebyte, 1);
        if(len > 0 && ' ' == onebyte) {
            offset = 0;
            memset(word, 0, sizeof(word));
            continue;
        } else if(len > 0 && '\n' == onebyte) {
            /*@brief broker is running, we can start mosquitto_sub - subscriber now. */
            if(!strncmp("running", word, 7)) {
                close(mFd[0]);
                break;
            }
        } else if(offset >= sizeof(word)) {
            offset = 0;
        } else {
            word[offset++] = onebyte;
        }
    }
    
    int32_t Fd[2];
    ///@brief Parent process
    if(pipe(Fd)) {
        fprintf(stderr,"%s:%d %s",__FUNCTION__,__LINE__, "pipe for Fd is failed\n");
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
            argv[2],
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

        if(execvp(_argv[0], _argv) < 0) {
            fprintf(stderr, "%s:%d %s\n", basename(__FILE__), __LINE__,  "Spawning of mosquitto_sub is failed");
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
            fprintf(stderr,"%s:%d %s",basename(__FILE__),__LINE__, "creation of epollFd failed\n");
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

        int32_t connFd = startAndConnectTCPClient("0.0.0.0", 28989);

        if(connFd < 0) {
            fprintf(stderr, "%s:%d Connect to Telemetry Server Failed for Vehicle Data\n", basename(__FILE__), __LINE__);
            exit(0);
        }

        fprintf(stderr, "%s:%d Telemetry client Fd:%d\n", basename(__FILE__), __LINE__, connFd);
        struct epoll_event *clntevt = (struct epoll_event*)malloc(sizeof(struct epoll_event));

        if(NULL == clntevt) {
            exit(1);
        }

        evtlist[1] = clntevt;
        ret = registerToepoll(epollFd, connFd, SERVICE_TYPE_NOTIFY_TELEMETRY_DATA , clntevt);

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
                    fprintf(stderr, "%s:%d channel:%d svc:%d\n", basename(__FILE__), __LINE__, channel, (int32_t)svc);

                    if(activeEvent[idx].events & EPOLLIN) {
                        if(handleIO(channel, svc, evtlist, 3) < 0) {
                            break;
                        }
                    } else if(activeEvent[idx].events & EPOLLHUP) {
                        fprintf(stderr, "%s:%d Telemetry Server is Down ... Attempting the connection\n", basename(__FILE__),__LINE__);
                        close(channel);
                        epoll_ctl(epollFd, EPOLL_CTL_DEL, channel, NULL);
                        int32_t connFd = startAndConnectTCPClient("0.0.0.0", 28989);
                        if(connFd > 0) {
                            registerToepoll(epollFd, connFd, SERVICE_TYPE_NOTIFY_TELEMETRY_DATA , clntevt);
                        }
                    } else if(activeEvent[idx].events & EPOLLOUT) {
                        struct sockaddr_in peer;
                        socklen_t peer_len = sizeof(peer);

                        if(getpeername(channel, (struct sockaddr *)&peer, &peer_len)) {
                            ///@brief Error...
                            fprintf(stderr, "%s:%d getpeername error:%d\n", basename(__FILE__),__LINE__, errno);
                            /*
                            if(ENOTCONN == errno) {
                                int so_error = -1;
                                socklen_t len = sizeof(so_error);
                                if(getsockopt(channel, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
                                    
                                } else if(!so_error) {
                                    fprintf(stderr, "%s:%d Connected to TCP Server\n", basename(__FILE__),__LINE__);
                                    activeEvent[idx].events = EPOLLHUP | EPOLLIN;
                                    epoll_ctl(epollFd, EPOLL_CTL_MOD, channel, &activeEvent[idx]);
                                }
                            }*/
                        } else {
                            fprintf(stderr, "%s:%d Connected to TCP Server successfully\n", basename(__FILE__),__LINE__);
                            clntevt->data.u64 = elm;
                            clntevt->events = EPOLLHUP | EPOLLIN;
                            epoll_ctl(epollFd, EPOLL_CTL_MOD, channel, clntevt);
                        }
                    }
                }
            }
        }
    }
    return(0);
}


#endif /*__adapter_c__*/