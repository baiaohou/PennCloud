#!/bin/sh
cd ~/T21/store
make all
./MasterNode stores.txt &
./StorageNode stores.txt 0 1 &
./StorageNode stores.txt 0 2 &
./StorageNode stores.txt 0 0 &
./StorageNode stores.txt 1 1 &
./StorageNode stores.txt 1 2 &
./StorageNode stores.txt 1 0 &
./StorageNode stores.txt 2 1 &
./StorageNode stores.txt 2 2 &
./StorageNode stores.txt 2 0
