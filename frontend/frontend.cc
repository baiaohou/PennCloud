/*
 * frontend.cc
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
#include "helpers.h"
#include "http-request.h"
#include "account_service.h"
#include "global_constants.h"
#include "mailservice.h"
#include "fileservice.h"
#include "admin_console.h"

using namespace std;

bool hasSuffix (string const &str, string const &suffix);
void sig_helper(int arg);
void* worker(void* comm_fd_arg);
unordered_map<string, int> parseLine(string line);

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
	// test mailservice and store

	// create sock and connect to master
//	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
//	struct sockaddr_in masteraddr;
//	bzero(&masteraddr, sizeof(masteraddr));
//	masteraddr.sin_family = AF_INET;
//	masteraddr.sin_port = htons(10000);// default 10000
//	inet_pton(AF_INET, "127.0.0.1", &(masteraddr.sin_addr));// localhost
//	connect(sockfd, (struct sockaddr*)&masteraddr, sizeof(masteraddr));
//
//	char buffer[BUFFER_SIZE];// buffer for write and read
//
//	// create zimao
//	tmp_create_user(sockfd, "zimao");
//
//	// create xiaoze
//	tmp_create_user(sockfd, "xiaoze");
//
//	// create baiao
//	tmp_create_user(sockfd, "baiao");
//
//	// create xuening
//	tmp_create_user(sockfd, "xuening");
//
//
//	char req_ip[100];
//	int req_port;
//	// request zimao
//	tmp_request_user_node(sockfd, "zimao", req_ip, &req_port);
//
//	// request xiaoze
//	tmp_request_user_node(sockfd, "xiaoze", req_ip, &req_port);
//
//	// request baiao
//	tmp_request_user_node(sockfd, "baiao", req_ip, &req_port);
//
//	// request xuening
//	tmp_request_user_node(sockfd, "xuening", req_ip, &req_port);
//
//	printf("go to sendmail\n");
//	sendmail(sockfd, "zimao", "xiaoze@localhost", "test123");
//	printf("after send\n\n");
//	sleep(1);
//
//	printf("go to sendmail\n");
//	sendmail(sockfd, "zimao", "xiaoze@localhost", "test123");
//	printf("after send\n\n");
//	sleep(1);
//
//	printf("go to sendmail\n");
//	sendmail(sockfd, "zimao", "xiaoze@localhost", "test123");
//	printf("after send\n\n");
//	sleep(1);
//
//	printf("go to listmail\n");
//	tmp_request_user_node(sockfd, "xiaoze", req_ip, &req_port);
//	printf("req_ip=%s, req_port=%d\n", req_ip, req_port);
//	vector<string> result;
//	listmail("xiaoze", string(req_ip), req_port, result);
//	printf("\n after listmail\n");
//	for (int i = 0; i < result.size(); i++) {
//		printf("i=%d, result=%s\n", i, result[i].c_str());
//	}
//
//	sleep(5);

//	try_login();
//	string rcpt_address = "xuening@localhost";
//	string msg_content = "test123";
//	string sender_node_ip;
//	int sender_node_port;
//	sendmail("zimao", rcpt_address, msg_content, sender_node_ip, sender_node_port);


	// just for test
//	cout << "Start" << endl;
//	ifstream t("test.txt");
//	string str((std::istreambuf_iterator<char>(t)),
//            std::istreambuf_iterator<char>());
//	HTTPRequest h(str);
//	return 0;

	/* Your code here */
	signal(SIGINT, sig_helper);
	// in case of broken pipe
	signal(SIGPIPE,SIG_IGN);


	/**
	 * Usage: ./frontend (-v) -p <port_no>
	 */

	int ch, portno = -1; // port number
	while ((ch = getopt(argc, argv, "p:k:l:va")) != -1) {
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
					cerr << "Invalid number! Try again." << endl;
					return -1;
				}
				break;
			case 'k':// syntax: -k <ip:port>
			{
				string info(optarg);
				if (info.find(":") == string::npos) {
					cerr << "Error! Usage: -p <portno> -k <(ip:port)> -l <(ip:port)> (-a) (-v)\r\n";
					return -1;
				}
				vector<string> kv_ip_port = string_to_tokens(info, ':');
				KV_MASTER_IP = (char*) kv_ip_port[0].c_str();
				KV_MASTER_PORT = stoi(kv_ip_port[1]);
				break;
			}
			case 'l':// syntax: -l <ip:port>
			{
				string info(optarg);
				if (info.find(":") == string::npos) {
					cerr << "Error! Usage: -p <portno> -k <(ip:port)> -l <(ip:port)> (-a) (-v)\r\n";
					return -1;
				}
				vector<string> lb_ip_port = string_to_tokens(info, ':');
				LOAD_BALANCER_IP = (char*) lb_ip_port[0].c_str();
				LOAD_BALANCER_PORT = stoi(lb_ip_port[1]);
				break;
			}
			case 'a':
				cerr << "Author: Baiao Hou / baiaohou\r\n";
				exit(1);
				break;
			case 'v':
				debug_mode = true;
				IS_DEBUG = true;
				break;
			default:
				cerr << "Error! Usage: -p <portno> -k <(ip:port)> -l <(ip:port)> (-a) (-v)\r\n";
				exit(1);
				break;
		}
	}

	if (portno == -1) {
		cerr << "Error! Usage: -p <portno> -k <(ip:port)> -l <(ip:port)> (-a) (-v)\r\n";
		exit(1);
	}

	vprint(debug_mode, "[KV_MASTER_IP] %s\r\n", KV_MASTER_IP);
	vprint(debug_mode, "[KV_MASTER_PORT] %d\r\n", KV_MASTER_PORT);

	// read kv store server list
	ifstream kv_servers(KVSTORE_CONFIG_FILE);
	string line;
	while (getline(kv_servers, line)) {
		kvstores.push_back(parseLine(line));
	}
	kv_servers.close();
	if (IS_DEBUG) {
		cout << "kvstores contain: " << endl;
		for (int i = 0; i < kvstores.size(); i++) {
			cout << "cluster " << i << endl;
			for (auto itr = kvstores[i].begin(); itr != kvstores[i].end(); itr++) {
				cout << itr->first << endl;
			}
		}
	}


	/**
	 * Set up the frontend server
	 */

	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		// get an error opening socket, terminate
		fprintf(stderr, "Cannot open socket (%s)\r\n", strerror(errno));
		exit(1);
	} else {
		// add this socket into the pool
		fd_pool.push_back(listen_fd);
	}

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
	while (true) {
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);
		int *comm_fd = (int*) malloc(sizeof(int));
		*comm_fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
		if (*comm_fd == -1) break; // or will go into infinite loop
		// Updated debug message
		//vprint(debug_mode, "[%d] New connection\r\n", *comm_fd);
		// add comm_fd into socket pool
		fd_pool.push_back(*comm_fd);

		/**
		 * Create a new thread to start individual work
		 */
		pthread_t temp;

		//pthread_create(&temp, NULL, thread_helper, comm_fd);// create a thread
		pthread_create(&temp, NULL, worker, comm_fd);// create a thread
		pthread_detach(temp); // detach this thread from the main thread

		thread_pool.push_back(temp); // add this thread into the thread pool
	}
	return 0;
}

