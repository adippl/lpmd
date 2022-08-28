#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "lpmd.h"
#include "lpmd_messages.h"
#include <sys/socket.h>
#include <sys/un.h>


int mode_daemon=0;
int action=0;
int sock=0;
int adm_sock=0;
int conn_fd=0;

char buf[512]={0};

void
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
				sock_path = daemon_sock_path;}
		sock_path = daemon_adm_sock_path;}
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
	if(connect(
		conn_fd, 
		(struct sockaddr *)&sock_addr, 
		sizeof(struct sockaddr_un)) == -1){
			error_errno_msg_exit("failed connect to ", sock_path);}
	fprintf(stderr, "connected to acpid at %s\n", sock_path);
}

void
parse_args(int argc, char** argv){
	for(;argc>1&&argv[1][0]=='-';argc--,argv++){
		printf("%d %s\n", argc, argv[1]);
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
		
		if( !strncmp( &argv[1][1], client_idle_action, MSG_MAX_LEN) )
			action = CLIENT_IDLE_ACTION;
		if( !strncmp( &argv[1][1], "client_idle", MSG_MAX_LEN) )
			action = CLIENT_IDLE_ACTION;
		
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
		printf(" %ld %ld\n", msg_size, write_size);
#endif
		error_errno_msg_exit("couldn't write full message to socket","");}
	write_size = write( conn_fd, "\n", 1 );
	}

void
wait_for_reply(); //TODO

int
main(int argc, char** argv){
	parse_args(argc, argv);
	connect_to_daemon();
	
	printf("%d\n", mode_daemon);
	if( mode_daemon ){
		printf("daemon mode not implemented\n");
		exit(EXIT_FAILURE);}
	else{
		switch(action){
		case CLIENT_SUSPEND_ASK:
			basic_send_msg( client_suspend_ask );
			break;
		case CLIENT_HIBERNATE_ASK:
			basic_send_msg( client_hibernate_ask );
			break;
		case CLIENT_NOTIFU_DAEMON_ABOUT_IDLE:
			basic_send_msg( client_notifu_daemon_about_idle );
			break;
		}
		printf("client mode not implemented\n");
		exit(EXIT_FAILURE);
		//wait_for_reply()
		}
	}

