#ifndef _LPMD_MESSAES_H
#define _LPMD_MESSAES_H

#define MSG_MAX_LEN 256
//long unsigned int MSG_MAX_LEN = 256;

const char* notif_AA="notif_AA";
#define NOTIF_AA 0
const char* notif_lid_close="notif_lid_close";
#define NOTIF_LID_CLOSE 1
const char* notif_lid_open="notif_lid_open";
#define NOTIF_LID_OPEN 2
const char* notif_charger_connected="notif_charger_connected";
#define NOTIF_CHARGER_CONNECTED 3
const char* notif_charger_disconnected="notif_charger_disconnected";
#define NOTIF_CHARGER_DISCONNECTED 4
const char* notif_suspend_sleep="notif_suspend_sleep";
#define NOTIF_SUSPEN_SLEEP 5
const char* notif_suspend_wake="notif_suspend_wake";
#define NOTIF_SUSPEN_WAKE 6
const char* notif_hibernate_sleep="notif_hibernate_sleep";
#define NOTIF_HIBERNATE_SLEEP 7
const char* notif_hibernate_wake="notif_hibernate_wake";
#define NOTIF_HIBERNATE_WAKE 8
const char* notif_suspend="notif_suspend";
#define NOTIF_SUSPEND 9
//const char* notif_suspend_wake="notif_suspend_wake";
//#define NOTIF_SUSPEND_WAKE 10

const char* client_idle_action="client_idle_action";
#define CLIENT_IDLE_ACTION 30
const char* client_notify_daemon_about_idle="client_notify_daemon_about_idle";
#define CLIENT_NOTIFY_DAEMON_ABOUT_IDLE 31
const char* client_ask_for_lock_failed="client_ask_for_lock_failed";
#define CLIENT_ASK_FOR_LOCK_FAILED 32
const char* client_listen_to_daemon="client_listen_to_daemon";
#define CLIENT_LISTEN_TO_DAEMON 33
const char* client_lock_sessions="client_lock_sessions";
#define CLIENT_LOCK_SESSIONS 33

const char* client_suspend_ask="client_suspend_ask";
#define CLIENT_SUSPEND_ASK 100
const char* client_hibernate_ask="client_hibernate_ask";
#define CLIENT_HIBERNATE_ASK 101
const char* daemon_suspend_refuse="daemon_suspend_refuse";
#define DAEMON_SUSPEND_REFUSE 102
const char* daemon_hibernate_refuse="daemon_hibernate_refuse";
#define DAEMON_HIBERNATE_REFUSE 103
const char* client_lock_ask="client_lock_ask";
#define CLIENT_LOCK_ASK 104
const char* client_lock_all_ask="client_lock_all_ask";
#define CLIENT_LOCK_ALL_ASK 105
const char* client_lock_success="client_lock_success";
#define CLIENT_LOCK_SUCCESS 106
const char* client_lock_fail="client_lock_fail";
#define CLIENT_LOCK_FAIL 106


//const struct msg { const int id; const char* str; };
//
//struct msg client_ask_zero_bat_thresholds = {
//	.id = 107,
//	.str = "client_ask_zero_bat_thresholds" };
//struct msg client_ask_restore_def_bat_thresholds = {
//	.id = 108,
//	.str = "client_ask_restore_def_bat_thresholds" };

const char* client_ask_zero_bat_thresholds="client_ask_zero_bat_thresholds";
#define CLIENT_ASK_ZERO_BAT_THRESHOLDS 107
const char* client_ask_restore_def_bat_thresholds="client_ask_restore_def_bat_thresholds";
#define CLIENT_ASK_RESTORE_DEF_BAT_THRESHOLDS 108
const char* client_ask_for_powersave_gov_lock="client_ask_for_powersave_gov_lock";
#define CLIENT_ASK_FOR_POWERSAVE_GOV_LOCK 109
const char* client_ask_for_performance_gov_lock="client_ask_for_performance_gov_lock";
#define CLIENT_ASK_FOR_PERFORMANCE_GOV_LOCK 110
const char* client_ask_for_automatic_governor_control="client_ask_for_automatic_governor_control";
#define CLIENT_ASK_FOR_AUTOMATIC_GOVERNOR_CONTROL 111


const char* cl_new="cl_new";
#define cl_new 199

const char* daemon_ask_for_screen_locks="daemon_ask_for_screen_locks";
#define DAEMON_ASK_FOR_SCREEN_LOCKS 201
const char* daemon_exit_goodbye="daemon_exit_goodbye";
#define DAEMON_EXIT_GOODBYE 202
const char* daemon_action_success="daemon_action_success";
#define DAEMON_ACTION_SUCCESS 203
const char* daemon_action_failure="daemon_action_failure";
#define DAEMON_ACTION_FAILURE 204
const char* daemon_action_refuse="daemon_action_refuse";
#define DAEMON_ACTION_REFUSE 205



//const char* client_ask_bat_info="client_ask_bat_info";
//const char* daemon_ask_bat_info="daemon_ask_bat_info";

#endif // _LPMD_MESSAES_H

