#include "admin_console.h"
#include "global_constants.h"
#include "helpers.h"

using namespace std;


bool is_admin(map<string, string> cookies) {
	if (cookies.find("sid") == cookies.end() || cookies.find("user") == cookies.end()) {
		return false;
	}
	if (string(ADMIN_USERNAME) != cookies["user"]) {
		cout << cookies["user"].size() << endl;
		return false;
	}
	string time_login_str = cookies["sid"];
	long int time_login = stol(time_login_str);
	long int time_now = stol(get_timestamp());
	double time_passed = difftime(time_now, time_login);
	if (time_passed > SESSION_MAX_DURATION) {
		// meaning the session expired
		return false;
	}
	return true;
}

vector<map<string, string>> get_storage_nodes_states() {
	vector<map<string, string>> storage_nodes_states;
	for (int i = 0; i < kvstores.size(); i++) {
		// this is storage node group i
		map<string, string> curr_sn_group;
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
				curr_sn_group[target_node_port] = "-1";
				itr++;
				continue;
			}
			// try to connect to the KV store storage node
			struct sockaddr_in storage_addr;
			bzero(&storage_addr, sizeof(storage_addr));
			storage_addr.sin_family = AF_INET;
			storage_addr.sin_port = htons(port_to_test);
			inet_pton(AF_INET, ip_to_test, &(storage_addr.sin_addr));
			if (IS_DEBUG) {
				cout << "PING " << target_addr << endl;
			}
			int ret = connect(sock, (struct sockaddr*)&storage_addr, sizeof(storage_addr));
			if (IS_DEBUG) {
				cout << "connect returns " << ret << endl;
			}
			// testing this node
			if (ret == 0) {
				// on success
				char ping_msg [15];
				memset(ping_msg, 0, 15);
				strcpy(ping_msg, "STATUS`\r\n");
				if (!do_write(sock, ping_msg, strlen(ping_msg))) {
					cerr << "Admin console: fail to connect with the KV store storage node\n" << endl;
					curr_sn_group[target_node_port] = "-1";
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
					printf("in do_get while read, buffer:%s\n, len=%ld\n", buffer, strlen(buffer));
					for (int i = 0; i < strlen(buffer); i++) {
						printf("i=%d, char=%d\n", i, (int)buffer[i]);
					}
					if (n < 0) {
						cerr << "Admin console: fail to connect with the KV store storage node\n" << endl;
						is_wrong = true;
						break;
					}
					curr_pos += n;
				}
				close(sock);
				if (is_wrong) {
					curr_sn_group[target_node_port] = "-1";
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
					token = strtok(NULL, " ");
					curr_sn_group[target_node_port] = string(token);
				} else {
					curr_sn_group[target_node_port] = "-1";
				}
			} else {
				// on failure
				close(sock);
				curr_sn_group[target_node_port] = "-1";
				if (IS_DEBUG) {
					cout << target_addr << " really fails" << endl;
				}
			}
			itr++;
		}
		storage_nodes_states.push_back(curr_sn_group);
	}
	return storage_nodes_states;
}

string get_frontend_states(int my_port) {
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Admin console: failed to create a socket!\n" << endl;
		return "";
	}
	// connect to the load balancer
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(LOAD_BALANCER_PORT);
	inet_pton(AF_INET, LOAD_BALANCER_IP, &(servaddr.sin_addr));
	int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if (ret < 0) {
		cerr << "Admin console: load balancer is down!\n" << endl;
		close(sockfd);
		return "";
	}
	char msg [20];
	string my_port_str = to_string(my_port);
	sprintf(msg, "*request*%s", my_port_str.c_str());
	if (!do_write(sockfd, msg, strlen(msg))) {
		cerr << "Admin console: cannot connect to load balancer\n" << endl;
		close(sockfd);
		return "";
	}
	// read the response
	char buffer [BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);
	char* curr_pos = buffer;
	while (strstr(buffer, "\r\n") == NULL) {
		int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
		if (n < 0) {
			cerr << "Admin console: cannot connect to load balancer\n" << endl;
			close(sockfd);
			return "";
		}
		curr_pos += n;
	}
	close(sockfd);

	// parse the result
	char result [BUFFER_SIZE];
	memset(result, 0, BUFFER_SIZE);
	char* msg_end = strstr(buffer, "\r\n");
	strncpy(result, buffer, msg_end - buffer);
	return string(result);
}

void kill_storage_node(string request_body) {
	map<string, string> params = get_params_from_request_body(request_body);
	string port_to_kill = params["port"];
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Admin console: failed to create a socket!\n" << endl;
		return;
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(port_to_kill.c_str()));
	inet_pton(AF_INET, KV_MASTER_IP, &(servaddr.sin_addr));

	int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if (ret < 0) {
		cerr << "Admin console: this node is already down!\n" << endl;
		close(sockfd);
		return;
	} else {
		char msg [20];
		memset(msg, 0, 20);
		strcpy(msg, "KILL`\r\n");
		if (!do_write(sockfd, msg, strlen(msg))) {
			cerr << "Admin console: this node is already down\n" << endl;
			close(sockfd);
			return;
		}
	}
}

void start_storage_node(string request_body) {
	map<string, string> params = get_params_from_request_body(request_body);
	string port_to_start = params["port"];
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Admin console: failed to create a socket!\n" << endl;
		return;
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(port_to_start.c_str()));
	inet_pton(AF_INET, KV_MASTER_IP, &(servaddr.sin_addr));

	int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if (ret < 0) {
		cerr << "Admin console: this node is already down!\n" << endl;
		close(sockfd);
		return;
	} else {
		char msg [20];
		memset(msg, 0, 20);
		strcpy(msg, "RESTART`\r\n");
		if (!do_write(sockfd, msg, strlen(msg))) {
			cerr << "Admin console: this node is already down\n" << endl;
			close(sockfd);
			return;
		}
	}
}

