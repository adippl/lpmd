#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "lpmd.h"
#include "lpmd_messages.h"
#include "error.h"
#include <sys/socket.h>
#include <sys/un.h>

#include <poll.h>
struct pollfd fds[1] = {0};


int mode_daemon=0;
int action=0;
int sock=0;
int adm_sock=0;
int conn_fd=0;

char buf[BUF_SIZE]={0};

int
connect_to_daemon(){
		struct sockaddr_un sock_addr;
		const char* sock_path = NULL;
	if(mode_daemon)
		sock_path = daemon_sock_path;
	else{
		switch(action){
			case CLIENT_SUSPEND_ASK:
			case CLIENT_HIBERNATE_ASK:
				sock_path = daemon_adm_sock_path;
				break;
			default:
				sock_path = daemon_sock_path;}}
	fprintf(stderr, "connecting to %s\n", sock_path);
	memset(buf,0,BUF_SIZE);
	if( (conn_fd=socket(AF_UNIX, SOCK_STREAM, 0)) == -1 )
		error_errno_msg_exit("couldn't create socket", "");
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sun_family = AF_UNIX;
	strncpy(
		sock_addr.sun_path,
		sock_path,
		sizeof(sock_addr.sun_path)-1);
	retry:
	if(connect(
		conn_fd, 
		(struct sockaddr *)&sock_addr, 
		sizeof(struct sockaddr_un)) == -1){
		
		if(mode_daemon){
			return(1);
			sleep(1);
			goto retry;}
		else{
			error_errno_msg_exit("failed connect to ", sock_path);}}
	fprintf(stderr, "connected to acpid at %s\n", sock_path);
	return(0);
}

void
parse_args(int argc, char** argv){
	for(;argc>1&&argv[1][0]=='-';argc--,argv++){
		if( !strncmp( &argv[1][1], "h", MSG_MAX_LEN) )
			printf("TODO help message\n");
		
		if( !strncmp( &argv[1][1], client_suspend_ask, MSG_MAX_LEN) )
			action = CLIENT_SUSPEND_ASK;
		if( !strncmp( &argv[1][1], "suspend", MSG_MAX_LEN) )
			action = CLIENT_SUSPEND_ASK;
		
		if( !strncmp( &argv[1][1], client_hibernate_ask, MSG_MAX_LEN) )
			action = CLIENT_HIBERNATE_ASK;
		if( !strncmp( &argv[1][1], "hibernate", MSG_MAX_LEN) )
			action = CLIENT_HIBERNATE_ASK;
		
		if( !strncmp( &argv[1][1], client_notify_daemon_about_idle, MSG_MAX_LEN) )
			action = CLIENT_NOTIFY_DAEMON_ABOUT_IDLE;
		if( !strncmp( &argv[1][1], "client_idle", MSG_MAX_LEN) )
			action = CLIENT_NOTIFY_DAEMON_ABOUT_IDLE;
		
		if( !strncmp( &argv[1][1], "daemon", MSG_MAX_LEN) )
			mode_daemon = 1;}
		if( action==0 && mode_daemon == 0 ){
			fprintf( stderr, "args are invalid\n");
			exit(EXIT_FAILURE);}}

void
basic_send_msg(const char* msg){
	size_t msg_size = strnlen(msg, MSG_MAX_LEN);
	ssize_t write_size = write( conn_fd, msg, msg_size );
	if( msg_size != (size_t)write_size){
#ifdef DEBUG
		fprintf(stderr, " %ld %ld\n", msg_size, write_size);
#endif
		error_errno_msg_exit("couldn't write full message to socket","");}
	write_size = write( conn_fd, "\n", 1 );
	}

void
sock_print(int fd){
	int numRead=-1;
	if( (numRead = read(fd, buf, BUF_SIZE)) > 0) {
		if( write(STDIN_FILENO, buf, numRead) != numRead) {
			fprintf(stderr,"partial/failed write");}}}

void
handle_clients(int fd){
	int numRead=-1;
	if( (numRead = read(fd, buf, BUF_SIZE)) > 0) {
		for( int i=0; i<MSG_MAX_LEN; i++ ){
			if( buf[i] == '\n' )
				buf[i] = '\0';}}
		if( !strncmp( buf, daemon_action_success, MSG_MAX_LEN)){
			fprintf(stderr, "action success\n");
			close(conn_fd);
			exit(EXIT_SUCCESS);
				}
		if( !strncmp( buf, daemon_action_refuse, MSG_MAX_LEN)){
			fprintf(stderr, "action refused\n");
			exit(EXIT_FAILURE);
				}
		if( !strncmp( buf, daemon_action_failure, MSG_MAX_LEN)){
			fprintf(stderr, "action failed");
			exit(EXIT_FAILURE);
				}
	memset(buf,0,BUF_SIZE);}

void
wait_for_reply(){
	int rc = poll(fds, 1, 1000);
	switch(rc){
	case(0): // timeout
		fprintf(stderr, "Timed out while waiting for reply");
		exit(EXIT_FAILURE);
	case(-1): //error
		fprintf(stderr, "Poll failure for reply\n");
		exit(EXIT_FAILURE);
	default:
		if( (fds[0].revents & POLLIN) == POLLIN )
			handle_clients( fds[0].fd );}
		if( (fds[0].revents & POLLIN) != POLLIN )
			error_errno_msg_exit("connection failure", "");
			}

int
main(int argc, char** argv){
	int rc=-2;
	parse_args(argc, argv);
	connect_to_daemon();
	fds[0].fd = conn_fd;
	fds[0].events = POLLIN;
	
	if( mode_daemon ){
		basic_send_msg( client_listen_to_daemon );
		//fprintf(stderr, "daemon mode not implemented\n");
		//exit(EXIT_FAILURE);
		while(1){
			rc = poll(fds, 1, 1000000);
			switch(rc){
			case(0): // timeout
				puts("DEBUG poll timeout");
				continue;
			case(-1): //error
				puts("DEBUG poll error");
				break;
			default:
				if( (fds[0].revents & POLLHUP) == POLLHUP ){
					printf("lost connection to daemon\n");
					close(fds[0].fd);
					connect_to_daemon();
					sleep(1);
					}
				if( (fds[0].revents & POLLERR) == POLLERR )
					error_errno_msg_exit("poll error",NULL);
				sock_print(fds[0].fd);
				continue;
			}
		}
	}else{
		switch(action){
		case CLIENT_SUSPEND_ASK:
			basic_send_msg( client_suspend_ask );
			break;
		case CLIENT_HIBERNATE_ASK:
			basic_send_msg( client_hibernate_ask );
			break;
		case CLIENT_NOTIFY_DAEMON_ABOUT_IDLE:
			basic_send_msg( client_notify_daemon_about_idle );
			break;
		}
		wait_for_reply();
		}
	}

