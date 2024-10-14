#ifndef __subscribe_hpp__
#define __subscribe_hpp__

#include <iostream>
#include <vector>

class MqttSubscriber {
    public:
        MqttSubscriber();
        ~MqttSubscriber();
        std::int32_t readTopicValue(std::string& out);
        void init();
        std::int32_t start();
    private:
        std::vector<std::int32_t> m_Fd;
        std::int32_t m_pid;

};


#endif /*__subscribe_hpp__*/