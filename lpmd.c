/*  lpmd simple linux power management daemon
 *  Copyright (C) 2020 Adam Prycki (email: adam.prycki@gmail.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <poll.h>

#include "lpmd.h"
#include "shared.h"
#include "error.h"
#include "lpmd_messages.h"

#define SSTR 64
#define msg_buff_size 128
char msg_buff[msg_buff_size]={0};


#ifndef DEBUG_CYCLES
	#define DEBUG_CYCLES 360
#endif

/* config section */
/* config variables */
const int def_bat_charge_start_threshold=45;
const int def_bat_charge_stop_threshold=75;

const float batMinSleepThreshold=0.15f;
const float batLowWarningThreshold=0.25f;
const int loopInterval=1;
const int manageGovernors=1;
const int syncBeforeSuspend=1;
const int suspendDelay=60;
const int wallNotify=1;
const char* wallSuspendWarning="WARNING!!!\nWARNING!!!  battery0 is low.\nWARNING!!!  Syncing filesystem and suspending to mem in 15 seconds...\n";
const char* wallLowBatWarning="WARNING!!!\nWARNING!!!  battery0 is below 25%\n";
int suspend_on_lid_close=true;

const char* powerState="/sys/power/state";
const char* pttyDir="/dev/pts/";
const char* cpupresetPath="/sys/devices/system/cpu/present";
const char* acpi_lid_path="/proc/acpi/button/lid/LID/state";
const char* intel_pstate_path="/sys/devices/system/cpu/intel_pstate";
const char* intel_pstate_turbo_path="/sys/devices/system/cpu/intel_pstate/no_turbo";

//enum governor { powersave=0, performance=1 , ondemand=2 };
#define GOV_POWERSAVE 0
#define GOV_PERFORMANCE 1
#define GOV_ONDEMAND 2
void setGovernor(int governor);
static const char* governor_string[] = {
	"powersave", "performance", "ondemand", };
#define CPU_BOOST_ON 0
#define CPU_BOOST_OFF 1
int user_lock_governor=0;
int current_governor=0;
int user_lock_cpu_boost=0;
int current_cpu_boost=0;

/* runtime variables */
int intel_pstate_present=0;
char acpi_lid_path_exist=0;
int powerStateExists=1;
int chargerConnected=-1;
int lowBatWarning_warned=0;
int numberOfCores=4;
int acpid_connected=0;
int lid_state=-1;
int lid_state_changed=1;


const int require_session_locks=0;
int waiting_for_session_lock_before_suspend=0;
int waiting_for_session_lock_before_hibernate=0;


/* connection to acpid*/
const char acpid_sock_path[]="/run/acpid.socket";
char buf[BUF_SIZE];
int fd_acpid=-1;
struct sockaddr_un acpid_sock_addr;
#define ACPID_STRCMP_MAX_LEN 32 
struct timeval select_timeout = { .tv_sec=loopInterval, .tv_usec=0 };



/* network */
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <sys/ioctl.h>
#define LISTEN_BACKLOG 32
#define MAX_FDS 40
#define D_SOCK 0
#define D_ADM_SOCK 1
#define D_ACPID_SOCK 2
const char* daemon_socket_user= 	"root";
const char* daemon_socket_group=	"users";
const int   daemon_socket_perm=		0660;
//const char* daemon_sock_path="./lpmd.socket";
struct sockaddr_un daemon_sock_addr;

const char* daemon_adm_socket_user= 	"root";
const char* daemon_adm_socket_group=	"wheel";
const int   daemon_adm_socket_perm=		0660;
//const char* daemon_adm_sock_path="./lpmd_adm.socket";
struct sockaddr_un daemon_adm_sock_addr;

struct pollfd fds[MAX_FDS] = {0};
int8_t fd_adm_sock[MAX_FDS] = {0};
int8_t lstng_clients[MAX_FDS] = {0};
int8_t lstng_clients_pos = 0;
int poll_timeout = 1000 * loopInterval;
int nfds = 0;
int c_nfds = MAX_FDS;

void setGovernor(int governor);
void cpu_boost_control(int i);
void wall(const char* message);
void send_msg_to_listening_lpmctl(const char* msg);
void lid_closed_suspend();

void
action_lid_open(){
	fprintf(stdout, "Lid opened\n");
	send_msg_to_listening_lpmctl( notif_lid_open );
	}
void
action_lid_close(){
	fprintf(stdout, "Lid closed\n");
	send_msg_to_listening_lpmctl( notif_lid_close );
	lid_closed_suspend();
	}
void
action_charger_connected(){
	fprintf(stdout, "Charger connected\n");
	setGovernor(GOV_PERFORMANCE);
	cpu_boost_control(CPU_BOOST_ON);
	}
void
action_charger_disconnected(){
	fprintf(stdout, "Charger disconnected\n");
	setGovernor(GOV_POWERSAVE);
	cpu_boost_control(CPU_BOOST_OFF);
	lowBatWarning_warned=0;
	if( suspend_on_lid_close ){
		lid_closed_suspend();
		}
	}


void
suspend(){
	if( ! require_session_locks )
		send_msg_to_listening_lpmctl(daemon_ask_for_screen_locks);
	else
		usleep(1000*100); /* sleep 100ms */
	send_msg_to_listening_lpmctl( notif_suspend_sleep );
#ifdef DEBUG
	fprintf(stderr , "THIS IS DEBUG MODE, lpm WON't put this machine to sleep. Compile in normal mode to enable this functionality\n");
	return;}
#endif
#ifndef DEBUG
	//wall(wallSuspendWarning);
	if(syncBeforeSuspend)
		sync();
	fprintf(stderr, "Suspending to memory\n");
	FILE* f=fopen(powerState, "w");
	if(f){
		fprintf(f, "mem");
		fclose(f);}
	else{
		perror(powerState);}}
