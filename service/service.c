#ifndef __service_c__
#define __service_c__

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


int main(int argc, char *argv[])
{
    pid_t pid = -1;
    char value[128];
    memset(value, 0, sizeof(value));

    if(argc > 1) {

        size_t idx = 0;
        char isValue = 0;
        size_t offset = 0;
        /* e.g. --peer-host=<ip address> */
        while(argv[1][idx] != '\0') {
            if(argv[1][idx] == '=') {
                isValue = 1;
            } else if(isValue) {
                value[offset++] = argv[1][idx];
            }
            ++idx;
        }

    } else {
        strncpy(value, "45.126.124.10", 127);
    }

    int32_t Fd[2];
    if(pipe(Fd)) {
        fprintf(stderr,"%s:%d %s",basename(__FILE__),__LINE__, " pipe for m_Fd is failed\n");
        return(-1); 
    }

    char *buff = NULL;
    ssize_t len;
    int32_t offset = 0;
    char onebyte;
    char length_str[256];
    memset(length_str, 0, sizeof(length_str));

    ///@brief 
    pid = fork();
    if(!pid) {

        ///@brief This is a Child Process.
        if(Fd[0] > 0) {
            close(Fd[0]);
        }

        if((Fd[1] > 0) && (dup2(Fd[1], 2)) < 0) {
            perror("dup2: Failed to map stderr");
            return(-1);
        }
        
        char* _argv[] = {
            "/opt/app/smtc",
            "--role", "client",
            "--peer-host", value,
            "--peer-port", "48989",
            "--userid", "user",
            "--password", "Pin@411048",
            "--local-host", "0.0.0.0",
            "--local-port", "38989",
            NULL
        };

        if(execvp(_argv[0], _argv) < 0) {
            fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__,  "Spawning of smtc is failed");
            perror("Error:");
        }
    } else {
        ///@brief This is a Parent process.
        if(Fd[1] > 0) {
            close(Fd[1]);
        }

        /*The input received has this format length value, the delimiter between length and value is ' ' (space)*/
        while(1) {
            len = read(Fd[0], (void *)&onebyte, 1);
            if(len > 0 && ' ' == onebyte) 
                break;
            length_str[offset++] = onebyte;
        }

        uint32_t length = atoi(length_str);
        offset = 0;
        buff = (char *)malloc(length+8);
        if(NULL != buff) {
            memset(buff, 0, length+8);

            while(offset != length) {
                len = read(Fd[0], (void *)&buff[offset], (length - offset));
                offset += len;
                if(len < 0) break;
            }
        }
        buff[offset++] = '/';
        buff[offset++] = '#';
        buff[offset] = '\0';

        ///@brief This is Parent
        char* _argv[] = {
            "/opt/app/mqtt_adapter",
            "--topic", buff,
            NULL
        };

        if(execvp(_argv[0], _argv) < 0) {
            fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__,  "Spawning of mqtt_adapter is failed");
            perror("Error:");
        }
    }
    return(0);
}


#endif /*__service_c__*/