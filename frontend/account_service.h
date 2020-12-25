#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <map>

using namespace std;

/*
 * Check whether the user has a valid session.
 * Return true if valid, false if expired.
 */
bool check_session(map<string, string> cookies, int my_port);

/*
 * Method for a user to login.
 * Return the corresponding http response string
 */
string try_login(string request_body, map<string, string> cookies, int my_port);

/*
 * Method for a user to register an account
 * Return the corresponding http response string
 */
string try_register(string request_body, int my_port);

/*
 * Method for a user to logout
 */
string do_logout(map<string, string> cookies, int my_port);

/*
 * Method for changing password
 */
string change_pswd(string request_body);

/*
 * Establish a TCP connection with the KV store master node, and
 * get the ip address and port of the returned storage node.
 * Return true upon success, false otherwise.
 */
bool connect_to_kv_master(char* storage_node_ip, int* storage_node_port, string user_name, string command);

/*
 * Send a GET(r, c) call to the KV store, and get the query result.
 * Return true upon success, false otherwise.
 */
bool do_GET(char* storage_node_ip, int storage_node_port, string r, string c, char* result);

/*
 * Send a PUT(r, c, v) call to the KV store, and get the query result.
 * Return true upon success, false otherwise.
 */
bool do_PUT(char* storage_node_ip, int storage_node_port, string r, string c, string v, char* result);

/*
 * Send a CPUT(r, c, v1, v2) call to the KV store, and get the query result.
 * Return true upon success, false otherwise.
 */
bool do_CPUT(char* storage_node_ip, int storage_node_port, string r, string c, string v1, string v2, char* result);

/*
 * Check whether the input storage node is alive, if not, switch to an alive one
 * if all storage nodes in that group fail, do nothing and just let the connection fails later
 */
void check_storage_node_connection(char* storage_node_ip, int* storage_node_port);

/*
 * Sync the connection states with load balancer
 */
void sync_load_balancer_state(int my_port, char* command);

