#!/bin/bash

ORIGINAL_DIR=$(pwd)
cd ../cpp/build

if [ -f /opt/intel/sgxsdk/environment ]; then
    source /opt/intel/sgxsdk/environment
else
    echo "Error: /opt/intel/sgxsdk/environment does not exist."
    exit 1
fi

ipaddr="localhost:50056"
ip_path="../../scripts/ip.txt"

./broker --broker-ip=$ipaddr --silo-ip=$ip_path

if [ $? -ne 0 ]; then  
    echo "Data broker FAIL"  
    exit 1  
fi 

cd "$ORIGINAL_DIR"



# cd ../cpp/build/
# ./broker --broker-ip=localhost:50056 --silo-ip=../../scripts/ip.txt