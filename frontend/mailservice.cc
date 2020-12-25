#include <stdlib.h>
#include <stdio.h>
#include <openssl/md5.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <string.h>
#include <vector>
#include <iostream>
#include "global_constants.h"
#include "account_service.h"
#include "helpers.h"
#include "mailservice.h"
#include "helpers.h"
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <resolv.h>
#include <netdb.h>

using namespace std;

/* cput value with old value */
bool tmp_do_CPUT(char* storage_node_ip, int storage_node_port, string r, string c, string v, string v2, char* result) {
	// create socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return false;
	}
	// connect to the KV store storage node
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(storage_node_port);
	inet_pton(AF_INET, storage_node_ip, &(servaddr.sin_addr));
	connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	char buffer [BUFFER_SIZE];

	// make an RPC call
	string to_write = "CPUT`" + r + "`" + c + "`" + v + "`" + v2 +"\r\n";
	memset(buffer, 0, BUFFER_SIZE);
	strcpy(buffer, to_write.c_str());
	printf("before cput, buffer is %s\n", buffer);
	if (!do_write(sockfd, buffer, strlen(buffer))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		return false;
	}

	// receive the response from the KV store master node
	memset(buffer, 0, BUFFER_SIZE);
	char* curr_pos = buffer;
	while (strstr(buffer, "\r\n") == NULL) {
		int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
		if (n < 0) {
			cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
			return false;
		}
		curr_pos += n;
	}

	char goodbye_msg [10];
	memset(goodbye_msg, 0, 10);
	strcpy(goodbye_msg, "quit`\r\n");
	if (!do_write(sockfd, goodbye_msg, strlen(goodbye_msg))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		close(sockfd);
		return false;
	}

	// close the connection
	close(sockfd);

	// parse the response
	char* msg_end = strstr(buffer, "\r\n");
	strncpy(result, buffer, msg_end - buffer);
	if (IS_DEBUG) {
		cout << "KV storage node " << "<" << storage_node_ip << ":" << storage_node_port << ">"
				<< "responds: " << result << endl;
	}

	return true;
}

bool tmp_do_DELETE(char* storage_node_ip, int storage_node_port, string r, string c, char* result) {
	check_storage_node_connection(storage_node_ip, &storage_node_port);
	if (IS_DEBUG) {
		cout << "do_CPUT connects to " << storage_node_ip << ":" << storage_node_port << endl;
	}
	// create socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return false;
	}
	// connect to the KV store storage node
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(storage_node_port);
	inet_pton(AF_INET, storage_node_ip, &(servaddr.sin_addr));
	connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	char buffer [BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);

	// make an RPC call
	string to_write = "DELETE`" + r + "`" + c +"\r\n";
	strcpy(buffer, to_write.c_str());
	printf("before DELETE, buffer is %s\n", buffer);
	if (!do_write(sockfd, buffer, strlen(buffer))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		return false;
	}

	// receive the response from the KV store master node
	memset(buffer, 0, BUFFER_SIZE);
	char* curr_pos = buffer;
	while (strstr(buffer, "\r\n") == NULL) {
		int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
		if (n < 0) {
			cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
			return false;
		}
		curr_pos += n;
	}

	char goodbye_msg [10];
	memset(goodbye_msg, 0, 10);
	strcpy(goodbye_msg, "quit`\r\n");
	if (!do_write(sockfd, goodbye_msg, strlen(goodbye_msg))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		close(sockfd);
		return false;
	}

	// close the connection
	close(sockfd);

	// parse the response
	char* msg_end = strstr(buffer, "\r\n");
	strncpy(result, buffer, msg_end - buffer);
	if (IS_DEBUG) {
		cout << "KV storage node " << "<" << storage_node_ip << ":" << storage_node_port << ">"
				<< "responds: " << result << endl;
	}

	return true;
}


