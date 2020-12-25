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
 */
void read_helper(int comm_fd, string& client_msg) {
	// "If the server limits the length of a command, the
	//  limit should be at least 1,000 characters"
	// So here set buffer size as 1200
	char buffer[1200];
	while (true) {
		int read_bytes = read(comm_fd, buffer, 1200);
		string temp(buffer, buffer + read_bytes);
		if (read_bytes < 1200) {
			client_msg = client_msg + temp;
			break;
		}
		client_msg = client_msg + temp;
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


