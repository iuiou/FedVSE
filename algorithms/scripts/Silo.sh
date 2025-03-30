#!/bin/bash
silo_id=0
data_path="/home/dataset/hybrid/deep/deep_${silo_id}.fivecs"
scalardata_path="/home/dataset/hybrid/deep/meta_${silo_id}.txt"
cluster_path="/home/yzengal/clusters/deep/deep_${silo_id}/cluster"
collection_name="LOCAL_DATA_deep_${silo_id}"

../cpp/build/silo --ip=0.0.0.0:50050 --id=$silo_id --data-path=$data_path --scalardata-path=$scalardata_path --cluster-path=$cluster_path --index-type=HNSW --collection-name=$collection_name