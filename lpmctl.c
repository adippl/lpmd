#include "error.h"
#include "lpmd.h"
#include "lpmd_messages.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <poll.h>
struct pollfd fds[1] = {0};


int mode_daemon=0;
int action=0;
int sock=0;
int adm_sock=0;
int conn_fd=0;

char buf[BUF_SIZE]={0};

// TODO replace hardcoded paths with config option
const char* default_lock_cmd_i3 = "i3lock -c 000000 --nofork";
const char* default_lock_cmd_sway = "swaylock -c 000000";
int lock_command_set=0;
char lock_cmd[MSG_MAX_LEN]={0};
char* lock_cmd_ptr = NULL;
int lock_cmd_converted=0;
char* lock_cmd_=0;
#define lock_argv_len 10
/* arr len +1 to make sure array is null terminated */
char* lock_argv[lock_argv_len+1] = {0};
int lock_argc=0;
int screen_locked=0;
pid_t lock_pid=0;

int map_str_to_str_arr(
	char* str,
	int str_max_len,
	char** char_arr_ptr,
	int char_arr_max_size,
	int* ret_char_arr_size);
void basic_send_msg(const char* msg);


void
lock_screen(){
	printf("locking screen\n");
	/* this supports max 10 arguments to locking commands */
	int rc=-9;
	int arrsize=0;
	
	/* check if we already have child process locking screen */
	int waitpid_status = waitpid( lock_pid, NULL, WNOHANG);
	printf("lock stats %d\n", waitpid_status);
	if( ! waitpid_status ){
		fprintf(stderr, "child locking program already running, not locking\n");
		return;}
	
	if( !lock_cmd_converted ){
		rc = map_str_to_str_arr( 
			lock_cmd_ptr,
			MSG_MAX_LEN, 
			lock_argv,
			lock_argv_len,
			&arrsize);
		if(!rc)
			lock_cmd_converted=1,
			printf("success\n");
	}
	
#ifdef DEBUG
	printf("progname %s\ndumping lock command arguments\n", lock_argv[0]);
	for(int i=0;i<10;i++)
		printf("arg %d %s\n", i, lock_argv[i]);
#endif

#ifndef DEBUG
	lock_pid = fork();
	if( lock_pid<0 )
		fprintf(stderr,"fork error\n");
//	if( !lock_pid ){
//		for(int i=0;i<10;i++)
//			printf("arg %d %s\n", i, lock_argv[i]);}
		
	if( !lock_pid && execvp( lock_argv[0], lock_argv ))
	    // child process
		printf("wtf\n");
	// parent process
	
	/* check if locking program is running */
	waitpid_status = 0;
	waitpid_status = waitpid( lock_pid, NULL, WNOHANG);
	printf("lock stats %d\n", waitpid_status);
	if( ! waitpid_status ){
		fprintf(stdout, "child process running\n");
		basic_send_msg(client_lock_success);}
	else{
		basic_send_msg(client_lock_fail);}
		
#endif
#ifdef DEBUG
	printf("DEBUG MODE, not locking up the screen\n");
	printf("DEBUG MODE, current lock_cmd %s\n", lock_cmd);
#endif
	}

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
setup_lock_command(){
	/* -lock_cmd was provided */
	if( lock_command_set ){
		lock_cmd_ptr = (char*)&lock_cmd;
		printf("using -lock_cmd '%s'\n", lock_cmd_ptr);
		return;}
	printf("-lock_cmd was not providing, searching for default locks\n");
	// TODO check if env has socket for i3 or sway
	/* -lock_cmd was not provided, testing default screen locks */
	if( access("/usr/bin/swaylock", F_OK | X_OK ) == 0 ){
		//lock_cmd_ptr = default_lock_cmd_sway;
		strncpy( lock_cmd, default_lock_cmd_sway, MSG_MAX_LEN);  
		lock_cmd_ptr = (char*)&lock_cmd;
		printf("found /usr/bin/swaylock, setting it as lock_cmd\n");
		return;}
	else if( access("/usr/bin/i3lock", F_OK | X_OK ) == 0 ){
		//lock_cmd_ptr = default_lock_cmd_i3;
		strncpy( lock_cmd, default_lock_cmd_i3, MSG_MAX_LEN);  
		lock_cmd_ptr = (char*)&lock_cmd;
		printf("found /usr/bin/i3lock, setting it as lock_cmd\n");
		return;}}


