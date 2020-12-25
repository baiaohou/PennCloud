/*
 * helpers.c
 *
 *  Created on: Nov 17, 2020
 *      Author: Baiao Hou
 */
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <stdarg.h>
#include <bits/stdc++.h>

#include "helpers.h"

/**
 * A helper function to create a buffer to read message from client
 * change to support large binary files
 */
//void read_helper(int comm_fd, string& client_msg) {
//	char buffer[1200];
//	 while (true) {
//	  int read_bytes = read(comm_fd, buffer, 1200);
//	  printf("read_bytes: %d\n", read_bytes);
//	  string temp(buffer, buffer + read_bytes);
//	  if (read_bytes < 1200) {
//		  printf("<1200, break\n");
//	   client_msg = client_msg + temp;
//	   cout << "client_msg: " << client_msg << endl;
//	   break;
//	  }
//	  client_msg = client_msg + temp;
//	 }
//}

void read_helper(int comm_fd, string& client_msg) {

	// check first 3 bytes, is GET or POST
	char buffer3[10];
	memset(buffer3, 0, 10);
	bool isGET = false;
	int read_bytes = read(comm_fd, buffer3, 3);// only read first three bytes
//	printf("read_bytes: %d\n", read_bytes);
	string temp(buffer3, buffer3 + read_bytes);
//	cout << "first three: " << temp << endl;
	if (temp != "POS") {
		// By only read first three bytes, this is not POST, it can be GET or something like *request* and *disable*5000
//		printf("By only read first three bytes, this is not POST, it can be GET or *request*\n");
		isGET = true;
		char getBuffer[1200];
		memset(getBuffer, 0, 1200);
		strcpy(getBuffer, buffer3);// copy GET or *request* to the start
		char* getBufferCur = getBuffer + read_bytes;// move cursor
//		int read_bytes = read_bytes;// at first, read 3 bytes
		while (true) {
			read_bytes += read(comm_fd, getBufferCur, 1200 - strlen(getBuffer));
//			printf("read_bytes: %d\n", read_bytes);
			string temp(getBuffer, getBuffer + read_bytes);
			if (read_bytes < 1200) {
//				printf("read bytes < 1200, return\n");
				client_msg = client_msg + temp;
//				cout << "client_msg: " << client_msg << endl;
				return;
			}
//			printf("read bytes >= 1200, break\n");
			client_msg = client_msg + temp;
			// clear to original
			getBufferCur = getBuffer;
			memset(getBuffer, 0, 1200);
			read_bytes = 0;
		}
	}

	// it is POST!
	printf("it is POS\n");
	int total_read_length = 0;// count total length of read bytes
	int store_to_client_msg_length = 0;// count total length of bytes that are stored to client_msg
	int remaining_length = 0;

	int tmp_size = 1200;
	char buffer[tmp_size];
	memset(buffer, 0, tmp_size);
	char* curr_buffer_pos = buffer;
	bool should_quit = false;
	int content_length = 0;

	int tmp_read_length = strlen(buffer3);
	remaining_length = tmp_read_length;
	strcpy(buffer, buffer3);
	curr_buffer_pos = buffer + tmp_read_length;
	while (true) {
//		tmp_read_length += read(comm_fd, curr_buffer_pos, tmp_size - strlen(buffer));// the length of bytes read
		tmp_read_length += read(comm_fd, curr_buffer_pos, tmp_size - remaining_length);// the length of bytes read
		printf("tmp_read_length: %d\n", tmp_read_length);
		total_read_length += tmp_read_length;
		if (tmp_read_length <= 0) {
			cout << "read len <= 0, break" << endl;
			string temp(buffer, buffer + tmp_read_length);
			client_msg = client_msg + temp;
			cout << "client_msg: " << client_msg << endl;
			break;
		}
		char* text_end = NULL;
		while ((text_end = strstr(buffer, "\r\n")) != NULL) {
			// add '\r' and '\n' back
			text_end += 2;
			char raw_msg_rcvd [tmp_size];
			strncpy(raw_msg_rcvd, buffer, text_end - buffer);
			raw_msg_rcvd[text_end - buffer] = '\0';

			// a line of raw message
			string one_row(raw_msg_rcvd);

			if (one_row == "\r\n") {
				// body
//				cout << "there is a rn in an independent line, should quit to read body" << endl;
				client_msg += one_row;
				should_quit = true;
			} else {
				string target = "Content-Length: ";
				if (one_row.find(target) != string::npos) {
					string content_length_str = one_row.substr(target.size());
					// remove \n
					content_length_str.erase(content_length_str.size() - 1);
					// remove \r
					content_length_str.erase(content_length_str.size() - 1);
					content_length = atoi(content_length_str.c_str());
				}
				client_msg += one_row;
			}

			// bytes of the length of (text_end - buffer) is added to client_msg
			int tmp_store_to_client_msg_length = text_end - buffer;
			store_to_client_msg_length += tmp_store_to_client_msg_length;


			//this read_helper_clean_buffer may ignore the '\0' in content
//			read_helper_clean_buffer(buffer, text_end);
			// ge remaining length, shift all remaining content to the leftmost
			remaining_length = total_read_length - store_to_client_msg_length;// the remaining content in buffer
			int buffer_pos = 0;
			for (buffer_pos = 0; buffer_pos < remaining_length; buffer_pos++) {
				buffer[buffer_pos] = *text_end;
				*text_end = 0;
				text_end++;
			}
			// clean the old-remaining content, the length should be the same as tmp_store_to_client_msg_length
			for (int k = 0; k < tmp_store_to_client_msg_length; k++) {
				buffer[buffer_pos + k] = 0;
			}


			if (should_quit) {
				break;
			}
		}


//		// keep the unused chars in the buffer, and continue to read more in the next iteration
//		curr_buffer_pos = buffer;
//		while (*curr_buffer_pos != '\0') {
//			curr_buffer_pos++;
//		}
		// maybe we cannot just find the '\0', because there may be '\0' in the body content
		curr_buffer_pos = buffer + remaining_length;// the buffer position for new bytes should be remaining_length


//		cout << "buffer left char is " << string(buffer, buffer + remaining_length) << endl;

		if (should_quit) {
			break;
		}

		// clear to original for next loop
		tmp_read_length = 0;
	}


	// handle body
	if (content_length > 0) {
//		cout << "Read body, content_length: " << content_length << endl;
		// first read all bytes in the buffer, cannot only read to '\0', should read by length
		char* body_buffer = new char[content_length];
		memset(body_buffer, 0, content_length);
		for (int index = 0; index < remaining_length; index++) {
			body_buffer[index] = buffer[index];
		}
		int pos = remaining_length;

//		printf("After copy all buffer to body_buffer, pos is %d\n", pos);
		int rcvd = pos;// the pos is also the length of received bytes
		while (rcvd < content_length) {
			int n = read(comm_fd, &body_buffer[rcvd], content_length - rcvd);
			if (n < 0) {
				break;
			}
			rcvd += n;
		}
//		cout << "client_msg length: " << client_msg.length() << endl;
//		cout << "client_msg: " << client_msg << endl;
		client_msg += string(body_buffer, body_buffer + content_length);
//		cout << "After add body_buffer, client_msg length: " << client_msg.length() << endl;
//		cout << "client_msg: " << client_msg << endl;

		printf("At the end of read_helper, length of client_msg is %ld\n", client_msg.length());
		delete body_buffer;
	}
}



