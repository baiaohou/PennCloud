#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <unordered_map>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

void* handleConnection(void* arg);
void sig_handler(int sig);
sockaddr_in constructSockaddr(string input);
string handlePut(string username, string columnName, string value);
void handleForwardedPut(string time, string username, string columnName, string value, int senderIndex);
string handleGet(string username, string columnName);
string handleCput(string username, string columnName, string prevValue, string currValue);
void handleForwardedCput(string time, string username, string columnName, string currValue, int senderIndex);
string handleDelete(string username, string columnName);
void handleForwardedDelete(string time, string username, string columnName, int senderIndex);
string timestamp();
void forwardToServer(int index, string message);
void readFromLogFile();
void writeLog(string time, string operation, string username, string columnName, string value);
void handleOneLog(string timestamp, string operation, string username, string columnName, string value, int senderIndex);
void handleSync(int comm_fd, string time);
void syncdata(int syncIndex);
void handleIndex(int comm_fd, int offset, int limit);


int clusterIndex;
int serverIndex;
bool vFlag = false;
string logFile;
string folderName;
ifstream logRead;
ofstream logWrite;
string latest = "2020:12:13:00:17:47:422307";
int bufLen = 10300;
int timeLen = 32;
bool alive = true;
int activeConnections = 0;

vector<sockaddr_in> cluster;
unordered_map<string, unordered_map<string, pair<string, string>>> tablet;

const char* unsupportedCommand = "-ERR Unsupported Command\r\n";
const char* shuttingDown = "-ERR Server shut down\r\n";
const char* connectionClosed = "+OK Connection closed\r\n";
const char* downMessage = "-ERR Down\r\n";
const char* restartingMessage = "+OK Restarting\r\n";
const char* quitMessage = "QUIT`\r\n";

vector<int> fileDescriptors;
vector<pthread_t> threads;

pthread_mutex_t logFileLock;
pthread_mutex_t forwardLock;


