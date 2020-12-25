


#include "smtp_func.h"



bool isHelo(char* buf)
{
	return  (buf[0] == 'H' || buf[0] == 'h') &&
			(buf[1] == 'E' || buf[1] == 'e') &&
			(buf[2] == 'L' || buf[2] == 'l') &&
			(buf[3] == 'O' || buf[3] == 'o');
}

bool isFrom(char* crlf, char* buf)
{
	return crlf - buf >= 10 &&
			(buf[0] == 'M' || buf[0] == 'm') &&
			(buf[1] == 'A' || buf[1] == 'a') &&
			(buf[2] == 'I' || buf[2] == 'i') &&
			(buf[3] == 'L' || buf[3] == 'l') &&
			 buf[4] == ' ' &&
			(buf[5] == 'F' || buf[5] == 'f') &&
			(buf[6] == 'R' || buf[6] == 'r') &&
			(buf[7] == 'O' || buf[7] == 'o') &&
			(buf[8] == 'M' || buf[8] == 'm') &&
			 buf[9] == ':';
}

bool isRcpt(char* buf)
{
	return  (buf[0] == 'R' || buf[0] == 'r') &&
			(buf[1] == 'C' || buf[1] == 'c') &&
			(buf[2] == 'P' || buf[2] == 'p') &&
			(buf[3] == 'T' || buf[3] == 't') &&
			 buf[4] == ' ' &&
			(buf[5] == 'T' || buf[5] == 't') &&
			(buf[6] == 'O' || buf[6] == 'o') &&
			 buf[7] == ':';
}

bool isData(char* crlf, char* buf)
{
	return crlf - buf == 4 &&
			(buf[0] == 'D' || buf[0] == 'd') &&
			(buf[1] == 'A' || buf[1] == 'a') &&
			(buf[2] == 'T' || buf[2] == 't') &&
			(buf[3] == 'A' || buf[3] == 'a');
}

bool isQuit(char* buf)
{
	return  (buf[0] == 'Q' || buf[0] == 'q') &&
			(buf[1] == 'U' || buf[1] == 'u') &&
			(buf[2] == 'I' || buf[2] == 'i') &&
			(buf[3] == 'T' || buf[3] == 't');
}

bool isRset(char* buf)
{
	return  (buf[0] == 'R' || buf[0] == 'r') &&
			(buf[1] == 'S' || buf[1] == 's') &&
			(buf[2] == 'E' || buf[2] == 'e') &&
			(buf[3] == 'T' || buf[3] == 't');
}

bool isNoop(char* crlf, char* buf)
{
	return crlf - buf == 4 &&
			(buf[0] == 'N' || buf[0] == 'n') &&
			(buf[1] == 'O' || buf[1] == 'o') &&
			(buf[2] == 'O' || buf[2] == 'o') &&
			(buf[3] == 'P' || buf[3] == 'p');
}