// OK for large files
//void read_helper(int comm_fd, string& client_msg) {
//	int total_read_length = 0;// count total length of read bytes
//	int store_to_client_msg_length = 0;// count total length of bytes that are stored to client_msg
//	int remaining_length = 0;
//
//	int tmp_size = 1200;
//	char buffer[tmp_size];
//	memset(buffer, 0, tmp_size);
//	char* curr_buffer_pos = buffer;
//	bool should_quit = false;
//	int content_length = 0;
//
//
//
//
//
//
//
//	while (true) {
//		int tmp_read_length = read(comm_fd, curr_buffer_pos, tmp_size - strlen(buffer));// the length of bytes read
//		printf("tmp_read_length: %d\n", tmp_read_length);
//		total_read_length += tmp_read_length;
//		if (tmp_read_length <= 0) {
//			cout << "read len <= 0, break" << endl;
//			string temp(buffer, buffer + tmp_read_length);
//			client_msg = client_msg + temp;
//			cout << "client_msg: " << client_msg << endl;
//			break;
//		}
//		char* text_end = NULL;
//		while ((text_end = strstr(buffer, "\r\n")) != NULL) {
//			// add '\r' and '\n' back
//			text_end += 2;
//			char raw_msg_rcvd [tmp_size];
//			strncpy(raw_msg_rcvd, buffer, text_end - buffer);
//			raw_msg_rcvd[text_end - buffer] = '\0';
//
//			// a line of raw message
//			string one_row(raw_msg_rcvd);
//
//			if (one_row == "\r\n") {
//				// body
////				cout << "there is a rn in an independent line, should quit to read body" << endl;
//				client_msg += one_row;
//				should_quit = true;
//			} else {
//				string target = "Content-Length: ";
//				if (one_row.find(target) != string::npos) {
//					string content_length_str = one_row.substr(target.size());
//					// remove \n
//					content_length_str.erase(content_length_str.size() - 1);
//					// remove \r
//					content_length_str.erase(content_length_str.size() - 1);
//					content_length = atoi(content_length_str.c_str());
//				}
//				client_msg += one_row;
//			}
//
//			// bytes of the length of (text_end - buffer) is added to client_msg
//			int tmp_store_to_client_msg_length = text_end - buffer;
//			store_to_client_msg_length += tmp_store_to_client_msg_length;
//
//
//			//this read_helper_clean_buffer may ignore the '\0' in content
////			read_helper_clean_buffer(buffer, text_end);
//			// ge remaining length, shift all remaining content to the leftmost
//			remaining_length = total_read_length - store_to_client_msg_length;// the remaining content in buffer
//			int buffer_pos = 0;
//			for (buffer_pos = 0; buffer_pos < remaining_length; buffer_pos++) {
//				buffer[buffer_pos] = *text_end;
//				*text_end = 0;
//				text_end++;
//			}
//			// clean the old-remaining content, the length should be the same as tmp_store_to_client_msg_length
//			for (int k = 0; k < tmp_store_to_client_msg_length; k++) {
//				buffer[buffer_pos + k] = 0;
//			}
//
//
//			if (should_quit) {
//				break;
//			}
//		}
//
//
////		// keep the unused chars in the buffer, and continue to read more in the next iteration
////		curr_buffer_pos = buffer;
////		while (*curr_buffer_pos != '\0') {
////			curr_buffer_pos++;
////		}
//		// maybe we cannot just find the '\0', because there may be '\0' in the body content
//		curr_buffer_pos = buffer + remaining_length;// the buffer position for new bytes should be remaining_length
//
//
////		cout << "buffer left char is " << string(buffer, buffer + remaining_length) << endl;
//
//		if (should_quit) {
//			break;
//		}
//
//	}
//
//
//	// handle body
//	if (content_length > 0) {
////		cout << "Read body, content_length: " << content_length << endl;
//		// first read all bytes in the buffer, cannot only read to '\0', should read by length
//		char* body_buffer = new char[content_length];
//		memset(body_buffer, 0, content_length);
//		for (int index = 0; index < remaining_length; index++) {
//			body_buffer[index] = buffer[index];
//		}
//		int pos = remaining_length;
//
////		printf("After copy all buffer to body_buffer, pos is %d\n", pos);
//		int rcvd = pos;// the pos is also the length of received bytes
//		while (rcvd < content_length) {
//			int n = read(comm_fd, &body_buffer[rcvd], content_length - rcvd);
//			if (n < 0) {
//				break;
//			}
//			rcvd += n;
//		}
////		cout << "client_msg length: " << client_msg.length() << endl;
////		cout << "client_msg: " << client_msg << endl;
//		client_msg += string(body_buffer, body_buffer + content_length);
////		cout << "After add body_buffer, client_msg length: " << client_msg.length() << endl;
////		cout << "client_msg: " << client_msg << endl;
//
//		printf("At the end of read_helper, length of client_msg is %ld\n", client_msg.length());
//		delete body_buffer;
//	}
//}



