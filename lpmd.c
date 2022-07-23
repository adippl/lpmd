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

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define SSTR 64

#ifndef DEBUG_CYCLES
	#define DEBUG_CYCLES 360
#endif

/* config section */
#ifndef ENERGY_FULL
	#define ENERGY_FULL
#endif
const int bat0ThrStrtVal=45;
const int bat0ThrStopVal=75;
const int bat1ThrStrtVal=45;
const int bat1ThrStopVal=75;
const float batMinSleepThreshold=0.15f;
const float batLowWarningThreshold=0.25f;
const int loopInterval=1;
const int manageGovernors=1;
const char* powersaveGovernor="powersave";
const char* performanceGovernor="performance";
const int syncBeforeSuspend=1;
const int suspendDelay=60;
const int wallNotify=1;
const char* wallSuspendWarning="WARNING!!!\nWARNING!!!  battery0 is low.\nWARNING!!!  Syncing filesystem and suspending to mem in 15 seconds...\n";
const char* wallLowBatWarning="WARNING!!!\nWARNING!!!  battery0 is below 25%\n";
/* end of config */

const char* powerDir="/sys/class/power_supply";
const char* bat0Dir="/sys/class/power_supply/BAT0";
const char* bat1Dir="/sys/class/power_supply/BAT1";
const char* bat0ThrStrt="/sys/class/power_supply/BAT0/charge_start_threshold";
const char* bat0ThrStop="/sys/class/power_supply/BAT0/charge_stop_threshold";
const char* bat1ThrStrt="/sys/class/power_supply/BAT1/charge_start_threshold";
const char* bat1ThrStop="/sys/class/power_supply/BAT1/charge_stop_threshold";
#ifndef ENERGY_FULL_DESIGN
const char* bat0EnFull="/sys/class/power_supply/BAT0/energy_full";
const char* bat1EnFull="/sys/class/power_supply/BAT1/energy_full";
#else
const char* bat0EnFull="/sys/class/power_supply/BAT0/energy_full_design";
const char* bat1EnFull="/sys/class/power_supply/BAT1/energy_full_design";
#endif
const char* bat0EnNow="/sys/class/power_supply/BAT0/energy_now";
const char* bat1EnNow="/sys/class/power_supply/BAT1/energy_now";
const char* chargerConnectedPath="/sys/class/power_supply/AC/online";
const char* powerState="/sys/power/state";
const char* pttyDir="/dev/pts/";
const char* cpupresetPath="/sys/devices/system/cpu/present";

int powerStateExists=1;
int bat0Exists=-1;
int bat1Exists=-1;
int bat0HasThresholds=0;
int bat1HasThresholds=0;
int bat0maxCharge=0;
int bat1maxCharge=0;

float bat0charge=0.0;
float bat1charge=0.0;
/* float bat0chargeDelta=0.0; */
/* float bat1chargeDelta=0.0; */
int chargerConnected=-1;
int lowBatWarning_warned=0;
int numberOfCores=4;

int acpid_connected=0;
int lid_state=0;
int lid_state_changed=1;

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
		//perror(path);
		return(0);}
	else{
		perror(path);
		return(0);}}

int
fileToint(const char* path){
	int i=0;
	FILE* file=fopen(path, "r");
	if(file){
		if(fscanf(file, "%d", &i)==1){
			fclose(file);
			return(i);}
		else{
			fclose(file);
			return(-1);}}
	return(-1);}


void
intToFile(const char* path, int i){
	FILE* f=fopen(path, "w");
	if(f){
		fprintf(f, "%d", i);
		fclose(f);}
	else{
		perror(path);}}

void
strToFile(const char* path, char* str){
	FILE* f=fopen(path, "w");
	if(f){
		fputs(str, f);
		fclose(f);}
	else{
		perror(path);}}

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
		fprintf(stderr, "Found %d core cpu\n", numberOfCores);
		fclose(f);}
	else
		perror(cpupresetPath);}


void
checkSysDirs(){
	int i=checkDir(bat0Dir);
	if(i!=bat0Exists){
		if(i){
			bat0Exists=1;
			fprintf(stderr, "BAT0 Detected\n");
			bat0maxCharge=fileToint(bat0EnFull);
			if(checkFile(bat0ThrStrt) && checkFile(bat0ThrStop)){
				bat0HasThresholds=1;}}
		else{
			bat0Exists=0;
			bat0HasThresholds=0;
			fprintf(stderr, "BAT0 Missing\n");}}
	
	i=checkDir(bat1Dir);
	if(i!=bat1Exists){
		if(i){
			bat1Exists=1;
			bat1maxCharge=fileToint(bat1EnFull);
			fprintf(stderr, "BAT1 Detected\n");
			if(checkFile(bat1ThrStrt) &&  checkFile(bat1ThrStop)){
				bat1HasThresholds=1;}}
		else{
			bat1Exists=0;
			bat1HasThresholds=0;
			fprintf(stderr, "BAT1 Missing\n");}}}

