#ifndef _ERROR_H
#define _ERROR_H

#include <stdlib.h>
#include <stdio.h>

#define e_msg_buff_size 128

void error_errno_msg_exit(const char* msg1, const char* msg2);

#endif // _ERROR_H
