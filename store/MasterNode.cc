#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <unordered_map>
#include <limits.h>
#include <sys/time.h>

using namespace std;

// Global variables
vector<unordered_map<string, int>> kvstores; // A vector of mappings from kvstore address to number of requests forwarded to that kvstore
unordered_map<string, int> users; // A map from username to kvstore address set
unordered_map<int, int> loads; // A map from kvstores cluster to number of users stored in that cluster

void* handleConnection(void* arg);
void sig_handler(int sig);
unordered_map<string, int> parseLine(string line);
string createUser(string username);
string getUser(string username);
int leastUsedCluster();
string leastLoadedKvstore(int index);
void readFromFile();
string timestamp();

unsigned short int portNo = 10000; // By default, use port 10000
bool vFlag = false;
int activeConnections = 0;
int bufLen = 1050;
int timeLen = 32;
string logFile;
ifstream logRead;
ofstream logWrite;
bool alive = true;

// Static Data
// Each command is terminated by a carriage return (\r) followed by a linefeed character (\n)
const char* unknownCommand = "-ERR Unknown command\r\n";
const char* okResponse = "+OK";
const char* goodbyeResponse = "+OK Goodbye!\r\n";
const char* shuttingDown = "-ERR Server shutting down\r\n";
const char* downMessage = "-ERR Down\r\n";
const char* restartingMessage = "+OK Restarting\r\n";

vector<int> fileDescriptors;
vector<pthread_t> threads;


int main(int argc, char *argv[])
{
	signal(SIGPIPE, SIG_IGN); // Avoid crashing when read/write fails
	signal(SIGINT, sig_handler);

	users["admin"] = 0;

	// -p specifies the port number
	// -v specifies whether to show debugging messages
	int c;
	while ((c = getopt(argc, argv, "p:v::")) != -1) {
		switch(c) {
			case 'p':
				portNo = stoi(optarg);
				break;
			case 'v':
				vFlag = true;
				break;
		}
	}

	// No configuration file is specified
	if (optind == argc) {
		fprintf(stderr, "No configuration file is given.\n");
		exit(3);
	 }

	string configFilePath = argv[optind];

	// Read the configuration file of all the kv stores
	ifstream configFile(configFilePath);
	string line;
	while (getline(configFile, line)) {
		kvstores.push_back(parseLine(line));
	}

	// Initialization
	for (int i = 0; i < kvstores.size(); i++) {
		loads[i] = 0;
	}

	// Open a TCP port and start accepting connections
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		exit(1);
	}
	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
	fileDescriptors.push_back(listen_fd);

	// Read from the log file
	logFile = "Master.log";
    logRead.open(logFile);
    if (logRead.good()) {
    	// The log file exists, so read the previous log from the log file
    	readFromFile();
    }

    logWrite.open(logFile, ios::app);

	// Create a new socket
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(portNo);
	bind(listen_fd, (struct sockaddr*) &servaddr, sizeof(servaddr));

	// Assume there will be no more than 100 concurrent connections
	listen(listen_fd, 100);

	while (true) {
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);
		int comm_fd = accept(listen_fd, (struct sockaddr*) &clientaddr, &clientaddrlen);
		if (comm_fd == -1) {
			break;
		}
		fileDescriptors.push_back(comm_fd);
		if (vFlag) {
			fprintf(stderr, "[%d] New connection\n", comm_fd);
		}
		// Start a new pthread
		pthread_t newThread;
		threads.push_back(newThread);
		pthread_create(&newThread, NULL, handleConnection, &comm_fd);
	}

	logWrite.close();

	return 0;
}

