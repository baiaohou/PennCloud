#include <stdlib.h>
#include <string>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <time.h>
#include <unordered_set>
#include "account_service.h"
#include "helpers.h"
#include "global_constants.h"

// just used for fixing front-end server connection number bug
// will NOT make the front-end stateful
unordered_set<string> alive_users;

bool check_session(map<string, string> cookies, int my_port) {
	if (cookies.find("sid") == cookies.end() || cookies.find("user") == cookies.end()) {
		return false;
	}
	string time_login_str = cookies["sid"];
	long int time_login = stol(time_login_str);
	long int time_now = stol(get_timestamp());
	double time_passed = difftime(time_now, time_login);
	if (time_passed > SESSION_MAX_DURATION) {
		// meaning the session expired
		if (alive_users.find(cookies["user"]) != alive_users.end()) {
			alive_users.erase(cookies["user"]);
			sync_load_balancer_state(my_port, "minus");
		}
		return false;
	}
	if (alive_users.find(cookies["user"]) == alive_users.end()) {
		alive_users.insert(cookies["user"]);
		sync_load_balancer_state(my_port, "plus");
	}
	return true;
}

string try_login(string request_body, map<string, string> cookies, int my_port) {
	map<string, string> params = get_params_from_request_body(request_body);
	string user_name = params["username"];
	int percent = user_name.find("%40");
	user_name = user_name.substr(0, percent);
	printf("try_login, username is %s\n", user_name.c_str());

	string user_pswd = params["password"];

	// ask the KV master for a storage node to connect
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	if (!connect_to_kv_master(storage_node_ip, &storage_node_port, user_name, "REQUEST")) {
		return response_generator("pages/inactive.html");
	}

	// meaning such user does not exist
	if (strlen(storage_node_ip) == 0 || storage_node_port == -1) {
		return response_generator("pages/login-failed.html");
	}

	// make an RPC call to the KV store
	char result [BUFFER_SIZE];
	memset(result, 0, BUFFER_SIZE);
	if (!do_GET(storage_node_ip, storage_node_port, user_name, "pswd", result)) {
		return response_generator("pages/inactive.html");
	}
	if (IS_DEBUG) {
		cout << "GET(" << user_name << ", " << "pswd) returns: " << result << endl;
	}

	char* result_state = strtok(result, " ");
	char* result_content = strtok(NULL, " ");

	if (strcasecmp(result_state, "-ERR") == 0 || strcasecmp(result_content, user_pswd.c_str()) != 0) {
		// meaning the account does not exist or the password is incorrect
		return response_generator("pages/login-failed.html");
	} else {
		// when the user_name and user_pswd are correct
		// set a latest time stamp as its session id in the cookie
		map<string, string> cookie_to_set;
		cookie_to_set["sid"] = get_timestamp();
		cookie_to_set["user"] = user_name;
		cookie_to_set["contact_ip"] = string(storage_node_ip);
		cookie_to_set["contact_port"] = to_string(storage_node_port);
		alive_users.insert(user_name);
		sync_load_balancer_state(my_port, "plus");
		return response_generator_cookie("pages/home.html", cookie_to_set);
	}
}

string try_register(string request_body, int my_port) {
	map<string, string> params = get_params_from_request_body(request_body);
	string user_name = params["username"];
	int percent = user_name.find("%40");
	user_name = user_name.substr(0, percent);
	printf("try_register, username is %s\n", user_name.c_str());

	string pswd_original = params["password_original"];
	string pswd_confirm = params["password_confirm"];

	// password must be confirmed
	if (pswd_original != pswd_confirm) {
		return response_generator("pages/register-failed.html");
	}
	string user_pswd(pswd_confirm);

	// ask the KV master for a storage node to connect
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	if (!connect_to_kv_master(storage_node_ip, &storage_node_port, user_name, "CREATE")) {
		return response_generator("pages/inactive.html");
	}
	// the user already exists
	if (strlen(storage_node_ip) == 0 || storage_node_port == -1) {
		return response_generator("pages/register-failed.html");
	}

	// new user, begin registering
	char result2 [BUFFER_SIZE];
	memset(result2, 0, BUFFER_SIZE);
	if (!do_PUT(storage_node_ip, storage_node_port, user_name, "pswd", user_pswd, result2)) {
		return response_generator("pages/inactive.html");
	}
	if (IS_DEBUG) {
		cout << "PUT(" << user_name << ", " << "pswd, " << user_pswd << ") returns: " << result2 << endl;
	}
	map<string, string> cookie_to_set;
	cookie_to_set["sid"] = get_timestamp();
	cookie_to_set["user"] = user_name;
	cookie_to_set["contact_ip"] = string(storage_node_ip);
	cookie_to_set["contact_port"] = to_string(storage_node_port);
	alive_users.insert(user_name);
	sync_load_balancer_state(my_port, "plus");
	return response_generator_cookie("pages/register-ok.html", cookie_to_set);
}

