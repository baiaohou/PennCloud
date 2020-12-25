/*
 * load-balancer.cc
 *
 *  Created on: Nov 17, 2020
 *      Author: Baiao Hou
 */

#include <stdio.h>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fstream>
#include <vector>
#include <bits/stdc++.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>
#include <sstream>
#include <regex>
#include <ctime>
#include <sys/file.h>
#include <map>
#include "helpers.h"
#include "http-request.h"
#include "global_constants.h"


using namespace std;


bool hasSuffix (string const &str, string const &suffix);
void sig_helper(int arg);
void* worker(void* comm_fd_arg);
string generate_rows();
void* worker_start_fserver(void* comm_fd_arg);

int fserver_to_restart;
int *backup_comm_fd;

class FServer { // short for front-end server
public:
	bool on_duty;
	string ip_addr;
	int port;
public:
	FServer();
	FServer(string ip_addr_str, string port_str) {
		on_duty = false;
		this->ip_addr = ip_addr_str;
		this->port = stoi(port_str);
	}
};

vector<FServer> fserver_pool;
map<int, int> fserver_connections; // port : connections


/**
 * Global variable for all functions to use
 */
bool debug_mode = false; // when user hits -v, debug mode is on (true)
vector<int> fd_pool; // a pool of all socket file descriptors
vector<pthread_t> thread_pool; // a pool of all threads / clients
string mailbox_dir; // mailbox directory input by user
vector<string> mailboxes; // a pool of all mailboxes
const regex pattern("(\\w+)(\\.)?(\\w*)@(\\w+)");

// mutex lockes
pthread_mutex_t individual_mb_lock[500];


/**
 * Entry of the program
 */
int main(int argc, char *argv[])
{

	/* Your code here */
	signal(SIGINT, sig_helper);

	/**
	 * Usage: ./frontend (-v) -p <port_no>
	 */
	int ch, portno = -1; // port number
	while ((ch = getopt(argc, argv, "p:va")) != -1) {
		switch (ch) {
			case 'p': // syntax: -p <portno>
				try {
					portno = stoi(optarg);
					// user input must be a positive int. If not, throw error
					if (portno < 0 || portno > 65535) {
						cerr << "Port number should be from 0 to 65535. Try again!\r\n";
						exit(1);
					}

				} catch (invalid_argument const &e) {
					// print error message for invalid input
					cout << "Invalid number! Try again." << endl;
					return -1;
				}
				break;
			case 'a':
				cerr << "Author: Baiao Hou / baiaohou\r\n";
				exit(1);
				break;
			case 'v':
				debug_mode = true;
				break;
			default:
				cerr << "Error! Usage: -p <portno> (-a) (-v) <txt-file>\r\n";
				exit(1);
				break;
		}
	}
	if (portno == -1) {
		cerr << "Error! Usage: -p <portno> (-a) (-v) <txt-file>\r\n";
		exit(1);
	}
	string txt;
	if (optind >= argc) {
		cerr << "Error! Usage: -p <portno> (-a) (-v) <txt-file>\r\n";
		exit(1);
	} else {
		// Copy mailbox directory name
		txt = argv[optind];
		printf("filename is {%s}\n", txt.c_str());
	}
	ifstream file(txt);
	if (!file) {
		cerr << "Error! Unable to open file.\r\n";
		exit(1);
	}
	string this_line;
	FServer nonsense("-1", "-1");
	fserver_pool.push_back(nonsense); // make the vector 1-indexed, so take place the 0th index first
	int count = 1;
	while (getline(file, this_line)) {
		string delim = ":";
		int pos = this_line.find(delim);
		string ip_addr = this_line.substr(0, pos);
		this_line.erase(0, pos + 1);
		string port = this_line.substr(0, pos);

		FServer curr(ip_addr, port);
		fserver_pool.push_back(curr);

		cout << "[" << count << "] " << fserver_pool[count].ip_addr <<  "+" << fserver_pool[count].port << endl;
		count++;
	}


	/**
	 * Set up the load balancer
	 */

	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		// get an error opening socket, terminate
		fprintf(stderr, "Cannot open socket (%s)\r\n", strerror(errno));
		exit(1);
	}