// This function is in thread signature and to handle a connection
void* handleConnection(void* arg) {
	// Get the file descriptor
	int comm_fd = *(int*) arg;
	activeConnections++;

	if (alive) {
	char buffer[bufLen];
	memset(buffer, 0, sizeof(buffer));
	int bufferLen = 0;
	int readLen = 0;
	char* linefeedStart = 0;
	bool quit = false;
	// Use a while loop to read the command because the input from the client can arrive in small pieces
	while (true) {
		// Maintain a buffer for each connection
		// When new data arrives, it should be added to the corresponding buffer
		bufferLen = strlen(buffer); // exclude the ending mark of the string
		readLen = read(comm_fd, &buffer[bufferLen], bufLen - bufferLen);
		// The server should check whether it has received a full line
		// Repeat until the buffer no longer contains a full line
		while ((linefeedStart = strstr(buffer, "\r\n")) != 0) {
			// If so, it should process the corresponding command
			int messageLen = linefeedStart - buffer + 2;
			char text[bufLen];
			strncpy(text, buffer, messageLen);
			text[messageLen] = 0;
			if (vFlag) {
				fprintf(stderr, "%s[%d]Master received %s", timestamp().c_str(), comm_fd, text);
			}

			char* token = strtok(text, "`");

			string response;
			// Commands should be treated as case-insensitive
			if (strcasecmp(token, "create") == 0) {
				// CREATE <username>
				token = strtok(NULL, "\r");
				string username = token;
				response = createUser(username);
				write(comm_fd, response.c_str(), response.length());
				quit = true;
			} else if (strcasecmp(token, "request") == 0) {
				// REQUEST <username>
				token = strtok(NULL, "\r");
				string username = token;
				response = getUser(username);
				write(comm_fd, response.c_str(), response.length());
				quit = true;
			} else if (strcasecmp(token, "status") == 0) {
				response = "+OK " + to_string(activeConnections - 1) + "\r\n";
				write(comm_fd, response.c_str(), response.length());
				quit = true;
			} else if (strcasecmp(token, "kill") == 0) {
				alive = false;
				quit = true;
			} else {
				write(comm_fd, unknownCommand, strlen(unknownCommand));
				response = unknownCommand;
				quit = true;
			}

			if (vFlag) {
				fprintf(stderr, "%s[%d]Master responded %s", timestamp().c_str(), comm_fd, response.c_str());
			}

			// Remove the line from the buffer
			for (int i = 0; i < bufLen; i++) {
				if (messageLen + i < bufLen) {
					buffer[i] = buffer[messageLen + i];
				} else {
					buffer[i] = 0;
				}
			}

			if (quit) {
				break;
			}
		}

		if (quit) {
			break;
		}
	}
	} else {
		char buffer[bufLen];
		memset(buffer, 0, sizeof(buffer));
		int bufferLen = 0;
		int readLen = 0;
		char* linefeedStart = 0;
		bool quit = false;
		// Use a while loop to read the command because the input from the client can arrive in small pieces
		while (true) {
			// Maintain a buffer for each connection
			// When new data arrives, it should be added to the corresponding buffer
			bufferLen = strlen(buffer); // exclude the ending mark of the string
			readLen = read(comm_fd, &buffer[bufferLen], bufLen - bufferLen);
			// The server should check whether it has received a full line
			// Repeat until the buffer no longer contains a full line
			while ((linefeedStart = strstr(buffer, "\r\n")) != 0) {
				// If so, it should process the corresponding command
				int messageLen = linefeedStart - buffer + 2;
				char text[bufLen];
				strncpy(text, buffer, messageLen);
				text[messageLen] = 0;
				if (vFlag) {
					fprintf(stderr, "%s[%d]Master received %s", timestamp().c_str(), comm_fd, text);
				}

				char* token = strtok(text, "`");

				string response;
				// Commands should be treated as case-insensitive
				if (strcasecmp(token, "restart") == 0) {
					alive = true;
					write(comm_fd, restartingMessage, strlen(restartingMessage));
					response = restartingMessage;
					quit = true;
				} else {
					write(comm_fd, downMessage, strlen(downMessage));
					response = downMessage;
					quit = true;
				}

				if (vFlag) {
					fprintf(stderr, "%s[%d]Master responded %s", timestamp().c_str(), comm_fd, response.c_str());
				}

				// Remove the line from the buffer
				for (int i = 0; i < bufLen; i++) {
					if (messageLen + i < bufLen) {
						buffer[i] = buffer[messageLen + i];
					} else {
						buffer[i] = 0;
					}
				}

				if (quit) {
					break;
				}
			}

			if (quit) {
				break;
			}
		}
	}

	// The server should properly clean up its resources by terminating pthreads
	// when their connection closes
	close(comm_fd);
	if (vFlag) {
		fprintf(stderr, "[%d] Connection closed\n", comm_fd);
	}
	activeConnections--;
	pthread_exit(NULL);
}