/* when user clicks delete of a mail, call try_deletemail */
string try_deletemail(map<string, string> cookies, string request_body) {
	printf("\r\n-------------------------------------------------------------------------------------------------\r\n");
	printf("call try_deletemail, request_body: %s\n", request_body.c_str());
	string user_name = cookies["user"];
//	user_name = user_name.substr(0, user_name.length() - 1);// cookies["user"] has 13=\r at last
	string contact_ip = cookies["contact_ip"];
	string contact_port = cookies["contact_port"];
	cout << user_name << endl;
	cout << user_name.length() << endl;
	cout << contact_ip << endl;
	cout << contact_ip.length() << endl;
	cout << contact_port << endl;
	cout << contact_port.length() << endl;
	// get node_ip and node_port
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	strcpy(storage_node_ip, contact_ip.c_str());
	storage_node_port = stoi(contact_port);

	// parse request_body, to get uidl
	map<string, string> params = get_params_from_request_body(request_body);
	string uidl = params["uidl"];
	printf("uidl: %s\n", uidl.c_str());

	// delete uidl-msg pair from KV-store
	char delete_response [BUFFER_SIZE];// to fetch uidl_list
	memset(delete_response, 0, BUFFER_SIZE);
	tmp_do_DELETE(storage_node_ip, storage_node_port, user_name, uidl, delete_response);
	printf("do_DELETE response: %s\n", delete_response);
	if (strstr(delete_response, "-ERR")) {
		printf("do DELETE failed\n");// TODO redirect to FAIL page
	}

	// get uidl_list
	printf("go to get_uidl_list of %s\n", user_name.c_str());
	vector<string> uidl_list;// uidls in a vector
	char origin_uidl_list [1000];// uidl_list of original format, e.g. "ABDED,FDSFS,", "" for none uidl_list
	memset(origin_uidl_list, 0, 1000);
	get_uidl_list(user_name, string(storage_node_ip), storage_node_port, uidl_list, origin_uidl_list);

	// create a new uidl_list and cput back to node
	vector<string> new_uidl_list;
	string new_uidl_list_str = "";
	for (int i = 0; i < uidl_list.size(); i++) {
		if (uidl_list[i] != uidl) {
			new_uidl_list.push_back(uidl_list[i]);
			new_uidl_list_str += uidl_list[i] + ",";
		}
	}
	printf("new_uidl_list_str: %s, len: %ld\n", new_uidl_list_str.c_str(), new_uidl_list_str.length());
	printf("go to do_cput, origin uidl list is %s\n", origin_uidl_list);
	char put_response [BUFFER_SIZE];
	memset(put_response, 0, BUFFER_SIZE);
	do_CPUT(storage_node_ip, storage_node_port, user_name, "uidl_list", string(origin_uidl_list), new_uidl_list_str, put_response);
	printf("cput_res is %s\n", put_response);

	// sendback response
	printf("sendback response\n");
	return response_generator("pages/mail-delete-ok.html");
}