//	fd_pool.push_back(listen_fd); // <-------
	// set port as reusable
	// ref: https://piazza.com/class/keab699r8w55i6?cid=334
	int optval = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	/**
	 * Create a new socket
	 */
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(portno);
	/**
	 * Bind and listen to socket
	 */
	int bind_ok = bind(listen_fd, (struct sockaddr*) &servaddr, sizeof(servaddr));
	if (bind_ok < 0) {
		fprintf(stderr, "Cannot bind socket!\r\n");
		exit(1);
	}
	int listen_ok = listen(listen_fd, 100); // assume there will be 100+ concurrent connections
	if (listen_ok < 0) {
		fprintf(stderr, "Cannot listen to socket!\r\n");
		exit(1);
	}

	// initialize fserver_connections
	for (int i = 1; i < fserver_pool.size(); i++) {
		fserver_connections[i] = 0;
	}

	/**
	 * Create a thread to communicate with each front-end server
	 */
	pthread_t temp;
	pthread_create(&temp, NULL, worker, NULL);// create a thread
	pthread_detach(temp); // detach this thread from the main thread

	while (true) {
		cout << "hi\n";
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);
		int *comm_fd = (int*) malloc(sizeof(int));
		*comm_fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
		if (*comm_fd == -1) continue;

//		fd_pool.push_back(*comm_fd); // <------

		backup_comm_fd = comm_fd;

		///////////////////////
		int c = *(int*) comm_fd;

		string text;
		read_helper(c, text);
		printf("/////////////////////////////////////\n");
		printf("/////////////////////////////////////\n");
		cout << text;
		printf("/////////////////////////////////////\n");
		printf("/////////////////////////////////////\n");

		if (text[0] == '*') {
			// message from front-end, not from client, not HTTP request
			// parse Xuening's message
			// Format: *

			vector<string> tokens = string_to_tokens(text, '*');
			for (int i = 0; i < tokens.size(); i++) {
				cout << "{" << tokens[i] << "}" << endl;
			}

			if (tokens[1] == "plus") {
				cout << "THIS IS GOING TO INCREMENT " << tokens[2] << "'S CONN COUNT!"<< endl;
				int sender_port = stoi(tokens[2]);

				int port_to_be_increment = -1;

				for (int i = 1; i < fserver_pool.size(); i++) {
					if (fserver_pool[i].port == sender_port) {
						port_to_be_increment = i;
					}
				}
				fserver_connections[port_to_be_increment]++;
			} else if (tokens[1] == "minus") {
				cout << "THIS IS GOING TO DECREMENT " << tokens[2] << "'S CONN COUNT!"<< endl;
				int sender_port = stoi(tokens[2]);

				int port_to_be_decrement = -1;

				for (int i = 1; i < fserver_pool.size(); i++) {
					if (fserver_pool[i].port == sender_port) {
						port_to_be_decrement = i;
					}
				}
				fserver_connections[port_to_be_decrement]--;
			} else if (tokens[1] == "request") {
				cout << "Front-end server asks for load balancing info."<< endl;
				int sender_port = stoi(tokens[2]);
//				cout << "Need to send the inner HTML table rows back to PORT " << sender_port << endl;
				// TO-DO
				string html_content = generate_rows();

				int port_to_be_send = -1;

				for (int i = 1; i < fserver_pool.size(); i++) {
					if (fserver_pool[i].port == sender_port) {
						port_to_be_send = i;
					}
				}
				string new_addr = fserver_pool[port_to_be_send].ip_addr + ":";
				new_addr += to_string(fserver_pool[port_to_be_send].port);
				write(*comm_fd, html_content.c_str(), html_content.length());
			} else if (tokens[1] == "disable") {
				cout << "ACCEPT KILLING COMMAND!!"<< endl;
				// First kill the assigned fserver
				int sender_port = stoi(tokens[2]);
				// TO-DO
				string s_send = "*disable";

				int port_to_be_send = -1;

				for (int i = 1; i < fserver_pool.size(); i++) {
					if (fserver_pool[i].port == sender_port) {
						port_to_be_send = i;
					}
				}
				string new_addr = fserver_pool[port_to_be_send].ip_addr + ":";
				new_addr += to_string(fserver_pool[port_to_be_send].port);
				write(*comm_fd, s_send.c_str(), s_send.length());


			} else if (tokens[1] == "enable") {
				cout << "ACCEPT RECOVERING COMMAND!!"<< endl;
				int sender_port = stoi(tokens[2]);
				fserver_to_restart = sender_port;



				pthread_t temp;
				pthread_create(&temp, NULL, worker_start_fserver, comm_fd);// create a thread
				pthread_detach(temp); // detach this thread from the main thread



			}

			close(*comm_fd);
			free(comm_fd);

			continue;// important! Do go down!
		}
		/////////////////////////





		int least_active_fserver_conn = 99999; // Least active fserver connection
		int least_active_fserver = -1; // The least active (but alive) server index
		for (int i = 1; i < fserver_pool.size(); i++) {
			if (fserver_pool[i].on_duty) {
				if (fserver_connections[i] < least_active_fserver_conn) {
					least_active_fserver_conn = fserver_connections[i];
					least_active_fserver = i;
				}
			}
		}
		if (least_active_fserver == -1) {
			// no server is active
			vprint(debug_mode, "A client attempts to connect, but no active front-end servers now.\r\n");

			string str = response_generator("pages/inactive.html");
			//vprint(debug_mode, "%s", str.c_str());
			write(*comm_fd, str.c_str(), str.length());

		} else {
			// We have one least-active fserver!
			cout << "Server #" << least_active_fserver << " has " << fserver_connections[least_active_fserver] << " connection." << endl;

			vprint(debug_mode, "Redirecting this new connection to fserver #%d\r\n", to_string(least_active_fserver));

			string new_addr = fserver_pool[least_active_fserver].ip_addr + ":";
			new_addr += to_string(fserver_pool[least_active_fserver].port);
			string old_str = "$fs";
			//string str = response_generator("pages/redirect.html", old_str, new_addr);
			new_addr = "http://" + new_addr;
			string str = response_generator302(new_addr);
			//vprint(debug_mode, "%s", str.c_str());
			write(*comm_fd, str.c_str(), str.length());
//			fserver_connections[least_active_fserver]++; ////////////////////////////<----No longer needed
			// ^^ is done by Xuening's message
		}
		close(*comm_fd);
		free(comm_fd);
	}
	close(listen_fd);
	return 0;
}

