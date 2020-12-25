#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include "global_constants.h"

using namespace std;

char* KV_MASTER_IP = "127.0.0.1";
int KV_MASTER_PORT = 10000;
char* LOAD_BALANCER_IP = "127.0.0.1";
int LOAD_BALANCER_PORT = 5050;
bool IS_DEBUG = false;
int BUFFER_SIZE = 102400;
double SESSION_MAX_DURATION = 1800;
int BIG_TABLE_MAX_ROWS = 100;
const char* KVSTORE_CONFIG_FILE = "../store/stores.txt";
vector<unordered_map<string, int>> kvstores = vector<unordered_map<string, int>>();

const char* ADMIN_USERNAME = "admin";
const char* ADMIN_PSWD = "admin";

const char* HTTP_OK = "200 OK";
const char* HTTP_SERVER_ERR = "500 Internal Server Error";


string testbinary;
int CHUNK_SIZE = 10240;
