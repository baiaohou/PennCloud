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
#include "fileservice.h"

////#include <iostream>
//#include <cstdio>
//#include <cstdlib>
//#include <cstring>
//#include <algorithm>

#include "base64.h"


using namespace std;




/* when user clicks drive-home, call try_listfile */
string try_listfile(map<string, string> cookies, string directory) {
	cout << "------------------------------------------------------\n\n\n";
	cout << "call try_listfile," << endl;
	printf("directory: %s\n", directory.c_str());

	// get username from cookies
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
	// get node_ip and node_port from cookies
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	strcpy(storage_node_ip, contact_ip.c_str());
	storage_node_port = stoi(contact_port);

	// get filename list as filenames
	// TODO get filenames of current folder, add current folder in the argument
	printf("go to get_filenames of %s\n", user_name.c_str());
	vector<string> filenames;// filenames in a vector
	char origin_filenames [1000];// filenames of original format, e.g. "a.txt,b.txt,", "" for none filenames
	memset(origin_filenames, 0, 1000);
	get_filenames(string(user_name), string(storage_node_ip), storage_node_port, filenames, origin_filenames);
	for (int i = 0; i < filenames.size(); i++) {
		printf("return filenames: %d, %s\n", i, filenames[i].c_str());
	}


	// create replace
	string content;
	printf("directory: %s, len: %ld\n", directory.c_str(), directory.length());
	for (int i = 0; i < filenames.size(); i++) {
		// check whether the prefix is the same as directory, and only show the closest layer
		int directory_len = directory.length();
		printf("filenames[%d]: %s\n", i, filenames[i].c_str());
		if (!(filenames[i].substr(0, directory_len) == directory)) {
			printf("this filename prefix does not match current directory\n");
			continue;
		}
		int last_slash = filenames[i].find_last_of("/");
		if (last_slash >= directory_len) {
			printf("this filename is in deeper layer\n");
			continue;
		}

		content += "<tr>";

			// check folder or file
			if (filenames[i].find(".") != filenames[i].npos) {
				printf("there is a dot, it is a file\n");
				content += "<td>📃" + filenames[i].substr(last_slash + 1) + "</td>";
				content += "<td>";
				content += "<form action='/download' method='post'>";
				content += "<button name='fullFilename' value='" + filenames[i]
						+ "' class='btn btn-success'>Download</button>";
				content += "</form>";
				content += "</td>";
			} else {
				printf("there is not a dot, it is a folder\n");
				content += "<td>📁" + filenames[i].substr(last_slash + 1) + "</td>";
				content += "<td>";
				content += "<form action='/drive-folder' method='post'>";
				content += "<button name='fullFilename' value='" + filenames[i]
						+ "' class='btn btn-success'>Open</button>";
				content += "</form>";
				content += "</td>";
			}

			// [Rename]
			content += "<td>";
			content += "<form action='/rename' method='post'>";
			content += "<input name='newname' placeholder='filename.txt or foldername' />";
			content += "<button name='fullFilename' value='" + filenames[i]
					+ "' class='btn btn-info'>Rename</button>";
			content += "</form>";
			content += "</td>";

			// [Move]
			content += "<td>";
			content += "<form action='/move' method='post'>";
			content += "<input name='newdirectory' placeholder='root/folder/' />";
			content += "<button name='fullFilename' value='" + filenames[i]
					+ "' class='btn btn-warning'>Move to</button>";
			content += "</form>";
			content += "</td>";

			// [Delete]
			content += "<td>";
			content += "<form action='/delete' method='post'>";
			content += "<button name='fullFilename' value='" + filenames[i]
					+ "' class='btn btn-danger'>Delete</button>";
			content += "</form>";
			content += "</td>";

		content += "</tr>";
	}



	// create response
	string response = response_generator("pages/drive-home.html", "$content", content, "$directory", directory);
	return response;
}

string try_download(map<string, string> cookies, string request_body) {
	printf("\r\n----------------------------------------------------------------------------------------------------\r\n\r\n");
	printf("call try_download, request_body is: %s\n", request_body.c_str());

	// get filename, username from cookies and request_body

	// get filename from request_body
	map<string, string> params = get_params_from_request_body(request_body);
	string fullFilename = params["fullFilename"];
//	while (fullFilename.find("%2F") != string::npos) {
//		fullFilename.replace(fullFilename.find("%2F"), 3, "/");
//	}
	fullFilename = UrlDecode(fullFilename);
	printf("fullFilename: %s, len=%ld\n", fullFilename.c_str(), fullFilename.length());
	// get username from cookies
	string user_name = cookies["user"];
	string contact_ip = cookies["contact_ip"];
	string contact_port = cookies["contact_port"];
	cout << user_name << endl;
	cout << user_name.length() << endl;
	cout << contact_ip << endl;
	cout << contact_ip.length() << endl;
	cout << contact_port << endl;
	cout << contact_port.length() << endl;
	// get node_ip and node_port from cookies
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	strcpy(storage_node_ip, contact_ip.c_str());
	storage_node_port = stoi(contact_port);

	// get chunksize, get chunkcontent from each chunk, combine them, decode, sendback

	// get chunksize from user's node
	char get_response [BUFFER_SIZE];// to fetch filename_list
	memset(get_response, 0, BUFFER_SIZE);
	if (!do_GET(storage_node_ip, storage_node_port, user_name, fullFilename, get_response)) {
		printf("failed to get filename_list\n");
	}
	printf("get res: %s\n", get_response);
	string chunksize = string(get_response + 4);// skip "+OK"
	printf("chunksize: %s\n", chunksize.c_str());
	int chunksize_int = stoi(chunksize);
	printf("chunksize_int: %d\n", chunksize_int);
	// get chunkcontent from each chunk and combine them
	string content = "";
	for (int i = 0; i < chunksize_int; i++) {
		// get chunkcontent from each chunk
		printf("i=%d\n", i);
		string fullFilenameChunk = fullFilename + "|" + to_string(i);
		memset(get_response, 0, BUFFER_SIZE);
		if (!do_GET(storage_node_ip, storage_node_port, user_name, fullFilenameChunk, get_response)) {
			printf("failed to get filename_list\n");
		}
		printf("get res: %s\n", get_response);
		string chunkcontent = string(get_response + 4);
		printf("length of chunkcontent: %ld\n", chunkcontent.length());
//		printf("chunkcontent: %s\n", chunkcontent.c_str());
		content += chunkcontent;
	}
	// decode base64
//	content = UrlDecode(content);
	content = base64_decode(content);
//	cout << "final content:" << endl;
//	cout << content << endl;
	string ret = "HTTP/1.1 200 OK\r\n";
	ret += "Content-type: application/octet-stream\r\n";
	ret += "Content-Disposition: attachment; filename=\"" + fullFilename + "\"";
	ret += "Content-length: " + to_string(content.length());
	ret += "\r\n";
	ret += "\r\n";
	ret += content;
	return ret;

	// test encode and decode content directly
//	printf("testbinary length: %ld\n", testbinary.length());
//	testbinary = base64_encode(reinterpret_cast<const unsigned char*>(testbinary.c_str()), testbinary.length());
//	content = base64_decode(testbinary);
//	printf("testbinary to content length: %ld\n", content.length());
//	ret += content;
//	return ret;
}


