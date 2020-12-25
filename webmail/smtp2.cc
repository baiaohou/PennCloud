#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <pthread.h>
#include <vector>
#include <csignal>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <fstream>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/ioctl.h>


#include "smtp_func.h"
using namespace std;

void* worker(void *arg);
void mail_header(char* text, char* from_user, char* from_domain, int* text_cur);
void handle_content(char* crlf, char* buf, int file_index, char* mail_name, char* text, int comm_fd, int* state, int* text_cur);
void handle_helo(char* crlf, char* buf, int comm_fd, int* state);
void handle_from(char* crlf, char* buf, int comm_fd, int* state, char* from_user, char* from_domain);
void handle_rcpt(char* crlf, char* buf, int comm_fd, int* state, char* to_user, char* to_domain, char* mail_name, int* file_index);
void handle_data(int comm_fd, int* state, char* text, char* from_user, char* from_domain, int* text_cur);
void handle_quit(char* crlf, char* buf, int comm_fd, bool* hasQuit);
void handle_rset(char* crlf, char* buf, int comm_fd, int* state);
void handle_noop(int comm_fd, int* state);


const char* GREETING = "220 localhost service ready\r\n";
const char* QUIT = "221 localhost Service closing transmission channel\r\n";
const char* OK_RESPONSE = "250 OK\r\n";
const char* LOC_RESPONSE = "250 localhost\r\n";
const char* START_DATA = "354 Start mail input; end with <CRLF>.<CRLF>\r\n";
const char* SYNTAX_ERROR = "500 Syntax error, command unrecognized\r\n";
const char* ARG_ERROR = "501 Syntax error in parameters or arguments\r\n";
const char* BAD_SEQUENCE = "503 Bad sequence of commands\r\n";
const char* NO_USER = "550 No such user here\r\n";

volatile int listen_fd;// server side fd for listening, use volatile to make atomic
int fds[100];// store fds for comm_fd
pthread_t threads[100];// store threads
bool hasV;// debug flag
pthread_mutex_t lock;// mutex lock for critical session
char mfiles[100][64];// mbox names, this can be replaced by K-V store
pthread_mutex_t file_locks[100];// each file has a lock
char dir[100];// mbox files directory, this can be replaced by K-V store
bool useFile;// use local files instead of K-V store
int bufLen = 10300;
void sendToOutside(string from, string to, string content);
void sendToOutside2();
char* parse_record(unsigned char* buffer, size_t r, const char* section, ns_sect s, int idx, ns_msg* m);

/* handle the SIGINT */
void signalHandler(int signum)
{
	printf("\n");// line feed for ctrl+c
	close(listen_fd);// close server side fd
	for (int i = 0; i < 100; i++)
	{
		pthread_mutex_lock(&lock);
		if (fds[i] != 0)
		{
			char shutd[100] = "-ERR Server shutting down\r\n";
			write(fds[i], shutd, strlen(shutd));
			close(fds[i]);// close fd so that thread will read -1 and break
			fds[i] = 0;// clear the fd
			int ret = pthread_kill(threads[i], SIGUSR1);// send SIGUSR1 to threads
			pthread_join(threads[i], NULL);// join to make worker threads exit before main
		}
		pthread_mutex_unlock(&lock);
	}
}

/* handler for the SIGUSR1 */
void usr1Handler(int signum)
{
	pthread_exit(NULL);// for ctrl+c situation, worker thread exits here, avoid closing fd repeatedly and detach
}


int main(int argc, char *argv[])
{
	signal(SIGINT, signalHandler);// register handler for SIGINT

	sendToOutside("zzm@localhost", "zhaozemin1997@gmail.com", "test");
	sendToOutside2();

	pthread_exit(NULL);// instead of exit(0), threads can still work after main exits
}