// test read_helper for large binary file
//void read_helper(int comm_fd, string& client_msg) {
//	// first read only 400 bytes to get content-length
//	// then read \r\n in an independent line, start to read content-length bytes to body
//
//	// read first 400 bytes to get content-length, 400 is short enough to avoid read content in buffer
//	// otherwise, it is hard to tell out whether a '\0' is in the content
//	int tmp_size = 400;
//	char buffer[tmp_size];
//	memset(buffer, 0, tmp_size);
//	int read_bytes = read(comm_fd, buffer, 400);// read first 400 bytes
//	printf("read_bytes: %d\n", read_bytes);
//	if (read_bytes < 0) {
//		cout << "read len <= 0, return" << endl;
//		return;
//	}
//	string temp(buffer, buffer + read_bytes);
//	bool isPost = false;
//	if (temp.substr(0, 4) == "POST") {
//		isPost = true;
//	}
//	if (!isPost) {
//		// this is a GET
//		printf("This is GET request\n");
//		while (true) {
//			read_bytes = read(comm_fd, buffer, 400);
//			if (read_bytes < 0) {
//				cout << "read len <= 0, return" << endl;
//				return;
//			}
//			temp(buffer, buffer + read_bytes);
//			if (read_bytes < 400) {
//				client_msg = client_msg + temp;
//				printf("Read all, break\n");
//				break;
//			}
//			client_msg = client_msg + temp;
//		}
//	}
//
//	// This is POST
//	printf("If reach here, this is a POST request\n");
//	// find content-length
//	int content_length = 0;
//	string target = "Content-Length: ";
//	if (temp.find(target) != string::npos) {
//		string content_length_str = one_row.substr(target.size());
//		// remove \n
//		content_length_str.erase(content_length_str.size() - 1);
//		// remove \r
//		content_length_str.erase(content_length_str.size() - 1);
//		content_length = atoi(content_length_str.c_str());
//		cout << "Content-length is " << content_length << endl;
//	} else {
//		cout << "No content-length!" << endl;
//	}
//
//	// read content-length + 500 more to include all message
//
//
//
//
//	char* curr_buffer_pos = buffer;
//	bool should_quit = false;
//
//	while (true) {
//		if ((read(comm_fd, curr_buffer_pos, tmp_size - strlen(buffer))) <= 0) {
//			cout << "read len <= 0, break" << endl;
//			break;
//		}
//		char* text_end = NULL;
//		while ((text_end = strstr(buffer, "\r\n")) != NULL) {
//			// add '\r' and '\n' back
//			text_end += 2;
//			char raw_msg_rcvd [tmp_size];
//			strncpy(raw_msg_rcvd, buffer, text_end - buffer);
//			raw_msg_rcvd[text_end - buffer] = '\0';
//			// a line of raw message
//			string one_row(raw_msg_rcvd);
//
//			if (one_row == "\r\n") {
//				// body
//				cout << "there is a rn in an independent line, should quit to read body" << endl;
//				client_msg += one_row;
//				should_quit = true;
//			} else {
//				string target = "Content-Length: ";
//				if (one_row.find(target) != string::npos) {
//					string content_length_str = one_row.substr(target.size());
//					// remove \n
//					content_length_str.erase(content_length_str.size() - 1);
//					// remove \r
//					content_length_str.erase(content_length_str.size() - 1);
//					content_length = atoi(content_length_str.c_str());
//				}
//				client_msg += one_row;
//			}
//			read_helper_clean_buffer(buffer, text_end);
//
//			if (should_quit) {
//				break;
//			}
//		}
//
//		// keep the unused chars in the buffer, and continue to read more in the next iteration
//		curr_buffer_pos = buffer;
//		while (*curr_buffer_pos != '\0') {
//			curr_buffer_pos++;
//		}
//
//		cout << "buffer left char is " << string(buffer) << endl;
//
//		if (should_quit) {
//			break;
//		}
//
//	}
//
//	// when there is a body
//	if (content_length > 0) {
//		cout << "Read body, content_length: " << content_length << endl;
//		// first read all bytes in the buffer, cannot only read to '\0', should
//		char* body_buffer = new char[content_length];
//		memset(body_buffer, 0, content_length);
//		int pos = 0;
////		while (buffer[pos] != '\0') {
////			body_buffer[pos] = buffer[pos];
////			pos++;
////		}
//		while (pos < content_length - 1) {
//			body_buffer[pos] = buffer[pos];
//			pos++;
//		}
//		printf("After copy all buffer to body_buffer, pos is %d\n", pos);
//		cout << "buffer left char is " << string(body_buffer) << endl;
//		int rcvd = pos;
//		while (rcvd < content_length) {
//			printf("still need to read, I think it is impossible\n");
//			int n = read(comm_fd, &body_buffer[rcvd], content_length - rcvd);
//			if (n < 0) {
//				break;
//			}
//			rcvd += n;
//		}
//		client_msg += string(body_buffer, body_buffer + content_length);
//		printf("At the end of read_helper, length of client_msg is %ld\n", client_msg.length());
//		delete body_buffer;
//	}
//
//}