vector<vector<string>>* get_big_table(vector<map<string, string>> kv_states) {
	string offset = "0";
	string limit = to_string(BIG_TABLE_MAX_ROWS);

	vector<vector<string>>* tables = new vector<vector<string>>();

	for (int i = 0; i < kv_states.size(); i++) {
		// for group i
		vector<string> curr_table;

		set<string> available_ports;
		for (auto itr = kv_states[i].begin(); itr != kv_states[i].end(); itr++) {
			if (itr->second != "-1") {
				available_ports.insert(itr->first);
			}
		}
		// if all nodes in this group dead
		if (available_ports.empty()) {
			tables->push_back(curr_table);
			continue;
		}

		// ask each available storage node for big table
		for (auto itr = available_ports.begin(); itr != available_ports.end(); itr++) {
			string node_port_to_contact = *itr;

			int sockfd = socket(PF_INET, SOCK_STREAM, 0);
			if (sockfd < 0) {
				continue;
			}

			struct sockaddr_in servaddr;
			bzero(&servaddr, sizeof(servaddr));
			servaddr.sin_family = AF_INET;
			servaddr.sin_port = htons(atoi(node_port_to_contact.c_str()));
			inet_pton(AF_INET, KV_MASTER_IP, &(servaddr.sin_addr));

			int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
			if (ret < 0) {
				close(sockfd);
				continue;
			}

			// ask the storage node for big table
			char msg [20];
			memset(msg, 0, 20);
			string msg_str = "index`" + offset + "`" + limit + "\r\n";
			if (IS_DEBUG) {
				cout << "Send " << msg_str << " to storage node at port " << node_port_to_contact << endl;
			}
			strcpy(msg, msg_str.c_str());
			if (!do_write(sockfd, msg, strlen(msg))) {
				cerr << "Admin console: this node is already down\n" << endl;
				close(sockfd);
				continue;
			}

			// read the response
			char buffer[BUFFER_SIZE];
			memset(buffer, 0, BUFFER_SIZE);
			char* curr_buffer_pos = buffer;
			bool should_quit = false;
			while (true) {
				if ((read(sockfd, curr_buffer_pos, BUFFER_SIZE - strlen(buffer))) <= 0) {
					break;
				}
				char* text_end = NULL;
				while ((text_end = strstr(buffer, "\r\n")) != NULL) {
					// a line of big table received from storage node
					char raw_msg_rcvd [BUFFER_SIZE];
					strncpy(raw_msg_rcvd, buffer, text_end - buffer);
					raw_msg_rcvd[text_end - buffer] = '\0';
					string one_row(raw_msg_rcvd);
					if (IS_DEBUG) {
						cout << "Storage node at port " << node_port_to_contact << " responds " << one_row << endl;
					}
					if (one_row == "quit`") {
						should_quit = true;
						break;
					} else if (one_row.find("-ERR") == string::npos) {
						string tmp = node_port_to_contact + "`" + one_row;
						curr_table.push_back(tmp);
					}
					// add '\r' and '\n' back
					text_end += 2;
					clean_buffer(buffer, text_end);
				}

				if (should_quit) {
					break;
				}
				// keep the unused chars in the buffer, and continue to read more in the next iteration
				curr_buffer_pos = buffer;
				while (*curr_buffer_pos != '\0') {
					curr_buffer_pos++;
				}

			}
			close(sockfd);

		}
		tables->push_back(curr_table);
	}
	if (IS_DEBUG) {
		for (int i = 0; i < tables->size(); i++) {
			cout << "Group " << i << " rows in big table: " << endl;
			for (int j = 0; j < (*tables)[i].size(); j++) {
				cout << (*tables)[i][j] << endl;
			}
		}
	}
	return tables;
}

/* remove a line of complete command from the buffer, and copy the remaining contents in the buffer to its start */
void clean_buffer(char* start, char* end) {
	char* i = start;
	char* j = end;
	while (*j != '\0') {
		*i = *j;
		i++;
		j++;
	}
	while (i < j) {
		*i = '\0';
		i++;
	}
}

void start_frontend_node(string request_body) {

	map<string, string> params = get_params_from_request_body(request_body);
	string port_to_start = params["port"];
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Admin console: failed to create a socket!\n" << endl;
		return;
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(LOAD_BALANCER_PORT);
	inet_pton(AF_INET, LOAD_BALANCER_IP, &(servaddr.sin_addr));

	int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if (ret < 0) {
		cerr << "Admin console: cannot connect to load balancer!\n" << endl;
		close(sockfd);
		return;
	} else {
		char msg [20];
		memset(msg, 0, 20);
		sprintf(msg, "*enable*%s", port_to_start.c_str());
		if (!do_write(sockfd, msg, strlen(msg))) {
			cerr << "Admin console: cannot connect to load balancer\n" << endl;
			close(sockfd);
			return;
		}
	}
}

void kill_frontend_node(string request_body) {
	map<string, string> params = get_params_from_request_body(request_body);
	string port_to_kill = params["port"];
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Admin console: failed to create a socket!\n" << endl;
		return;
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(port_to_kill.c_str()));
	inet_pton(AF_INET, LOAD_BALANCER_IP, &(servaddr.sin_addr));

	int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if (ret < 0) {
		cerr << "Admin console: cannot connect to load balancer\n" << endl;
		close(sockfd);
		return;
	} else {
		char msg [20];
		memset(msg, 0, 20);
		sprintf(msg, "*disable*%s", port_to_kill.c_str());
		if (!do_write(sockfd, msg, strlen(msg))) {
			cerr << "Admin console: cannot connect to load balancer\n" << endl;
			close(sockfd);
			return;
		}
	}
}