int main(int argc, char *argv[])
{
	signal(SIGPIPE, SIG_IGN); // Avoid crashing when read/write fails
	signal(SIGINT, sig_handler);

	unordered_map<string, pair<string, string>> adminMap;
	adminMap["pswd"] = make_pair("admin", "9999:12:13:00:17:47:422307");
	tablet["admin"] = adminMap;

	// -v specifies whether to print debugging messages
	int c;
	while ((c = getopt(argc, argv, "v::")) != -1) {
		switch(c) {
			case 'v':
				vFlag = true;
				break;
		}
	}

	// No configuration file is specified
	if (optind == argc) {
		fprintf(stderr, "No configuration file is given.\n");
		exit(1);
	 }
	string configFilePath = argv[optind];
	optind++;

	// No cluster index is specified
	if (optind == argc) {
		fprintf(stderr, "No cluster index is given.\n");
		exit(2);
	}
	clusterIndex = atoi(argv[optind]);
	optind++;

	// No server index is specified
	if (optind == argc) {
		fprintf(stderr, "No server index is given.\n");
		exit(3);
	}
	serverIndex = atoi(argv[optind]);
	optind++;

	// Read the configuration file of all the kv stores
	ifstream configFile(configFilePath);
	string line;
	int i = 0;
	while (getline(configFile, line)) {
		if (i == clusterIndex) {
			int prevCommaIndex = -1;
			int commaIndex = line.find(",");
			while (commaIndex != line.npos) {
				cluster.push_back(constructSockaddr(line.substr(prevCommaIndex + 1, commaIndex - prevCommaIndex - 1)));
				prevCommaIndex = commaIndex;
				commaIndex = line.find(",", prevCommaIndex + 1);
			}
			cluster.push_back(constructSockaddr(line.substr(prevCommaIndex + 1, line.length() - prevCommaIndex - 1)));
		}
		i++;
	}

	if (vFlag) {
		for (int i = 0; i < cluster.size(); i++) {
			fprintf(stderr, "S %d %d %s:%d\n", clusterIndex, i, inet_ntoa(cluster[i].sin_addr), ntohs(cluster[i].sin_port));
		}
	}

	if (cluster.empty()) {
		fprintf(stderr, "There is no such kvstore cluster\n");
		exit(4);
	}

	if (cluster.size() <= serverIndex) {
		fprintf(stderr, "The server index is out of bound\n");
		exit(5);
	}



	// Check for the log file
	logFile = "S_" + to_string(clusterIndex) + "_" + to_string(serverIndex) + ".log";
    logRead.open(logFile);
    if (logRead.good()) {
    	// The log file exists, so read the previous log from the log file
    	readFromLogFile();
    }

    logWrite.open(logFile, ios::app);

    /*
    // Check for the folder
    folderName = "./S_" + to_string(clusterIndex) + "_" + to_string(serverIndex) + "_data";
    int existingRes = access(folderName.c_str(), W_OK);
    if (existingRes < 0) {
    	fprintf(stderr, "Create the folder %s\n", folderName.c_str());
    	mkdir(folderName.c_str(), S_IRWXU);
    }
    */


    if (serverIndex != 0) {
    	// Synchronize data with the primary node
    	syncdata(0);
    } else {
    	for (int i = 1; i < cluster.size(); i++) {
    		syncdata(i);
    	}
    }


	// Open a TCP socket to accept requests from the frontend
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		exit(6);
	}
	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
	fileDescriptors.push_back(listen_fd);

	struct sockaddr_in servaddr = cluster[serverIndex];
	bind(listen_fd, (struct sockaddr*) &servaddr, sizeof(servaddr));
	listen(listen_fd, 100);

	pthread_mutex_init(&logFileLock, 0);
	pthread_mutex_init(&forwardLock, 0);


	while (true) {
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);
		int comm_fd = accept(listen_fd, (struct sockaddr*) &clientaddr, &clientaddrlen);
		if (comm_fd == -1) {
			break;
		}
		fileDescriptors.push_back(comm_fd);
		if (vFlag) {
			fprintf(stderr, "%s[%d]S %d %d accepted a new connection from %s:%d\n", timestamp().c_str(), comm_fd,
					clusterIndex, serverIndex, inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
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
		if (readLen > 0) {
		printf("current buffer: %s", buffer);
		}
		while ((linefeedStart = strstr(buffer, "\r\n")) != 0) {
			// printf("current buffer: %s", buffer);
			// If so, it should process the corresponding command
			int messageLen = linefeedStart - buffer + 2;
			char text[bufLen];
			strncpy(text, buffer, messageLen);
			text[messageLen] = 0;
			if (vFlag) {
				fprintf(stderr, "Current latest timestamp: %s\n", latest.c_str());
				fprintf(stderr, "%s[%d]S %d %d received %s", timestamp().c_str(), comm_fd,
						clusterIndex, serverIndex, text);
			}
			char* token = strtok(text, "`");

			string response;
			// Commands should be treated as case-insensitive
			if (strcasecmp(token, "put") == 0) {
				// PUT(r,c,v)
				token = strtok(NULL, "`");
				string username = token;
				token = strtok(NULL, "`");
				string columnName = token;
				token = strtok(NULL, "\r");
				string value = token;
				response = handlePut(username, columnName, value);
				write(comm_fd, response.c_str(), response.length());
			} else if (strcasecmp(token, "get") == 0) {
				// GET(r,c)
				token = strtok(NULL, "`");
				string username = token;
				token = strtok(NULL, "\r");
				string columnName = token;
				response = handleGet(username, columnName);
				write(comm_fd, response.c_str(), response.length());
			} else if (strcasecmp(token, "cput") == 0){
				// CPUT(r,c,v1,v2)
				token = strtok(NULL, "`");
				string username = token;
				token = strtok(NULL, "`");
				string columnName = token;
				token = strtok(NULL, "`");
				string prevValue = token;
				token = strtok(NULL, "\r");
				string currValue = token;
				response = handleCput(username, columnName, prevValue, currValue);
				write(comm_fd, response.c_str(), response.length());
			} else if (strcasecmp(token, "delete") == 0) {
				// DELETE(r,c)
				token = strtok(NULL, "`");
				string username = token;
				token = strtok(NULL, "\r");
				string columnName = token;
				response = handleDelete(username, columnName);
				write(comm_fd, response.c_str(), response.length());
			} else if (strcasecmp(token, "forward") == 0) {
				token = strtok(NULL, "`");
				string time = token;
				token = strtok(NULL, "`");
				int senderIndex = atoi(token);
				token = strtok(NULL, "`");
				if (strcasecmp(token, "put") == 0) {
					token = strtok(NULL, "`");
					string username = token;
					token = strtok(NULL, "`");
					string columnName = token;
					token = strtok(NULL, "\r");
					string value = token;
					handleForwardedPut(time, username, columnName, value, senderIndex);
				} else if (strcasecmp(token, "cput") == 0) {
					token = strtok(NULL, "`");
					string username = token;
					token = strtok(NULL, "`");
					string columnName = token;
					token = strtok(NULL, "\r");
					string value = token;
					handleForwardedCput(time, username, columnName, value, senderIndex);
				} else if (strcasecmp(token, "delete") == 0) {
					token = strtok(NULL, "`");
					string username = token;
					token = strtok(NULL, "\r");
					string columnName = token;
					handleForwardedDelete(time, username, columnName, senderIndex);
				}
				quit = true;
			} else if (strcasecmp(token, "quit") == 0) {
				write(comm_fd, connectionClosed, strlen(connectionClosed));
				quit = true;
			} else if (strcasecmp(token, "sync") == 0) {
				// SYNC(timestamp) is to request all the transactions after the specified timestamp
				token = strtok(NULL, "`");
				string time = token;
				token = strtok(NULL, "\r");
				int senderIndex = atoi(token);
				handleSync(comm_fd, time);
				quit = true;
			} else if (strcasecmp(token, "status") == 0) {
				// STATUS is to check the heartbeat
				response = "+OK " + to_string(activeConnections - 1) + "\r\n";
				write(comm_fd, response.c_str(), response.length());
				quit = true;
			} else if (strcasecmp(token, "kill") == 0) {
				// KILL is to fake kill the server
				alive = false;
				quit = true;
			} else if (strcasecmp(token, "index") == 0) {
				token = strtok(NULL, "`");
				int offset = atoi(token);
				token = strtok(NULL, "\r");
				int limit = atoi(token);
				handleIndex(comm_fd, offset, limit);
				quit = true;
			} else {
				write(comm_fd, unsupportedCommand, strlen(unsupportedCommand));
				quit = true;
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
				// printf("current buffer: %s", buffer);
				// If so, it should process the corresponding command
				int messageLen = linefeedStart - buffer + 2;
				char text[bufLen];
				strncpy(text, buffer, messageLen);
				text[messageLen] = 0;
				if (vFlag) {
					fprintf(stderr, "%s[%d]S %d %d received %s", timestamp().c_str(), comm_fd,
							clusterIndex, serverIndex, text);
				}
				char* token = strtok(text, "`");

				// Commands should be treated as case-insensitive
				if (strcasecmp(token, "restart") == 0) {
					// RESTART is to restart the server
					alive = true;
					quit = true;
				    if (serverIndex != 0) {
				    	// Synchronize data with the primary node
				    	syncdata(0);
				    } else {
				    	for (int i = 1; i < cluster.size(); i++) {
				    		syncdata(i);
				    	}
				    }
					write(comm_fd, restartingMessage, strlen(restartingMessage));
				} else if (strcasecmp(token, "sync") == 0) {
					write(comm_fd, quitMessage, strlen(quitMessage));
				} else {
					write(comm_fd, downMessage, strlen(downMessage));
					quit = true;
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
		fprintf(stderr, "%s[%d] Connection closed\n", timestamp().c_str(), comm_fd);
	}
	activeConnections--;
	pthread_exit(NULL);
}

// This function is to construct a sockaddr_in based on a string of the format "127.0.0.1:5001"
sockaddr_in constructSockaddr(string input) {
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;

	int colonIndex = input.find(":");
	string ipAddress = input.substr(0, colonIndex);
	string portNumber = input.substr(colonIndex + 1, input.length() - colonIndex - 1);

	servaddr.sin_port = htons(atoi(portNumber.c_str()));
	inet_pton(AF_INET, ipAddress.c_str(), &servaddr.sin_addr);

	return servaddr;
}

// This function is to handle PUT(r,c,v)
string handlePut(string username, string columnName, string value) {
	string time = timestamp();
	if (time.compare(latest) > 0) {
		latest = time;
	}
	string response;
	string forwardMessage = "FORWARD`" + time + "`" + to_string(serverIndex) + "`PUT`" + username + "`" + columnName + "`" + value + "\r\n";
	if (tablet.find(username) == tablet.end()) {
		if (serverIndex == 0) {
			// The primary propagates the write to all other replicas
			for (int i = 1; i < cluster.size(); i++) {
				forwardToServer(i, forwardMessage);
			}
		} else {
			// If a non-primary replica receives a write, it forwards it to the primary
			forwardToServer(0, forwardMessage);
		}
		unordered_map<string, pair<string, string>> m;
		m[columnName] = make_pair(value, time);
		tablet[username] = m;
		response = "+OK Successfully create a row for " + username + "\r\n";
		writeLog(time, "PUT", username, columnName, value);
	} else if (tablet[username].find(columnName) == tablet[username].end()) {
		if (serverIndex == 0) {
			// The primary propagates the write to all other replicas
			for (int i = 1; i < cluster.size(); i++) {
				forwardToServer(i, forwardMessage);
			}
		} else {
			// If a non-primary replica receives a write, it forwards it to the primary
			forwardToServer(0, forwardMessage);
		}
		tablet[username][columnName] = make_pair(value, time);
		response = "+OK Successfully create a column for " + username + "\r\n";
		writeLog(time, "PUT", username, columnName, value);
	} else {
		response = "-ERR The column already exists for " + username + "\r\n";
	}
	return response;
}

// This function is to handle forwarded PUT(r,c,v)
void handleForwardedPut(string time, string username, string columnName, string value, int senderIndex) {
	/*
	printf("time: %s\n", time.c_str());
	printf("username: %s\n", username.c_str());
	printf("columnName: %s\n", columnName.c_str());
	printf("value: %s\n", value.c_str());
	printf("senderIndex: %d\n", senderIndex);
	*/
	if (time.compare(latest) > 0) {
		latest = time;
	}
	if (senderIndex >= 0) {
		// If senderIndex < 0, this operation comes from the log file
		if (serverIndex == 0) {
			// The primary propagates the write to all other replicas except the sender server
			string forwardMessage = "FORWARD`" + time + "`" + to_string(senderIndex) + "`PUT`" + username + "`" + columnName + "`" + value + "\r\n";
			for (int i = 1; i < cluster.size(); i++) {
				if (i != senderIndex) {
					forwardToServer(i, forwardMessage);
				}
			}
		}
	}
	if (tablet.find(username) == tablet.end()) {
		unordered_map<string, pair<string, string>> m;
		m[columnName] = make_pair(value, time);
		tablet[username] = m;
	} else if (tablet[username].find(columnName) == tablet[username].end()) {
		tablet[username][columnName] = make_pair(value, time);
	} else if (time.compare(tablet[username][columnName].second) > 0) {
		// There is a conflict between the current data and the forwarded data
		// And the forwarded data is newer than the current data
		tablet[username][columnName].first = value;
		tablet[username][columnName].second = time;
	}
	if (senderIndex >= 0) {
		writeLog(time, "PUT", username, columnName, value);
	}
}

// This function is to handle GET(r,c)
string handleGet(string username, string columnName) {
	string time = timestamp();
	if (time.compare(latest) > 0) {
		latest = time;
	}
	string response;
	if (tablet.find(username) == tablet.end()) {
		response = "-ERR No information for " + username + "\r\n";
	} else if (tablet[username].find(columnName) == tablet[username].end()) {
		response = "-ERR No column for " + columnName + "\r\n";
	} else {
		response = "+OK " + tablet[username][columnName].first + "\r\n";
	}
	return response;
}

// This function is to handle CPUT(r,c,v1,v2)
string handleCput(string username, string columnName, string prevValue, string currValue) {
	string response;
	string time = timestamp();
	if (time.compare(latest) > 0) {
		latest = time;
	}
	if (tablet.find(username) == tablet.end()) {
		response = "-ERR No information for " + username + "\r\n";
	} else if (tablet[username].find(columnName) == tablet[username].end()) {
		response = "-ERR No column for " + columnName + "\r\n";
	} else if (tablet[username][columnName].first == prevValue) {
		string forwardMessage = "FORWARD`" + time + "`" + to_string(serverIndex) + "`CPUT`" + username + "`" + columnName + "`" + currValue + "\r\n";
		if (serverIndex == 0) {
			// The primary propagates the write to all other replicas
			for (int i = 1; i < cluster.size(); i++) {
				forwardToServer(i, forwardMessage);
			}
		} else {
			// If a non-primary replica receives a write, it forwards it to the primary
			forwardToServer(0, forwardMessage);
		}
		tablet[username][columnName].first = currValue;
		tablet[username][columnName].second = time;
		response = "+OK " + tablet[username][columnName].first + "\r\n";
		writeLog(time, "CPUT", username, columnName, currValue);
	} else {
		response = "-ERR Previous value doesn't match the record\r\n";
	}
	return response;
}

// This function is handle forwarded CPUT(r,c,v2)
void handleForwardedCput(string time, string username, string columnName, string currValue, int senderIndex) {
	if (time.compare(latest) > 0) {
		latest = time;
	}
	if (senderIndex >= 0) {
		if (serverIndex == 0) {
			// The primary propagates the write to all other replicas except the sender server
			string forwardMessage = "FORWARD`" + time + "`" + to_string(senderIndex) + "`CPUT`" + username + "`" + columnName + "`" + currValue + "\r\n";
			for (int i = 1; i < cluster.size(); i++) {
				if (i != senderIndex) {
					forwardToServer(i, forwardMessage);
				}
			}
		}
	}
	if (tablet.find(username) != tablet.end() && tablet[username].find(columnName) != tablet[username].end()
			&& time.compare(tablet[username][columnName].second) > 0) {
		tablet[username][columnName].first = currValue;
		tablet[username][columnName].second = time;
		if (senderIndex >= 0) {
			writeLog(time, "CPUT", username, columnName, currValue);
		}
	}
}

// This function is to handle DELETE(r,c)
string handleDelete(string username, string columnName) {
	string response;
	string time = timestamp();
	if (time.compare(latest) > 0) {
		latest = time;
	}
	if (tablet.find(username) == tablet.end()) {
		response = "-ERR No information for " + username + "\r\n";
	} else if (tablet[username].find(columnName) == tablet[username].end()) {
			response = "-ERR No column for " + columnName + "\r\n";
	} else {
		string forwardMessage = "FORWARD`" + time + "`" + to_string(serverIndex) + "`DELETE`" + username + "`" + columnName + "\r\n";
		if (serverIndex == 0) {
			// The primary propagates the write to all other replicas
			for (int i = 1; i < cluster.size(); i++) {
				forwardToServer(i, forwardMessage);
			}
		} else {
			// If a non-primary replica receives a write, it forwards it to the primary
			forwardToServer(0, forwardMessage);
		}
		tablet[username].erase(tablet[username].find(columnName));
		response = "+OK Successfully delete the column " + columnName + "\r\n";
		writeLog(time, "DELETE", username, columnName, "0");
	}
	return response;
}

// This function is to handle forwarded DELETE(r,c)
void handleForwardedDelete(string time, string username, string columnName, int senderIndex) {
	if (time.compare(latest) > 0) {
		latest = time;
	}
	if (senderIndex >= 0) {
		if (serverIndex == 0) {
			// The primary propagates the write to all other replicas except the sender server
			string forwardMessage = "FORWARD`" + time + "`" + to_string(senderIndex) + "`DELETE`" + username + "`" + columnName + "\r\n";
			for (int i = 1; i < cluster.size(); i++) {
				if (i != senderIndex) {
					forwardToServer(i, forwardMessage);
				}
			}
		}
	}
	if (tablet.find(username) != tablet.end() && tablet[username].find(columnName) != tablet[username].end()
			&& time.compare(tablet[username][columnName].second) > 0) {
		tablet[username].erase(tablet[username].find(columnName));
		if (senderIndex >= 0) {
			writeLog(time, "DELETE", username, columnName, "0");
		}
	}
}

// This function is to forward the message to the server of a specific index in the cluster
void forwardToServer(int index, string message) {
	printf("Send index: %d\n", index);
	printf("Send string: %s\n", message.c_str());
	pthread_mutex_lock(&forwardLock);
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	int res = connect(sockfd, (struct sockaddr*)&cluster[index], sizeof(cluster[index]));
	if (res == 0) {
		int n = write(sockfd, message.c_str(), message.length());
		write(sockfd, quitMessage, strlen(quitMessage));
	} else {
		fprintf(stderr, "Cannot connect to S %d %d\n", clusterIndex, index);
	}
	close(sockfd);
	pthread_mutex_unlock(&forwardLock);
}

// Handle signal
void sig_handler(int sig) {
	// if (sig == SIGINT) {
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
			// Terminate every thread
			// pthread_kill(threads[i - 1], 0);
		}
		logWrite.close();
		/*
		// pthread_kill can cause segment fault
		for (int i = 0; i < threads.size(); i++) {
			pthread_kill(threads[i], 0);
		}
		*/
	// }
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

// This function is to write a log into the log file
void writeLog(string time, string operation, string username, string columnName, string value) {
	pthread_mutex_lock(&logFileLock);
	logWrite << time << "`" << operation << "`" << username << "`" << columnName << "`" << value << "`";
	pthread_mutex_unlock(&logFileLock);
}

// This function is to read from the log file
void readFromLogFile() {
	fprintf(stderr, "%s\n", "Recovering from the log file...");
	string timestamp, operation, username, columnName, value, buffer;
	int state = 0;
	while (getline(logRead, buffer, '`')) {
		if (state % 5 == 0) {
			timestamp = buffer;
		} else if (state % 5 == 1) {
			operation = buffer;
		} else if (state % 5 == 2) {
			username = buffer;
		} else if (state % 5 == 3) {
			columnName = buffer;
		} else if (state % 5 == 4){
			value = buffer;
			handleOneLog(timestamp, operation, username, columnName, value, -1);
		}
		state++;
		state = state % 5;
	}
	logRead.close();
	if (vFlag) {
		fprintf(stderr, "Current latest timestamp: %s\n", latest.c_str());
	}
	fprintf(stderr, "%s\n", "Recovered from the log file.");
}

// This function is to handle one log entry
void handleOneLog(string timestamp, string operation, string username, string columnName, string value, int senderIndex) {
	if (operation == "PUT") {
		handleForwardedPut(timestamp, username, columnName, value, senderIndex);
	} else if (operation == "CPUT") {
		handleForwardedCput(timestamp, username, columnName, value, senderIndex);
	} else if (operation == "DELETE") {
		handleForwardedDelete(timestamp, username, columnName, senderIndex);
	}
	if (timestamp.compare(latest) > 0) {
		latest = timestamp;
	}
}

void handleSync(int comm_fd, string time) {
	pthread_mutex_lock(&logFileLock);
	logWrite.close();
	logRead.open(logFile);
	fprintf(stderr, "Sync process starts...\n");
	string timestamp, buffer;
	string response = "";
	int state = 0;
	while (getline(logRead, buffer, '`')) {
		/*
		printf("buffer: %s\n", buffer.c_str());
		printf("length of buffer: %ld\n", buffer.length());
		*/
		if (state % 5 == 0) {
			timestamp = buffer;
			response += (buffer + "`");
		} else if (state % 5 != 4) {
			response += (buffer + "`");
		} else if (state % 5 == 4) {
			response += (buffer + "\r\n");
			/*
			printf("length of timestamp: %d\n", timestamp.length());
			printf("length of specified time: %d\n", time.length());
			printf("timestamp: %s\n", timestamp.c_str());
			printf("specified time: %s\n", time.c_str());
			printf("timestamp > the specified one: %d\n", (timestamp.compare(time) >= 0));
			*/
			if (timestamp.compare(time) >= 0) {
				if (vFlag) {
					fprintf(stderr, "Sent length: %ld\n", response.length());
					fprintf(stderr, "Sent: %s", response.c_str());
				}
				write(comm_fd, response.c_str(), response.length());
			}
			response = "";
		}
		state++;
		state = state % 5;
	}
	response = "quit`\r\n";
	write(comm_fd, response.c_str(), response.length());
	logRead.close();
	fprintf(stderr, "Sync process ends.\n");
	logWrite.open(logFile, ios::app);
	pthread_mutex_unlock(&logFileLock);
}

void syncdata(int syncIndex) {
	fprintf(stderr, "Recovering from S %d %d...\n", clusterIndex, syncIndex);
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	int res = connect(sockfd, (struct sockaddr*)&cluster[syncIndex], sizeof(cluster[syncIndex]));
	if (res == 0) {
		string message = "SYNC`" + latest + "`" + to_string(serverIndex) + "\r\n";
		int n = write(sockfd, message.c_str(), message.length());

		char buffer[bufLen];
		memset(buffer, 0, sizeof(buffer));
		int bufferLen = 0;
		int readLen = 0;
		char* linefeedStart = 0;
		bool quit = false;
		while (true) {
			// Maintain a buffer for each connection
			// When new data arrives, it should be added to the corresponding buffer
			bufferLen = strlen(buffer);
			readLen = read(sockfd, &buffer[bufferLen], bufLen - bufferLen);
			// The server should check whether it has received a full line
			// Repeat until the buffer no longer contains a full line
			while ((linefeedStart = strstr(buffer, "\r\n")) != 0) {
				// printf("current buffer: %s", buffer);
				// If so, it should process the corresponding command
				int messageLen = linefeedStart - buffer + 2;
				char text[bufLen];
				strncpy(text, buffer, messageLen);
				text[messageLen] = 0;
				if (vFlag) {
					fprintf(stderr, "Received Length: %ld\n", strlen(text));
					fprintf(stderr, "From S %d %d: %s", clusterIndex, syncIndex, text);
				}

				char* token = strtok(text, "`");
				if (strcasecmp(token, "quit") == 0) {
					quit = true;
				} else {
					string time = token;
					token = strtok(NULL, "`");
					string operation = token;
					token = strtok(NULL, "`");
					string username = token;
					token = strtok(NULL, "`");
					string columnName = token;
					token = strtok(NULL, "\r");
					string value = token;
					handleOneLog(time, operation, username, columnName, value, syncIndex);
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
		fprintf(stderr, "Cannot connect to S %d %d to synchronize data\n", clusterIndex, syncIndex);
	}
	close(sockfd);
	if (vFlag) {
		fprintf(stderr, "Current latest timestamp: %s\n", latest.c_str());
	}
	fprintf(stderr, "Recovered from S %d %d.\n", clusterIndex, syncIndex);
}

void handleIndex(int comm_fd, int offset, int limit) {
	int i = 0;
	for (auto it = tablet.begin(); it != tablet.end(); ++it) {
		string username = it->first;
		unordered_map<string, pair<string, string>> row = it->second;
		for (auto it2 = row.begin(); it2 != row.end(); ++it2) {
			if (i >= offset && i < offset + limit) {
				string columnName = it2->first;
				string value = it2->second.first;
				if (value.length() > 300) {
					value = "Length of " + to_string(value.length());
				}
				string time = it2->second.second;
				string resp = time + "`" + username + "`" + columnName + "`" + value + "\r\n";
				write(comm_fd, resp.c_str(), resp.length());
			}
			i++;
			if (i >= offset + limit) {
				write(comm_fd, quitMessage, strlen(quitMessage));
				return;
			}
		}
	}
}