void read_helper_clean_buffer(char* start, char* end) {
	char* i = start;
	char* j = end;
	while (*j != '\0') {
		*i = *j;
		i++;
		j++;
	}
	while (i < j) {
		*i = '\0';
		i++;
	}
}

/**
 * Print debug message if debug mode is on.
 * Use vfprintf function here to print in format.
 * http://www.cplusplus.com/reference/cstdio/vfprintf/
 */
void vprint(bool debug_mode, const char* str, ...) {
	if (!debug_mode) return; // if -v is off, return
	va_list args;
	va_start(args, str);
	vfprintf(stderr, str, args);
	va_end(args);
}

void vprint(bool debug_mode, string str, ...) {
	const char* charstr = str.c_str();
	if (!debug_mode) return; // if -v is off, return
	va_list args;
	va_start(args, charstr);
	vfprintf(stderr, charstr, args);
	va_end(args);
}


/**
 * Given a string with white spaces, split the string by spaces
 * and return a vector of each tokens.
 */
vector<string> string_to_tokens(string& str, char c) {
	vector<string> ret;
	stringstream check(str);
	string temp;
	while (getline(check, temp, c)) {
		ret.push_back(temp);
	}

	return ret;

}

/**
 * Given an HTML file, generate the corresponding HTTP response.
 */
