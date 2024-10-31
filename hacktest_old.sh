#!/bin/sh
 
# enable debug to get data in /var/log/messages
tm set log.system[Vehicle].level="Debug"
#mkdir -p /mnt/user/ctrmgr/workdir/volumes/telemetryData/
Tpath="/tmp/telemetry"
echo Tpath is $Tpath
mkdir -p $Tpath
 
# Define a timestamp function
timestamp() {
  date +"%T" # current time
}
 
# Start loop
while [ 1 ]
export j=10
do
    # Start loop of 10 sec:
    for i in $(seq 1 10);
    do
        sleep 1
        echo `expr $i % $j`
        # Do soemting at every 1 sec:
        if [ `expr $i % $j` -eq 1 ]; then
            timestamp
        speed=$(grep 'DECODED: signal Vehicle speed' /var/log/messages | tail -1 | grep -Eo '[0-9]+$')
        rpm=$(grep 'DECODED: signal Engine rpm' /var/log/messages | tail -1 | grep -Eo '[0-9]+$')
        echo "speed = " $speed
        echo "rpm = " $rpm
        echo $speed > $Tpath/speed.txt
        echo $rpm > $Tpath/rpm.txt
        fi
        # Do soemting at every 5 sec:
        if [ `expr $i % $j` -eq 5 ]; then
        engine_load=$(grep 'DECODED: signal Calculated engine load' /var/log/messages | tail -1 | grep -Eo '[0-9]+$')
        module_voltage=$(grep 'DECODED: signal Control module voltage' /var/log/messages | tail -1 | grep -Eo '[0-9]+$')
        echo "engine load= " $engine_load
        echo "module voltage= " $module_voltage
        echo $engine_load > $Tpath/engine_load.txt
        echo $module_voltage > $Tpath/module_voltage.txt
        fi
        # Do soemting at every 10 sec:
        if [ `expr $i % $j` -lt 1 ]; then
        coolant_temperature=$(grep 'DECODED: signal Engine coolant temperature' /var/log/messages | tail -1 | grep -Eo '[0-9]+$')
        echo "coolant temperature= " $coolant_temperature
        echo $coolant_temperature > $Tpath/coolant_temperature.txt
        fi
    done
done