#endif

void
hibernate(){
	send_msg_to_listening_lpmctl( daemon_ask_for_screen_locks );
	send_msg_to_listening_lpmctl( notif_hibernate_sleep );
#ifdef DEBUG
	fprintf(stderr , "THIS IS DEBUG MODE, lpm WON't hibernate this machine. Compile in normal mode to enable this functionality\n");
	return;}
#endif
#ifndef DEBUG
	wall(wallSuspendWarning);
	if(syncBeforeSuspend)
		sync();
	fprintf(stderr, "Suspending to memory\n");
	FILE* f=fopen(powerState, "w");
	if(f){
		fprintf(f, "mem");
		fclose(f);}
	else{
		perror(powerState);}}
#endif

void
lid_closed_suspend(){ /* untested; TODO TEST */
	if( suspend_on_lid_close && !chargerConnected && !lid_state ){
		fprintf(stderr, "DEBUG lid closed, suspending computer\n");
		suspend();}}

int
checkFile(const char* path){
	if(access(path,F_OK)==0){
		return(1);}
	else{
		return(0);}}

int
checkDir(const char* path){
	DIR* dir = opendir(path);
	if(dir){
		closedir(dir);
		return(1);}
	else if(ENOENT==errno){
#ifdef DEBUG
		//error_errno_msg_exit("couldn't open", path);
		fprintf(stderr, "couldn't open %s\n", path);
#endif
		return(0);}
	else{
		perror(path);
		return(0);}}


void
detectNumberOfCpus(){
	FILE* f=fopen(cpupresetPath, "r");
	int ec=0;
	if(f){
		/* discards first variable */
		ec=fscanf(f, "%d-%d", &numberOfCores, &numberOfCores);
		if(ec!=2)
			fprintf(stderr, "cpu detect had problem parsing\n");
		numberOfCores++;
		fprintf(stdout, "Found %d core cpu\n", numberOfCores);
		fclose(f);}
	else
		perror(cpupresetPath);}

void
detect_intel_pstate(){
	if(checkDir(intel_pstate_path)){
		fprintf(stdout,
			"Detected intel_pstate, turbo boost control available\n");
			intel_pstate_present=1;
		if( access( intel_pstate_turbo_path, R_OK|W_OK ) ){
			fprintf(stdout,
				"%s is not writable, disabling turbo boost control\n",
				intel_pstate_turbo_path);
			intel_pstate_present=0; //TODO reenable
			}}}

int
fdToint(int fd){ //test function
	int rd=0;
	char str[16]={0};
	if(fd){
		if(!lseek(fd, 0, SEEK_SET)){
			perror("failed seeking file (fd)");
			return(-1);}
		if( (rd=read(fd, str, 16)) <1 ){
			perror("failed to read file (fd)");
			return(-1);}
		return(atoi(str));}
	else{
		return(-1);}}

void
updatePowerPerc(){
	for(int i=0; i < BAT_MAX; i++){
		if( bat[i].exists ){
			bat[i].cache_bat_perc = get_battery_power(i);}}}

void
checkSysDirs(){
	int rescan_class_power=0;
	int d=0;
	for(int i=0; i < BAT_MAX; i++){
		if( bat[i].dir[0] != '\0' )
			d=checkDir(bat[i].dir);
		else
			continue;
		if( d != bat[i].exists ){
			if(d){
				fprintf(stdout, "BAT%d Detected at %s\n", i, bat[i].dir);
				}
			else{
				bat[i].exists=0;
				rescan_class_power=1;
				fprintf(stdout, "BAT%d Missing\n", i);}}}
	/* bakcup, use only when working without acpid */
	if( ! acpid_connected && rescan_class_power){
		fprintf(stdout, "battery disconnected, rescanning power devices\n");
		zero_device_path_names();
		detect_power_supply_class_devices();
		populate_sys_paths();}}


int
get_lid_stat_from_sys(){
	char* strtok_saveptr=NULL;
	char str[32]={0};
	int i=-1;
	int f=open(acpi_lid_path, O_RDONLY);
	char* token=NULL;
	if(access(acpi_lid_path, R_OK)){
		acpi_lid_path_exist=0;
		fprintf(stderr, "Could not find acpi lid. Disabling lid related functionalities. (missing %s)\n", acpi_lid_path);
		return(1);}
	if(f){
		if( read(f, str, 31) <1 ){
			fprintf(stderr, "couldn't read from %s\n", acpi_lid_path);}
		close(f);
		//got the string
		//remove newline
		for(int i=0; str[i]!='\0'; i++){
			if(str[i]=='\n'){
				str[i]='\0';
				break;}}
		token = strtok_r((char*)&str, " ", &strtok_saveptr);
		token = strtok_r(NULL, " ", &strtok_saveptr);
		if(!token) {// second field doesn't exist
			return(1);}

		if(!strncmp("open", token ,10)){
			i=1;}
		else if(!strncmp("closed", token ,10)){
			i=0;}
		else{
			fprintf(stderr, "DEBUG strange LID state \"%s\"\n", token);}
		if(i!=lid_state){
			lid_state_changed=1;
			lid_state=i;}
	return(0);}
	else{
		perror(powerState);}
		return(1);}

void
chown_custom(const char* path, const char* user, const char* group){
	gid_t uid = 0;
	struct passwd* s_passwd = getpwnam( user );
	gid_t gid = 0;
	struct group* s_group = getgrnam( group );
	if(!s_passwd)
		error_errno_msg_exit("user doesn't exist", user);
	uid = s_passwd->pw_uid;
	if(!s_group)
		error_errno_msg_exit("group doesn't exist", group);
	gid = s_group->gr_gid;
	if( chown(path, uid, gid) == -1 )
		error_errno_msg_exit("couldn't set group on file ", path);}

