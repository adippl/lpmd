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

#define SSTR 64

#ifndef DEBUG_CYCLES
#define DEBUG_CYCLES 360
#endif

const char* powerDir="/sys/class/power_supply";
const char* bat0Dir="/sys/class/power_supply/BAT0";
const char* bat1Dir="/sys/class/power_supply/BAT1";
const char* bat0ThrStrt="/sys/class/power_supply/BAT0/charge_start_threshold";
const char* bat0ThrStop="/sys/class/power_supply/BAT0/charge_stop_threshold";
const char* bat1ThrStrt="/sys/class/power_supply/BAT1/charge_start_threshold";
const char* bat1ThrStop="/sys/class/power_supply/BAT1/charge_stop_threshold";
const char* bat0EnFull="/sys/class/power_supply/BAT0/energy_full";
const char* bat1EnFull="/sys/class/power_supply/BAT1/energy_full";
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

/* config section */
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
const char* wallSuspendWarning="WARNING!!!\n WARNING!!!  battery0 is low.\n WARNING!!!  Syncing filesystem and suspending to mem in 15 seconds...\n";
const char* wallLowBatWarning="WARNING!!!\n WARNING!!!  battery0 is below 25%\n";
/* end of config */

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
updateChargerState(){
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

void
checkForLowPower(){
	int bat0low=bat0charge<batMinSleepThreshold;
	int bat1low=bat1charge<batMinSleepThreshold;
	int bat0lowWarn=bat0charge<batLowWarningThreshold;
	int bat1lowWarn=bat1charge<batLowWarningThreshold;
	if(!chargerConnected){
		if( (bat0Exists&&bat0low)||\
			(bat1Exists&&bat0low&&bat1low)){
			
			suspend();
			return;}}
		if(((bat0Exists&&bat0lowWarn)||\
			(bat1Exists&&bat0lowWarn&&bat1lowWarn))&& \
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
	checkSysDirs();
	setThresholds();
	chargerConnected=fileToint(chargerConnectedPath);
#ifdef DEBUG
		for(int i=0; i<DEBUG_CYCLES; i++){
#else
		for(;;){
#endif
			checkSysDirs();
			updatePowerPerc();
			updateChargerState();
			checkForLowPower();
#ifdef DEBUG_PRINT
			printf("bat0 %f bat1 %f\n",bat0charge, bat0charge);
#endif
			sleep(loopInterval);}
	return(0);}
