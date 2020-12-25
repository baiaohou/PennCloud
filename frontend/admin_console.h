#include <string>
#include <map>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <time.h>
#include <vector>
#include <map>

using namespace std;


/*
 * Check whether each storage nodes are alive or not
 * vector[i] means the storage node group i
 * unordered_map maps storage node address in this group to its number of connections
 */
vector<map<string, string>> get_storage_nodes_states();


/* double check whether the user is admin */
bool is_admin(map<string, string> cookies);

/*
 * Ask the load balancer for the states of each front end server
 * return "" if fails, otherwise return a string to replace the $content1 in admin.html
 */
string get_frontend_states(int my_port);

void kill_storage_node(string request_body);

void start_storage_node(string request_body);

void start_frontend_node(string request_body);

void kill_frontend_node(string request_body);

/*
 * Get the big tables rows
 * vector[i] means group i
 * inner vector means rows of the big table
 */
vector<vector<string>>* get_big_table(vector<map<string, string>> kv_states);

/*
 * Remove a line of complete command from the buffer, and copy the remaining contents in the buffer to its start
 */
void clean_buffer(char* start, char* end);