/**
 *
 */
void* worker(void* comm_fd_arg) {
	while (true) {
		vprint(debug_mode, "%s\r\n", generate_time().c_str());

		// connect to each front-end server
		for (int i = 1; i < fserver_pool.size(); i++) {
			// 1-index
			struct sockaddr_in fserver_addr;
			int fd = socket(PF_INET, SOCK_STREAM, 0);
			inet_aton(fserver_pool[i].ip_addr.c_str(), &(fserver_addr.sin_addr));

			fserver_addr.sin_port = htons(fserver_pool[i].port);
			fserver_addr.sin_family = AF_INET;

			int ret = connect(fd, (struct sockaddr*) &fserver_addr, sizeof(fserver_addr));

			// Convert in format: FS[003] e.g.
			stringstream ss;
			ss << "FS[" << setw(3) << setfill('0') << i << ']';
			string ss_str = ss.str();

			if (ret == 0) {
				fserver_pool[i].on_duty = true;
				vprint(debug_mode, "  %s %s:%d ON DUTY (Conn: %d)\r\n",
				  ss_str.c_str(), fserver_pool[i].ip_addr.c_str(), fserver_pool[i].port, fserver_connections[i]);
			} else {
				fserver_pool[i].on_duty = false;
				vprint(debug_mode, "  %s (%s:%d) OFF DUTY\r\n",
				  ss_str.c_str(), fserver_pool[i].ip_addr.c_str(), fserver_pool[i].port);
				fserver_connections[i] = 0;
			}

			close(fd);
		}
//		printf("/////////////////////////////////////\n");
//		cout << generate_rows() << endl;
//		printf("/////////////////////////////////////\n");

		sleep(1);
	}
}




/**
 * A string helper function judging if a string is the suffix of another string
 */