string do_logout(map<string, string> cookies, int my_port) {
	map<string, string> cookie_to_set;
	cookie_to_set["sid"] = "0";
	cookie_to_set["user"] = "?";
	cookie_to_set["contact_ip"] = "0";
	cookie_to_set["contact_port"] = "0";
	alive_users.erase(cookies["user"]);
	sync_load_balancer_state(my_port, "minus");
	return response_generator_cookie("pages/login.html", cookie_to_set);
}

string change_pswd(string request_body) {
	map<string, string> params = get_params_from_request_body(request_body);
	string user_name = params["username"];
	int percent = user_name.find("%40");
	user_name = user_name.substr(0, percent);
	string pswd_original = params["password_original"];
	string pswd_new = params["password_new"];

	// ask the KV master for a storage node to connect
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	if (!connect_to_kv_master(storage_node_ip, &storage_node_port, user_name, "REQUEST")) {
		return response_generator("pages/inactive.html");
	}

	// meaning such user does not exist
	if (strlen(storage_node_ip) == 0 || storage_node_port == -1) {
		return response_generator("pages/change-pswd-failed.html");
	}

	char result2 [BUFFER_SIZE];
	memset(result2, 0, BUFFER_SIZE);
	if (!do_CPUT(storage_node_ip, storage_node_port, user_name, "pswd", pswd_original, pswd_new, result2)) {
		return response_generator("pages/inactive.html");
	}
	if (IS_DEBUG) {
		cout << "CPUT(" << user_name << ", " << "pswd, " << pswd_original
				<< " change to " << pswd_new << ") returns: " << result2 << endl;
	}
	char* result_state = strtok(result2, " ");
	if (strcasecmp(result_state, "+OK") != 0) {
		return response_generator("pages/change-pswd-failed.html");
	}
	return response_generator("pages/login.html");
}

bool connect_to_kv_master(char* storage_node_ip, int* storage_node_port, string user_name, string command) {
	// create socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return false;
	}

	// connect to the KV store master node
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(KV_MASTER_PORT);
	inet_pton(AF_INET, KV_MASTER_IP, &(servaddr.sin_addr));
	connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	char buffer [BUFFER_SIZE];

	// request a connection address to a storage node
	string to_write = command + "`" + user_name + "\r\n";
	memset(buffer, 0, BUFFER_SIZE);
	strcpy(buffer, to_write.c_str());
	if (!do_write(sockfd, buffer, strlen(buffer))) {
		cerr << "Login service: fail to connect with the KV store master\n" << endl;
		close(sockfd);
		return false;
	}

	if (IS_DEBUG) {
		cout << "Sent: " << to_write << "to kv master" << endl;
	}

	// receive the response from the KV store master node
	memset(buffer, 0, BUFFER_SIZE);
	char* curr_pos = buffer;
	while (strstr(buffer, "\r\n") == NULL) {
		int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
		if (IS_DEBUG) {
			cout << "n: " << n << endl;
		}
		if (n < 0) {
			cerr << "Login service: fail to connect with the KV store master\n" << endl;
			close(sockfd);
			return false;
		}
		curr_pos += n;
	}

	// close the connection
	close(sockfd);

	// parse the response
	char master_feedback [BUFFER_SIZE];
	memset(master_feedback, 0, BUFFER_SIZE);
	char* msg_end = strstr(buffer, "\r\n");
	strncpy(master_feedback, buffer, msg_end - buffer);
	if (IS_DEBUG) {
		cout << "KV master responds: " << master_feedback << endl;
	}

	// get the response code first: +OK or -ERR
	char* token = strtok(master_feedback, " ");
	if (strcasecmp(token, "+OK") != 0) {
		// meaning there is already such user
		return true;
	}
	// continue to parse the address of the storage node
	token = strtok(NULL, " ");
	char storage_node_addr [strlen(token) + 1];
	strcpy(storage_node_addr, token);

	token = strtok(storage_node_addr, ":");
	strcpy(storage_node_ip, token);
	token = strtok(NULL, ":");
	*storage_node_port = atoi(token);

	return true;
}

