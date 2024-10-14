#ifndef __subscriber_cpp__
#define __subscriber_cpp__

#include "subscriber.hpp"

extern "C" {
    #include <stdio.h>
    #include <string.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <fcntl.h>
}


MqttSubscriber::MqttSubscriber() {
    m_Fd.clear();
    m_pid = -1;
}

MqttSubscriber::~MqttSubscriber() {

}

std::int32_t MqttSubscriber::readTopicValue(std::string& out) {

    return(0);
}

void MqttSubscriber::init() {
    m_Fd.reserve(2);
    if(::pipe(m_Fd.data())) {
        std::cout << __FUNCTION__<<":"<<__LINE__ << "pipe for m_Fd is failed" << std::endl;
        return; 
    }
}

std::int32_t MqttSubscriber::start() {

    m_pid = ::fork();

    if(!m_pid) {
        fprintf(stderr, "\nChild Process\n");
        ///@brief This is a Child Process.
        ::close(m_Fd[0]);

        if(::dup2(m_Fd[1], fileno(stdout)) < 0) {
            perror("dup2: Failed to mapp stdout");
            return(-1);
        }

        //::setuid(0);
        //::seteuid(0);
        //::setgid(0);
        if(execlp("mosquitto_sub", "mosquitto_sub", "--debug", "-t test/topic -v", NULL) < 0) {
            std::printf("%s:%d %s\n", __FUNCTION__, __LINE__,  "Spawning of mosquitto_sub is failed");
            ::perror("Error:");
            ::close(m_Fd[1]);
        }

    } else {
        ///@brief This is a Parent Process.
        fprintf(stderr, "\nParent Process\n");
        ::close(m_Fd[1]);
        std::string buff;

        while(1) {
            buff.resize(1024);
            buff.clear();
            //std::cout << __FUNCTION__ <<":"<< __LINE__ << "Waiting for data to publish" << std::endl;
            auto len = ::read(m_Fd[0], buff.data(), buff.size());
            if(len > 0) {
                buff.resize(len);
                std::cout <<__FUNCTION__ <<":" << __LINE__ <<"Received Broker:" << buff << std::endl;
                //len = write(m_writeFd[1], buff, len);
            } else {
                //perror("read Failed:");
                continue;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    

    MqttSubscriber inst;
    inst.init();
    inst.start();
}

#endif /*__subscriber_cpp__*/