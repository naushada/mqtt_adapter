FROM ubuntu:jammy AS build
ENV TZ=Asia/Calcutta
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y upgrade
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-cacert \
    cmake \
    build-essential \
    libboost-all-dev \
    libssl-dev \
    wget \
    zlib1g-dev

RUN apt-get update && \
    apt-get -y upgrade && \
    apt-get -y install libzstd-dev && \
    apt-get -y install git

# MQTT release/2.0 compilation
WORKDIR /tmp
RUN git clone -b release/2.0 https://github.com/eclipse/mosquitto.git && \
    cd /tmp/mosquitto && \
    make install WITH_CJSON=no WITH_DOCS=no&&\
    ldconfig

RUN git clone https://github.com/google/googletest/ && \
    cd /tmp/googletest && \
    mkdir build && cd build && cmake .. && make install &&\
    cd .. &&\
    mkdir source && \
    cd source

COPY smtc /tmp/source/smtc
COPY mqtt /tmp/source/mqtt
COPY service /tmp/source/service
COPY mosquitto.conf /tmp/source/mosquitto.conf

RUN cd /tmp/source/smtc &&\
    mkdir build &&\
    cd build &&\
    cmake .. && make

RUN cd /tmp/source/mqtt &&\
    mkdir build &&\
    cd build &&\
    cmake .. && make

RUN cd /tmp/source/service &&\
    mkdir build &&\
    cd build &&\
    cmake .. && make

FROM ubuntu:jammy AS final

WORKDIR /opt/app
# Create a user group 'cordoba'
RUN groupadd cordoba
# Create a user 'edge' under 'cordoba'
RUN adduser edge
# Chown all the files to the app user.
RUN chown -R edge:cordoba /opt/app
# Switch to 'edge'
USER edge

COPY --from=build /tmp/source/smtc/build/smtc /opt/app/smtc
COPY --from=build /tmp/source/smtc/build/test/smtc_test /opt/app/test/smtc_test
COPY --from=build /tmp/source/mqtt/build/mqtt_adapter /opt/app/mqtt_adapter
COPY --from=build /tmp/source/mosquitto.conf /opt/app/mosquitto.conf
COPY --from=build /tmp/source/service/build/service /opt/app/service
COPY --from=build /tmp/mosquitto/src/mosquitto /opt/app/mosquitto
COPY --from=build /tmp/mosquitto/client/mosquitto_sub /opt/app/mosquitto_sub
USER root
COPY --from=build /tmp/mosquitto/lib/libmosquitto.so.1 /usr/lib/libmosquitto.so.1
RUN ln -s /usr/lib/libmosquitto.so.1 /usr/lib/libmosquitto.so
RUN ldconfig
USER edge
CMD ["/opt/app/service"]