/**
 *
 */
void* worker(void* comm_fd_arg) {
	int comm_fd = *(int*) comm_fd_arg;



	HTTPRequest r(comm_fd);

	// If request is empty (i.e. from load balancer), close fd and exit
	if (r.empty) {
		vector<string> t;
		if (r.other.length() != 0) t = string_to_tokens(r.other, '*');
//		cout << "(((((((((((((((((((((((((((((((((((((" << endl;
//		cout << "[" << r.other << "]" << r.other.length() << endl;
//		if (r.other.length() != 0) cout << t[1] << endl;
//		cout << "(((((((((((((((((((((((((((((((((((((" << endl;
		vector<string> tokens = string_to_tokens(r.other, '*');
		if (r.other.length() != 0 && t[1] == "disable") {
			close(comm_fd);
//			pthread_exit(NULL);
			cout << "Killed by admin console" << endl;
			exit(0);
		}
		close(comm_fd);
		pthread_exit(NULL);
		return nullptr;
	}

	// Xuening: maybe here is the place where we direct the user to the correct service
	// Leo: Yes! Still working on this

	string http_method = r.method;
	string request_path = r.path;
	map<string, string> cookies = r.cookies;
	string request_body = r.body;
	int my_port = r.port_no;

	// only the registered users allowed to use our service
	// let them login if they are unregistered or have session expired
	if (!check_session(cookies, my_port)) {
		if (request_path != "/login" && request_path != "/register" && request_path != "/logout"
				&& request_path != "/register.html" && request_path != "/change-pswd.html" && request_path != "/change_pswd") {
			// --> this line only used by leo/ && request_path != "/mail-home.html" && request_path != "/home.html"
			string str = response_generator("pages/login.html");
			write(comm_fd, str.c_str(), str.length());
			close(comm_fd);
			pthread_exit(NULL);
		}
	}

	cout << "go to direct request_path" << endl;

	if (request_path == "/") {
		// when user enters 127.0.0.1:xxxx, where the path is "/" (root directory)
//		string response = response_generator("pages/home.html", "$name", cookies["user"].substr(0, cookies["user"].find("%40")));
		string response = response_generator("pages/home.html", "$name", cookies["user"]);

		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/login") {
		string response = try_login(request_body, cookies, my_port);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/register") {
		string response = try_register(request_body, my_port);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/register.html") {
		string response = response_generator("pages/register.html");
		write(comm_fd, response.c_str(), response.length());
//	} else if (request_path == "/home.html") { // <---- delete (only use by leo)
//		string response = response_generator("pages/home.html"); // <---- delete (only use by leo)
//		write(comm_fd, response.c_str(), response.length()); // <---- delete (only use by leo)
	} else if (request_path == "/logout") {
		string response = do_logout(cookies, my_port);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/mail-home.html") {
		string response = try_listmail(cookies);// put all cookies
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/mail-delete") {
		string response = try_deletemail(cookies, request_body);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/mail-send.html") {
		string tmpname = cookies["user"];
//		tmpname = tmpname.substr(0, tmpname.length() - 1) + "@penncloud";
		tmpname = tmpname + "@penncloud";
		string response = response_generator("pages/mail-send.html", "$this_email", tmpname);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/mail-reply.html") {
		// direct to mail-reply.html, fill in sender and recipient
		string currentUser = cookies["user"];// get current user from cookies
		currentUser = currentUser + "@penncloud";// append domain to user
		map<string, string> params = get_params_from_request_body(request_body);// map request_body
		string rcptEmail = params["rcptEmail"];// get rcptEmail from request_body
		rcptEmail.replace(rcptEmail.find("%40"), 3, "@");// replace %40 with "@"
		string response = response_generator("pages/mail-reply.html", "$this_email", currentUser, "$rcpt_email", rcptEmail);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/mail-forward.html") {
		// direct to mail-forward.html, fill in sender, subject and content
		string currentUser = cookies["user"];// get current user from cookies
		currentUser = currentUser + "@penncloud";// append domain to user
		map<string, string> params = get_params_from_request_body(request_body);// map request_body
		string entire_msg = params["entireMessage"];// get rcptEmail from request_body
		entire_msg = UrlDecode(entire_msg);
		string old_subject = params["oldSubject"];// get oldSubject from request_body
		old_subject = UrlDecode(old_subject);
		string response = response_generator("pages/mail-forward.html", "$this_email", currentUser, "$oldSubject", old_subject, "$msg_content", entire_msg);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/compose") {
		string response = try_sendmail(cookies, request_body);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/mail-detail") {
		string response = try_showdetail(cookies, request_body);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/drive-home.html") {
		// TODO, get file folder name
		string response = try_listfile(cookies, "root/");
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/drive-folder") {
		printf("frontend: direct to /drive-folder\n");
		map<string, string> params = get_params_from_request_body(request_body);
		string filedirectory = params["fullFilename"];
		while (filedirectory.find("%2F") != string::npos) {
			filedirectory.replace(filedirectory.find("%2F"), 3, "/");
		}
		filedirectory += "/";// add a slash, e.g. root/folder => root/folder/
		string response = try_listfile(cookies, filedirectory);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/upload") {
		// TODO, get file folder name
		printf("go to try_upload, request_body length: %ld\n", request_body.length());
		string response = try_upload(cookies, request_body);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/download") {
		string response = try_download(cookies, request_body);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/newFolder") {
		string response = try_createfolder(cookies, request_body);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/rename") {
		string response = try_rename(cookies, request_body);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/move") {
		string response = try_move(cookies, request_body);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/delete") {
		string response = try_deletefile(cookies, request_body);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/admin.html") {
		if (!is_admin(cookies)) {
			// No permission
			string response = response_generator("pages/admin-failed.html");
			write(comm_fd, response.c_str(), response.length());
		} else {
			string new_content1 = get_frontend_states(my_port);
			vector<map<string, string>> kv_states = get_storage_nodes_states();
			cout << "printing kv states: " << endl;
			for (int i = 0; i < kv_states.size(); i++) {
				cout << "Group " << i << endl;
				for (auto itr = kv_states[i].begin(); itr != kv_states[i].end(); itr++) {
					cout << "<" << itr->first << ":" << itr->second << ">"<< endl;
				}
			}
			string new_content2 = generate_backend_rows(kv_states);
			vector<vector<string>>* big_table = get_big_table(kv_states);
			string new_content3 = generate_bigtable_rows(*big_table);
			unordered_map<string, string> old_and_new;
			old_and_new["$content1"] = new_content1;
			old_and_new["$content2"] = new_content2;
			old_and_new["$content3"] = new_content3;
			string response = response_generator("pages/admin.html", old_and_new);
			write(comm_fd, response.c_str(), response.length());
			delete big_table;
		}
	} else if (request_path == "/change-pswd.html") {
		string response = response_generator("pages/change-pswd.html");
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/change_pswd") {
		string response = change_pswd(request_body);
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/enable-bserver") {
		start_storage_node(request_body);
		//string response = response_generator("pages/admin-action-ok.html");
		string response = response_generator302("/admin.html");
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/disable-bserver") {
		kill_storage_node(request_body);
		//string response = response_generator("pages/admin-action-ok.html");
		string response = response_generator302("/admin.html");
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/enable-fserver") {
		start_frontend_node(request_body);
		//string response = response_generator("pages/admin-action-ok.html");
		sleep(1);
		string response = response_generator302("/admin.html");
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/disable-fserver") {
		kill_frontend_node(request_body);
		//string response = response_generator("pages/admin-action-ok.html");
		sleep(1);
		string response = response_generator302("/admin.html");
		write(comm_fd, response.c_str(), response.length());
	} else if (request_path == "/lets-do-it") {

	}

	close(comm_fd);
	pthread_exit(NULL);
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
}

/**
 * This function is to parse a line in the server list file
 */
unordered_map<string, int> parseLine(string line) {
	int prevCommaIndex = -1;
	int commaIndex = line.find(",");
	unordered_map<string, int> res;
	while (commaIndex != line.npos) {
		res[line.substr(prevCommaIndex + 1, commaIndex - prevCommaIndex - 1)] = 0;
		prevCommaIndex = commaIndex;
		commaIndex = line.find(",", prevCommaIndex + 1);
	}
	res[line.substr(prevCommaIndex + 1, line.length() - prevCommaIndex + 1)] = 0;
	return res;
}