bool hasSuffix (string const &str, string const &suffix) {
    if (str.length() >= suffix.length()) {
        return (0 == str.compare (str.length() - suffix.length(), suffix.length(), suffix));
    }
    return false;
}



/**
 * Define a signal handler
 * Reference: slides #2 Page 27
 */
void sig_helper(int arg) {
	/**
	 * When SIGINT is received, close sockets and end the work.
	 */

//	cout << "ACCEPT KILLING COMMAND!!"<< endl;
//	for (int i = 1; i < fserver_pool.size(); i++) {
//		int dying_port = fserver_pool[i].port;
//		cout << "This dying port is "<< dying_port << endl;
//		cout << "fd_pool size is "<< fd_pool.size() << endl;
//		// TO-DO
//		string s_send = "*disable";
//		int c = fd_pool[i-1];
//
//		string new_addr = fserver_pool[dying_port].ip_addr + ":";
//		new_addr += to_string(fserver_pool[dying_port].port);
//		write(c, s_send.c_str(), s_send.length());
//	}


//	cout << "about to execute shell command: sh reset.sh"<< endl;
//	string shell_str = "kill -9 $(lsof -i:5002|tail -1|awk '\"$1\"!=\"\"{print $2}')"; // "sh reset.sh"
//	FILE *pp = popen(shell_str.c_str(), "r"); // build pipe
//	pclose(pp);

	char* str = "-ERR Server shutting down\r\n";
	for (int i = 0; i < fd_pool.size(); i++) {
		if (i == 0) {
			// the first / 0th socket is the listen fd
			close(fd_pool.front());
			continue;
		}
		/**
		 * tell client that server is shutting down
		 */
		write(fd_pool[i], str, strlen(str));
		// close sockets
		close(fd_pool[i]);

		// cancel the threads from thread pool
		// Note: thread # is one less than socket #, so index here is i-1
		int status = pthread_cancel(thread_pool[i-1]);
	}
	exit(0);
}

string generate_rows() {
	string content;
	for (int i = 1; i < fserver_pool.size(); i++) {
		int curr_port = fserver_pool[i].port;
		bool curr_duty = fserver_pool[i].on_duty;
		int curr_conn = fserver_connections[i];

	  content += "<tr>";
	  {
	    // [Port No.]
	    content += "<td>" + to_string(curr_port) + "</td>";

	    // [On-duty]
	    content += "<td>";
	    content += curr_duty ? "<b style='color:darkgreen'>ACTIVE</b>" : "<b style='color:darkred'>INACTIVE</b>";
	    content += "</td>";

	    // [Active Connections]
	    content += "<td>" + (curr_duty ? to_string(curr_conn) : "/") + "</td>";

	    // [Enable button]
	    content += "<td>";
	    if (curr_duty) {
	      // when on-duty, enable not availale
	      content += "<button class='btn btn-default disabled'>Enable</button>";
	    } else {
	      // when off duty, enable yes
	      content += "<form action='/enable-fserver' method='post'>";
	      content += "<button name='port' value='" + to_string(curr_port) + "' class='btn btn-warning'>Enable</button>";
	      content += "</form>";
	    }
	    content += "</td>";

	    // [Disable button]
	    content += "<td>";
	      if (!curr_duty) {
	        // when off-duty, disable not availale
	        content += "<button class='btn btn-default disabled'>Disable</button>";
	      } else {
	        // when on duty, disable yes
	        content += "<form action='/disable-fserver' method='post'>";
	        content += "<button name='port' value='" + to_string(curr_port) + "' class='btn btn-danger'>Disable</button>";
	        content += "</form>";
	      }
	      content += "</td>";
	  }
	  content += "</tr>";
	}
	content += "\r\n";
	return content;
}

/**
 * Start a new thread (to avoid crash/stuck with main function).
 * Thru pipes, we call popen() to start a sub-process and execute shell cmd.
 */
void* worker_start_fserver(void* comm_fd_arg) {
	int comm_fd = *(int*) comm_fd_arg;
	cout << "Starting a new process...\n";

	string shell_str = "./frontend -v -p " + to_string(fserver_to_restart);
	FILE *pp = popen(shell_str.c_str(), "r"); // build pipe
	// Now our fserver is started! :)
}