bool do_GET(char* storage_node_ip, int storage_node_port, string r, string c, char* result) {
	printf("in do_GET, row=%s, len=%ld\n", r.c_str(), r.length());
	check_storage_node_connection(storage_node_ip, &storage_node_port);
	if (IS_DEBUG) {
		cout << "do_GET connects to " << storage_node_ip << ":" << storage_node_port << endl;
	}
	// create socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return false;
	}
	// connect to the KV store storage node
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(storage_node_port);
	inet_pton(AF_INET, storage_node_ip, &(servaddr.sin_addr));
	connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	char buffer [BUFFER_SIZE];

	// make an RPC call
	string to_write = "GET`" + r + "`" + c + "\r\n";
	cout << "GET " << c << endl;
	memset(buffer, 0, BUFFER_SIZE);
	strcpy(buffer, to_write.c_str());
	if (!do_write(sockfd, buffer, strlen(buffer))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		close(sockfd);
		return false;
	}

	// receive the response from the KV store master node
	memset(buffer, 0, BUFFER_SIZE);
	char* curr_pos = buffer;
	while (strstr(buffer, "\r\n") == NULL) {
		int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
		if (n < 0) {
			cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
			close(sockfd);
			return false;
		}
		curr_pos += n;
	}

	char goodbye_msg [10];
	memset(goodbye_msg, 0, 10);
	strcpy(goodbye_msg, "quit`\r\n");
	if (!do_write(sockfd, goodbye_msg, strlen(goodbye_msg))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		close(sockfd);
		return false;
	}

	// close the connection
	close(sockfd);

	// parse the response
	char* msg_end = strstr(buffer, "\r\n");
	strncpy(result, buffer, msg_end - buffer);
//	if (IS_DEBUG) {
//		cout << "KV storage node " << "<" << storage_node_ip << ":" << storage_node_port << ">"
//				<< "responds: " << result << endl;
//	}

	return true;

}

bool do_PUT(char* storage_node_ip, int storage_node_port, string r, string c, string v, char* result) {
	printf("in do_PUT, row=%s, len=%ld\n", r.c_str(), r.length());
	check_storage_node_connection(storage_node_ip, &storage_node_port);
	if (IS_DEBUG) {
		cout << "do_PUT connects to " << storage_node_ip << ":" << storage_node_port << endl;
	}
	// create socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return false;
	}
	// connect to the KV store storage node
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(storage_node_port);
	inet_pton(AF_INET, storage_node_ip, &(servaddr.sin_addr));
	connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	char buffer [BUFFER_SIZE];

	// make an RPC call
	string to_write = "PUT`" + r + "`" + c + "`" + v +"\r\n";
//	cout << "to_write" << endl;
//	cout << to_write << endl;
	cout << "PUT " << c << ", to_write length: " << endl;
	cout << to_write.length() << endl;
	memset(buffer, 0, BUFFER_SIZE);
	strcpy(buffer, to_write.c_str());
	if (!do_write(sockfd, buffer, strlen(buffer))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		close(sockfd);
		return false;
	}

	// receive the response from the KV store master node
	memset(buffer, 0, BUFFER_SIZE);
	char* curr_pos = buffer;
	while (strstr(buffer, "\r\n") == NULL) {
		int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
		if (n < 0) {
			cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
			close(sockfd);
			return false;
		}
		curr_pos += n;
	}

	char goodbye_msg [10];
	memset(goodbye_msg, 0, 10);
	strcpy(goodbye_msg, "quit`\r\n");
	if (!do_write(sockfd, goodbye_msg, strlen(goodbye_msg))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		close(sockfd);
		return false;
	}

	// close the connection
	close(sockfd);

	// parse the response
	char* msg_end = strstr(buffer, "\r\n");
	strncpy(result, buffer, msg_end - buffer);
	if (IS_DEBUG) {
		cout << "KV storage node " << "<" << storage_node_ip << ":" << storage_node_port << ">"
				<< " responds: " << result << endl;
	}

	return true;

}