/* when user clicks mail-home, call try_listmail */
string try_listmail(map<string, string> cookies) {
	cout << "call try_listmail" << endl;
	string user_name = cookies["user"];
//	user_name = user_name.substr(0, user_name.length() - 1);// cookies["user"] has 13=\r at last
	string contact_ip = cookies["contact_ip"];
	string contact_port = cookies["contact_port"];
	cout << user_name << endl;
	cout << user_name.length() << endl;
	cout << contact_ip << endl;
	cout << contact_ip.length() << endl;
	cout << contact_port << endl;
	cout << contact_port.length() << endl;
	// get node_ip and node_port
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	strcpy(storage_node_ip, contact_ip.c_str());
	storage_node_port = stoi(contact_port);

	// get uidl_list
	printf("go to get_uidl_list of %s\n", user_name.c_str());
	vector<string> uidl_list;// uidls in a vector
	char origin_uidl_list [1000];// uidl_list of original format, e.g. "ABDED,FDSFS,", "" for none uidl_list
	memset(origin_uidl_list, 0, 1000);
	get_uidl_list(user_name, string(storage_node_ip), storage_node_port, uidl_list, origin_uidl_list);
	printf("after get all the uidls from uidlList\n");

	// get entire messages and headers according to uidl
	printf("go to fetch entire msgs\n");
	vector<string> entire_msgs;
	for (int i = 0; i < uidl_list.size(); i++) {
		char get_response [BUFFER_SIZE];// to fetch uidl_list
		memset(get_response, 0, BUFFER_SIZE);
		printf("i = %d, go to get %s\n", i, uidl_list[i].c_str());
		if (!do_GET(storage_node_ip, storage_node_port, user_name, uidl_list[i], get_response)) {
			printf("failed to get uidl\n");
		}
		printf("get_response: %s\n", get_response);
		string entire_msg = string(get_response + 4);// skip +OK
		entire_msgs.push_back(entire_msg);
		printf("push entire_msg: %s\n", get_response);
	}
	printf("\n after get all contents of mails in the list\n");
	cout << "---------------------------------------------\n\n";
	vector<string> senders;
	vector<string> timests;
	vector<string> subjects;
	vector<string> contents;
	for (int i = 0; i < entire_msgs.size(); i++) {
		printf("i=%d, entire_msgs=%s\n", i, entire_msgs[i].c_str());

		// sender
		int left_arrow = 5;// position of From <
		int right_arrow = entire_msgs[i].find(">");
		string sender = entire_msgs[i].substr(left_arrow + 1, right_arrow - (left_arrow + 1));
		senders.push_back(sender);

		// time
		int first_newline = entire_msgs[i].find("\n");
		string timest = entire_msgs[i].substr(right_arrow + 2, first_newline - (right_arrow + 2));
		timests.push_back(timest);

		// subject
		int second_newline = entire_msgs[i].find("\n", first_newline + 1);
		string subject = entire_msgs[i].substr(first_newline + 1, second_newline - (first_newline + 1));
		printf("subject: %s\n", subject.c_str());
		subject = UrlDecode(subject);// decode urlencoding
		printf("subject: %s\n", subject.c_str());
		subjects.push_back(subject);

		// content
		string content = entire_msgs[i].substr(second_newline + 1);
		printf("content: %s\n", content.c_str());
		content = UrlDecode(content);// decode urlencoding
		printf("content: %s\n", content.c_str());
		contents.push_back(content);
	}

	for (int i = 0; i < uidl_list.size(); i++) {
		cout << i << endl;
		cout << senders[i] << endl;
		cout << timests[i] << endl;
		cout << contents[i] << endl;
		cout << uidl_list[i] << endl;
	}

	// create replace
	string content;
	for (int i = uidl_list.size() - 1; i >= 0; i--) { // 1-index
		content += "<tr>";
		{
			// [No.]
			content += "<td>" + to_string(uidl_list.size() - i) + "</td>";

			// [Sender]
			content += "<td>" + senders[i] + "</td>";

			// [Subject]
			content += "<td>" + subjects[i] + "</td>";

//			// [Content]
//			content += "<td>" + contents[i] + "</td>";

			// [Detail]
			content += "<td>";
			content += "<form action='/mail-detail' method='post'>";
			content += "<button name='uidl' value='" + uidl_list[i]
					+ "' class='btn btn-info'>Details</button>";
			content += "</form>";
			content += "</td>";

			// [Time]
			content += "<td>" + timests[i] + "</td>";



			// [Delete]
			content += "<td>";
			content += "<form action='/mail-delete' method='post'>";
			content += "<button name='uidl' value='" + uidl_list[i]
					+ "' class='btn btn-danger'>Delete</button>";
			content += "</form>";
			content += "</td>";
		}
		content += "</tr>";
	}



	// create response
	string response = response_generator("pages/mail-home.html", "$content", content);
	return response;
}


/* send message to seas server and receive reply messages */
void write_and_read(int sock, string to_write) {
	char write_buffer [BUFFER_SIZE];
	memset(write_buffer, 0, BUFFER_SIZE);
	strcpy(write_buffer, to_write.c_str());
	cout << "Write: " << write_buffer << endl;
	int write_len = write(sock, write_buffer, strlen(write_buffer));
	char read_buffer [BUFFER_SIZE];
	memset(read_buffer, 0, BUFFER_SIZE);
	int read_len = read(sock, read_buffer, BUFFER_SIZE);
	cout << "read_len: " << read_len << endl;
	cout << "read_buffer: " << read_buffer << endl;
}


