#!/bin/bash

DIR_PATH="../cpp/build"


if [ -d "$DIR_PATH" ]; then
  echo "Directory $DIR_PATH already exists. Removing it..."
  rm -rf "$DIR_PATH"
fi

mkdir -p ../cpp/build
cp -r ../refs ../cpp/build
cd ../cpp/build/
cmake ../ -DUSE_SGX=ON
make -j 1