void
daemon_sock_create(){
	int on=1;
	int daemon_sock=0;
	/* check if file exist */
	if(access(daemon_sock_path,F_OK) == 0 )
		error_errno_msg_exit(
			daemon_sock_path,
			"exists, another instance may exists. Exitting");
	
	/* create socket */
	daemon_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if(daemon_sock == -1)
		error_errno_msg_exit("couldn't open socket", daemon_sock_path);
	
	daemon_sock_addr.sun_family=AF_UNIX;

	/* make socket non blocking */
	if( ioctl( daemon_sock, FIONBIO, (char *)&on) < 0 )
		error_errno_msg_exit("ioctl() failed to set socket non blocking","");
	
	strncpy(
		daemon_sock_addr.sun_path,
		daemon_sock_path,
		sizeof(daemon_sock_addr.sun_path)-1);
//	
	if( bind(
			daemon_sock,
			(struct sockaddr *)&daemon_sock_addr,
			sizeof(daemon_sock_addr)
		) == -1 )
		error_errno_msg_exit("couldn't bind socket", daemon_sock_path);
	
	/* set socket permisions */
	if(chmod(daemon_sock_path, 0660)==-1)
		error_errno_msg_exit("chmod failed on socket",NULL);
	
/* don't set group to run rootless in test */
#ifndef DEBUG
	chown_custom(
		daemon_sock_path,
		daemon_socket_user,
		daemon_socket_group);
#endif
	chown_custom(
		daemon_sock_path,
		daemon_socket_user,
		daemon_socket_group);
	
	if( listen(daemon_sock, LISTEN_BACKLOG) == -1 )
		error_errno_msg_exit("listen command failed",NULL);
	fds[D_SOCK].fd=daemon_sock;
	fds[D_SOCK].events=POLLIN;
	nfds++;
	fprintf(stdout, "Created listening socket %s\n", daemon_sock_path);
	}

void
daemon_sock_close(){
	if( fds[D_SOCK].fd == -2)
		return;
	// TODO daemon_send_goodbye_msg_to_clients
	if( shutdown( fds[D_SOCK].fd, SHUT_RDWR) == -1)
		error_errno_msg_exit("couldn't shutdown socket", daemon_sock_path);
	if( close( fds[D_SOCK].fd ))
		error_errno_msg_exit("couldn't close socket", daemon_sock_path);
	fds[D_SOCK].fd = (-1);
	if( ! access(daemon_sock_path,F_OK) && unlink(daemon_sock_path) == -1)
		error_errno_msg_exit("couldn't unlink daemon sock", daemon_sock_path);
	}

void
daemon_adm_sock_create(){
	int on=1;
	int daemon_adm_sock=0;
	/* check if file exist */
	if(access(daemon_adm_sock_path,F_OK) == 0 )
		error_errno_msg_exit(
			daemon_adm_sock_path,
			"exists, another instance may exists. Exitting");
	
	/* create socket */
	daemon_adm_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if(daemon_adm_sock == -1)
		error_errno_msg_exit("couldn't open socket", daemon_adm_sock_path);
	
	daemon_adm_sock_addr.sun_family=AF_UNIX;
	
	/* make socket non blocking */
	if( ioctl( daemon_adm_sock, FIONBIO, (char *)&on) < 0 )
		error_errno_msg_exit("ioctl() failed to set socket non blocking","");
	
	strncpy(
		daemon_adm_sock_addr.sun_path,
		daemon_adm_sock_path,
		sizeof(daemon_adm_sock_addr.sun_path)-1);
	
	if( bind(
			daemon_adm_sock,
			(struct sockaddr *)&daemon_adm_sock_addr,
			sizeof(daemon_adm_sock_addr)
		) == -1 )
		error_errno_msg_exit("couldn't bind socket", daemon_adm_sock_path);
	
	/* set socket permisions */
	if(chmod(daemon_adm_sock_path, 0660)==-1)
		error_errno_msg_exit("chmod failed on socket",NULL);
	
///* don't set group to run rootless in test */
//#ifndef DEBUG
	chown_custom(
		daemon_adm_sock_path,
		daemon_adm_socket_user,
		daemon_adm_socket_group);
//#endif
	
	if( listen(daemon_adm_sock, LISTEN_BACKLOG) == -1 )
		error_errno_msg_exit("listen command failed",NULL);
	fds[D_ADM_SOCK].fd=daemon_adm_sock;
	fds[D_ADM_SOCK].events=POLLIN;
	nfds++;
	fprintf(stdout, "Created listening admin socket %s\n", daemon_adm_sock_path);
	}

void
daemon_adm_sock_close(){
	if( fds[D_ACPID_SOCK].fd == -2)
		return;
	// TODO daemon_send_goodbye_msg_to_clients
	if( shutdown( fds[D_ACPID_SOCK].fd, SHUT_RDWR) == -1)
		error_errno_msg_exit("couldn't shutdown socket", daemon_adm_sock_path);
	if( close( fds[D_ACPID_SOCK].fd))
		error_errno_msg_exit("couldn't close socket", daemon_adm_sock_path);
	fds[D_ACPID_SOCK].fd = (-1);
	if( ! access(daemon_adm_sock_path,F_OK) && unlink(daemon_adm_sock_path) == -1)
		error_errno_msg_exit("couldn't unlink daemon sock", daemon_adm_sock_path);
	nfds--;}


#define DYN_FDS_MIN 3
#define DYN_FDS_MAX 30
int
fds_append(int fd, int event){
	for(int i=DYN_FDS_MIN; i<DYN_FDS_MAX; i++){
		if( fds[i].fd == 0 || fds[i].fd == -1 ){
			fds[i].fd=fd;
			fds[i].events=event;
			fds[i].revents=0;
			nfds++;
			return(0);}}
	return(1);}