/* when user clicks compose, call try_sendmail */
string try_sendmail(map<string, string> cookies, string request_body) {
	printf("--------------------------------------------------------------\r\n");
	printf("call try_sendmail, request_body: %s\n", request_body.c_str());
	// get sender from cookies
	string user_name = cookies["user"];
//	user_name = user_name.substr(0, user_name.length() - 1);// cookies["user"] has 13=\r at last
	string contact_ip = cookies["contact_ip"];
	string contact_port = cookies["contact_port"];
	cout << user_name << endl;
	cout << user_name.length() << endl;
	cout << contact_ip << endl;
	cout << contact_ip.length() << endl;
	cout << contact_port << endl;
	cout << contact_port.length() << endl;
	string sender_name = user_name;

	// parse request_body, to get rcpt_address, content e.g. rcpt_address=xiaoze%40penncloud&content=hihihi+hi
	map<string, string> params = get_params_from_request_body(request_body);
	string rcpt_address = params["rcpt_address"];
	string msg_content = params["msg_content"];
	string msg_subject = params["msg_subject"];
	printf("rcpt_address: %s, msg_content: %s, msg_subject: %s, subjectLen=%ld\n", rcpt_address.c_str(), msg_content.c_str(), msg_subject.c_str(), msg_subject.length());

	// create entire message including From sender_name + timestamp + msg_content, will add entire_msg-creator later
	char time_stamp[100];// get timestamp
	time_t timestp;
	time(&timestp);// get time with format of time_t
	strcpy(time_stamp, asctime(gmtime(&timestp)));// gmtime-struct tm format, asctime-normal format, includes LF='\n'
	string entire_msg = "From <" + sender_name + "@penncloud" + "> " + string(time_stamp) + msg_subject + "\n" + msg_content;
	printf("entire_msg: %s\n", entire_msg.c_str());

	// do not use sendmail, try another way without given sockfd, connect master to get rcpt_node ip:port
	// 2020.12.17: cookies only have sender's node, still need to ask master to get rcpt node
	// check whether rcpt is inner system
	int pos_at = rcpt_address.find("%40");
	string rcpt_name = rcpt_address.substr(0, pos_at);
//	string rcpt_name = rcpt_address;// in the register page, use zimao%40penncloud as username ??
	string rcpt_domain = rcpt_address.substr(pos_at + 3);
	printf("rcpt_name:%s, rcpt_domain:%s\n", rcpt_name.c_str(), rcpt_domain.c_str());

	if (rcpt_domain == "penncloud") {
		printf("domain is penncloud\n");

		// first ask master the node address for recipient
		char rcpt_ip[50];
		memset(rcpt_ip, 0, 50);
		int rcpt_port = -1;
		printf("try_sendmail: connect_to_kv_master\n");
		if (!connect_to_kv_master(rcpt_ip, &rcpt_port, rcpt_name, "REQUEST")) {
			return response_generator("pages/inactive.html");
		}

		// put uidl-msg pair to K-V store node
		printf("entire_msg: %s\n", entire_msg.c_str());
		string uidl = getUIDL(entire_msg);// compute uidl
		printf("created uidl: %s\n", uidl.c_str());
		char put_response [BUFFER_SIZE];
		memset(put_response, 0, BUFFER_SIZE);
		printf("go to do_PUT %s, %s\n", uidl.c_str(), entire_msg.c_str());
		printf("send, before do_PUT, rcpt_name=%s, len=%ld\n", rcpt_name.c_str(), rcpt_name.length());
		printf("rcpt_ip: %s, port: %d\n", rcpt_ip, rcpt_port);
		if (!do_PUT(rcpt_ip, rcpt_port, rcpt_name, uidl, entire_msg, put_response)) {
			printf("failed to put uidl-msg pair\n");
		}
		printf("after put uidl, put_res: %s\n", put_response);

		// get uidl_list
		printf("go to get_uidl_list of %s\n", rcpt_name.c_str());
		vector<string> uidl_list;
		char origin_uidl_list [1000];
		memset(origin_uidl_list, 0, 1000);
		printf("rcpt_name: %s, rcpt_ip: %s, rcpt_port: %d\n", rcpt_name.c_str(), rcpt_ip, rcpt_port);
		get_uidl_list(string(rcpt_name), string(rcpt_ip), rcpt_port, uidl_list, origin_uidl_list);
		for (int i = 0; i < uidl_list.size(); i++) {
			printf("return uidl_list: %d, %s\n", i, uidl_list[i].c_str());
		}

		// append current uidl to the list
		printf("append current uidl to the list vector\n");
		uidl_list.push_back(uidl);// append current uidl to local list
		printf("convert uidl list vector to string\n");
		string uidl_list_str = "";// string format of uidl_list
		for (int i = 0; i < uidl_list.size(); i++) {
			uidl_list_str += uidl_list[i] + ",";// keep the comma at last, e.g."ABCDE,"
		}

		// if uidl list was deleted to empty before, origin_uidl_list is set to "\n" in the get_uidl_list

		// update uidl list in K-V store
		printf("go to put/cput back the uidl list\n");
		memset(put_response, 0, BUFFER_SIZE);
		if (strlen(origin_uidl_list) == 0) {
			// first time to put uidl list, use PUT
			printf("origin uidl list is empty, go to do_PUT\n");
			do_PUT(rcpt_ip, rcpt_port, rcpt_name, "uidl_list", uidl_list_str, put_response);
			printf("put_res is %s\n", put_response);
		} else {
			// use CPUT
			printf("origin uidl list is not empty, go to do_cput, origin uidl list is %s\n", origin_uidl_list);
			do_CPUT(rcpt_ip, rcpt_port, rcpt_name, "uidl_list", string(origin_uidl_list), uidl_list_str, put_response);
			printf("cput_res is %s\n", put_response);
		}


		// sendback response
		printf("sendback response\n");
		return response_generator("pages/mail-send-ok.html");

	} else {
		// based on stackoverflow example posted on piazza
		// rcpt_domain can be "seas.upenn.edu", it is not authorized to send to gamil
		cout << "use method of StackOverflow example on piazza@792" << endl;
		string target_domain;// the IP address of target
		int N = 4096;
		u_char nsbuf[N];
		char dispbuf[N];
		ns_msg msg;
		ns_rr rr;
		int i, l;

		// get MX RECORD
		printf("MX records : \n");
		l = res_query(rcpt_domain.c_str(), C_IN, T_MX, nsbuf, sizeof(nsbuf));//		l = res_query(rcpt_domain.c_str(), ns_c_any, ns_t_mx, nsbuf, sizeof(nsbuf));
		cout << "l: " << l << endl;
		if (l < 0) {
		  cout << "error: l < 0" << endl;
		} else {
		  	// grab the MX answer
			ns_initparse(nsbuf, l, &msg);
			l = ns_msg_count(msg, ns_s_an);
			cout << "l: " << l << endl;
			for (i = 0; i < l; i++) {
				ns_parserr(&msg, ns_s_an, i, &rr);
				ns_sprintrr(&msg, &rr, NULL, NULL, dispbuf, sizeof(dispbuf));
				string dispbuf_str(dispbuf);
				int last_space_pos = dispbuf_str.find_last_of(" ");// domain is after last space
				target_domain = dispbuf_str.substr(last_space_pos + 1);
				break;// just get the first one
			}
		}

		// change domain to servaddr
		// change hostname to host entry
		struct hostent *host_entry;
		host_entry = gethostbyname(target_domain.c_str());
		// change network to string
		char *IPbuffer;
		IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
		printf("Target Address: %s", IPbuffer);
		struct sockaddr_in seasAddr;
		bzero(&seasAddr, sizeof(seasAddr));
		seasAddr.sin_addr.s_addr = inet_addr(IPbuffer);
		seasAddr.sin_family = AF_INET;
		seasAddr.sin_port = htons(25);

		// connect and send messages
		// create socket
		int extern_sock = socket(AF_INET, SOCK_STREAM, 0);
		// connect to seas server
		int conn_ret = connect(extern_sock, (struct sockaddr*)&seasAddr, sizeof(seasAddr));
		if (conn_ret < 0) {
			printf("error: connect failed\n");
		}
		printf("conn_ret: %d\n", conn_ret);
		// send messages, receive messages
		write_and_read(extern_sock, "HELO penncloud.com\r\n");
		write_and_read(extern_sock, "MAIL FROM: <" + sender_name + "@penncloud.com>\r\n");
		write_and_read(extern_sock, "RCPT TO: <" + rcpt_name + "@" + rcpt_domain + ">\r\n");
		write_and_read(extern_sock, "DATA\r\n");
		write_and_read(extern_sock, UrlDecode(entire_msg) + "\r\n.\r\n");
		write_and_read(extern_sock, "QUIT\r\n");
		close(conn_ret);
	}


	// sendback response
	printf("sendback response\n");
	return response_generator("pages/mail-send-ok.html");
}



