#include <vector>
#include <unordered_map>

using namespace std;

/*
 * global constants used all over the front end
 */
extern char* KV_MASTER_IP;
extern int KV_MASTER_PORT;
extern char* LOAD_BALANCER_IP;
extern int LOAD_BALANCER_PORT;
extern bool IS_DEBUG;
/* the maximum size of a buffer used in socket communication */
extern int BUFFER_SIZE;
/* the maximum duration time of a session */
extern double SESSION_MAX_DURATION;
extern int BIG_TABLE_MAX_ROWS;
/* the directory of the kv store configuration file */
extern const char* KVSTORE_CONFIG_FILE;
/* a vector of kv storage node clusters */
extern vector<unordered_map<string, int>> kvstores;

extern const char* ADMIN_USERNAME;
extern const char* ADMIN_PSWD;

extern const char* HTTP_OK;
extern const char* HTTP_SERVER_ERR;

extern string testbinary;
extern int CHUNK_SIZE;
