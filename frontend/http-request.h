/*
 * http-request.h
 *
 *  Created on: Nov 17, 2020
 *      Author: Baiao Hou
 */

#ifndef HTTP_REQUEST_H_
#define HTTP_REQUEST_H_

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "helpers.h"

using namespace std;

class HTTPRequest {
public: // attributes
	string method;
	string path;
	string protocol_ver;
	map<string, string> cookies;
	string body;
	int port_no;
	bool empty = false;
	string other;

public: // constructor
	//HTTPRequest(string str) { // (int comm_fd) {
	HTTPRequest(int comm_fd) {
		string html;

		//html = str;//read_helper(comm_fd, line);
//		cout << "In HTTPRequest class, go to read_helper: " << endl;
		read_helper(comm_fd, html);

		// print html length
//		printf("html len: %ld\n", html.length());
//		cout << "html:" << endl;
//		cout << html << endl;

		string line;
		istringstream f(html);

		// Read first line of request
		getline(f, line);

		vector<string> args = string_to_tokens(line, ' ');
		if (args.size() != 3) {
			//cerr << "Invalid HTTP request head!\r\n";
			this->empty = true;
			if (args.size() != 0) this->other = args[0];
			return;
		}

		vprint(1, "[%d] New connection\r\n", comm_fd);

		cerr<<"["<<html<<"]"<<endl;

		method = args[0];
		path = args[1];
		protocol_ver = args[2];
		cout << "Method: " << method << endl;
		cout << "Path: " << path << endl;
		cout << "Protocol Version: " << protocol_ver << endl;

		//bool body_sign = false;
		while(getline(f, line)) {
			// remove ending \r character
			line.erase(line.size() - 1);
//			if (body_sign) {
//				this->body = line;
//				cout << "Body is {" << this->body << "}" << endl;
//				break;
//			}
			//cout << "{" << line.length() << "}" << endl;
			if (line.rfind("Cookie: ", 0) == 0) {
				// This line starts with this prefix
				string trim = line.substr(8);

				vector<string> cookie_pairs = string_to_tokens(trim, ';');
				for (int i = 0; i < cookie_pairs.size(); i++) {
					vector<string> key_and_value = string_to_tokens(cookie_pairs[i], '=');
					if (key_and_value[0].at(0) == ' ') {
						this->cookies[key_and_value[0].substr(1)] = key_and_value[1];
					} else {
						// not beginning with space
						this->cookies[key_and_value[0]] = key_and_value[1];
					}
				}

				for (auto const& pair: this->cookies) {
					std::cout << "Cookie {" << pair.first << ":" << pair.second << "}\n";
				}
			} else if (line.rfind("Host: ", 0) == 0) {
				// This line starts with this prefix
				string trim = line.substr(6);
				vector<string> ip_port = string_to_tokens(trim, ':');
				this->port_no = stoi(ip_port[1]);
				cout << "Port Number: " << this->port_no << endl;

			}

			// meet empty line, finish parsing header
			if (line.length() == 0) {
				break;
			}
		}


		// test html string
//		printf("-----------------------------------go to test html\n");
//		int content_length_pos = html.find("Content-Length: ");
//		if (content_length_pos != string::npos) {
//			int content_length_start = content_length_pos + 16;
//			printf("content_length_start: %d\n", content_length_start);
//			int content_length_end = html.find("\r\n", content_length_start);
//			printf("content_length_end: %d\n", content_length_end);
//			int content_length = stoi(html.substr(content_length_start, content_length_end - content_length_pos));
//			printf("content_length: %d\n", content_length);
//			// find\r\n\r\n
//			int rnrn_pos = html.find("\r\n\r\n");
//			printf("rnrn_pos: %d\n", rnrn_pos);
//			int content_start = rnrn_pos + 4;
//			int content_end = content_start + content_length;
//			printf("content_start: %d\n", content_start);
//			printf("content_end: %d\n", content_end);
//			// test boundary end
//			for (int i = 100; i >= 1; i--) {
//				cout << i << endl;
//				printf("ch_str=%s\n", html.substr(content_end-i, 1).c_str());
//			}
//		}
//
//
//		int rnganggang_pos = 0;
//		rnganggang_pos = html.find("\r\n--", rnganggang_pos);
//		while(rnganggang_pos != string::npos) {
//			printf("rnganggang_pos: %d\n", rnganggang_pos);
//			rnganggang_pos = html.find("\r\n--", rnganggang_pos + 1);
//		}



		// parse the body
		stringbuf stringbuf;
		while (!f.eof()) {
			f >> &stringbuf;
		}

		this->body = stringbuf.str();
//		cout << "Body is {" << this->body << "}" << endl;
		cout << "Body length is {" << this->body.length() << "}" << endl;


		if (this->method == "POST") {
			// do sth
		}

	}
};



#endif /* HTTP_REQUEST_H_ */
