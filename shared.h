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
#ifndef _SHARED_H
#define _SHARED_H

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "error.h"

void zero_device_path_names();
void detect_power_supply_class_devices();
void populate_sys_paths();
void replace_newline_with_null(char* str, size_t str_size);
int fileToStr(const char* path, char* buffer, int buffsize);

int fileToint(const char* path);


struct power_class_dev {
	int		type;
	char	name[28]; /* should add up to 32 on intel machines */
};
#define POWER_CLASS_NAME_MAX_LEN 28
#define POWER_SUPPLY_DEVS_MAX 8
//struct power_class_dev power_supply_devs[POWER_SUPPLY_DEVS_MAX];
//int power_supply_devs_size;

#define POWER_SUPPLY_MAINS 0
#define POWER_SUPPLY_BATTERY 1
//static const char* class_power_supply[2];
extern const char* power_supply_class;


#define PATHNAME_MAX_LEN 128
#define BAT_MAX 2
struct battery{
	char exists;
	char charge_instead_of_energy;
	char has_thresholds;
	char dir[ PATHNAME_MAX_LEN ];
	char charge_start_threshold[ PATHNAME_MAX_LEN ];
	char charge_stop_threshold[ PATHNAME_MAX_LEN ];
	char energy_now[ PATHNAME_MAX_LEN ];
	char energy_full[ PATHNAME_MAX_LEN ];
	char energy_full_design[ PATHNAME_MAX_LEN ];
	char charge_now[ PATHNAME_MAX_LEN ];
	char charge_full[ PATHNAME_MAX_LEN ];
	char charge_full_design[ PATHNAME_MAX_LEN ];
	char voltage_now[ PATHNAME_MAX_LEN ];
	char voltage_min_design[ PATHNAME_MAX_LEN ];
	char cycle_count[ PATHNAME_MAX_LEN ];
	char status[ PATHNAME_MAX_LEN ];
	
	char config_charge_start_threshold;
	char config_charge_stop_threshold;
	char low;
	char low_warn;
	int  cache_bat_capacity;
	float  cache_bat_perc;
};

extern struct battery bat[ BAT_MAX ];

extern char chargerConnectedPath[ PATHNAME_MAX_LEN ];

float get_battery_power(int i);


void intToFile(const char* path, int i);
int intToFile_check(const char* path, int i);
void strToFile(const char* path, char* str);

#endif // _SHARED_H