void
updatePowerPerc(){
	int a=0;
	if(bat0Exists){
		a=fileToint(bat0EnNow);
		bat0charge=(float)a/bat0maxCharge;}
	if(bat1Exists){
		a=fileToint(bat1EnNow);
		bat1charge=(float)a/bat0maxCharge;}}

void
changeGovernor(){
	const char* gov=NULL;
	char path[SSTR]={0};
	if(manageGovernors){
		switch(chargerConnected){
			case 0:
				gov=powersaveGovernor;
				break;
			case 1:
				gov=performanceGovernor;
				break;
			default:
				fprintf(stderr,"ERR changeGovernor inconsistent chargerConnected\n");
				return;}
		for(int i=0; i<numberOfCores; i++){
			sprintf((char*)&path, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
			strToFile((char*)&path, (char*)gov);}}}

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
setThresholds(){
	fprintf(stderr, "Setting charge thresholds\n");
	if(bat0HasThresholds){
		intToFile(bat0ThrStrt, bat0ThrStrtVal);
		intToFile(bat0ThrStop, bat0ThrStopVal);}
	if(bat1HasThresholds){
		intToFile(bat1ThrStrt, bat1ThrStrtVal);
		intToFile(bat1ThrStop, bat1ThrStopVal);}}

void
chargerChangedState(){
	changeGovernor();
	if(chargerConnected){
		fprintf(stderr, "Charger connected\n");}
	else{
		fprintf(stderr, "Charger disconnected\n");
		lowBatWarning_warned=0;}}

void
lid_state_handler(){
	puts("awdawdwadwadaw");
	if(lid_state_changed){
	puts("AWDAWDWADWADAW");
		lid_state_changed=0;
		if(lid_state==1){
			fprintf(stderr, "Lid opened\n");
			//TODO Lid opening action
			}
		else if(lid_state==0) {
			fprintf(stderr, "Lid closed\n");
			//TODO lid closing action
			}}}

void
updateChargerState(){
	if(acpid_connected) return; // skip primitive check if acpid is avaliable
#ifdef DEBUG
	puts(" updateChargerState ");
#endif
	int c=fileToint(chargerConnectedPath);
	if(c != chargerConnected){
		chargerConnected=c;
		chargerChangedState();}}

void
suspend(){
#ifdef DEBUG
		fprintf(stderr , "THIS IS DEBUG MODE, lpm WON't put this machine to sleep. Compile in normal mode to enable this functionality\n");
		return;}
#endif
#ifndef DEBUG
		fprintf(stderr, "BAT0 power low at %d and not charging. Suspending system to mem in %d seconds.\n", (int)bat0charge, suspendDelay);
		wall(wallSuspendWarning);
		if(syncBeforeSuspend)
			sync();
		FILE* f=fopen(powerState, "w");
		if(f){
			fprintf(f, "mem");
			fclose(f);}
		else{
			perror(powerState);}}
#endif


#define BUF_SIZE 512
//char acpid_sock_path[]="/var/run/acpid.socket";
const char acpid_sock_path[]="/run/acpid.socket";
char buf[BUF_SIZE];
int fd_acpid=-1;
struct sockaddr_un acpid_sock_addr;
#define ACPID_STRCMP_MAX_LEN 32 
int
connect_to_acpid(){
	memset(buf,0,BUF_SIZE);
	// fd alredy set, skipping setup because we're probably reconnecting
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
	acpid_connected=1;
	return(0);}