/* when user clicks upload, call try_upload */
string try_upload(map<string, string> cookies, string request_body) {
	cout << "------------------------------------------------------\n\n\n";
//	printf("call try_upload, request_body: %s\n", request_body.c_str());
//	cout << "request_body printed by cout:" << endl;
//	cout << request_body << endl;
	printf("request_body num: %ld\n", request_body.length());

	// get filedirectory, filename, filecontent from request_body TODO: change dilimeter to boundary

	// get filedirectory from multiform request
	int rnrn_pos1 = request_body.find("\r\n\r\n");
	int rnganggang_pos1 = request_body.find("\r\n--", rnrn_pos1 + 4);
	string tmp_filedirectory = request_body.substr(rnrn_pos1 + 4, rnganggang_pos1 - (rnrn_pos1 + 4));
	printf("rnrn_pos1: %d\n", rnrn_pos1);
	printf("rnganggang_pos1: %d\n", rnganggang_pos1);
	printf("tmp_filedirectory: %s\n", tmp_filedirectory.c_str());
	// get filename from multiform request
	int tmp_pos = request_body.find("filename=\"", rnganggang_pos1);
	int filename_start = tmp_pos + 10;
	int filename_end = request_body.find("\"", filename_start);
	string tmp_filename = request_body.substr(filename_start, filename_end - filename_start);
	printf("tmp_pos: %d\n", tmp_pos);
	printf("filename_start: %d\n", filename_start);
	printf("filename_end: %d\n", filename_end);
	printf("tmp_filename: %s\n", tmp_filename.c_str());

	// get filecontent from multiform request
	int rnrn_pos2 = request_body.find("\r\n\r\n", filename_end);
	int rnganggang_pos2 = request_body.find("\r\n--", rnrn_pos2 + 4);
	string tmp_filecontent = request_body.substr(rnrn_pos2 + 4, rnganggang_pos2 - (rnrn_pos2 + 4));
	printf("rnrn_pos2: %d\n", rnrn_pos2);
	printf("rnganggang_pos2: %d\n", rnganggang_pos2);
//	cout << "tmp_filecontent: " << endl;
//	cout << tmp_filecontent << endl;
	// tmp_names are for historical problems
	string filedirectory = tmp_filedirectory;
	string filename = tmp_filename;
	string filecontent = tmp_filecontent;

	testbinary = tmp_filecontent;// test whether the upload string can be download and open, this is used to decode and encode continuously in download

	filecontent = base64_encode(reinterpret_cast<const unsigned char*>(filecontent.c_str()), filecontent.length());
	while (filedirectory.find("%2F") != string::npos) {
		printf("firedirectory has 2F, though I do not think so, this is a historical problem\n");
		filedirectory.replace(filedirectory.find("%2F"), 3, "/");
	}
	// check whether there is a file in the input
	if (filename.length() == 0) {
		printf("no file uploaded\n");
		return response_generator("pages/drive-action-failed.html");
	}
	// concat directory+filename
	string fullFilename = filedirectory + filename;
	printf("fullFilename: %s, len=%ld\n", fullFilename.c_str(), fullFilename.length());


	// get username from cookies
	string user_name = cookies["user"];
	string contact_ip = cookies["contact_ip"];
	string contact_port = cookies["contact_port"];
	cout << user_name << endl;
	cout << user_name.length() << endl;
	cout << contact_ip << endl;
	cout << contact_ip.length() << endl;
	cout << contact_port << endl;
	cout << contact_port.length() << endl;
	// get node_ip and node_port from cookies
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	strcpy(storage_node_ip, contact_ip.c_str());
	storage_node_port = stoi(contact_port);


	// change to large-file mode, each filename has a chunk_size
	// e.g. root/icon.jpg has chunksize=3, content of it is stored in column root/icon.jpg|0, root/icon.jpg|1, root/icon.jpg|2

	// put filename-chunksize pair to K-V store node

	// calculate chunksize for encoded content
	int chunksize_int = ceil(filecontent.length() *1.0 / CHUNK_SIZE);
	printf("chunksize_int: %d\n", chunksize_int);
	string chunksize = to_string(chunksize_int);
	printf("chunksize: %s\n", chunksize.c_str());
	printf("encoded filecontent.length: %ld\n", filecontent.length());
	// first get the original chunksize, and check whether there is already this filename
	char get_response [BUFFER_SIZE];// to fetch chunksize
	memset(get_response, 0, BUFFER_SIZE);
	do_GET(storage_node_ip, storage_node_port, user_name, fullFilename, get_response);
	printf("get_response: %s\n", get_response);
	bool hasFilename = (strstr(get_response, "+OK")!=NULL);
	string origin_chunksize;
	if (hasFilename) {
		printf("hasFilename\n");
		origin_chunksize = string(get_response + 4);// skip "+OK"
		printf("origin_chunksize: %s\n", origin_chunksize.c_str());
	} else {
		printf("not have filename\n");
	}
	// put/cput filename-chunksize pair to k-v store
	char cput_response [BUFFER_SIZE];
	memset(cput_response, 0, BUFFER_SIZE);
	char put_response [BUFFER_SIZE];
	memset(put_response, 0, BUFFER_SIZE);
	if (hasFilename) {
		printf("go to do_CPUT, user_name: %s\n", user_name.c_str());
		do_CPUT(storage_node_ip, storage_node_port, user_name, fullFilename, origin_chunksize, chunksize, cput_response);
		printf("cput_res is %s\n", put_response);
	} else {
		printf("go to do_PUT, user_name\n", user_name.c_str());
		do_PUT(storage_node_ip, storage_node_port, user_name, fullFilename, chunksize, put_response);
		printf("put_res is %s\n", put_response);
	}


	// delete old chunks, put new chunks

	// if the filename already exists, delete all its old chunks
	if (hasFilename) {
		for (int i = 0; i < stoi(origin_chunksize); i++) {
			string fullFilenameChunk = fullFilename + "|" + to_string(i);
			char delete_response [BUFFER_SIZE];// to fetch uidl_list
			memset(delete_response, 0, BUFFER_SIZE);
			tmp_do_DELETE(storage_node_ip, storage_node_port, user_name, fullFilenameChunk, delete_response);
			printf("try_upload: do_DELETE response: %s\n", delete_response);
			if (strstr(delete_response, "-ERR")) {
				printf("\n do DELETE failed\n");// TODO redirect to FAIL page
			}
		}
	}
	// put filename-filecontent pairs to K-V store node
	for (int i = 0; i < chunksize_int; i++) {
		string fullFilenameChunk = fullFilename + "|" + to_string(i);
		printf("i=%d, fullFilenameChunk: %s\n", i, fullFilenameChunk.c_str());
		string chunkcontent = filecontent.substr(i * CHUNK_SIZE, CHUNK_SIZE);
		// put filenamechunk-chunkcontent pair to k-v store
		char put_response [BUFFER_SIZE];
		memset(put_response, 0, BUFFER_SIZE);
		do_PUT(storage_node_ip, storage_node_port, user_name, fullFilenameChunk, chunkcontent, put_response);
		printf("put_res is %s\n", put_response);
	}


	// if already has this filename, do not need to append filename in list
	if (hasFilename) {
		printf("hasFilename, do not need to append filename in list, sendback response and return\n");
		return response_generator("pages/drive-action-ok.html");
	}
	// else need to append filename to filelist

	// get filename list as filenames
	printf("go to get_filenames of %s\n", user_name.c_str());
	vector<string> filenames;// filenames in a vector
	char origin_filenames [1000];// filenames of original format, e.g. "a.txt,b.txt,", "" for none filenames
	memset(origin_filenames, 0, 1000);
	get_filenames(string(user_name), string(storage_node_ip), storage_node_port, filenames, origin_filenames);
	for (int i = 0; i < filenames.size(); i++) {
		printf("return filenames: %d, %s\n", i, filenames[i].c_str());
	}
	// append current filename to the list, if filename is not in list
	printf("append current filename to the list vector\n");
	filenames.push_back(fullFilename);// append current fullFilename to local list
	printf("convert filename list vector to string\n");
	string filenames_str = "";// string format of filenames
	for (int i = 0; i < filenames.size(); i++) {
		filenames_str += filenames[i] + ",";// keep the comma at last, e.g."ABCDE,"
	}
	// update filename list in K-V store
	printf("go to put/cput back the filename list\n");
//	char put_response [BUFFER_SIZE];
	memset(put_response, 0, BUFFER_SIZE);
	if (strlen(origin_filenames) == 0) {
		// first time to put filenames, use PUT
		printf("origin_filenames is empty, go to do_put, user_name\n", user_name.c_str());
		do_PUT(storage_node_ip, storage_node_port, user_name, "filenames", filenames_str, put_response);
		printf("put_res is %s\n", put_response);
	} else {
		// use CPUT
		printf("origin_filenames is not empty, go to do_cput, user_name:%s, origin_filenames is %s\n", user_name.c_str(), origin_filenames);
		do_CPUT(storage_node_ip, storage_node_port, user_name, "filenames", string(origin_filenames), filenames_str, put_response);
		printf("cput_res is %s\n", put_response);
	}


	// sendback response
	printf("sendback response\n");
	return response_generator("pages/drive-action-ok.html");
}

