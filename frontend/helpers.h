/*
 * helpers.h
 *
 *  Created on: Nov 17, 2020
 *      Author: baiaohou
 */

#ifndef HELPERS_H_
#define HELPERS_H_

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <bits/stdc++.h>


using namespace std;

/**
 * A helper function to create a buffer to read message from client.
 */
void read_helper(int comm_fd, string& client_msg);
void read_helper_clean_buffer(char* start, char* end);

/**
 * Print debug message if debug mode is on.
 * Use vfprintf function here to print in format.
 * http://www.cplusplus.com/reference/cstdio/vfprintf/
 */
void vprint(bool debug_mode, const char* string, ...);
void vprint(bool debug_mode, string string, ...);

/**
 * Given a string with white spaces, split the string by spaces
 * and return a vector of each tokens.
 */
vector<string> string_to_tokens(string& string, char c);

/**
 * Given an HTML file, generate the corresponding HTTP response.
 */
string response_generator(char* dir);
string response_generator(char* dir, string old, string curr);
string response_generator(char* dir, unordered_map<string, string> old_and_new);
string response_generator(char* dir, string old, string curr, string old2, string curr2);
string response_generator(char* dir, string old, string curr, string old2, string curr2, string old3, string curr3);
string response_generator(char* dir, string old, string curr, string old2, string curr2, string old3, string curr3, string old4, string curr4);
string response_generator_cookie(char* dir, string key, string value);
string response_generator_cookie(char* dir, string key1, string value1, string key2, string value2);
string response_generator_cookie(char* dir, map<string, string> kv_pairs);

/**
 * Generate a 302 response
 */
string response_generator302(string new_location);

/**
 * Parse the parameters in the request body: e.g. "username=a&password=123"
 */
map<string, string> get_params_from_request_body(string request_body);

/**
 * write a fixed length of message to a TCP connection
 * return true upon success, false upon failure
 */
bool do_write(int fd, char* buffer, int len);

/**
 * Generate time header
 */
string generate_time();

/**
 * Generate a timestamp as the session id
 */
string get_timestamp();

/**
 * Generate an inner HTML to display back end nodes in admin console
 */
string generate_backend_rows(vector<map<string, string>>& groups);

/**
 * Generate an inner HTML to display back BigTable data in admin console
 */
string generate_bigtable_rows(vector<vector<string>>& groups);

#endif /* HELPERS_H_ */
