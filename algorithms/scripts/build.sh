#!/bin/sh

mkdir ../cpp/build
cp -r ../refs ../cpp/build
cd ../cpp/build/
cmake ../
make