int
fds_remove(int fd){
	for(int i=DYN_FDS_MIN; i<DYN_FDS_MAX; i++){
		if( fds[i].fd == fd ){
			fds[i].fd=0;
			fds[i].events=0;
			fds[i].revents=0;
			nfds--;
			return(0);}}
	return(1);}

int
adm_conn_append(int fd){
	for(int i=DYN_FDS_MIN; i<DYN_FDS_MAX; i++){
		if( fd_adm_sock[i] == 0 ){
			fd_adm_sock[i] = fd;
			return(0);}}
	return(1);}

int
adm_conn_check(int fd){
	for(int i=DYN_FDS_MIN; i<DYN_FDS_MAX; i++){
		if( fd_adm_sock[i] == fd )
			return(1);}
	return(0);}

int
adm_conn_remove(int fd){
	for(int i=DYN_FDS_MIN; i<DYN_FDS_MAX; i++){
		if( fd_adm_sock[i] == fd ){
			fd_adm_sock[i] = 0;
			return(0);}}
	return(1);}

int
lstn_cli_append(int fd){
	for(int i=DYN_FDS_MIN; i<DYN_FDS_MAX; i++){
		if( lstng_clients[i] == 0 ){
			lstng_clients[i] = fd;
			return(0);}}
	return(1);}

int
lstn_cli_check(int fd){
	for(int i=DYN_FDS_MIN; i<DYN_FDS_MAX; i++){
		if( lstng_clients[i] == fd )
			return(1);}
	return(0);}

int
lstn_cli_remove(int fd){
	for(int i=DYN_FDS_MIN; i<DYN_FDS_MAX; i++){
		if( lstng_clients[i] == fd ){
			lstng_clients[i] = 0;
			return(0);}}
	return(1);}
int
lstn_cli_iter(){
	if( lstng_clients_pos < DYN_FDS_MAX ) {
		lstng_clients_pos++;
		return( lstng_clients[ lstng_clients_pos ]);}
	lstng_clients_pos++;
	return(-1);}

void
lpmd_clanup(){
	daemon_sock_close();
	daemon_adm_sock_close();
	}

/* signal section */
#include <signal.h>
void
sigterm_handler(int signal, siginfo_t *info, void *_unused){
	fprintf(stderr, "Received SIGTERM from process with pid = %u (%d %p)\n",
		info->si_pid,
		signal,
		_unused);
	lpmd_clanup();
	exit(0);}
void
sigint_handler(int signal, siginfo_t *info, void *_unused){
	fprintf(stderr, "Received SIGINT from process with pid = %u (%d %p)\n",
		info->si_pid,
		signal,
		_unused);
	lpmd_clanup();
	exit(0);}

void
setup_sigaction(){
	struct sigaction action = { 0 };
	action.sa_flags = SA_SIGINFO;
	action.sa_sigaction = &sigterm_handler;
	if(sigaction(SIGTERM, &action, NULL) == -1)
		error_errno_msg_exit("sigaction failed",NULL);
	action.sa_sigaction = &sigint_handler;
	if(sigaction(SIGINT, &action, NULL) == -1)
		error_errno_msg_exit("sigaction failed",NULL);
		}
/* END signal section */

void
_setGovernor(int governor){
	const char* gov=NULL;
	char path[SSTR]={0};
	if(manageGovernors){
		if(intel_pstate_present &&
			(governor!=GOV_PERFORMANCE && governor!=GOV_POWERSAVE)){
			
			fprintf(stderr, "intel_pstate does not support \"%s\" governor\n",
				governor_string[governor]);
			fprintf(stderr, "not changing governor\n");
				return;}
		switch(governor){
			case GOV_POWERSAVE:
				gov = governor_string[ GOV_POWERSAVE ];
				break;
			case GOV_PERFORMANCE:
				gov = governor_string[ GOV_PERFORMANCE ];
				break;
			default:
				fprintf(stderr,
					"ERR wrong governor type, not switching\n");
				return;}
		for(int i=0; i<numberOfCores; i++){
			sprintf( (char*)&path, 
				"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
			strToFile((char*)&path, (char*)gov);}
		/* log governor change */
		fprintf(stdout, "cpu governor changed to \'%s\'\n", gov);}
	else
		fprintf(stderr, 
			"not switching cpu governor, governor management disabled\n");}

void
setGovernor(int governor){
	if( user_lock_governor )
		fprintf(stderr, 
			"user locked governor at %s, not changing current governor\n",
			governor_string[ current_governor ]);
	else{
		_setGovernor(governor);
		current_governor = governor;
	}
}

void
cpu_boost_control(int gov_id){
	int no_boost=-1; /* /sys/devices/system/cpu/intel_pstate/no_turbo */
	if( user_lock_cpu_boost )
		fprintf(stderr, 
			"user locked cpu boost as %s, not changing current boost setting\n",
			current_cpu_boost ? "enabled" : "disabled"
			);
		
	if(intel_pstate_present){
		switch( gov_id ){
			case CPU_BOOST_OFF:
				no_boost=1;
				fprintf(stdout, "disabling cpu boost via intel_pstate\n");
				break;
			case CPU_BOOST_ON:
				no_boost=0;
				fprintf(stdout, "enabling cpu boost via intel_pstate\n");
				break;
			default:
				fprintf(stderr,
					"ERR %s wrong argument %d\n", __func__, gov_id);
				return;}
		if( !intToFile_check(intel_pstate_turbo_path, no_boost) )
			current_cpu_boost = gov_id;}}

void
restore_auto_cpu_power_management(){
	user_lock_governor = 0;
	user_lock_cpu_boost = 0;
	if( chargerConnected ){
		setGovernor( GOV_PERFORMANCE );
		cpu_boost_control( CPU_BOOST_ON );}
	else{
		setGovernor( GOV_POWERSAVE );
		cpu_boost_control( CPU_BOOST_OFF );}
	}


void
wall(const char* message){
#ifndef NO_WALL_MESS
	DIR* d = opendir("/dev/pts/");
	struct dirent *dirent;
	char path[512]={0}; /* size is way to big because of a warning */
	if(d){
		while((dirent = readdir(d)) != NULL){
			if(*dirent->d_name >= '0' && *dirent->d_name <= '9'){
				snprintf((char*)&path, 512, "%s%s", pttyDir, dirent->d_name);
				strToFile((char*)&path, (char*)message);}}}
		closedir(d);}
#endif
#ifdef NO_WALL_MESS
	return;}
