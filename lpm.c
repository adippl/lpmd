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

int bat0Exists=1;
int bat1Exists=1;
int bat0HasThresholds=0;
int bat1HasThresholds=0;

float bat0charge=0.0;
float bat1charge=0.0;
//float bat0chargeDelta=0.0;
//float bat1chargeDelta=0.0;

int chargerState=-1;

/* config */
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
		return(0);}
	else if(ENOENT==errno){
		printf("dir %s does not exist\n", path);
		return(1);}
	else{
		printf("opendir failed to open %s\n", path);
		return(1);}}

char*
readFileToStr(const char* path){
	char* buffer=NULL;
	long length;
	FILE* f=fopen(path, "rb");
	
	if(f){
		fseek(f, 0, SEEK_END);
		length=ftell(f);
		fseek(f, 0, SEEK_SET);
		buffer=malloc(length);
		if(buffer){
			size_t s=fread(buffer, 1, length, f);
			if(!s)
				fprintf(stderr, "ERR fread returned %ld\n", s);}
		fclose(f);}
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
		fclose(f);}}

void
strToFile(const char* path, char* str){
	FILE* f=fopen(path, "w");
	if(f){
		fputs(str, f);
		fclose(f);}}

void
checkSysDirs(){
	if(checkDir(powerDir))exit(1);
	if(checkDir(bat0Dir)){
		exit(1);
		bat0Exists=0;
		if(checkFile(bat0TrStrt) &&  checkFile(bat0TrStop)){
			bat0HasThresholds=1;}
		}
	if(checkDir(bat1Dir)){
		bat1Exists=0;
		if(checkFile(bat1TrStrt) &&  checkFile(bat1TrStop)){
			bat1HasThresholds=1;}
			}
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
			strToFile(path, (char*)gov);}}}
			

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
		printf("Charger connected\n");}
	else{
		printf("Charger disconnected\n");}}

void
updateChargerState(){
	int c=fileToint(chargerStatePath);
	if(c != chargerState){
		chargerState=c;
		chargerChangedState();}}

void
suspend(){
	#ifdef DEBUG
		printf("\nTHIS IS DEBUG MODE, lpm WON't put this machine to sleep. Compile in normal mode to enable this functionality\n");
		return;}
		#endif
	#ifndef DEBUG
		printf("Battery power low, suspending system to mem.\n");
		FILE* f=fopen(powerState, "w");
		if(f){
			fprintf(f, "mem");
			fclose(f);}}
		#endif

void
checkForLowPower(){
	if(!chargerState && bat0charge < batMinSleepThreshold){
		suspend();}}

void
loop(){
	for(int i=0; i<6; i++){
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
	checkSysDirs();
	setThresholds();
	loop();
	return(0);}