string response_generator(char* dir) {

	ifstream stream(dir);
	string plain_html;
	if (stream) {
		ostringstream ss;
		ss << stream.rdbuf();
		plain_html = ss.str();
	}

	string ret = "HTTP/1.1 200 OK\r\n";
	ret += "Content-type: text/html\r\n";
	ret += "Content-length: ";
	ret += to_string(plain_html.length());
	ret += "\r\n";
	ret += "\r\n";
	ret += plain_html;
	return ret;
}

/**
 * Overload version 2: replace the old target chunk towards new in HTML
 */
string response_generator(char* dir, string old, string curr) {

	ifstream stream(dir);
	string plain_html;
	if (stream) {
		ostringstream ss;
		ss << stream.rdbuf();
		plain_html = ss.str();
	}

	while (plain_html.find(old) != string::npos) {
		plain_html.replace(plain_html.find(old), old.length(), curr);
	}

	cout << plain_html;

	string ret = "HTTP/1.1 200 OK\r\n";
	ret += "Content-type: text/html\r\n";
	ret += "Content-length: ";
	ret += to_string(plain_html.length());
	ret += "\r\n";
	ret += "\r\n";
	ret += plain_html;
	return ret;
}

/**
 * Overload version 3: replace the old target chunk towards new in HTML
 */
string response_generator(char* dir, unordered_map<string, string> old_and_new) {

	ifstream stream(dir);
	string plain_html;
	if (stream) {
		ostringstream ss;
		ss << stream.rdbuf();
		plain_html = ss.str();
	}

	for (auto itr = old_and_new.begin(); itr != old_and_new.end(); itr++) {
		while (plain_html.find(itr->first) != string::npos) {
			plain_html.replace(plain_html.find(itr->first), itr->first.length(), itr->second);
		}
	}

	cout << plain_html;

	string ret = "HTTP/1.1 200 OK\r\n";
	ret += "Content-type: text/html\r\n";
	ret += "Content-length: ";
	ret += to_string(plain_html.length());
	ret += "\r\n";
	ret += "\r\n";
	ret += plain_html;
	return ret;
}

