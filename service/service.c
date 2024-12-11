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

static char* getValue(char* argv[], int32_t at);

static char* getPeerHost(char* argv[]) {
    return(getValue(argv, 1));
}

static char* getUser(char* argv[]) {
    return(getValue(argv, 2));
}

static char* getPassword(char* argv[]) {
    return(getValue(argv, 3));
}

static char* getValue(char* argv[], int32_t at) {
    char *value = (char *)malloc(256);
    size_t idx = 0;
    char isValue = 0;
    size_t offset = 0;

    if(NULL == value) return(NULL);

    memset(value, 0, 256);
    /* e.g. --peer-host=<ip address> --user=<value> --password=<value> */
    while(argv[at][idx] != '\0') {
        if(argv[at][idx] == '=') {
            isValue = 1;
        } else if(offset >= 255) {
            offset = 0;
            memset(value, 0, 256);
        } else if(isValue) {
            value[offset++] = argv[at][idx];
        }
        ++idx;
    }

    return(value);
}

/**
 * @brief This Function reads the input format %d %s, where %d is for length , %s for data of fixed length indicated by %d length.
 *        It first reads the length and then allocated memory for length size and then reads data of that length into that memory.
 *        The delimeter between length and data is space.
 * @param channel reading length and data on this channel/fd.
 * @return memory allocated from heap will have value and caller must free the heap memory if it's not NULL.
 */
static char* getTopic(int32_t channel) {
    char *buff = NULL;
    ssize_t len;
    int32_t offset = 0;
    char onebyte;
    char length_str[256];
    memset(length_str, 0, sizeof(length_str));

    /*The input received has this format length value, the delimiter between length and value is ' ' (space)*/
    while(1) {

        len = read(channel, (void *)&onebyte, 1);
        if(len > 0 && ' ' == onebyte) {
            length_str[offset] = '\0';
            break;
        } else if(len > 0 && offset >= sizeof(length_str)) {
            offset = 0;
            memset(length_str, 0, sizeof(length_str));
        } else {
            length_str[offset++] = onebyte;
        }
    }

    uint32_t length = atoi(length_str);
    offset = 0;

    buff = (char *)malloc(length+8);
    if(NULL != buff) {
        memset(buff, 0, length+8);

        while(offset != length) {
            len = read(channel, (void *)&buff[offset], (length - offset));
            offset += len;
            if(len < 0) break;
        }

        buff[offset++] = '/';
        buff[offset++] = '#';
        buff[offset] = '\0';
    }

    return(buff);
}

int main(int argc, char *argv[])
{
    if(argc < 3) {
        fprintf(stderr,"%s:%d %s%d",basename(__FILE__),__LINE__, " Invalid number of commandline arguments\n"
                       "--peer-host=<> --user=<> --password=<>\n"
                       "argc:", argc);
        return(-1); 
    }

    pid_t pid = -1;
    int32_t Fd[2];

    if(pipe(Fd)) {
        fprintf(stderr,"%s:%d %s",basename(__FILE__),__LINE__, " pipe for m_Fd is failed\n");
        return(-1); 
    }

    ///@brief 
    pid = fork();
    if(!pid) {
        ///@brief This is a Child Process.
        if(Fd[0] > 0) {
            close(Fd[0]);
        }

        /**
         * @brief map stderr stream of smtc process to Fd[1], If smtc process writes anything
         *        at stderr stream it will be delivered to Fd[1] stream/file descriptor.Anything
         *        received at Fd[1] will be available to Fd[0] to other process, This way we can establish the inter process 
         *        communication between processes.
         */

        if((Fd[1] > 0) && (dup2(Fd[1], 2)) < 0) {
            perror("dup2: Failed to map stderr");
            return(-1);
        }
        
        char* _argv[] = {
            "/opt/app/smtc",
            "--role", "client",
            "--peer-host", getPeerHost(argv),
            "--peer-port", "48989",
            "--userid", getUser(argv),
            "--password", getPassword(argv),
            "--local-host", "0.0.0.0",
            "--local-port", "38989",
            (char *)NULL
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

        char topic[256];
        char *value = getTopic(Fd[0]);

        if(NULL != value) {
            memset(topic, 0, sizeof(topic));
            strncpy(topic, value, sizeof(topic));
            free(value);
        }

        ///@brief This is Parent
        char* _argv[] = {
            "/opt/app/mqtt_adapter",
            "--topic", topic,
            (char *)NULL
        };

        if(execvp(_argv[0], _argv) < 0) {
            fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__,  "Spawning of mqtt_adapter is failed");
            perror("Error:");
        }
    }

    return(0);
}


#endif /*__service_c__*/