/* get filenames from a user */
void get_filenames(string user_name, string storage_node_ip, int storage_node_port, vector<string> &result, char* origin_filenames) {
	printf("call get_filenames, user_name=%s, len=%ld\n", user_name.c_str(), user_name.length());

	// do_GET filenames from user's node
	char get_response [BUFFER_SIZE];// to fetch filename_list
	memset(get_response, 0, BUFFER_SIZE);
	char tmp_input [storage_node_ip.length() + 1];
	strcpy(tmp_input, storage_node_ip.c_str());
	printf("get_filenames: go to do_GET, tmp_input: %s, storage_node_port: %d\n", tmp_input, storage_node_port);
	if (!do_GET(tmp_input, storage_node_port, user_name, "filenames", get_response)) {
		printf("failed to get filename_list\n");
	}
	printf("get res: %s\n", get_response);

	// parse filenames of original format, create filenames vector
	vector<string> filenames;// to store filename list to local
	if (!strstr(get_response, "-ERR")) {
		printf("no -ERR\n");
		// the node has a filename_list, (if there is -ERR, there is no filename_list in the node)
		strcpy(origin_filenames, get_response + 4);// skip "+OK "
		printf("origin_filenames: %s\n", origin_filenames);
		char* token = strtok(get_response + 4, ",");// skip "+OK", cannot use origin_filenames, which will be polluted
		while (token != NULL) {
			printf("inside get_filenames, go to push token=%s, len=%ld\n", token, strlen(token));
			filenames.push_back(string(token));
			token = strtok(NULL, ",");
		}
	}
	// if filenames is not in node, res is -ERR, origin_filenames remain to be null
	printf("origin_filenames is %s, len=%ld\n", origin_filenames, strlen(origin_filenames));
	printf("0th char is %d\n", (int)origin_filenames[0]);
	printf("created a vector of string, filenames, size is %ld\n", filenames.size());
	result = filenames;
}