/**
 * Overload version 4: replace the two old target chunks towards new in HTML
 */
string response_generator(char* dir, string old, string curr, string old2, string curr2) {

	ifstream stream(dir);
	string plain_html;
	if (stream) {
		ostringstream ss;
		ss << stream.rdbuf();
		plain_html = ss.str();
	}

	while (plain_html.find(old) != string::npos) {
		plain_html.replace(plain_html.find(old), old.length(), curr);
	}

	while (plain_html.find(old2) != string::npos) {
		plain_html.replace(plain_html.find(old2), old2.length(), curr2);
	}

	cout << plain_html;

	string ret = "HTTP/1.1 200 OK\r\n";
	ret += "Content-type: text/html\r\n";
	ret += "Content-length: ";
	ret += to_string(plain_html.length());
	ret += "\r\n";
	ret += "\r\n";
	ret += plain_html;
	return ret;
}

/**
 * Overload version 5: replace the four old target chunks towards new in HTML
 */
string response_generator(char* dir, string old, string curr, string old2, string curr2, string old3, string curr3) {

	ifstream stream(dir);
	string plain_html;
	if (stream) {
		ostringstream ss;
		ss << stream.rdbuf();
		plain_html = ss.str();
	}

	while (plain_html.find(old) != string::npos) {
		plain_html.replace(plain_html.find(old), old.length(), curr);
	}

	while (plain_html.find(old2) != string::npos) {
		plain_html.replace(plain_html.find(old2), old2.length(), curr2);
	}

	while (plain_html.find(old3) != string::npos) {
		plain_html.replace(plain_html.find(old3), old3.length(), curr3);
	}

	cout << plain_html;

	string ret = "HTTP/1.1 200 OK\r\n";
	ret += "Content-type: text/html\r\n";
	ret += "Content-length: ";
	ret += to_string(plain_html.length());
	ret += "\r\n";
	ret += "\r\n";
	ret += plain_html;
	return ret;
}

/**
 * Overload version 6: replace the four old target chunks towards new in HTML
 */
string response_generator(char* dir, string old, string curr, string old2, string curr2, string old3, string curr3, string old4, string curr4) {

	ifstream stream(dir);
	string plain_html;
	if (stream) {
		ostringstream ss;
		ss << stream.rdbuf();
		plain_html = ss.str();
	}

	while (plain_html.find(old) != string::npos) {
		plain_html.replace(plain_html.find(old), old.length(), curr);
	}

	while (plain_html.find(old2) != string::npos) {
		plain_html.replace(plain_html.find(old2), old2.length(), curr2);
	}

	while (plain_html.find(old3) != string::npos) {
		plain_html.replace(plain_html.find(old3), old3.length(), curr3);
	}

	while (plain_html.find(old4) != string::npos) {
		plain_html.replace(plain_html.find(old4), old4.length(), curr4);
	}

	cout << plain_html;

	string ret = "HTTP/1.1 200 OK\r\n";
	ret += "Content-type: text/html\r\n";
	ret += "Content-length: ";
	ret += to_string(plain_html.length());
	ret += "\r\n";
	ret += "\r\n";
	ret += plain_html;
	return ret;
}

/**
 * With cookie info -- 1 cookie
 * [format] Set-Cookie: key=value (limiting one)
 */
string response_generator_cookie(char* dir, string key, string value) {

	ifstream stream(dir);
	string plain_html;
	if (stream) {
		ostringstream ss;
		ss << stream.rdbuf();
		plain_html = ss.str();
	}

	string ret = "HTTP/1.1 200 OK\r\n";
	ret += "Content-type: text/html\r\n";
	ret += "Content-length: ";
	ret += to_string(plain_html.length());
	ret += "\r\n";
	// [format] Set-Cookie: key=value (limiting one)
	ret += "Set-Cookie: " + key + "=" + value + "\r\n";
	ret += "\r\n";
	ret += plain_html;
	return ret;
}

/**
 * With cookie info -- 2 cookies
 * [format] Set-Cookie: key=value (limiting one)
 */
