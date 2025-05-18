#!/bin/bash

ORIGINAL_DIR=$(pwd)
cd ../cpp/build
silo_id=4
number=$((0*5 + ${silo_id}))
data_path="/home/dataset/hybrid/YouTube-audio/YouTube_${number}.fivecs"
scalardata_path="/home/dataset/hybrid/YouTube-audio/meta_${number}.txt"
cluster_path="/home/dataset/hybrid/YouTube-audio/YouTube_${number}.cluster/cluster"
collection_name="LOCAL_DATA_${number}"
milvus_port="50055"
cluster_option="OFF"
milvus_option="OFF"
alpha=0.05
cluster_num=10
ipaddr=localhost:50054
 
./silo --ip=$ipaddr --id=$number --data-path=$data_path --scalardata-path=$scalardata_path --cluster-path=$cluster_path --index-type=HNSW --collection-name=$collection_name --milvus-port=$milvus_port --cluster-option=$cluster_option --milvus-option=$milvus_option --alpha=$alpha --cluster-num=$cluster_num

cd "$ORIGINAL_DIR"