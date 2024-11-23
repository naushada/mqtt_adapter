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

        while(argv[1][idx] != '\0') {
            if(argv[1][idx] == '=') {
                isValue = 1;
            } else if(isValue) {
                value[offset++] = argv[idx];
            }
            ++idx;
        }

    } else {
        strncpy(value, "45.126.124.10", 127);
    }

    ///@brief 
    pid = fork();
    if(!pid) {
        ///@brief This is a Child Process.
        char* _argv[] = {
            "/opt/app/smtc",
            "--role", "client",
            //"--peer-host", "13.200.242.232",
            //"--peer-host", "10.203.77.36",
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
        sleep(2);
        ///@brief This is Parent
        char* _argv[] = {
            "/opt/app/mqtt_adapter",
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