void sendToOutside(string from, string to, string content) {
	string hostname = "seas.upenn.edu";
	string response;
	unsigned char hostBuffer[bufLen];
	int r_size = res_query(hostname.c_str(), C_IN, T_MX, hostBuffer, bufLen);
	if (r_size == -1) {
		response = "-ERR Cannot find host server\r\n";
		printf("%s", response.c_str());
	} else if (r_size == bufLen) {
		response = "-ERR Buffer too small\r\n";
		printf("%s", response.c_str());
	}
	HEADER* hdr = reinterpret_cast<HEADER*>(hostBuffer);
	if (hdr->rcode != NOERROR) {
		response = "-ERR There is some error\r\n";
		printf("%s", response.c_str());
	}
	int question = ntohs(hdr->qdcount);
	int answers = ntohs(hdr->ancount);
	int nameservers = ntohs(hdr->nscount);
	int addrrecords = ntohs(hdr->arcount);

	ns_msg m;
	int k = ns_initparse(hostBuffer, r_size, &m);
	if (k == -1) {
		cerr << errno << " " << strerror(errno) << endl;
	}

	for (int i = 0; i < question; ++i) {
		ns_rr rr;
		int k = ns_parserr(&m, ns_s_qd, i, &rr);
		if (k == -1) {
			cout << "reach 2 " << i;
			cerr << errno << " " << strerror(errno) << endl;
		}
	}

	char* IP_address = parse_record(hostBuffer, r_size, "addrrecords", ns_s_ar, 2, &m);


}

char* parse_record (unsigned char *buffer, size_t r,
		const char *section, ns_sect s,
		int idx, ns_msg *m) {

	ns_rr rr;
	int k = ns_parserr (m, s, idx, &rr);
	if (k == -1) {
		std::cerr << errno << " " << strerror (errno) << "\n";
		return 0;
	}

	std::cout << section << " " << ns_rr_name (rr) << " "
			<< ns_rr_type (rr) << " " << ns_rr_class (rr)<< " "
			<< ns_rr_ttl (rr) << " ";

	const size_t size = NS_MAXDNAME;
	unsigned char name[size];
	int t = ns_rr_type (rr);

	const u_char *data = ns_rr_rdata (rr);
	if (t == T_MX) {
		cout<<"t == T_MX"<<endl;
		int pref = ns_get16 (data);
		ns_name_unpack (buffer, buffer + r, data + sizeof (u_int16_t),
				name, size);
		char name2[size];
		ns_name_ntop (name, name2, size);
		std::cout << pref << " " << name2;
		return NULL;
	}
	else if (t == T_A) {
		cout<<"t == T_A"<<endl;
		unsigned int addr = ns_get32 (data);
		struct in_addr in;
		in.s_addr = ntohl (addr);
		char *a = inet_ntoa (in);
		//		std::cout << a;
		return a;
	}
	else if (t == T_NS) {
		cout<<"t == T_NS"<<endl;
		ns_name_unpack (buffer, buffer + r, data, name, size);
		char name2[size];
		ns_name_ntop (name, name2, size);
		std::cout << name2;
		return NULL;
	}
	else {
		std::cout << "unhandled record";
	}

	std::cout << "\n";
}