bool do_CPUT(char* storage_node_ip, int storage_node_port, string r, string c, string v1, string v2, char* result) {
	printf("in do_CPUT, row=%s, len=%ld\n", r.c_str(), r.length());
	check_storage_node_connection(storage_node_ip, &storage_node_port);
	if (IS_DEBUG) {
		cout << "do_CPUT connects to " << storage_node_ip << ":" << storage_node_port << endl;
	}
	// create socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return false;
	}
	// connect to the KV store storage node
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(storage_node_port);
	inet_pton(AF_INET, storage_node_ip, &(servaddr.sin_addr));
	connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	char buffer [BUFFER_SIZE];

	// make an RPC call
	string to_write = "CPUT`" + r + "`" + c + "`" + v1 + "`" + v2 +"\r\n";
	cout << "to_write" << endl;
	cout << to_write << endl;
	memset(buffer, 0, BUFFER_SIZE);
	strcpy(buffer, to_write.c_str());
	if (!do_write(sockfd, buffer, strlen(buffer))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		close(sockfd);
		return false;
	}

	// receive the response from the KV store master node
	memset(buffer, 0, BUFFER_SIZE);
	char* curr_pos = buffer;
	while (strstr(buffer, "\r\n") == NULL) {
		int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
		if (n < 0) {
			cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
			close(sockfd);
			return false;
		}
		curr_pos += n;
	}

	char goodbye_msg [10];
	memset(goodbye_msg, 0, 10);
	strcpy(goodbye_msg, "quit`\r\n");
	if (!do_write(sockfd, goodbye_msg, strlen(goodbye_msg))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		close(sockfd);
		return false;
	}

	// close the connection
	close(sockfd);

	// parse the response
	char* msg_end = strstr(buffer, "\r\n");
	strncpy(result, buffer, msg_end - buffer);
	if (IS_DEBUG) {
		cout << "KV storage node " << "<" << storage_node_ip << ":" << storage_node_port << ">"
				<< " responds: " << result << endl;
	}

	return true;

}