void
sock_print(){
	int numRead=-1;
	while ((numRead = read(fd_acpid, buf, BUF_SIZE)) > 0) {
		if (write(STDIN_FILENO, buf, numRead) != numRead) {
		  fprintf(stderr,"partial/failed write");
		}
	}
}
#define ACPID_EV_MAX 5
void
specific_events(char acpidEvent[ ACPID_EV_MAX ][ ACPID_STRCMP_MAX_LEN ]){
		int i=0;
		puts(acpidEvent[0]);
		//for(int i=0; i<ACPID_EV_MAX; i++){
		if(!strncmp("ac_adapter", acpidEvent[0] ,ACPID_STRCMP_MAX_LEN-1 )){
			i=atoi(acpidEvent[3]);
			printf("charger state %d\n", i);
			if(i != chargerConnected){
				chargerConnected=1;
				chargerChangedState();}}
		if( !strncmp("button/lid", acpidEvent[0] ,ACPID_STRCMP_MAX_LEN-1 ) && 
			!strncmp("LID", acpidEvent[1] ,ACPID_STRCMP_MAX_LEN-1 )){
			
			puts(acpidEvent[1]);
			puts(acpidEvent[2]);
			if(!strncmp("open", acpidEvent[2] ,ACPID_STRCMP_MAX_LEN-1 )){
				i=1;}
			if(!strncmp("close", acpidEvent[2] ,ACPID_STRCMP_MAX_LEN-1 )){
				i=0;}
			if(i!=lid_state){
				lid_state_changed=1;}
				puts("lid state changed");
			lid_state=i;}}

void
handle_acpid_events(){
	int numRead=-1;
	char* token=NULL;
	char* p_buf=buf;
	char acpidEvent[ ACPID_EV_MAX ][ ACPID_STRCMP_MAX_LEN ]={0};
	if(fd_acpid==-1) return; // exit if fd is unset
	while ((numRead = read(fd_acpid, buf, BUF_SIZE)) > 0){
		//debug
		//if (write(STDIN_FILENO, buf, numRead) != numRead) {
		//	fprintf(stderr,"partial/failed write");
		//}
		//end debug
		for(int i=0; buf[i]!='\0'; i++){
			if(buf[i]=='\n')
				buf[i]='\0';}
		memset(acpidEvent, 0, sizeof(acpidEvent));
		token = strtok_r(buf, " ", &p_buf);
		for(int i=0; (i<ACPID_EV_MAX)&&(token); i++){
			strncpy(acpidEvent[i], token, ACPID_STRCMP_MAX_LEN-1);
			printf("%d \"%s\" len:%lo\n",i,
				acpidEvent[i],
				strnlen(acpidEvent[i],ACPID_STRCMP_MAX_LEN));
			token = strtok_r(NULL, " ", &p_buf);}
		specific_events(acpidEvent); }
	if( numRead == -1 || numRead == 0 ){
		if(acpid_connected)
			fprintf(stderr, "lost connection to acpid\n");
		if(close(fd_acpid)){
			perror("socket close error");}
		acpid_connected=0;
		fd_acpid=-1;}
	memset(buf, 0, sizeof(buf));
}
void
reconnect_to_acpid(){
	if(!acpid_connected && !access(acpid_sock_path, R_OK)){
		connect_to_acpid();}}


void
checkForLowPower(){
	int bat0low=bat0charge<batMinSleepThreshold;
	int bat1low=bat1charge<batMinSleepThreshold;
	int bat0lowWarn=bat0charge<batLowWarningThreshold;
	int bat1lowWarn=bat1charge<batLowWarningThreshold;
	if(!chargerConnected){
		if( (bat0Exists&&bat0low)||(bat1Exists&&bat0low&&bat1low)){
			
			suspend();
			return;}}
		if(((bat0Exists&&bat0lowWarn)||\
			(bat1Exists&&bat0lowWarn&&bat1lowWarn))&&\
			!lowBatWarning_warned){
			
			wall(wallLowBatWarning);
			lowBatWarning_warned=1;}}

int
main(){
	if(!checkDir(powerDir))
		exit(1);
	if(checkFile(powerState)){
		powerStateExists=0;}
	fprintf(stderr, "Lpmd starts\n");
	detectNumberOfCpus();
	//connect_to_acpid();
	reconnect_to_acpid();
	checkSysDirs();
	setThresholds();
	chargerConnected=fileToint(chargerConnectedPath);
#ifdef DEBUG
		for(int i=0; i<DEBUG_CYCLES; i++){
#else
		for(;;){
#endif
			checkSysDirs();
			//sock_print();
			reconnect_to_acpid();
			handle_acpid_events();
			lid_state_handler();
			updatePowerPerc();
			reconnect_to_acpid();
			updateChargerState();
			checkForLowPower();
#ifdef DEBUG_PRINT
			printf("bat0 %f bat1 %f\n",bat0charge,bat1charge);
#endif
			sleep(loopInterval);}
	return(0);}
