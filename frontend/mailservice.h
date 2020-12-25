#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>

using namespace std;

bool tmp_do_DELETE(char* storage_node_ip, int storage_node_port, string r, string c, char* result);
//string try_deletemail(string user_name, string request_body);
string try_deletemail(map<string, string> cookies, string request_body);
string try_showdetail(map<string, string> cookies, string request_body);
//string try_sendmail(string sender_name, string request_body);
string try_sendmail(map<string, string> cookies, string request_body);
//string try_listmail(string user_name);
string try_listmail(map<string, string> cookies);
bool tmp_do_CPUT(char* storage_node_ip, int storage_node_port, string r, string c, string v, string v2, char* result);
void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer);
string getUIDL(string msg);
void get_uidl_list(string user_name, string storage_node_ip, int storage_node_port, vector<string> &result, char* origin_uidl_list);
bool do_PUT2(char* storage_node_ip, int storage_node_port, string r, string c, string v, char* result);
bool do_GET2(char* storage_node_ip, int storage_node_port, string r, string c, char* result);
bool connect_to_kv_master2(char* storage_node_ip, int* storage_node_port, string user_name, string command);
string UrlDecode(const string& szToDecode);