/* create new folder */
// TODO: combine this with createfolder or extract the common part (update list)
string try_createfolder(map<string, string> cookies, string request_body) {
	cout << "------------------------------------------------------\n\n\n";
//	printf("call try_createfolder, request_body: %s\n", request_body.c_str());
	printf("call try_createfolder, request_body len: %ld\n", request_body.length());

	// parse request_body, to get filename and filecontent
	map<string, string> params = get_params_from_request_body(request_body);
	string filedirectory = params["filedirectory"];
	string foldername = params["foldername"];
	printf("filedirectory: %s, len=%ld\n", filedirectory.c_str(), filedirectory.length());
	while (filedirectory.find("%2F") != string::npos) {
		filedirectory.replace(filedirectory.find("%2F"), 3, "/");
	}
	printf("filedirectory: %s, len=%ld\n", filedirectory.c_str(), filedirectory.length());
	printf("foldername: %s, len=%ld\n", foldername.c_str(), foldername.length());
	// if length==0, change to a failure page
	if (foldername.length() == 0) {
		printf("no foldername\n");
		return response_generator("pages/drive-action-failed.html");
	}

	// concat directory+filename
	string fullFoldername = filedirectory + foldername;
	printf("fullFoldername: %s, len=%ld\n", fullFoldername.c_str(), fullFoldername.length());

	// get username from cookies
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
	// get node_ip and node_port from cookies
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	strcpy(storage_node_ip, contact_ip.c_str());
	storage_node_port = stoi(contact_port);

	// get filename list as filenames
	printf("go to get_filenames of %s\n", user_name.c_str());
	vector<string> filenames;// filenames in a vector
	char origin_filenames [1000];// filenames of original format, e.g. "a.txt,b.txt,", "" for none filenames
	memset(origin_filenames, 0, 1000);
	get_filenames(string(user_name), string(storage_node_ip), storage_node_port, filenames, origin_filenames);
	for (int i = 0; i < filenames.size(); i++) {
		printf("return filenames: %d, %s\n", i, filenames[i].c_str());
	}

	// append current foldername to the list, if filename is not in list
	printf("append current foldername to the list vector\n");
	filenames.push_back(fullFoldername);// append current fullFoldername to local list
	printf("convert filename list vector to string\n");
	string filenames_str = "";// string format of filenames
	for (int i = 0; i < filenames.size(); i++) {
		filenames_str += filenames[i] + ",";// keep the comma at last, e.g."ABCDE,"
	}

	// update filename list in K-V store
	printf("go to put/cput back the filename list, user_name\n", user_name.c_str());
	char put_response [BUFFER_SIZE];
	memset(put_response, 0, BUFFER_SIZE);
	if (strlen(origin_filenames) == 0) {
		// first time to put filenames, use PUT, origin_filenames is not operated in get_filenames
		printf("origin_filenames is empty, go to do_put\n");
		do_PUT(storage_node_ip, storage_node_port, user_name, "filenames", filenames_str, put_response);
		printf("put_res is %s\n", put_response);
	} else {
		// use CPUT
		printf("origin_filenames is not empty, go to do_cput, origin_filenames is %s\n", origin_filenames);
		do_CPUT(storage_node_ip, storage_node_port, user_name, "filenames", string(origin_filenames), filenames_str, put_response);
		printf("cput_res is %s\n", put_response);
	}

	// sendback response
	printf("sendback response\n");
	return response_generator("pages/drive-action-ok.html");
}