/* get uidl_list from a user */
void get_uidl_list(string user_name, string storage_node_ip, int storage_node_port, vector<string> &result, char* origin_uidl_list) {
	printf("call get_uidl_list, user_name=%s, len=%ld\n", user_name.c_str(), user_name.length());
	char get_response [BUFFER_SIZE];// to fetch uidl_list
	memset(get_response, 0, BUFFER_SIZE);
	char tmp_input [storage_node_ip.length() + 1];
	strcpy(tmp_input, storage_node_ip.c_str());
	printf("get_uidl_list: go to do_GET, tmp_input: %s, storage_node_port: %d\n", tmp_input, storage_node_port);
	if (!do_GET(tmp_input, storage_node_port, user_name, "uidl_list", get_response)) {
		printf("failed to get uidl_list\n");
	}
	cout << "test messy code" << endl;

	cout << "inside get_uidl_list, after do_GET, get res is" << endl;
	cout << get_response << endl;
	cout << "len of get_response=" << endl;
	cout << strlen(get_response) << endl;

//	printf("in get_uidl_list, traverse get_response\n");
//	for (int i = 0; i < strlen(get_response); i++) {
//		printf("i=%d, char=%d\n", i, (int)get_response[i]);
//	}

	vector<string> uidl_list;// to store uidl list to local

	if (strlen(get_response) == 5) {
		// all mails have been deleted, uidl_list in node only response "+OK \n\r\n" (why \n)
		printf("get_response len is 5\n");
		for (int i = 0; i < 5; i++) {
			printf("i=%d, char=%d\n", i, *(get_response+i));
		}
		strcpy(origin_uidl_list, "\n");
	} else if (!strstr(get_response, "-ERR")) {
		printf("no -ERR\n");
		// the node has a uidl_list, (if there is -ERR, there is no uidl_list in the node)
		strcpy(origin_uidl_list, get_response + 4);// skip "+OK "
		char* token = strtok(get_response + 4, ",");// skip "+OK"
		while (token != NULL) {
			printf("inside get_uidl_list, go to push token=%s, len=%ld\n", token, strlen(token));
			uidl_list.push_back(string(token));
			token = strtok(NULL, ",");
		}
	}
	printf("origin_uidl_list is %s, len=%ld\n", origin_uidl_list, strlen(origin_uidl_list));
	printf("first char is %d\n", (int)origin_uidl_list[0]);
	printf("created a vector of string, uidl_list, size is %ld\n", uidl_list.size());
	result = uidl_list;
}

