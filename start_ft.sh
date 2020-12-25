#!/bin/sh
cd ~/T21/frontend
make all
./load-balancer -p 5050 fservers.txt &
./frontend -p 5000 &
./frontend -p 5001 &
./frontend -p 5002