/* rename a file or folder, cannot rename as an existed name */
string try_rename(map<string, string> cookies, string request_body) {
	cout << "------------------------------------------------------\n\n\n";
	printf("call try_rename, request_body: %s\n", request_body.c_str());

	// get information from cookies and request_body

	// get username from cookies
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
	// get node_ip and node_port from cookies
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	strcpy(storage_node_ip, contact_ip.c_str());
	storage_node_port = stoi(contact_port);
	// parse request_body, to get fullFilename and newname
	map<string, string> params = get_params_from_request_body(request_body);
	string fullFilename = params["fullFilename"];
	string newname = params["newname"];
	printf("try_rename: fullFilename: %s, len=%ld\n", fullFilename.c_str(), fullFilename.length());
	while (fullFilename.find("%2F") != string::npos) {
		fullFilename.replace(fullFilename.find("%2F"), 3, "/");
	}
	fullFilename = UrlDecode(fullFilename);
	printf("try_rename: fullFilename: %s, len=%ld\n", fullFilename.c_str(), fullFilename.length());
	if (newname.length() == 0 || newname.find("%") != newname.npos || newname.find("+") != newname.npos) {
		printf("no newname or newname has special characters\n");
		return response_generator("pages/drive-action-failed.html");
	}
	int last_slash = fullFilename.find_last_of("/");
	string newFullFilename = fullFilename.substr(0, last_slash + 1) + newname;
	printf("newFullFilename: %s\n", newFullFilename.c_str());


	// get all fullfilenames, traverse to check whether there is already newname (this can be a file or a folder)
	printf("go to get_filenames of %s\n", user_name.c_str());
	vector<string> filenames;// filenames in a vector
	char origin_filenames [1000];// filenames of original format, e.g. "a.txt,b.txt,", "" for none filenames
	memset(origin_filenames, 0, 1000);
	get_filenames(string(user_name), string(storage_node_ip), storage_node_port, filenames, origin_filenames);
	for (int i = 0; i < filenames.size(); i++) {
		printf("return filenames: %d, %s\n", i, filenames[i].c_str());
		if (filenames[i] == newFullFilename) {
			printf("already has this filename! Do nothing.\n");
			return response_generator("pages/drive-action-failed.html");
		}
	}


	// reaching here means there is not such filename, rename the file or the whole folder
	vector<string> newfilenames;
	if (fullFilename.find(".") != fullFilename.npos) {
		// this is a file
		printf("there is a dot, it is a file, only need to change the filename and delete filename-filecontent pair, put new pair\n");

		// create a new filenames vector, transfer it to a string, cput it

		// create a new filenames vector
		for (int i = 0; i < filenames.size(); i++) {
			printf("traverse filenames: %d, %s\n", i, filenames[i].c_str());
			if (filenames[i] == fullFilename) {
				printf("This is original file, push the new filename to the vector\n");
				newfilenames.push_back(newFullFilename);
			} else {
				printf("This is not original file, push it to the vector\n");
				newfilenames.push_back(filenames[i]);
			}
		}
		// convert newfilenames vector to string
		printf("convert newfilenames vector to string\n");
		string filenames_str = "";// string format of filenames
		for (int i = 0; i < newfilenames.size(); i++) {
			filenames_str += newfilenames[i] + ",";// keep the comma at last, e.g."ABCDE,"
		}
		// cput the newfilenames string (the list will not be empty)
		char cput_response [BUFFER_SIZE];
		memset(cput_response, 0, BUFFER_SIZE);
		printf("go to do_CPUT, user_name\n", user_name.c_str());
		do_CPUT(storage_node_ip, storage_node_port, user_name, "filenames", string(origin_filenames), filenames_str, cput_response);
		printf("cput_res is %s\n", cput_response);


		// get this file's content, delete the old filename-content pair, put new pair
		delete_old_put_new_file_pair(storage_node_ip, storage_node_port, user_name, fullFilename, newFullFilename);
	} else {
		// this is a folder
		printf("there is no dot, it is a folder\n");

		// create a new filenames vector, replace all the files and folders with prefix of fullFilename, transfer it to a string, cput it

		// create a new filenames vector
		string fullFilename_as_prefix = fullFilename + "/";
		for (int i = 0; i < filenames.size(); i++) {
			printf("Update list: traverse filenames: %d, %s\n", i, filenames[i].c_str());
			if (filenames[i] == fullFilename) {
				printf("This is original folder, push the new foldername to the vector\n");
				newfilenames.push_back(newFullFilename);
			} else if (filenames[i].substr(0, fullFilename_as_prefix.length()) == fullFilename_as_prefix) {
				printf("This is subfile/subfolder, push the new filename/foldername to the vector\n");
				string newSubname = newFullFilename + "/" + filenames[i].substr(fullFilename_as_prefix.length());
				printf("newSubname: %s\n", newSubname.c_str());
				newfilenames.push_back(newSubname);
			} else {
				printf("This is neither original file, nor the subfile/subfolder, push it to the vector\n");
				newfilenames.push_back(filenames[i]);
			}
		}
		// convert newfilenames vector to string
		printf("convert newfilenames vector to string\n");
		string filenames_str = "";// string format of filenames
		for (int i = 0; i < newfilenames.size(); i++) {
			filenames_str += newfilenames[i] + ",";// keep the comma at last, e.g."ABCDE,"
		}
		// cput the newfilenames string (the list will not be empty)
		char cput_response [BUFFER_SIZE];
		memset(cput_response, 0, BUFFER_SIZE);
		printf("go to do_CPUT, user_name\n", user_name.c_str());
		do_CPUT(storage_node_ip, storage_node_port, user_name, "filenames", string(origin_filenames), filenames_str, cput_response);
		printf("cput_res is %s\n", cput_response);


		// for each renamed subfile, get this file's content, delete the old filename-content pair, put new pair
		for (int i = 0; i < filenames.size(); i++) {
			printf("Update subfile: traverse filenames: %d, %s\n", i, filenames[i].c_str());
			if (filenames[i] != fullFilename &&
				filenames[i].substr(0, fullFilename_as_prefix.length()) == fullFilename_as_prefix &&
				filenames[i].find(".") != filenames[i].npos) {
				// this is a subfile
				printf("this is a subfile\n");
				string newSubname = newFullFilename + "/" + filenames[i].substr(fullFilename_as_prefix.length());
				printf("newSubname: %s\n", newSubname.c_str());
				delete_old_put_new_file_pair(storage_node_ip, storage_node_port, user_name, filenames[i], newSubname);
			}
		}
	}

	// sendback response
	printf("sendback response\n");
	return response_generator("pages/drive-action-ok.html");
}

