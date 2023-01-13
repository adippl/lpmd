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
#include "shared.h"


char bat0Dir[ PATHNAME_MAX_LEN ] = {0};
char bat1Dir[ PATHNAME_MAX_LEN ] = {0};
char bat0ThrStrt[ PATHNAME_MAX_LEN ] = {0};
char bat0ThrStop[ PATHNAME_MAX_LEN ] = {0};
char bat1ThrStrt[ PATHNAME_MAX_LEN ] = {0};
char bat1ThrStop[ PATHNAME_MAX_LEN ] = {0};
char bat0EnNow[ PATHNAME_MAX_LEN ] = {0};
char bat1EnNow[ PATHNAME_MAX_LEN ] = {0};
char chargerConnectedPath[ PATHNAME_MAX_LEN ] = {0};
char bat0EnFull[ PATHNAME_MAX_LEN ] = {0};
char bat1EnFull[ PATHNAME_MAX_LEN ] = {0};

struct power_class_dev power_supply_devs[POWER_SUPPLY_DEVS_MAX] = {0};
int power_supply_devs_size = 0;


const char* power_supply_class="/sys/class/power_supply";
static const char* class_power_supply[] = {
	"Mains",
	"Battery", };


void
replace_newline_with_null(char* str, size_t str_size){
	for(unsigned int i=0; i<str_size; i++){
		if( str[i]=='\n' )
			str[i]='\0';
		if( str[i]=='\0' )
			return;}}

void
detect_power_supply_class_devices(){
	struct dirent *dir = NULL;
	DIR* pdir = opendir( power_supply_class );
	char temp_path[256]={0};
	char type_buffer[ POWER_CLASS_NAME_MAX_LEN ]={0};
	FILE* typefile = NULL;
	/* clear power_supply_devs */
	memset( power_supply_devs, 0, sizeof(power_supply_devs));
	power_supply_devs_size=0;
	/* populate power_supply_devs */
	if( !pdir )
		error_errno_msg_exit("couldn't open ", power_supply_class );
	while( (dir = readdir(pdir)) != NULL){
		if( dir->d_name[0] == '.' )
			continue;
		if( POWER_SUPPLY_DEVS_MAX <= power_supply_devs_size ){
			/* our arrry is full, not adding new devices */
			/* TODO add error message */
			fprintf(stderr, "power_supply_devs array is full, not adding more devices\n");
			continue;}
		snprintf(
			temp_path,
			128,
			"%s/%.32s/type",
			power_supply_class,
			dir->d_name);
		typefile=NULL;
		typefile = fopen( temp_path, "rb" );
		if( !typefile )
			error_errno_msg_exit("file is not read accessible ", temp_path);
		fread( type_buffer, 1, POWER_CLASS_NAME_MAX_LEN, typefile);
		fclose( typefile );
		type_buffer[ POWER_CLASS_NAME_MAX_LEN-1 ]='\0';
		replace_newline_with_null( type_buffer, POWER_CLASS_NAME_MAX_LEN);
#ifdef DEBUG
		fprintf(stderr,
			"Detected power_supply class device: \"%s\", type: \"%s\":\n",
			dir->d_name,
			type_buffer);
#endif
		/* add device to array */
		if( !strncmp( type_buffer, class_power_supply[ POWER_SUPPLY_MAINS ]  , POWER_CLASS_NAME_MAX_LEN))
			power_supply_devs[ power_supply_devs_size ].type = POWER_SUPPLY_MAINS;
		else if( !strncmp( type_buffer, class_power_supply[ POWER_SUPPLY_BATTERY ]  , POWER_CLASS_NAME_MAX_LEN))
			power_supply_devs[ power_supply_devs_size ].type = POWER_SUPPLY_BATTERY;
		else{
			fprintf(stderr, "ERROR, %s/%.28s device type unkonwn: %s\n",
				power_supply_class,
				dir->d_name,
				type_buffer);
			continue;}
		strncpy(
			power_supply_devs[ power_supply_devs_size ].name,
			dir->d_name,
			POWER_CLASS_NAME_MAX_LEN );
		/* just to be sure */
		power_supply_devs[ power_supply_devs_size ].name[27]='\0';
		power_supply_devs_size++;
	}
	closedir(pdir);
	}