/* when user clicks detail of a mail, call try_showdetail */
string try_showdetail(map<string, string> cookies, string request_body) {
	printf("\r\n-------------------------------------------------------------------------------------------------\r\n");
	printf("call try_showdetail, request_body: %s\n", request_body.c_str());
	string user_name = cookies["user"];
//	user_name = user_name.substr(0, user_name.length() - 1);// cookies["user"] has 13=\r at last
	string contact_ip = cookies["contact_ip"];
	string contact_port = cookies["contact_port"];
	cout << user_name << endl;
	cout << user_name.length() << endl;
	cout << contact_ip << endl;
	cout << contact_ip.length() << endl;
	cout << contact_port << endl;
	cout << contact_port.length() << endl;
	// get node_ip and node_port
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	strcpy(storage_node_ip, contact_ip.c_str());
	storage_node_port = stoi(contact_port);

	// parse request_body, to get uidl
	map<string, string> params = get_params_from_request_body(request_body);
	string uidl = params["uidl"];
	printf("uidl: %s\n", uidl.c_str());


	// get uidl-entire-msg pair from KV-store
	char get_response [BUFFER_SIZE];// to fetch uidl_list
	memset(get_response, 0, BUFFER_SIZE);
	printf("go to get %s\n", uidl.c_str());
	if (!do_GET(storage_node_ip, storage_node_port, user_name, uidl, get_response)) {
		printf("failed to get uidl's value\n");
	}
	printf("get_response: %s\n", get_response);
	string entire_msg = string(get_response + 4);// skip +OK

	// sender
	int left_arrow = 5;// position of From <
	int right_arrow = entire_msg.find(">");
	string sender = entire_msg.substr(left_arrow + 1, right_arrow - (left_arrow + 1));

	// time
	int first_newline = entire_msg.find("\n");
	string timest = entire_msg.substr(right_arrow + 2, first_newline - (right_arrow + 2));

	// subject
	int second_newline = entire_msg.find("\n", first_newline + 1);
	string subject = entire_msg.substr(first_newline + 1, second_newline - (first_newline + 1));
	printf("subject: %s\n", subject.c_str());
	subject = UrlDecode(subject);// decode urlencoding
	printf("subject: %s\n", subject.c_str());

	// content
	string content = entire_msg.substr(second_newline + 1);
	printf("content: %s\n", content.c_str());
	content = UrlDecode(content);// decode urlencoding
	printf("content: %s\n", content.c_str());

	// create a well-formed entire_msg for forward
	string entire_msg_forward = "From: " + sender + "\r\nTo: " + user_name  + "@penncloud\r\n" +
			"Time: " + timest + "\r\nSubject: " + subject + "\r\nContent:\r\n" + content;

	// create and sendback response
	printf("sendback response\n");
	string response = response_generator("pages/mail-detail.html", "$from", sender, "$subject", subject, "$content", content, "$entireMessage", entire_msg_forward);
	return response;
}