/* when a file is renamed, delete the old pair, put the new pair, for large-file mode, need to delete all old chunks and put new chunks */
void delete_old_put_new_file_pair(char* storage_node_ip, int storage_node_port, string user_name, string fullFilename, string newFullFilename) {
	// get this file's chunksize, delete the old filename-chunksize pair, put new pair

	// get chunksize
	char get_response [BUFFER_SIZE];// to fetch filename_list
	memset(get_response, 0, BUFFER_SIZE);
	printf("go to do_GET, storage_node_ip: %s, storage_node_port: %d\n", storage_node_ip, storage_node_port);
	if (!do_GET(storage_node_ip, storage_node_port, user_name, fullFilename, get_response)) {
		printf("failed to get filename_list\n");
	}
	printf("get res: %s\n", get_response);
	string chunksize = string(get_response + 4);
	printf("This file's chunksize is %s\n", chunksize.c_str());
	// delete old pair
	char delete_response [BUFFER_SIZE];// to fetch uidl_list
	memset(delete_response, 0, BUFFER_SIZE);
	tmp_do_DELETE(storage_node_ip, storage_node_port, user_name, fullFilename, delete_response);
	printf("delete_old_put_new_file_pair: do_DELETE response: %s\n", delete_response);
	if (strstr(delete_response, "-ERR")) {
		printf("\n do DELETE failed\n");// TODO redirect to FAIL page
	}
	// put new pair
	char put_response [BUFFER_SIZE];
	memset(put_response, 0, BUFFER_SIZE);
	printf("go to do_PUT, user_name\n", user_name.c_str());
	if (!do_PUT(storage_node_ip, storage_node_port, user_name, newFullFilename, chunksize, put_response)) {
		printf("failed to put new pair\n");
	}
	printf("after put newFullFilename-chunksize pair, put_res: %s\n", put_response);


	// for each chunk, get the content, delete old pair, put new pair
	int chunksize_int = stoi(chunksize);
	for (int i = 0; i < chunksize_int; i++) {
		string fullFilenameChunk = fullFilename + "|" + to_string(i);
		string newFullFilenameChunk = newFullFilename + "|" + to_string(i);
		// get chunkcontent
//		char get_response [BUFFER_SIZE];// to fetch filename_list
		memset(get_response, 0, BUFFER_SIZE);
		printf("go to do_GET, storage_node_ip: %s, storage_node_port: %d\n", storage_node_ip, storage_node_port);
		if (!do_GET(storage_node_ip, storage_node_port, user_name, fullFilenameChunk, get_response)) {
			printf("failed to get filename_list\n");
		}
		printf("get res: %s\n", get_response);
		string chunkcontent = string(get_response + 4);
		printf("This file's chunkcontent is %s\n", chunkcontent.c_str());
		// delete old pair
//		char delete_response [BUFFER_SIZE];// to fetch uidl_list
		memset(delete_response, 0, BUFFER_SIZE);
		tmp_do_DELETE(storage_node_ip, storage_node_port, user_name, fullFilenameChunk, delete_response);
		printf("delete_old_put_new_file_pair: do_DELETE response: %s\n", delete_response);
		if (strstr(delete_response, "-ERR")) {
			printf("\n do DELETE failed\n");// TODO redirect to FAIL page
		}
		// put new pair
//		char put_response [BUFFER_SIZE];
		memset(put_response, 0, BUFFER_SIZE);
		printf("go to do_PUT, user_name\n", user_name.c_str());
		if (!do_PUT(storage_node_ip, storage_node_port, user_name, newFullFilenameChunk, chunkcontent, put_response)) {
			printf("failed to put new pair\n");
		}
		printf("after put newFullFilenameChunk-chunkcontent pair, put_res: %s\n", put_response);
	}
}


/* delete a file or folder */
string try_deletefile(map<string, string> cookies, string request_body) {
	cout << "------------------------------------------------------\n\n\n";
	printf("call try_deletefile, request_body: %s\n", request_body.c_str());

	// get information from cookies and request_body

	// get username from cookies
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
	// get node_ip and node_port from cookies
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	strcpy(storage_node_ip, contact_ip.c_str());
	storage_node_port = stoi(contact_port);
	// parse request_body, to get fullFilename and newname
	map<string, string> params = get_params_from_request_body(request_body);
	string fullFilename = params["fullFilename"];
	printf("fullFilename: %s, len=%ld\n", fullFilename.c_str(), fullFilename.length());
//	while (fullFilename.find("%2F") != string::npos) {
//		fullFilename.replace(fullFilename.find("%2F"), 3, "/");
//	}
	fullFilename = UrlDecode(fullFilename);
	printf("fullFilename: %s, len=%ld\n", fullFilename.c_str(), fullFilename.length());
	int last_slash = fullFilename.find_last_of("/");


	// get all fullfilenames, to find subfile/subfolder
	printf("go to get_filenames of %s\n", user_name.c_str());
	vector<string> filenames;// filenames in a vector
	char origin_filenames [1000];// filenames of original format, e.g. "a.txt,b.txt,", "" for none filenames
	memset(origin_filenames, 0, 1000);
	get_filenames(string(user_name), string(storage_node_ip), storage_node_port, filenames, origin_filenames);


	// delete the file or the whole folder
	vector<string> newfilenames;
	if (fullFilename.find(".") != fullFilename.npos) {
		// this is a file
		printf("there is a dot, it is a file, only need to delete the filename from list and delete filename-filecontent pair\n");

		// create a new filenames vector, transfer it to a string, update(cput/delete) it

		// create a new filenames vector
		for (int i = 0; i < filenames.size(); i++) {
			printf("traverse filenames: %d, %s\n", i, filenames[i].c_str());
			if (filenames[i] == fullFilename) {
				printf("This is original file, do nothing\n");
			} else {
				printf("This is not original file, push it to the vector\n");
				newfilenames.push_back(filenames[i]);
			}
		}
		// convert newfilenames vector to string
		printf("convert newfilenames vector to string\n");
		string filenames_str = "";// string format of filenames
		for (int i = 0; i < newfilenames.size(); i++) {
			filenames_str += newfilenames[i] + ",";// keep the comma at last, e.g."ABCDE,"
		}
		// update(cput/delete) the newfilenames string
		if (newfilenames.size() == 0) {
			// list is empty, delete it to avoid complex remaining characters
			printf("newfilenames.size=0, do not cput list, just delete list\n");
			char delete_response [BUFFER_SIZE];
			memset(delete_response, 0, BUFFER_SIZE);
			tmp_do_DELETE(storage_node_ip, storage_node_port, user_name, "filenames", delete_response);
			printf("try_deletefile: do_DELETE response: %s\n", delete_response);
			if (strstr(delete_response, "-ERR")) {
				printf("\n do DELETE failed\n");// TODO redirect to FAIL page
			}
		} else {
			// cput the newfilenames string
			printf("newfilenames.size>0, cput the list\n");
			char cput_response [BUFFER_SIZE];
			memset(cput_response, 0, BUFFER_SIZE);
			printf("go to do_CPUT, user_name\n", user_name.c_str());
			do_CPUT(storage_node_ip, storage_node_port, user_name, "filenames", string(origin_filenames), filenames_str, cput_response);
			printf("cput_res is %s\n", cput_response);
		}


		// delete filename-chunksize and chunks
		delete_file_helper(storage_node_ip, storage_node_port, user_name, fullFilename);
		cout << "after delete_file_helper" << endl;
	} else {
		// this is a folder
		printf("there is no dot, it is a folder\n");

		// create a new filenames vector, exclude all the files and folders with prefix of fullFilename, transfer it to a string, cput it

		// create a new filenames vector
		string fullFilename_as_prefix = fullFilename + "/";
		for (int i = 0; i < filenames.size(); i++) {
			printf("Update list: traverse filenames: %d, %s\n", i, filenames[i].c_str());
			if (filenames[i] == fullFilename) {
				printf("This is original folder, do nothing\n");
			} else if (filenames[i].substr(0, fullFilename_as_prefix.length()) == fullFilename_as_prefix) {
				printf("This is subfile/subfolder, do nothing\n");
			} else {
				printf("This is neither original file, nor the subfile/subfolder, push it to the vector\n");
				newfilenames.push_back(filenames[i]);
			}
		}
		// convert newfilenames vector to string
		printf("convert newfilenames vector to string\n");
		string filenames_str = "";// string format of filenames
		for (int i = 0; i < newfilenames.size(); i++) {
			filenames_str += newfilenames[i] + ",";// keep the comma at last, e.g."ABCDE,"
		}
		// update(cput/delete) the newfilenames string
		if (newfilenames.size() == 0) {
			// list is empty, delete it to avoid complex remaining characters
			printf("newfilenames.size=0, do not cput list, just delete list\n");
			char delete_response [BUFFER_SIZE];// to fetch uidl_list
			memset(delete_response, 0, BUFFER_SIZE);
			tmp_do_DELETE(storage_node_ip, storage_node_port, user_name, "filenames", delete_response);
			printf("try_deletefile: do_DELETE response: %s\n", delete_response);
			if (strstr(delete_response, "-ERR")) {
				printf("\n do DELETE failed\n");// TODO redirect to FAIL page
			}
		} else {
			// cput the newfilenames string
			printf("newfilenames.size>0, cput the list\n");
			char cput_response [BUFFER_SIZE];
			memset(cput_response, 0, BUFFER_SIZE);
			printf("go to do_CPUT, user_name\n", user_name.c_str());
			do_CPUT(storage_node_ip, storage_node_port, user_name, "filenames", string(origin_filenames), filenames_str, cput_response);
			printf("cput_res is %s\n", cput_response);
		}


		// for each deleted subfile, delete the old filename-content pair
		for (int i = 0; i < filenames.size(); i++) {
			printf("Update subfile: traverse filenames: %d, %s\n", i, filenames[i].c_str());
			if (filenames[i] != fullFilename &&
				filenames[i].substr(0, fullFilename_as_prefix.length()) == fullFilename_as_prefix &&
				filenames[i].find(".") != filenames[i].npos) {
				// this is a subfile
				printf("this is a subfile\n");
				// delete filename-chunksize and chunks
				delete_file_helper(storage_node_ip, storage_node_port, user_name, filenames[i]);
			}
		}
	}

	// sendback response
	printf("sendback response\n");
	return response_generator("pages/drive-action-ok.html");
}