#endif

void
setThresholds_default(){
	fprintf(stdout, "Setting charge thresholds\n");
	for( int i=0; i < BAT_MAX; i++){
		if( bat[i].exists && bat[i].has_thresholds ){
			intToFile(bat[i].charge_start_threshold, bat[i].config_charge_start_threshold);
			intToFile(bat[i].charge_stop_threshold,  bat[i].config_charge_stop_threshold);}}}

void
setThresholds_zero(){
	fprintf(stdout, "Setting charge thresholds\n");
	for( int i=0; i < BAT_MAX; i++){
		if( bat[i].exists && bat[i].has_thresholds ){
			intToFile(bat[i].charge_start_threshold, 0);
			intToFile(bat[i].charge_stop_threshold, 100);}}}

void
chargerChangedState(){
	if(chargerConnected)
		action_charger_connected();
	else
		action_charger_disconnected();}

void
lid_state_handler(){
	if(!acpid_connected && acpi_lid_path_exist ){
		// acpid connection lost 
		// falling back to checing via /sys path
		get_lid_stat_from_sys();}
		
	if(lid_state_changed){
		lid_state_changed=0;
		if(lid_state==1)
			action_lid_open();
		else if(lid_state==0)
			action_lid_close();}}

void
print_time_no_newline(){
	time_t timer;
	char buffer[26];
	struct tm* tm_info;
	timer = time(NULL);
	tm_info = localtime(&timer);
	strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
	fprintf(stderr, "%s ", buffer);}

void
updateChargerState(){
	/* always check if charger is connected */
	/* lpmd may miss acpid event if charger was connected while system was suspended */
	/* TODO detect if system comes out of suspend state??? */
	int c;
	/* check if system has detected charger status file */
	if(chargerConnectedPath[0]){
		c=fileToint(chargerConnectedPath);
		if(c != chargerConnected){
			chargerConnected=c;
			chargerChangedState();}}}

void
initial_charger_state_setup(){
	if( chargerConnectedPath[0] == '\0' ){
		/* if lpmd didn't detect file with charger state */
		/*  */
		chargerConnected = false;
		/* enable performance modes */
		setGovernor(GOV_PERFORMANCE);
		cpu_boost_control(CPU_BOOST_ON);}}

void
battery_low_suspend(){
		fprintf(stderr, "Battery power low and not charging. Suspending system to mem in %d seconds.\n", suspendDelay);
		sleep( suspendDelay );
		updateChargerState();
		if( chargerConnected ){
			fprintf(stderr, "Charger connected, canceling low power suspend.\n");
			return;}
		wall(wallSuspendWarning);
		suspend();}