void
zero_device_path_names(){
	memset( bat0Dir, 0, PATHNAME_MAX_LEN);
	memset( bat1Dir, 0, PATHNAME_MAX_LEN);
	memset( bat0ThrStrt, 0, PATHNAME_MAX_LEN);
	memset( bat0ThrStop, 0, PATHNAME_MAX_LEN);
	memset( bat1ThrStrt, 0, PATHNAME_MAX_LEN);
	memset( bat1ThrStop, 0, PATHNAME_MAX_LEN);
	memset( bat0EnNow, 0, PATHNAME_MAX_LEN);
	memset( bat1EnNow, 0, PATHNAME_MAX_LEN);
	memset( chargerConnectedPath, 0, PATHNAME_MAX_LEN);
	memset( bat0EnFull, 0, PATHNAME_MAX_LEN);
	memset( bat1EnFull, 0, PATHNAME_MAX_LEN);}

void
populate_sys_paths(){
	int bat0set=0;
	int bat1set=0;
	/* listing array backwards */
	for(int i=power_supply_devs_size-1; i>=0; i--){
//#ifdef DEBUG
//		fprintf(stderr,
//			"listing power_supply class device: \"%s\", type: \"%s\":\n",
//			power_supply_devs[i].name,
//			class_power_supply[ power_supply_devs[i].type ]);
//#endif
		/* setup for ac adapter */
		if( power_supply_devs[i].type == POWER_SUPPLY_MAINS ){
			snprintf( chargerConnectedPath,
				PATHNAME_MAX_LEN,
				"%s/%s/online",
				power_supply_class,
				power_supply_devs[i].name);
			continue;}
		if( power_supply_devs[i].type == POWER_SUPPLY_BATTERY ){
			/* setup for bat0 */
			if( ! bat0set ){
				snprintf( bat0Dir,
					PATHNAME_MAX_LEN,
					"%s/%s",
					power_supply_class,
					power_supply_devs[i].name);
				snprintf( bat0ThrStrt,
					PATHNAME_MAX_LEN,
					"%s/%s/%s", /* concat 3 strings to avoid gcc printf truncation errors */
					power_supply_class,
					power_supply_devs[i].name,
					"charge_start_threshold");
				snprintf( bat0ThrStop,
					PATHNAME_MAX_LEN,
					"%s/%s/%s",
					power_supply_class,
					power_supply_devs[i].name,
					"charge_stop_threshold");
				snprintf( bat0EnFull,
					PATHNAME_MAX_LEN,
					"%s/%s/%s",
					power_supply_class,
					power_supply_devs[i].name,
					"energy_full");
				snprintf( bat0EnNow,
					PATHNAME_MAX_LEN,
					"%s/%s/%s",
					power_supply_class,
					power_supply_devs[i].name,
					"energy_now");
				bat0set=1;}
			/* setup for bat1 */
			else if( ! bat1set ){
				snprintf( bat1Dir,
					PATHNAME_MAX_LEN,
					"%s/%s",
					power_supply_class,
					power_supply_devs[i].name);
				snprintf( bat1ThrStrt,
					PATHNAME_MAX_LEN,
					"%s/%s/%s", /* concat 3 strings to avoid gcc printf truncation errors */
					power_supply_class,
					power_supply_devs[i].name,
					"charge_start_threshold");
				snprintf( bat1ThrStop,
					PATHNAME_MAX_LEN,
					"%s/%s/%s",
					power_supply_class,
					power_supply_devs[i].name,
					"charge_stop_threshold");
				snprintf( bat1EnFull,
					PATHNAME_MAX_LEN,
					"%s/%s/%s",
					power_supply_class,
					power_supply_devs[i].name,
					"energy_full");
				snprintf( bat1EnNow,
					PATHNAME_MAX_LEN,
					"%s/%s/%s",
					power_supply_class,
					power_supply_devs[i].name,
					"energy_now");
				bat1set=1;}
			else{
				fprintf(stderr, "ERROR, lpmd doesn't support 3rd battery");
				fprintf(stderr,
					"listing power_supply class device: \"%s\", type: \"%s\":\n",
					power_supply_devs[i].name,
					class_power_supply[ power_supply_devs[i].type ]);}
			continue;}
		}}