/* delete file-chunksize pair and chunks */
void delete_file_helper(char* storage_node_ip, int storage_node_port, string user_name, string fullFilename) {
	// get chunksize, delete filename-chunksize pair, delete chunks

	// get chunksize
	char get_response [BUFFER_SIZE];// to fetch filename_list
	memset(get_response, 0, BUFFER_SIZE);
	printf("go to do_GET, storage_node_ip: %s, storage_node_port: %d\n", storage_node_ip, storage_node_port);
	if (!do_GET(storage_node_ip, storage_node_port, user_name, fullFilename, get_response)) {
		printf("failed to get filename_list\n");
	}
	printf("get res: %s\n", get_response);
	string chunksize = string(get_response + 4);
	printf("This file's chunksize is %s\n", chunksize.c_str());
	// delete old pair
	char delete_response [BUFFER_SIZE];// to fetch uidl_list
	memset(delete_response, 0, BUFFER_SIZE);
	tmp_do_DELETE(storage_node_ip, storage_node_port, user_name, fullFilename, delete_response);
	printf("delete_file_helper: do_DELETE response: %s\n", delete_response);
	if (strstr(delete_response, "-ERR")) {
		printf("\n do DELETE failed\n");// TODO redirect to FAIL page
	}
	// for each chunk, delete old pair
	int chunksize_int = stoi(chunksize);
	for (int i = 0; i < chunksize_int; i++) {
		printf("i=%d\n", i);
		string fullFilenameChunk = fullFilename + "|" + to_string(i);
		// delete old pair
		memset(delete_response, 0, BUFFER_SIZE);
		tmp_do_DELETE(storage_node_ip, storage_node_port, user_name, fullFilenameChunk, delete_response);
		printf("delete_file_helper: do_DELETE response: %s\n", delete_response);
		if (strstr(delete_response, "-ERR")) {
			printf("\n do DELETE failed\n");// TODO redirect to FAIL page
		}
	}
}

