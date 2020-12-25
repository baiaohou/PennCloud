#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>

using namespace std;


string try_listfile(map<string, string> cookies, string directory);
string try_download(map<string, string> cookies, string request_body);
string try_upload(map<string, string> cookies, string request_body);
string try_createfolder(map<string, string> cookies, string request_body);
string try_rename(map<string, string> cookies, string request_body);
string try_move(map<string, string> cookies, string request_body);
string try_deletefile(map<string, string> cookies, string request_body);
void delete_old_put_new_file_pair(char* storage_node_ip, int storage_node_port, string user_name, string fullFilename, string newFullFilename);
void get_filenames(string user_name, string storage_node_ip, int storage_node_port, vector<string> &result, char* origin_filenames);
void delete_file_helper(char* storage_node_ip, int storage_node_port, string user_name, string fullFilename);


//string base64_encode(string str);
//char a_to_c(int x);
//int find_int(char ch);
//string base64_decode(string str);
