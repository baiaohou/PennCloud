#!/bin/sh
cd ~/T21/frontend
kill -9 $(lsof -i:5000|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:5001|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:5002|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:5050|tail -1|awk '"$1"!=""{print $2}') &
cd ~/T21/store
kill -9 $(lsof -i:8000|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:8001|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:8002|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:8003|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:8004|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:8005|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:8006|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:8007|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:8008|tail -1|awk '"$1"!=""{print $2}') &
kill -9 $(lsof -i:10000|tail -1|awk '"$1"!=""{print $2}')