//
//void replymail();
//
//void forwardmail();





void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer) {
  /* The digest will be written to digestBuffer, which must be at least MD5_DIGEST_LENGTH bytes long */

  MD5_CTX c;
  MD5_Init(&c);
  MD5_Update(&c, data, dataLengthBytes);
  MD5_Final(digestBuffer, &c);
}

/* compute uidl and transfer to string (piazza@597) */
string getUIDL(string msg) {
	unsigned char buffer[10000];
	computeDigest((char*)msg.c_str(), msg.length(), buffer);
	string result;
	for (std::size_t i = 0; i != 16; ++i) {
		result += "0123456789ABCDEF"[buffer[i] / 16];
		result += "0123456789ABCDEF"[buffer[i] % 16];
	}
	return result;
}


/* not used */
bool connect_to_kv_master2(char* storage_node_ip, int* storage_node_port, string user_name, string command) {
	// create socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return false;
	}

	// connect to the KV store master node
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(KV_MASTER_PORT);
	inet_pton(AF_INET, KV_MASTER_IP, &(servaddr.sin_addr));
	connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	char buffer [BUFFER_SIZE];

	// request a connection address to a storage node
	string to_write = command + "`" + user_name + "\r\n";
	memset(buffer, 0, BUFFER_SIZE);
	strcpy(buffer, to_write.c_str());
	if (!do_write(sockfd, buffer, strlen(buffer))) {
		cerr << "Login service: fail to connect with the KV store master\n" << endl;
		return false;
	}

	if (IS_DEBUG) {
		cout << "Sent: " << to_write << "to kv master" << endl;
	}

	// receive the response from the KV store master node
	memset(buffer, 0, BUFFER_SIZE);
	char* curr_pos = buffer;
	while (strstr(buffer, "\r\n") == NULL) {
		int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
		if (IS_DEBUG) {
			cout << "n: " << n << endl;
		}
		if (n < 0) {
			cerr << "Login service: fail to connect with the KV store master\n" << endl;
			return false;
		}
		curr_pos += n;
	}

	// close the connection
	close(sockfd);

	// parse the response
	char master_feedback [BUFFER_SIZE];
	memset(master_feedback, 0, BUFFER_SIZE);
	char* msg_end = strstr(buffer, "\r\n");
	strncpy(master_feedback, buffer, msg_end - buffer);
	if (IS_DEBUG) {
		cout << "KV master responds: " << master_feedback << endl;
	}

	// get the response code first: +OK or -ERR
	char* token = strtok(master_feedback, " ");
	if (strcasecmp(token, "+OK") != 0) {
		// meaning there is already such user
		return true;
	}
	// continue to parse the address of the storage node
	token = strtok(NULL, " ");
	char storage_node_addr [strlen(token) + 1];
	strcpy(storage_node_addr, token);

	token = strtok(storage_node_addr, ":");
	strcpy(storage_node_ip, token);
	token = strtok(NULL, ":");
	*storage_node_port = atoi(token);

	return true;
}

