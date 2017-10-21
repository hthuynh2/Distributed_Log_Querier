//
//  Message.h
//  
//
//  Created by Hieu Huynh on 9/13/17.
//
//

#ifndef Message_h
#define Message_h


#include "common.h"

#define ERROR INT_MIN
using namespace std;

class Message{
	int length;
	char * msg_ptr;
	
public:
	Message(){
		msg_ptr = NULL;
		length = 0;
	}
	
	Message(int input_length, const char* buf){
		length = input_length;
		msg_ptr = new char[length];
		memcpy(msg_ptr, buf, length);
	}
	
	~Message(){
		if (msg_ptr){
			delete msg_ptr;
		}
		length = 0;
		msg_ptr = NULL;
	}
	
	/*
	 This method append the input char buf to the end of the msg and update the length
	 */
	void append(int input_length,  char* buf){
		char* t = new char[input_length + length];
		if(length != 0 && msg_ptr!= NULL){
			memcpy(t, msg_ptr, length);
		}
		memcpy(t + length, buf, input_length);
		if(msg_ptr != NULL)
			delete msg_ptr;
		msg_ptr = t;
		return;
	}
	/*
	 This method convert input integer from from host byte order to network byte order and send it to the input socket_fd
	 */
	int send_int_msg(int number , int socket_fd){
		int ret;
		int net_number = htonl(number);
		if((ret = send(socket_fd, &net_number, sizeof(net_number), 0)) == -1){
			return -1;
		}
		return ret;
	}
	
	/*
	 This method receive the integer, and convert it from network byte order to host byte order
	 */
	int receive_int_msg(int socket_fd){
		int number;
		if(recv(socket_fd, &number,sizeof(int), 0) == -1){
			perror("Message: receive_int");
			return ERROR;
		}
		number = ntohl(number);
		return number;
	}
	
	/*
	 This method send the msg length and then the msg to the input socket_fd
	 */
	int send_msg(int socket_fd){
		int ret;
		if(send_int_msg(length, socket_fd) == -1){
			perror("Message: send_int");
			return -1;
		}
		while(length >0){
			if((ret = send(socket_fd, msg_ptr, length, 0)) == -1){
				perror("Message: send");
				return -1;
			}
			length -= ret;
		}
		return length;
	}
	
	
};

#endif /* Message_h */