int
map_str_to_str_arr(
	char* str,
	int str_max_len,
	char** char_arr_ptr,
	int char_arr_max_size,
	int* ret_char_arr_size){
	
	size_t org_strlen=0; 
	int pos=0;
	int str_pos=0;
	//char* loc_argv[] = char_arr_ptr;
	
	/* check input */
	if( !str || !char_arr_ptr || !ret_char_arr_size )
		return(-1);
	if( str_max_len < 1 )
		return(-2);
	if( char_arr_max_size < 2 )
		return(-3);
	
	org_strlen = strnlen( str, str_max_len);
	*ret_char_arr_size = 0;
	
	while( str[pos] != '\0' ){
		if( isspace( str[pos] ))
			str[pos]='\0';
		pos++;}
	//lock_progname=lock_cmd;
	/* jump to first argument */
	for( int i=0; i<MSG_MAX_LEN-1; i++ ){
		if( str[i] == '\0' && str[i+1] != '\0' ){
			str_pos = i+1;
			break;}}
	/* create array of args for execv() */
	char_arr_ptr[(*ret_char_arr_size)++] = str;
	/* set argument */
	char_arr_ptr[(*ret_char_arr_size)++] = &str[ str_pos ];
	str_pos++;
	/* check if arguments are repeating */
	/* find next argument */
	for( unsigned int i=str_pos; i<org_strlen; i++ ){
		if( str[i-1] == '\0' && str[i] != '\0' ){
			str_pos = i;
			char_arr_ptr[(*ret_char_arr_size)++] = &str[ str_pos ];
		}
	}
	return(0);}

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
			case CLIENT_LOCK_ALL_ASK:
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
	if(connect( conn_fd, (struct sockaddr *)&sock_addr, 
		sizeof(struct sockaddr_un)) == -1){
		
		if(mode_daemon)
			return(1);
		else
			error_errno_msg_exit("failed connect to ", sock_path);}
	fprintf(stderr, "connected to acpid at %s\n", sock_path);
	fds[0].fd = conn_fd;
	fds[0].events = POLLIN;
	if( mode_daemon )
		basic_send_msg( client_listen_to_daemon );
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
		if( !strncmp( &argv[1][1], "hibernate", MSG_MAX_LEN) ) /* -hibernate */
			action = CLIENT_HIBERNATE_ASK;
		
		if( !strncmp( &argv[1][1], client_notify_daemon_about_idle, MSG_MAX_LEN) )
			action = CLIENT_NOTIFY_DAEMON_ABOUT_IDLE;
		if( !strncmp( &argv[1][1], "client_idle", MSG_MAX_LEN) ) /* -client_idle */
			action = CLIENT_NOTIFY_DAEMON_ABOUT_IDLE;
		if( !strncmp( &argv[1][1], "lock_all", MSG_MAX_LEN) ) /* -lock_all */
			action = CLIENT_LOCK_ALL_ASK;

		if( !strncmp( &argv[1][1], "fullCharge", MSG_MAX_LEN) ) /* -fullCharge */
			action = CLIENT_ASK_ZERO_BAT_THRESHOLDS;
		if( !strncmp( &argv[1][1], "safeCharge", MSG_MAX_LEN) ) /* -safeCharge */
			action = CLIENT_ASK_RESTORE_DEF_BAT_THRESHOLDS;
		
		if( !strncmp( &argv[1][1], "forcePowersave", MSG_MAX_LEN) ) /* -forcePowersave */
			action = CLIENT_ASK_FOR_POWERSAVE_GOV_LOCK;
		if( !strncmp( &argv[1][1], "forcePerformance", MSG_MAX_LEN) ) /* -forcePerformance */
			action = CLIENT_ASK_FOR_PERFORMANCE_GOV_LOCK;
		if( !strncmp( &argv[1][1], "autoGov", MSG_MAX_LEN) ) /* -autoGov */
			action = CLIENT_ASK_FOR_AUTOMATIC_GOVERNOR_CONTROL;
		
		if( !strncmp( &argv[1][1], "daemon", MSG_MAX_LEN) )
			mode_daemon = 1;
		
		if( !strncmp( &argv[1][1], "lock_cmd", MSG_MAX_LEN) ){
			printf("fesfesefsef %s %d \n", &argv[1][0], argc );
			/* check if next argument exists */
			/* check if next doesn't start with '-' */
			if( argc>=3 && argv[2][0] != '-' ){
				/* shift to next arg */
				argc--;
				argv++;
				printf("lock command '%s'\n", &argv[1][0]);
				strncpy( (char*)&lock_cmd, &argv[1][0], MSG_MAX_LEN);
				lock_cmd[MSG_MAX_LEN-1]='\0';
				lock_command_set=1;
			}
			else{
				fprintf(stderr, "-lock_cmd used without argument\n");
				exit(EXIT_FAILURE);}
		}
	}
		
	if( action==0 && mode_daemon == 0 ){
		fprintf( stderr, "args are invalid\n");
		exit(EXIT_FAILURE);}}


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
//			printf("%s\n", buf);
		if( !strncmp( buf, daemon_action_success, MSG_MAX_LEN)){
#ifdef DEBUG
			fprintf(stderr, "action success\n");
#endif
			close(conn_fd);
			exit(EXIT_SUCCESS);
				}
		if( !strncmp( buf, daemon_action_refuse, MSG_MAX_LEN)){
#ifdef DEBUG
			fprintf(stderr, "action refused\n");
#endif
			exit(EXIT_FAILURE);
				}
		if( !strncmp( buf, daemon_action_failure, MSG_MAX_LEN)){
#ifdef DEBUG
			fprintf(stderr, "action failed");
#endif
			exit(EXIT_FAILURE);
				}
		if( !strncmp( buf, daemon_ask_for_screen_locks, MSG_MAX_LEN)){
			fprintf(stderr, "lpmd asked to lock screen\n");
			lock_screen();
			return;
			}
		if( !strncmp( buf, suspend_sleep, MSG_MAX_LEN)){
			fprintf(stderr, "lpmd is suspending system\n");
			//TODO reaction to syspend
			return;
			}
		if( !strncmp( buf, hibernate_sleep, MSG_MAX_LEN)){
			fprintf(stderr, "lpmd is hibernating system\n");
			//TODO react to hibernate
			return;
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
	
//	int arrsize=0;
//	setup_lock_command();
//	rc = map_str_to_str_arr( 
//		lock_cmd_ptr,
//		MSG_MAX_LEN, 
//		(char**)&lock_argv,
//		10,
//		&arrsize);
//	if(!rc)
//		lock_cmd_converted=1, printf("success\n");
	
	parse_args(argc, argv);
	setup_lock_command();
	connect_to_daemon();
	if( mode_daemon ){
		//fprintf(stderr, "daemon mode not implemented\n");
		//exit(EXIT_FAILURE);
		while(1){
			rc = poll(fds, 1, 1000000);
			switch(rc){
			case(0): // timeout
#ifdef DEBUG
				puts("DEBUG poll timeout");
				perror("poll");
#endif
				continue;
			case(-1): //error
#ifdef DEBUG
				puts("DEBUG poll error");
				perror("poll");
#endif
				break;
			default:
				if( (fds[0].revents & POLLHUP) == POLLHUP ){
					printf("lost connection to daemon\n");
					close(fds[0].fd);
					connect_to_daemon();
					sleep(1);
					continue;
					}
				if( (fds[0].revents & POLLERR) == POLLERR )
					error_errno_msg_exit("poll error",NULL);
				handle_clients( fds[0].fd);
				//sock_print(fds[0].fd);
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
		case CLIENT_LOCK_ALL_ASK:
			basic_send_msg( client_lock_all_ask );
			break;
		case CLIENT_ASK_ZERO_BAT_THRESHOLDS:
			basic_send_msg( client_ask_zero_bat_thresholds );
			break;
		case CLIENT_ASK_RESTORE_DEF_BAT_THRESHOLDS:
			basic_send_msg( client_ask_restore_def_bat_thresholds );
			break;
		
		case CLIENT_ASK_FOR_POWERSAVE_GOV_LOCK:
			basic_send_msg( client_ask_for_powersave_gov_lock );
			break;
		case CLIENT_ASK_FOR_PERFORMANCE_GOV_LOCK:
			basic_send_msg( client_ask_for_performance_gov_lock );
			break;
		case CLIENT_ASK_FOR_AUTOMATIC_GOVERNOR_CONTROL:
			basic_send_msg( client_ask_for_automatic_governor_control );
			break;
		}
		wait_for_reply();
		}
	}