bool do_GET2(char* storage_node_ip, int storage_node_port, string r, string c, char* result) {
	printf("in do_GET2, row=%s, len=%ld", r.c_str(), r.length());
	// create socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return false;
	}
	// connect to the KV store storage node
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(storage_node_port);
	inet_pton(AF_INET, storage_node_ip, &(servaddr.sin_addr));
	connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	char buffer [BUFFER_SIZE];

	// make an RPC call
	string to_write = "GET`" + r + "`" + c + "\r\n";
	memset(buffer, 0, BUFFER_SIZE);
	strcpy(buffer, to_write.c_str());
	if (!do_write(sockfd, buffer, strlen(buffer))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		return false;
	}

	// receive the response from the KV store master node
	memset(buffer, 0, BUFFER_SIZE);
	char* curr_pos = buffer;
	while (strstr(buffer, "\r\n") == NULL) {
		int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
//		printf("in do_GET2 while read, buffer:%s\n, len=%ld\n", buffer, strlen(buffer));
//		for (int i = 0; i < strlen(buffer); i++) {
//			printf("i=%d, char=%d\n", i, (int)buffer[i]);
//		}
		if (n < 0) {
			cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
			return false;
		}
		curr_pos += n;
	}

	// close the connection
	close(sockfd);

	// parse the response
	char* msg_end = strstr(buffer, "\r\n");
	strncpy(result, buffer, msg_end - buffer);
//	if (IS_DEBUG) {
//		cout << "KV storage node " << "<" << storage_node_ip << ":" << storage_node_port << ">"
//				<< "responds: " << result << endl;
//	}

	return true;

}

bool do_PUT2(char* storage_node_ip, int storage_node_port, string r, string c, string v, char* result) {
	printf("in do_PUT2, row=%s, len=%ld\n", r.c_str(), r.length());
	// create socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cerr << "Login service: fail to open a socket\n" << endl;
		return false;
	}
	// connect to the KV store storage node
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(storage_node_port);
	inet_pton(AF_INET, storage_node_ip, &(servaddr.sin_addr));
	connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	char buffer [BUFFER_SIZE];

	// make an RPC call
	string to_write = "PUT`" + r + "`" + c + "`" + v +"\r\n";
	memset(buffer, 0, BUFFER_SIZE);
	strcpy(buffer, to_write.c_str());
	printf("stirng to_write: %s\n", to_write.c_str());
	if (!do_write(sockfd, buffer, strlen(buffer))) {
		cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
		return false;
	}

	// receive the response from the KV store master node
	memset(buffer, 0, BUFFER_SIZE);
	char* curr_pos = buffer;
	while (strstr(buffer, "\r\n") == NULL) {
		int n = read(sockfd, curr_pos, BUFFER_SIZE - strlen(buffer));
		if (n < 0) {
			cerr << "Login service: fail to connect with the KV store storage node\n" << endl;
			return false;
		}
		curr_pos += n;
	}

	// close the connection
	close(sockfd);

	// parse the response
	char* msg_end = strstr(buffer, "\r\n");
	strncpy(result, buffer, msg_end - buffer);
	if (IS_DEBUG) {
		cout << "KV storage node " << "<" << storage_node_ip << ":" << storage_node_port << ">"
				<< " responds: " << result << endl;
	}

	return true;

}

/* decode urlencoding */
string UrlDecode(const string& content) {
//	printf("call UrlDecode\n");
	string result;
	for (long i = 0; i < content.length(); i++) {
//		printf("content[%ld]: %d, %s\n", i, content[i], content.substr(i, 1).c_str());
		if (content[i] == '+') {
//			printf("+\n");
			result += ' ';// space
		} else if (content[i] == '%') {
//			printf("has%\n");
			// this is urlencoding, e.g. %40="@"
			if (isxdigit(content[i + 1]) && isxdigit(content[i + 2])) {
//				printf("hexdigits\n");
				string hexStr = content.substr(i + 1, 2);
				int hex = strtol(hexStr.c_str(), 0, 16);// endpointer='0', base 16
//				printf("hex: %d, %c\n", hex, char(hex));

				result += char(hex);
				i += 2;

			}else {
//				printf("=%\n");
				result += '%';
			}
		} else {
//			printf("original\n");
			result += content[i];
		}
//		printf("UrlDecode result: %s\n", result.c_str());
	}
	return result;
}




