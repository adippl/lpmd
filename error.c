#include <stdlib.h>
#include <stdio.h>
#define e_msg_buff_size 128
char e_msg_buff[e_msg_buff_size]={0};
void
error_errno_msg_exit(const char* msg1, const char* msg2){
	if(msg2)
		snprintf(
			e_msg_buff,
			e_msg_buff_size,
			"EXIT_ERROR %s %s",
			msg1,
			msg2);
	else
		snprintf(
			e_msg_buff,
			e_msg_buff_size,
			"EXIT_ERROR %s",
			msg1);
	perror(e_msg_buff);
	exit(EXIT_FAILURE);}