/* move a file/folder to a directory, e.g. move to "root/folder1/" */
string try_move(map<string, string> cookies, string request_body) {
	cout << "------------------------------------------------------\n\n\n";
	printf("call try_move, request_body: %s\n", request_body.c_str());

	// get information from cookies and request_body

	// get username from cookies
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
	// get node_ip and node_port from cookies
	char storage_node_ip [50];
	memset(storage_node_ip, 0, 50);
	int storage_node_port = -1;
	strcpy(storage_node_ip, contact_ip.c_str());
	storage_node_port = stoi(contact_port);
	// parse request_body, to get fullFilename and newname
	map<string, string> params = get_params_from_request_body(request_body);
	string fullFilename = params["fullFilename"];
	string newdirectory = params["newdirectory"];
	printf("fullFilename: %s, len=%ld\n", fullFilename.c_str(), fullFilename.length());
	printf("newdirectory: %s, len=%ld\n", newdirectory.c_str(), newdirectory.length());
//	while (fullFilename.find("%2F") != string::npos) {
//		fullFilename.replace(fullFilename.find("%2F"), 3, "/");
//	}
	fullFilename = UrlDecode(fullFilename);
	printf("fullFilename: %s, len=%ld\n", fullFilename.c_str(), fullFilename.length());
//	while (newdirectory.find("%2F") != string::npos) {
//		newdirectory.replace(newdirectory.find("%2F"), 3, "/");
//	}
	newdirectory = UrlDecode(newdirectory);
	printf("newdirectory: %s, len=%ld\n", newdirectory.c_str(), newdirectory.length());
	if (newdirectory.length() == 0) {
		printf("no newdirectory\n");
		return response_generator("pages/drive-action-failed.html");
	}


	// check if newdirectory has dot "."
	if (newdirectory.find(".") != newdirectory.npos) {
		printf("The new directory has dot, this may be a file, do nothing\n");
		return response_generator("pages/drive-action-failed.html");
	}
	// check if newdirectory goes into a subfolder of the original folder
	if (newdirectory.substr(0, fullFilename.length()) == fullFilename) {
		printf("The new directory is inside this folder, do nothing\n");
		return response_generator("pages/drive-action-failed.html");
	}



	// concat newdirectory and fullFilename to the newfullfilename
	int last_slash = fullFilename.find_last_of("/");
	string newFullFilename = newdirectory + fullFilename.substr(last_slash + 1);
	printf("newFullFilename: %s\n", newFullFilename.c_str());


	// get all fullfilenames, traverse to check whether there is already newname (this can be a file or a folder)
	printf("go to get_filenames of %s\n", user_name.c_str());
	vector<string> filenames;// filenames in a vector
	char origin_filenames [1000];// filenames of original format, e.g. "a.txt,b.txt,", "" for none filenames
	memset(origin_filenames, 0, 1000);
	get_filenames(string(user_name), string(storage_node_ip), storage_node_port, filenames, origin_filenames);
	for (int i = 0; i < filenames.size(); i++) {
		printf("return filenames: %d, %s\n", i, filenames[i].c_str());
		if (filenames[i] == newFullFilename) {
			printf("already has this filename! Do nothing.\n");
			return response_generator("pages/drive-action-failed.html");
		}
	}


	// reaching here means there is no such filename, move the file or the whole folder
	vector<string> newfilenames;
	if (fullFilename.find(".") != fullFilename.npos) {
		// this is a file
		printf("there is a dot, it is a file, only need to change the filename and delete filename-chunksize pair, put new pair, delete chunks, put new chunks\n");

		// create a new filenames vector, transfer it to a string, cput it

		// create a new filenames vector
		for (int i = 0; i < filenames.size(); i++) {
			printf("traverse filenames: %d, %s\n", i, filenames[i].c_str());
			if (filenames[i] == fullFilename) {
				printf("This is original file, push the new filename to the vector\n");
				newfilenames.push_back(newFullFilename);
			} else {
				printf("This is not original file, push it to the vector\n");
				newfilenames.push_back(filenames[i]);
			}
		}
		// convert newfilenames vector to string
		printf("convert newfilenames vector to string\n");
		string filenames_str = "";// string format of filenames
		for (int i = 0; i < newfilenames.size(); i++) {
			filenames_str += newfilenames[i] + ",";// keep the comma at last, e.g."ABCDE,"
		}
		// cput the newfilenames string (the list will not be empty)
		char cput_response [BUFFER_SIZE];
		memset(cput_response, 0, BUFFER_SIZE);
		printf("go to do_CPUT, user_name\n", user_name.c_str());
		do_CPUT(storage_node_ip, storage_node_port, user_name, "filenames", string(origin_filenames), filenames_str, cput_response);
		printf("cput_res is %s\n", cput_response);


		// get this file's content, delete the old filename-content pair, put new pair
		delete_old_put_new_file_pair(storage_node_ip, storage_node_port, user_name, fullFilename, newFullFilename);
	} else {
		// this is a folder
		printf("there is no dot, it is a folder\n");

		// create a new filenames vector, replace all the files and folders with prefix of fullFilename, transfer it to a string, cput it

		// create a new filenames vector
		string fullFilename_as_prefix = fullFilename + "/";
		for (int i = 0; i < filenames.size(); i++) {
			printf("Update list: traverse filenames: %d, %s\n", i, filenames[i].c_str());
			if (filenames[i] == fullFilename) {
				printf("This is original folder, push the new foldername to the vector\n");
				newfilenames.push_back(newFullFilename);
			} else if (filenames[i].substr(0, fullFilename_as_prefix.length()) == fullFilename_as_prefix) {
				printf("This is subfile/subfolder, push the new filename/foldername to the vector\n");
				string newSubname = newFullFilename + "/" + filenames[i].substr(fullFilename_as_prefix.length());
				printf("newSubname: %s\n", newSubname.c_str());
				newfilenames.push_back(newSubname);
			} else {
				printf("This is neither original file, nor the subfile/subfolder, push it to the vector\n");
				newfilenames.push_back(filenames[i]);
			}
		}
		// convert newfilenames vector to string
		printf("convert newfilenames vector to string\n");
		string filenames_str = "";// string format of filenames
		for (int i = 0; i < newfilenames.size(); i++) {
			filenames_str += newfilenames[i] + ",";// keep the comma at last, e.g."ABCDE,"
		}
		// cput the newfilenames string (the list will not be empty)
		char cput_response [BUFFER_SIZE];
		memset(cput_response, 0, BUFFER_SIZE);
		printf("go to do_CPUT, user_name\n", user_name.c_str());
		do_CPUT(storage_node_ip, storage_node_port, user_name, "filenames", string(origin_filenames), filenames_str, cput_response);
		printf("cput_res is %s\n", cput_response);


		// for each moved subfile, get this file's chunksize, delete the old filename-chunksize pair, put new pair, delete old chunks, put new chunks
		for (int i = 0; i < filenames.size(); i++) {
			printf("Update subfile: traverse filenames: %d, %s\n", i, filenames[i].c_str());
			if (filenames[i] != fullFilename &&
				filenames[i].substr(0, fullFilename_as_prefix.length()) == fullFilename_as_prefix &&
				filenames[i].find(".") != filenames[i].npos) {
				// this is a subfile
				printf("this is a subfile\n");
				string newSubname = newFullFilename + "/" + filenames[i].substr(fullFilename_as_prefix.length());
				printf("newSubname: %s\n", newSubname.c_str());
				delete_old_put_new_file_pair(storage_node_ip, storage_node_port, user_name, filenames[i], newSubname);
			}
		}
	}

	// sendback response
	printf("sendback response\n");
	return response_generator("pages/drive-action-ok.html");
}