void sendToOutside2() {

	string server_name = "gmail.com";
	int BUFFER_SIZE = bufLen;

const char* host = server_name.c_str();
unsigned char host_buffer[BUFFER_SIZE];
int r_size = res_query(host,C_IN,T_MX, host_buffer, BUFFER_SIZE);

if(r_size == -1){
	char err_host[] = "ERR: Cannot find host server!";
	cout<<err_host<<endl;
}
else if(r_size == static_cast<int> (BUFFER_SIZE)){
	char err[] = "ERR: Buffer too small, finding host server truncated!";
	cout<<err<<endl;
}
HEADER *hdr = reinterpret_cast<HEADER*> (host_buffer);
if (hdr->rcode != NOERROR) {

	std::cerr << "Error: ";
	switch (hdr->rcode) {
	case FORMERR:
		std::cerr << "Format error";
		break;
	case SERVFAIL:
		std::cerr << "Server failure";
		break;
	case NXDOMAIN:
		std::cerr << "Name error";
		break;
	case NOTIMP:
		std::cerr << "Not implemented";
		break;
	case REFUSED:
		std::cerr << "Refused";
		break;
	default:
		std::cerr << "Unknown error";
		break;
	}
}

//print information of the answers
int question = ntohs (hdr->qdcount);
int answers = ntohs (hdr->ancount);
int nameservers = ntohs (hdr->nscount);
int addrrecords = ntohs (hdr->arcount);

//			std::cout << "Reply: question: " << question << ", answers: " << answers
//					<< ", nameservers: " << nameservers
//					<< ", address records: " << addrrecords << "\n";

ns_msg m;
int k = ns_initparse (host_buffer, r_size, &m);
if (k == -1) {
	std::cerr << errno << " " << strerror (errno) << "\n";
}

for (int i = 0; i < question; ++i) {
	ns_rr rr;
	int k = ns_parserr (&m, ns_s_qd, i, &rr);
	if (k == -1) {
		std::cerr << errno << " " << strerror (errno) << "\n";
	}
}

//			for (int i = 0; i < answers; ++i) {
//				parse_record (host_buffer, r_size, "answers", ns_s_an, i, &m);
//			}
//
//			for (int i = 0; i < nameservers; ++i) {
//				parse_record (host_buffer, r_size, "nameservers", ns_s_ns, i, &m);
//			}
//
//			for (int i = 0; i < addrrecords; ++i) {
//				parse_record (host_buffer, r_size, "addrrecords", ns_s_ar, i, &m);
//			}

char* IP_address = parse_record (host_buffer, r_size, "addrrecords", ns_s_ar, 2, &m);

cout<<"IP_address = "<<IP_address<<endl;

// connect to mail server
/*

struct connection conn;

conn.fd = -1;
conn.bufferSizeBytes = BUFFER_SIZE;
conn.bytesInBuffer = 0;
conn.buf = (char*)malloc(BUFFER_SIZE);

conn.fd = socket(PF_INET, SOCK_STREAM, 0);
if (conn.fd < 0)
	panic("Cannot open socket (%s)", strerror(errno));

struct sockaddr_in servaddr;
bzero(&servaddr, sizeof(servaddr));
servaddr.sin_family=AF_INET;
servaddr.sin_port=htons(25);
inet_pton(AF_INET, IP_address, &(servaddr.sin_addr));

if (connect(conn.fd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0)
	panic("Cannot connect to localhost:10000 (%s)", strerror(errno));

conn.bytesInBuffer = 0;

expectToRead(&conn, "220 localhost *");
expectNoMoreData(&conn);

writeString(&conn, "HELO tester\r\n");
expectToRead(&conn, "250 localhost");
expectNoMoreData(&conn);

// Specify the sender and the receipient (with one incorrect recipient)
char resp[BUFFER_SIZE] = "MAIL FROM:<";
strcat(resp, sender.c_str());
strcat(resp, ">\r\n");

writeString(&conn, resp);
expectToRead(&conn, "250 OK");
expectNoMoreData(&conn);

char resp1[BUFFER_SIZE] = "RCPT TO:<";
strcat(resp1, MsgQueue[j].rcvr.c_str());
strcat(resp1, ">\r\n");
writeString(&conn, resp1);
expectToRead(&conn, "250 OK");
expectNoMoreData(&conn);

// Send the actual data

writeString(&conn, "DATA\r\n");
expectToRead(&conn, "354 *");
expectNoMoreData(&conn);
string content = lines.str();
writeString(&conn, content.c_str());
expectToRead(&conn, "250 OK");
expectNoMoreData(&conn);

// Close the connection

writeString(&conn, "QUIT\r\n");
expectToRead(&conn, "221 *");
expectRemoteClose(&conn);
closeConnection(&conn);

freeBuffers(&conn);
*/
}