string response_generator_cookie(char* dir, string key1, string value1, string key2, string value2) {

	ifstream stream(dir);
	string plain_html;
	if (stream) {
		ostringstream ss;
		ss << stream.rdbuf();
		plain_html = ss.str();
	}

	// if we want the home page, remeber to replace "$name" with the real user name: value2
	string placeholder = "$name";
	if (strcmp(dir, "pages/home.html") == 0) {
		plain_html.replace(plain_html.find(placeholder), placeholder.size(), value2.substr(0, value2.find("%40")));
	}

	string ret = "HTTP/1.1 200 OK\r\n";
	ret += "Content-type: text/html\r\n";
	ret += "Content-length: ";
	ret += to_string(plain_html.length());
	ret += "\r\n";
	// [format] Set-Cookie: key=value (limiting one)
	ret += "Set-Cookie: " + key1 + "=" + value1 + "\r\n";
	ret += "Set-Cookie: " + key2 + "=" + value2 + "\r\n";
	ret += "\r\n";
	ret += plain_html;
	return ret;
}

/**
 * With cookie info
 * [format] Set-Cookie: key=value
 */
string response_generator_cookie(char* dir, map<string, string> kv_pairs) {

	ifstream stream(dir);
	string plain_html;
	if (stream) {
		ostringstream ss;
		ss << stream.rdbuf();
		plain_html = ss.str();
	}

	// if we want the home page, remeber to replace "$name" with the real user name: value2
	string placeholder = "$name";
	if (strcmp(dir, "pages/home.html") == 0) {
		plain_html.replace(plain_html.find(placeholder), placeholder.size(), kv_pairs["user"].substr(0, kv_pairs["user"].find("%40")));
	}

	string ret = "HTTP/1.1 200 OK\r\n";
	ret += "Content-type: text/html\r\n";
	ret += "Content-length: ";
	ret += to_string(plain_html.length());
	ret += "\r\n";
	// [format] Set-Cookie: key=value
	auto itr = kv_pairs.begin();
	while (itr != kv_pairs.end()) {
		ret += "Set-Cookie: " + itr->first + "=" + itr->second + "\r\n";
		itr++;
	}
	ret += "\r\n";
	ret += plain_html;
	return ret;
}

string response_generator302(string new_location) {
	string ret = "HTTP/1.1 302 OK\r\n";
	ret += "Content-type: text/html\r\n";
	ret += "Content-length: 0\r\n";
	ret += "Location: " + new_location + "\r\n";
	return ret;
}

/**
 * Parse the parameters in the request body: e.g. "username=a&password=123"
 */
map<string, string> get_params_from_request_body(string request_body) {
	map<string, string> request_body_params;
	char* tmp_body = new char[request_body.size() + 1];
	strcpy(tmp_body, request_body.c_str());
	char* token = strtok(tmp_body, "&");
	while (token != NULL) {
		string pair(token);
		int pos = pair.find("=");
		string key = pair.substr(0, pos);
		string val = pair.substr(pos + 1);
		request_body_params[key] = val;
		token = strtok(NULL, "&");
	}
	delete[] tmp_body;
	return request_body_params;
}


bool do_write(int fd, char* buffer, int len) {
	int sent = 0;
	while (sent < len) {
		int n = write(fd, &buffer[sent], len - sent);
		if (n < 0) {
			return false;
		}
		sent += n;
	}
	return true;
}

/**
 * Generate time header
 */
string generate_time() {
	// Ref: https://stackoverflow.com/questions/24686846/
	using namespace std::chrono;
	auto now_time = system_clock::now();
	auto us_count = duration_cast<microseconds>(now_time.time_since_epoch()) % 1000000;
	auto timer = system_clock::to_time_t(now_time);
	std::tm broken_time = *std::localtime(&timer);
	std::ostringstream oss;
	oss << std::put_time(&broken_time, "%H:%M:%S"); // Format: HH:MM:SS
	oss << '.' << std::setfill('0') << std::setw(6) << us_count.count();
	return oss.str();
}

string get_timestamp() {
	time_t now = time(NULL);
	char* time_str = new char[20];
	sprintf(time_str, "%ld", now);
	string ret(time_str);
	delete[] time_str;
	return ret;
}

