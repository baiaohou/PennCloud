# PennCloud

**PennCloud** is a ***Distributed Cloud Platform*** that supports cloud drive service *(analogous to Google Drive)* and email service *(analogous to Gmail)*.


## Team

Baiao Hou, Xuening Zhang, Zemin Zhao, Zimao Wang

*Computer & Information Science Department*

***University of Pennsylvania***

## Running by Shell Scripts

Please make sure you have this repository folder under `~/`, or you can also customize the paths in `start_smtp.sh`, `start_kv.sh` and `start_ft.sh`

(1) Run `reset.sh`

(2) Run `start_smtp.sh`

(3) Run `start_kv.sh`

(4) Run `start_ft.sh`

(5) Go to `127.0.0.1:5050` to access our service

Note: An admin account has been built in:

    Username: admin@penncloud 	
    Password: admin

	
By default, a session automatically expires after half an hour

Delete `S_0_0.log`, `S_0_1.log`, `S_0_2.log`, ..., and `Master.log` under the `store` folder, if you have run our service before and want a completely new key-value store.

If you would like to upload big files, we recommend you do the followings:

## Customized Running

Quick guide:

	cd ~/PennCloud/webmail
	make smtp
	./smtp -p 2500
	
	cd ~/T21/store
	make all
	./MasterNode stores.txt
	./StorageNode stores.txt 0 1
	./StorageNode stores.txt 0 2
	./StorageNode stores.txt 0 0
	./StorageNode stores.txt 1 1
	./StorageNode stores.txt 1 2
	./StorageNode stores.txt 1 0
	./StorageNode stores.txt 2 1
	./StorageNode stores.txt 2 2
	./StorageNode stores.txt 2 0
	
	cd ~/PennCloud/frontend
	make all
	./load-balancer -p 5050 fservers.txt
	./frontend -p 5000
	./frontend -p 5001
	./frontend -p 5002

### 1. SMTP server for receiving external emails

(1) `cd` to the folder `webmail` under `PennCloud`

(2) run `./smtp -p 2500`

`-p` specifies at which port you want to run the SMTP server (default will be 2500)
	 
### 2. Back-end servers (key-value store master node and storage nodes)

(1) `cd` to the folder `store` under `PennCloud`

(2) To make store clean, you can first delete `S_0_0.log`, `S_0_1.log`, `S_0_2.log`, ..., and `Mastet.log` if they exist

(3) make `MasterNode`

(4) make `StorageNode`

(5) run `./MasterNode -v -p <port number> [stores.txt]`

`-v` specifies whether to print the debugging messages

`-p` specifies at which port you want to run the master node (default will be 10000)

`[stores.txt]` is the configuration file for kvstores' clusters

(6) run `./StorageNode -v stores.txt <cluster index> <server index>`

`-v` specifies whether to print the debugging messages

`[stores.txt]` is the configuration file for kvstores' clusters

`<cluster index>` is the index of cluster

`<server index>` is the index of server in that cluster

### 3. Front-end servers and load balancer

(1) `cd` to the folder `frontend` under `PennCloud`

(2) make `all`

(3) run `./load-balancer -p <port number> -v [fservers.txt]`

`-p` (required) specifies at which port you want to run the load balancer

`-v` specifies whether to print the debugging messages

`[fservers.txt]` is the configuration file containing the addresses of all the front-end servers
	
(4) run `./frontend -p <port number> -k <ip:port> -l <ip:port> -v`

`-p` (required) specifies at which port you want to run this front-end server

`-k` specifies the address of the key-value store master node, default to `127.0.0.1:10000`

`-l` specifies the address of the load balancer, default to `127.0.0.1:5050`

`-v` specifies whether to print the debugging messages
