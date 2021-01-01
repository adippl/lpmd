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

#define SSTR 64

const char* powerDir="/sys/class/power_supply";
const char* bat0Dir="/sys/class/power_supply/BAT0";
const char* bat1Dir="/sys/class/power_supply/BAT1";
const char* bat0TrStrt="/sys/class/power_supply/BAT0/charge_start_threshold";
const char* bat0TrStop="/sys/class/power_supply/BAT0/charge_start_threshold";
const char* bat1TrStrt="/sys/class/power_supply/BAT1/charge_start_threshold";
const char* bat1TrStop="/sys/class/power_supply/BAT1/charge_start_threshold";
const char* bat0EnFull="/sys/class/power_supply/BAT0/energy_full";
const char* bat1EnFull="/sys/class/power_supply/BAT1/energy_full";
const char* bat0EnNow="/sys/class/power_supply/BAT0/energy_now";
const char* bat1EnNow="/sys/class/power_supply/BAT1/energy_now";
const char* chargerStatePath="/sys/class/power_supply/AC/online";
const char* powerState="/sys/power/state";

int powerStateExists=1;
int bat0Exists=0;
int bat1Exists=0;
int bat0HasThresholds=0;
int bat1HasThresholds=0;

float bat0charge=0.0;
float bat1charge=0.0;
/* float bat0chargeDelta=0.0; */
/* float bat1chargeDelta=0.0; */

int chargerState=-1;

/* config section */
const int bat0TrStrtVal=45;
const int bat0TrStopVal=75;
const int bat1TrStrtVal=45;
const int bat1TrStopVal=75;

const float batMinSleepThreshold=0.15f;

const int loopInterval=1;

const int manageGovernors=1;
const int numberOfCores=4;
const char* powersaveGovernor="powersave";
const char* performanceGovernor="performance";

const int syncBeforeSuspend=0;
const int suspendDeley=5;
/* end of config */

/*
void
scat(char *buf, const char* str1, const char* str2){
	memset(buf,0,SSTR);
	strncpy(buf, str1, SSTR);
	strncat(buf, str2, SSTR);}
*/

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
		perror(path);
		return(0);}
	else{
		perror(path);
		return(0);}}

char*
readFileToStr(const char* path){
	char* buffer=NULL;
	size_t len;
	FILE* file=fopen(path, "rb");
	
	if(file){
		fseek(file, 0, SEEK_END);
		len=ftell(file);
		fseek(file, 0, SEEK_SET);
		buffer=malloc(len);
		if(buffer){
			size_t s=fread(buffer, 1, len, file);
			if(!s)
				fprintf(stderr, "ERR fread returned %ld\n", s);}
		fclose(file);}
	else{
		perror(path);}
	if(buffer){
		return(buffer);}
	else{
		free(buffer);
		return(NULL);}}

int
fileToint(const char* path){
	int i=0;
	char* str=readFileToStr(path);
	
	if(!str){
		i=0;
		return(-1);}
	i=atoi(str);
	
	free(str);
	return(i);}

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
checkSysDirs(){
	if(!checkDir(powerDir))
		exit(1);
	if(checkDir(bat0Dir)){
		bat0Exists=1;
		if(checkFile(bat0TrStrt) &&  checkFile(bat0TrStop)){
			bat0HasThresholds=1;}}
	else{
		bat0Exists=0;
		bat0HasThresholds=0;}
	if(checkDir(bat1Dir)){
		bat1Exists=1;
		if(checkFile(bat1TrStrt) &&  checkFile(bat1TrStop)){
			bat1HasThresholds=1;}}
	else{
		bat1Exists=0;
		bat1HasThresholds=1;}
	if(checkFile(powerState)){
		powerStateExists=0;}}

void
updatePowerPerc(){
	int a=0;
	int b=0;
	if(bat0Exists){
		a=fileToint(bat0EnNow);
		b=fileToint(bat0EnFull);
		bat0charge=(float)a/b;}
	
	if(bat1Exists){
		a=fileToint(bat1EnNow);
		b=fileToint(bat1EnFull);
		bat1charge=(float)a/b;}}

void
changeGovernor(){
	const char* gov=NULL;
	if(manageGovernors){
		switch(chargerState){
			case 0:
				gov=powersaveGovernor;
				break;
			case 1:
				gov=performanceGovernor;
				break;
			default:
				fprintf(stderr,"ERR changeGovernor inconsistent chargerState\n");
				return;}
		char* path=calloc(SSTR, sizeof(char));
		for(int i=0; i<numberOfCores; i++){
			sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
			strToFile(path, (char*)gov);}
		free(path);}}
			

void
setThresholds(){
	intToFile(bat0TrStrt, bat0TrStrtVal);
	intToFile(bat0TrStop, bat0TrStopVal);
	intToFile(bat1TrStrt, bat1TrStrtVal);
	intToFile(bat1TrStop, bat1TrStopVal);}

void
chargerChangedState(){
	changeGovernor();
	if(chargerState){
		fprintf(stderr, "Charger connected\n");}
	else{
		fprintf(stderr, "Charger disconnected\n");}}

void
updateChargerState(){
	int c=fileToint(chargerStatePath);
	if(c != chargerState){
		chargerState=c;
		chargerChangedState();}}

void
suspend(){
	#ifdef DEBUG
		fprintf(stderr , "THIS IS DEBUG MODE, lpm WON't put this machine to sleep. Compile in normal mode to enable this functionality\n");
		return;}
		#endif
	#ifndef DEBUG
		#ifndef NOLOGGER
			fprintf(stderr, "Battery power low, suspending system to mem in %d seconds.\n", suspendDeley);
			#endif
		FILE* f=fopen(powerState, "w");
		if(f){
			fprintf(f, "mem");
			fclose(f);}
		else{
			perror(powerState);}}
		#endif

void
checkForLowPower(){
	if(!chargerState && bat0charge < batMinSleepThreshold){
		suspend();}}

void
loop(){
	#ifdef DEBUG
	for(int i=0; i<6; i++){
	#endif
	#ifndef DEBUG
	for(;;){
	#endif
		updatePowerPerc();
		updateChargerState();
		checkForLowPower();
		#ifdef DEBUG
			printf("bat0 %f\n",bat0charge);
			printf("bat1 %f\n",bat1charge);
			#endif
		sleep(loopInterval);}}

int
main(){
	fprintf(stderr, "Lpmd starts\n");
	checkSysDirs();
	setThresholds();
	loop();
	return(0);}