string generate_backend_rows(vector<map<string, string>>& groups) {
	// Data Structure
	// vector<unordered_map<string, string>>
	// groups[0] <=> group #0
	// groups[1] <=> group #1
	// groups[1]["8000"] <=> conn_count -> "-1": not connected; >= 0: connected


	string content;
	for (int i = 0; i < groups.size(); i++) {
		if (i != 0) content += "<tr> <td></td> <td></td> <td></td> <td></td> <td></td> <td></td> </tr>";
		// Current Group is a map<string,string>, need iteration
		for ( auto it = groups[i].begin(); it != groups[i].end(); ++it ) {

			int curr_port = stoi(it->first);
			int curr_conn = stoi(it->second);
			bool curr_duty = !(curr_conn == -1);

		  content += "<tr>";
		  {

			// [Group No]
			content += "<td> Group #" + to_string(i + 1) + "</td>";

		    // [Port No.]
		    content += "<td>" + to_string(curr_port) + "</td>";

		    // [On-duty]
		    content += "<td>";
		    content += curr_duty ? "<b style='color:darkgreen'>ACTIVE</b>" : "<b style='color:darkred'>INACTIVE</b>";
		    content += "</td>";

		    // [Active Connections]
		    content += "<td>" + (curr_duty ? to_string(curr_conn) : "/") + "</td>";

		    // [Enable button]
		    content += "<td>";
		    if (curr_duty) {
		      // when on-duty, enable not availale
		      content += "<button class='btn btn-default disabled'>Enable</button>";
		    } else {
		      // when off duty, enable yes
		      content += "<form action='/enable-bserver' method='post'>";
		      content += "<button name='port' value='" + to_string(curr_port) + "' class='btn btn-warning'>Enable</button>";
		      content += "</form>";
		    }
		    content += "</td>";

		    // [Disable button]
		    content += "<td>";
		      if (!curr_duty) {
		        // when off-duty, disable not availale
		        content += "<button class='btn btn-default disabled'>Disable</button>";
		      } else {
		        // when on duty, disable yes
		        content += "<form action='/disable-bserver' method='post'>";
		        content += "<button name='port' value='" + to_string(curr_port) + "' class='btn btn-danger'>Disable</button>";
		        content += "</form>";
		      }
		      content += "</td>";
		  }
		  content += "</tr>";
		}
	}
	return content;
}

string generate_bigtable_rows(vector<vector<string>>& groups) {
	// Data Structure
	// vector<vector<string>>
	// groups[0] <=> group #0
	// groups[1] <=> group #1
	// groups[1][0-end] <=> string content
	// e.g. 8003`14:00`leo`pswd`123
	// [port] => 8003
	// [username] => leo
	// [column] => pswd
	// [value] => 123


	string content;
	for (int i = 0; i < groups.size(); i++) {
		content += "<h4><b style='color:darkred'>Group #" + to_string(i + 1) + "</b></h4>";
		content += "<table class='table table-hover'>";
		content += "<thead><tr><th>Port</th><th>Username</th><th>Column</th><th>Value</th></tr></thead>";
		// Current Group is a map<string,string>, need iteration
		string prev_port_no = "-1";
		for (int j = 0; j < groups[i].size(); j++) {
			string long_str = groups[i][j];
			vector<string> tokens = string_to_tokens(long_str, '`');

			string port_no = tokens[0];
			string username = tokens[2];//2
			string column = tokens[3];//3
			string value = tokens[4];//4

			if (prev_port_no != port_no && prev_port_no != "-1") {
				content += "<tr> <td></td> <td></td> <td></td> <td></td> </tr>";
				content += "<tr> <td></td> <td></td> <td></td> <td></td> </tr>";
			}
			prev_port_no = port_no;


		  content += "<tr>";
		  {
			// [port_no]
			content += "<td>" + port_no + "</td>";

		    // [username]
		    content += "<td>" + username + "</td>";

		    // [column]
		    content += "<td>" + column + "</td>";

		    // [value]
		    content += "<td>" + value + "</td>";
		  }
		  content += "</tr>";
		}
		content += "</table>";
	}
	return content;
}