// This function is to parse a line in the configuration file
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

// This function is to create a user
string createUser(string username) {
	string response;
	if (users.find(username) == users.end()) {
		int leastUsedClusterIndex = leastUsedCluster();
		users[username] = leastUsedClusterIndex;
		loads[leastUsedClusterIndex]++;
		string leastLoadedKvstoreAddress = leastLoadedKvstore(leastUsedClusterIndex);
		kvstores[leastUsedClusterIndex][leastLoadedKvstoreAddress]++;
		response = "+OK " + leastLoadedKvstoreAddress + "\r\n";
		logWrite << username << "`" << to_string(leastUsedClusterIndex) << "\n";
	} else {
		response = "-ERR This username has been taken\r\n";
	}
	return response;
}

// This function is to get the kvstore storing the specified user
string getUser(string username) {
	string response;
	if (users.find(username) == users.end()) {
		response = "-ERR This username doesn't exist\r\n";
	} else {
		int clusterIndex = users[username];
		string leastLoadedKvstoreAddress = leastLoadedKvstore(clusterIndex);
		kvstores[clusterIndex][leastLoadedKvstoreAddress]++;
		response = "+OK " + leastLoadedKvstoreAddress + "\r\n";
	}
	return response;
}

// This function is to find the kvstore cluster that stores the least number of users
int leastUsedCluster() {
	int leastIndex = -1;
	int leastLoad = INT_MAX;
    for (auto iter = loads.begin(); iter != loads.end(); iter++){
        if (iter->second < leastLoad) {
        	leastIndex = iter->first;
        	leastLoad = iter->second;
        }
    }
    return leastIndex;
}

// This function is to find the least loaded kvstore in a cluster
string leastLoadedKvstore(int index) {
	string leastAddress;
	int leastLoad = INT_MAX;
	for (auto iter = kvstores[index].begin(); iter != kvstores[index].end(); iter++) {
		if (iter->second < leastLoad) {
			leastAddress = iter->first;
			leastLoad = iter->second;
		}
	}
	return leastAddress;
}

// Handle signal
void sig_handler(int sig) {
		close(fileDescriptors[0]); // close the server socket
		int numberOfFileDescriptors = fileDescriptors.size();
		for (int i = 1; i < numberOfFileDescriptors; i++) {
			// Write to each open connection
			write(fileDescriptors[i], shuttingDown, strlen(shuttingDown));
			// Close all open sockets
			close(fileDescriptors[i]);
			if (vFlag) {
				fprintf(stderr, "[%d] Connection closed\n", fileDescriptors[i]);
			}
		}
		logWrite.close();
}

// This function is to read from the log file
void readFromFile() {
	fprintf(stderr, "%s\n", "Recovering from the log file...");
	string buffer;
	while (getline(logRead, buffer)) {
		int delimIndex = buffer.find("`");
		string username = buffer.substr(0, delimIndex);
		int cluster = atoi(buffer.substr(delimIndex + 1, buffer.length() - delimIndex - 1).c_str());
		if (users.find(username) == users.end()) {
			users[username] = cluster;
		}
	}
	logRead.close();
	fprintf(stderr, "%s\n", "Recovered from the log file.");
}

// This function is to generate a timestamp in the format of "yyyy:MM:dd:hh:mm:ss:uuuuu"
string timestamp() {
	struct timeval current;
	gettimeofday(&current, NULL);
	tm* ltm = localtime(&current.tv_sec);
	char formattedTime[timeLen];
	sprintf(formattedTime, "%04d:%02d:%02d:%02d:%02d:%02d:%06ld", 1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday,
			ltm->tm_hour, ltm->tm_min, ltm->tm_sec, current.tv_usec);
	string prefix = formattedTime;
	return prefix;
}