void check_storage_node_connection(char* storage_node_ip, int* storage_node_port) {
	// check the input storage address connectivity
	// create socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return;
	}
	// try to connect to the KV store storage node
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(*storage_node_port);
	inet_pton(AF_INET, storage_node_ip, &(servaddr.sin_addr));
	int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	// if original node can be connected
	if (ret == 0) {
		char ping_msg [15];
		memset(ping_msg, 0, 15);
		strcpy(ping_msg, "STATUS`\r\n");
		if (!do_write(sockfd, ping_msg, strlen(ping_msg))) {
			cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
			close(sockfd);
		} else {
			// read the response
			char buffer [BUFFER_SIZE];
			memset(buffer, 0, BUFFER_SIZE);
			char* curr_pos = buffer;
			bool is_wrong = false;
			while (strstr(buffer, "\r\n") == NULL) {
				int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
				if (n < 0) {
					cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
					is_wrong = true;
					break;
				}
				curr_pos += n;
			}
			close(sockfd);
			if (!is_wrong) {
				// parse the result
				char result [BUFFER_SIZE];
				memset(result, 0, BUFFER_SIZE);
				char* msg_end = strstr(buffer, "\r\n");
				strncpy(result, buffer, msg_end - buffer);
				if (IS_DEBUG) {
					cout << "Original storage node responds " << result << endl;
				}
				char* token = strtok(result, " ");
				if (strcasecmp(token, "+OK") == 0) {
					if (IS_DEBUG) {
						cout << "Original storage node is okay to connect" << endl;
					}
					return;
				}
			}
		}
	}

	// when you reach here, it means the original node is not available
	// get the string: <ip>:<port>
	stringstream ssTemp;
	ssTemp<<*storage_node_port;
	string storage_node_port_str = ssTemp.str();
	string storage_node_addr = string(storage_node_ip) + ":" + storage_node_port_str;

	for (int i = 0; i < kvstores.size(); i++) {
		// find the correct storage node group first
		if (kvstores[i].find(storage_node_addr) != kvstores[i].end()) {
			// try every storage node in this group
			auto itr = kvstores[i].begin();
			while (itr != kvstores[i].end()) {
				// get the ip and port separately
				string target_addr = itr->first;
				string target_node_ip = target_addr.substr(0, target_addr.find(":"));
				string target_node_port = target_addr.substr(target_addr.find(":") + 1);
				char ip_to_test [20];
				memset(ip_to_test, 0, 20);
				strcpy(ip_to_test, target_node_ip.c_str());
				char tmp_port [10];
				memset(tmp_port, 0, 10);
				strcpy(tmp_port, target_node_port.c_str());
				int port_to_test = atoi(tmp_port);

				// create socket
				int sock = socket(PF_INET, SOCK_STREAM, 0);
				if (sock < 0) {
					cerr << "Login service: fail to open a socket\n" << endl;
					itr++;
					continue;
				}
				// try to connect to the KV store storage node
				struct sockaddr_in storage_addr;
				bzero(&storage_addr, sizeof(storage_addr));
				storage_addr.sin_family = AF_INET;
				storage_addr.sin_port = htons(port_to_test);
				inet_pton(AF_INET, ip_to_test, &(storage_addr.sin_addr));
				// testing this node
				if (connect(sock, (struct sockaddr*)&storage_addr, sizeof(storage_addr)) == 0) {
					// on success
					char ping_msg [15];
					memset(ping_msg, 0, 15);
					strcpy(ping_msg, "STATUS`\r\n");
					if (!do_write(sock, ping_msg, strlen(ping_msg))) {
						cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
						close(sock);
						itr++;
						continue;
					}

					// read the response
					char buffer [BUFFER_SIZE];
					memset(buffer, 0, BUFFER_SIZE);
					char* curr_pos = buffer;
					bool is_wrong = false;
					while (strstr(buffer, "\r\n") == NULL) {
						int n = read(sock, curr_pos, BUFFER_SIZE - strlen(buffer));
						if (n < 0) {
							cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
							is_wrong = true;
							break;
						}
						curr_pos += n;
					}
					close(sock);
					if (is_wrong) {
						itr++;
						continue;
					}
					// parse the result
					char result [BUFFER_SIZE];
					memset(result, 0, BUFFER_SIZE);
					char* msg_end = strstr(buffer, "\r\n");
					strncpy(result, buffer, msg_end - buffer);
					if (IS_DEBUG) {
						cout << target_addr << " responds " << result << endl;
					}
					char* token = strtok(result, " ");
					if (strcasecmp(token, "+OK") == 0) {
						if (IS_DEBUG) {
							cout << "storage node: " << storage_node_ip << ":" << *storage_node_port
									<< " switched to " << ip_to_test << ":" << port_to_test << endl;
						}
						strcpy(storage_node_ip, ip_to_test);
						*storage_node_port = port_to_test;
						return;
					} else {
						itr++;
						continue;
					}
				} else {
					// on failure, continue trying
					close(sock);
					itr++;
					continue;
				}
			}
		}
	}
	// when reach here, we know all storage nodes in this group fail
	// no need to reset the storage_node_ip and storage_node_port, just let it fail later
}

void sync_load_balancer_state(int my_port, char* command) {
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return;
	}
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(LOAD_BALANCER_PORT);
	inet_pton(AF_INET, LOAD_BALANCER_IP, &(servaddr.sin_addr));
	int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if (ret < 0) {
		return;
	}
	char msg [20];
	string my_port_str = to_string(my_port);
	sprintf(msg, "*%s*%s", command, my_port_str.c_str());
	if (!do_write(sockfd, msg, strlen(msg))) {
		cerr << "Failed to sync with load balancer\n" << endl;
		close(sockfd);
		return;
	}
	close(sockfd);
}