int
connect_to_acpid(){
	memset(buf,0,BUF_SIZE);
	if( (fd_acpid=socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ){
		perror("socket open");
		return(1);}
	memset(&acpid_sock_addr, 0, sizeof(acpid_sock_addr));
	acpid_sock_addr.sun_family = AF_UNIX;
	strncpy(
		acpid_sock_addr.sun_path,
		acpid_sock_path,
		sizeof(acpid_sock_addr.sun_path)-1);
	if(connect(
		fd_acpid, 
		(struct sockaddr *)&acpid_sock_addr, 
		sizeof(struct sockaddr_un)) == -1){
		
		perror("acpid socket connect");
		return(1);}
	fprintf(stdout, "connected to acpid at %s\n", acpid_sock_path);
	acpid_connected=1;
	fds[D_ACPID_SOCK].fd=fd_acpid;
	fds[D_ACPID_SOCK].events=POLLIN;
	nfds++;
	return(0);}

void
sock_print(int fd){
	int numRead=-1;
	if( (numRead = read(fd, buf, BUF_SIZE)) > 0) {
		//buf[ BUF_SIZE - 1 ] = '\0'; //just to be sure
		if( write(STDIN_FILENO, buf, numRead) != numRead) {
			fprintf(stderr,"partial/failed write");}}}

void
basic_send_msg(int conn_fd, const char* msg){
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
send_msg_to_listening_lpmctl(const char* msg){
	int fd=-1;
	lstng_clients_pos=0;
	while( (fd = lstn_cli_iter()) != -1 ){
		if( fd ){
			printf("sending message to lpmctl at %d fd\n", fd);
			basic_send_msg( fd, msg);}}}


/* TODO this whole functionality requires separate thread */
void
react_to_session_lock_result(){
	if( waiting_for_session_lock_before_suspend ){
		waiting_for_session_lock_before_suspend=0;
		suspend();}
	if( waiting_for_session_lock_before_hibernate ){
		waiting_for_session_lock_before_hibernate=0;
		hibernate();}
}

void
handle_clients(int i){
	int numRead=-1;
	int fd = fds[i].fd;
	if( (numRead = read(fd, buf, BUF_SIZE)) > 0) {
		//buf[ BUF_SIZE - 1 ] = '\0'; //just to be sure
		for( int i=0; i<MSG_MAX_LEN; i++ ){
			if( buf[i] == '\n' )
				buf[i] = '\0';}}
		if( !strncmp( buf, client_suspend_ask, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT ASKED FOR SUSPEND\n");
			if( !adm_conn_check(fd) ){
				basic_send_msg(fds[i].fd, daemon_action_refuse);}
			else{
				basic_send_msg(fds[i].fd, daemon_action_success);}
				if(require_session_locks){
					send_msg_to_listening_lpmctl(daemon_ask_for_screen_locks);
					waiting_for_session_lock_before_suspend=1;
					waiting_for_session_lock_before_hibernate=0;}
				else
					suspend();
				}
		if( !strncmp( buf, client_hibernate_ask, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT ASKED FOR HINERNATION\n");
			if( !adm_conn_check(fd) ){
				basic_send_msg(fds[i].fd, daemon_action_refuse);
				}
			else{
				basic_send_msg(fds[i].fd, daemon_action_success);}
				if( require_session_locks ){
					send_msg_to_listening_lpmctl(daemon_ask_for_screen_locks);
					waiting_for_session_lock_before_suspend=0;
					waiting_for_session_lock_before_hibernate=1;}
				else
					hibernate();
				}
		if( !strncmp( buf, client_notify_daemon_about_idle, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT NOTIFIED ABOUT IDLE DESKTOP\n");
			basic_send_msg(fds[i].fd, daemon_action_success);
			//TODO do something
			}
		if( !strncmp( buf, client_lock_success, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT SUCCESSFULY LOCKED IT'S SESSION\n");
			basic_send_msg(fds[i].fd, daemon_action_success);
			//TODO do something
			react_to_session_lock_result();
			}
		if( !strncmp( buf, client_lock_fail, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT FAILED TO LOCKED IT'S SESSION\n");
			basic_send_msg(fds[i].fd, daemon_action_success);
			//TODO do something
			react_to_session_lock_result();
			}
		if( !strncmp( buf, client_lock_ask, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT ASKED TO LOCK ALL SESSIONS\n");
			basic_send_msg(fds[i].fd, daemon_action_success);
			send_msg_to_listening_lpmctl( daemon_ask_for_screen_locks );
			//TODO do something
			}
		if( !strncmp( buf, client_lock_all_ask, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT ASKED TO LOCK ALL SESSIONS\n");
			basic_send_msg(fds[i].fd, daemon_action_success);
			send_msg_to_listening_lpmctl( daemon_ask_for_screen_locks );
			//TODO do something
			}
		if( !strncmp( buf, client_listen_to_daemon, MSG_MAX_LEN)){
			fprintf(stderr, "LISTENING CLIENT CONNECTED\n");
			//basic_send_msg(fds[i].fd, daemon_action_success);
			lstn_cli_append(fd);
			}
		if( !strncmp( buf, client_ask_zero_bat_thresholds, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT ASKED TO ZERO BATTERY THRESHOLDS\n");
			setThresholds_zero();
			/* TODO validate if action was successful */
			basic_send_msg(fds[i].fd, daemon_action_success);
			}
		if( !strncmp( buf, client_ask_restore_def_bat_thresholds, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT ASKED TO RESTORE DEFAULT BATTERY THRESHOLDS\n");
			setThresholds_default();
			/* TODO validate if action was successful */
			basic_send_msg(fds[i].fd, daemon_action_success);
			}
		if( !strncmp( buf, client_ask_for_powersave_gov_lock, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT ASKED TO LOCK CPU GOVERNOR IN POWERSAVE MODE\n");
			user_lock_governor = 0;
			user_lock_cpu_boost = 0;
			setGovernor( GOV_POWERSAVE );
			cpu_boost_control( CPU_BOOST_OFF );
			user_lock_governor = 1;
			user_lock_cpu_boost = 1;
			/* TODO validate if action was successful */
			basic_send_msg(fds[i].fd, daemon_action_success);
			}
		if( !strncmp( buf, client_ask_for_performance_gov_lock, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT ASKED TO LOCK CPU GOVERNOR IN PERFORMANCE MODE\n");
			user_lock_governor = 0;
			user_lock_cpu_boost = 0;
			setGovernor( GOV_PERFORMANCE );
			cpu_boost_control( CPU_BOOST_ON );
			user_lock_governor = 1;
			user_lock_cpu_boost = 1;
			/* TODO validate if action was successful */
			basic_send_msg(fds[i].fd, daemon_action_success);
			}
		if( !strncmp( buf, client_ask_for_automatic_governor_control, MSG_MAX_LEN)){
			fprintf(stderr, "CLIENT ASKED TO RESTORE AUTOMATIC CPU GOVERNOR MODE\n");
			//user_lock_governor = 0;
			//user_lock_cpu_boost = 0;
			restore_auto_cpu_power_management();
			/* TODO validate if action was successful */
			basic_send_msg(fds[i].fd, daemon_action_success);
			}

	memset(buf,0,BUF_SIZE);}

#define ACPID_EV_MAX 5
void
acpi_handle_events(char acpidEvent[ ACPID_EV_MAX ][ ACPID_STRCMP_MAX_LEN ]){
		int i=0;
#ifdef DEBUG
		fprintf(stderr, "DEBUG acpi_handle_events() handles %s\n", acpidEvent[0]);
#endif
		//for(int i=0; i<ACPID_EV_MAX; i++){
		if(!strncmp("ac_adapter", acpidEvent[0] ,ACPID_STRCMP_MAX_LEN )){
			i=atoi(acpidEvent[3]);
#ifdef DEBUG
			fprintf(stderr, "charger state from acpid %d\n", i);
#endif
			if(i != chargerConnected){
				chargerConnected=i;
				chargerChangedState();}}
		if( !strncmp("button/lid", acpidEvent[0] ,ACPID_STRCMP_MAX_LEN ) && 
			!strncmp("LID", acpidEvent[1] ,ACPID_STRCMP_MAX_LEN )){
			if(!strncmp("open", acpidEvent[2] ,ACPID_STRCMP_MAX_LEN )){
				i=1;}
			if(!strncmp("close", acpidEvent[2] ,ACPID_STRCMP_MAX_LEN)){
				i=0;}
			if(i!=lid_state){
				lid_state_changed=1;}
				fprintf(stderr, "lid state changed\n");
			lid_state=i;}
		if( !strncmp("battery", acpidEvent[0] ,ACPID_STRCMP_MAX_LEN )){ 
			/* handle battery disconnect */
			/* these values work on lenovo thinkpad x270 */
			if( !strncmp("00000080", acpidEvent[2] ,ACPID_STRCMP_MAX_LEN ) &&
			!strncmp("00000001", acpidEvent[3] ,ACPID_STRCMP_MAX_LEN )){
				fprintf(stderr, "battery state channge, rescanning power devices\n");
				zero_device_path_names();
				detect_power_supply_class_devices();
				populate_sys_paths();}
			/* handle battery connection */
			/* these values work on lenovo thinkpad x270 */
			if( !strncmp("00000003", acpidEvent[2] ,ACPID_STRCMP_MAX_LEN ) &&
			!strncmp("00000000", acpidEvent[3] ,ACPID_STRCMP_MAX_LEN )){
				fprintf(stderr, "battery disconnected, rescanning power devices\n");
				zero_device_path_names();
				detect_power_supply_class_devices();
				populate_sys_paths();}
				}}

int
handle_read_from_acpid_sock(){
	int numRead=-2;
	char* strtok_saveptr=NULL;
	char acpidEvent[ ACPID_EV_MAX ][ ACPID_STRCMP_MAX_LEN ]={0};
	char* token=NULL;
	if((numRead = read(fd_acpid, buf, BUF_SIZE)) > 0){
		//buf[ BUF_SIZE -1 ] = '\0'; //just to be sure
		for(int i=0; buf[i]!='\0'; i++){
			if(buf[i]=='\n')
				buf[i]='\0';}
#ifdef DEBUG
		fprintf(stderr, "acpid event\n");
#endif
		memset(acpidEvent, 0, sizeof(acpidEvent));
		token = strtok_r(buf, " ", &strtok_saveptr);
		for(int i=0; (i<ACPID_EV_MAX)&&(token); i++){
			strncpy(acpidEvent[i], token, ACPID_STRCMP_MAX_LEN-1);
#ifdef DEBUG
			fprintf(stderr, "\t%d \"%s\" len:%lo\n",i,
				acpidEvent[i],
				strnlen(acpidEvent[i],ACPID_STRCMP_MAX_LEN));
#endif
			token = strtok_r(NULL, " ", &strtok_saveptr);}
		acpi_handle_events(acpidEvent);}
	else if( numRead <= 0 ){
		if(acpid_connected){
			fds[D_ACPID_SOCK].fd=0;
			fds[D_ACPID_SOCK].events=0;
			nfds--;
			fprintf(stderr, "lost connection to acpid\n");}
		if(close(fd_acpid)){
			perror("socket close error");}
		acpid_connected=0;
		fd_acpid=-1;}
	return(numRead);}


void
accept_connection(int fd, int adm){
	int new_fd=0;
	if( (new_fd = accept(fd, NULL, NULL)) < 0 ){
		if(errno != EWOULDBLOCK)
			error_errno_msg_exit("connection accept() failed","");
		fprintf(stderr, "ERR failed to accept() connetion %d\n", new_fd);
		return;}
	if( fds_append( new_fd, POLLIN )){
		fprintf(stderr, "ERR failed to accept() connetion FDS full\n");
		return;}
//	struct ucred ucred;
//	int len=sizeof(ucred);
	if( adm )
		adm_conn_append( new_fd );}

void
cleanup_connection(int fd_pos){
	close(fds[fd_pos].fd);
	adm_conn_remove( fds[fd_pos].fd );
	lstn_cli_remove( fds[fd_pos].fd );
	fds[fd_pos].fd=-1;
	fds[fd_pos].events=0;
	fds[fd_pos].revents=0;
	nfds--;}

void
zero_fds(){
	for( int i=DYN_FDS_MIN; i<=DYN_FDS_MAX; i++ ){
		fds[i].fd=-1;}}

void
poll_fds(){
	int rc=-1;
	int current_nfds=0;
//	if( nfds == 0 ) return; // there are no active FDs
//	if( nfds <= 0 ) {
//		fprintf(stderr, "ERROR nfds value is negative. exitting\n");
//		exit(EXIT_FAILURE);}
//#ifdef DEBUG
//	printf("running poll() nfds size %d\n", nfds);
//#endif
	current_nfds = c_nfds;
	rc = poll(fds, c_nfds, poll_timeout);
	switch(rc){
	case(0): // timeout
#ifdef DEBUG
		fprintf(stderr, "DEBUG poll timeout\n");
#endif
		return;
	case(-1): //error
#ifdef DEBUG
		fprintf(stderr, "DEBUG poll error\n");
#endif
		break;
	default:
#ifdef DEBUG
		fprintf(stderr, "DEBUG poll rw\n");
#endif
		for(int i=0; i < current_nfds ;i++){
			if( fds[i].revents == 0 )
				continue;
			if( (fds[i].revents & POLLHUP)==POLLHUP || (fds[i].revents & POLLERR)==POLLERR ){
#ifdef DEBUG
				printf("lost connection on %d fd, arrpos %d, rev %d\n",
					fds[i].fd, i, fds[i].revents);
#endif
				cleanup_connection(i);
				continue;}
			if( fds[i].revents != POLLIN ){
				printf("ERROR %d %08x fd %d\n", i, fds[i].revents ,fds[i].fd);
				if((fds[i].revents & POLLERR) == POLLERR){
					printf("POLLERR\n");}
				}
			/* handle messages from acpid */
			if( i == D_ACPID_SOCK && fds[i].revents == POLLIN ) {
				handle_read_from_acpid_sock();
				continue;}
			/* handle ctrl socket incomming connections */
			if( i == D_ADM_SOCK || i == D_SOCK ){
				if( fds[i].revents == POLLIN ){
					accept_connection( fds[i].fd, i == D_ADM_SOCK ? 1:0 );
					continue;}}
			/* handle connection to lpmctl */
			if( i>=DYN_FDS_MIN && i<=DYN_FDS_MAX && fds[i].events == POLLIN ){
#ifdef DEBUG
				printf("  fd=%d; revents: %s%s%s\n", fds[i].fd,
					(fds[i].revents & POLLIN) ==POLLIN  ? "POLLIN "  : "",
					(fds[i].revents & POLLHUP)==POLLHUP ? "POLLHUP " : "",
					(fds[i].revents & POLLERR)==POLLERR ? "POLLERR " : "");
#endif
				if( (fds[i].revents & POLLIN) == POLLIN ){
#ifdef DEBUG
					printf("something on %d arrpos %d\n", fds[i].fd, i);
#endif
					//sock_print(fds[i].fd);
					handle_clients(i);
					if( (fds[i].revents & POLLHUP) == POLLHUP ){
#ifdef DEBUG
						printf("POLLHUP on %d arrpos %d\n", fds[i].fd, i);
#endif
						cleanup_connection(i);}
					continue;}
					}
		}
	}
	memset(buf, 0, sizeof(buf));
}

void
reconnect_to_acpid(){
	if(!acpid_connected && !access(acpid_sock_path, R_OK)){
		connect_to_acpid();}}

void
checkForLowPower(){
	char batteries=0;
	char low=0;
	char low_warn=0;
	if( ! chargerConnected ){
		for( int i=0; i < BAT_MAX; i++){
			if( bat[i].exists ){
				bat[i].low = bat[i].cache_bat_perc < batMinSleepThreshold;
				bat[i].low_warn = bat[i].cache_bat_perc < batLowWarningThreshold;
				batteries++;
				low += bat[i].low;
				low_warn += bat[i].low_warn;}}
		if( batteries == 0 ) /* skip if system has no batteries */
			return;
		if( low == batteries ){
#ifdef DEBUG
			fprintf(stderr, "debug checkForLowPower(): batteries %d low %d low_warn %d chargerConnected %d\n",
				batteries,
				low,
				low_warn,
				chargerConnected);
#endif
			battery_low_suspend();
			return;}
		if( low_warn == batteries && ! lowBatWarning_warned ){
			wall( wallLowBatWarning);
			lowBatWarning_warned = 1;}}}

void
debug_dump_batteries(){
	char batteries=0;
	char low=0;
	char low_warn=0;
	for( int i=0; i < BAT_MAX; i++){
		if( bat[i].exists ){
			bat[i].low = bat[i].cache_bat_perc < batMinSleepThreshold;
			bat[i].low_warn = bat[i].cache_bat_perc < batLowWarningThreshold;
			batteries++;
			low += bat[i].low;
			low_warn += bat[i].low_warn;}}
	print_time_no_newline();
	fprintf(stderr, "bat0 %f bat1 %f ", get_battery_power(0), bat[1].cache_bat_perc);
	fprintf(stderr, "debug debug_dump_batteries(): batteries %d low %d low_warn %d\n",
		batteries,
		low,
		low_warn);}
	


int
main(){
	if(!checkDir(power_supply_class))
		exit(1);
	if(checkFile(powerState)){
		powerStateExists=0;}
	fprintf(stdout, "Lpmd starts\n");
	detectNumberOfCpus();
	detect_power_supply_class_devices();
	populate_sys_paths();
	checkSysDirs();
	detect_intel_pstate();
	setThresholds_default();
	//chargerConnected=fileToint(chargerConnectedPath);
	get_lid_stat_from_sys();
	initial_charger_state_setup(); /* check if system has charger at all */
	updateChargerState(); /* detect charger before acpid is connected */
	reconnect_to_acpid();
	zero_fds();
	daemon_sock_create();
	daemon_adm_sock_create();
	setup_sigaction();
#ifdef DEBUG
		for(int i=0; i<DEBUG_CYCLES; i++){
			fprintf(stderr, "\n\nDEBUG main loop start\n");
			fprintf(stderr,"nfds %d\n", nfds);
#else
		for(;;){
#endif
			checkSysDirs();
			reconnect_to_acpid();
			poll_fds();
			lid_state_handler();
			updatePowerPerc();
			updateChargerState();
			checkForLowPower();
			//puts("DEBUG main loop end\n");
#ifdef DEBUG
			print_time_no_newline();
			if(bat[1].exists)
				fprintf(stderr, "bat0 %f bat1 %f", get_battery_power(0), bat[1].cache_bat_perc);
			else
				fprintf(stderr, "bat0 %f",bat[0].cache_bat_perc);
			fprintf(stderr, "\n");
			debug_dump_batteries();
#endif
			if(!acpid_connected)
				sleep(loopInterval);
			/* these ↓ flush buffers to avoid delays 
			 * while sending messages to logger in openrc */
			fflush(stdout);
			fflush(stderr);
			}
	return(0);}
