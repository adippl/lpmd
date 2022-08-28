#ifndef _LPMD_MESSAES_H
#define _LPMD_MESSAES_H

#define MSG_MAX_LEN 256
//long unsigned int MSG_MAX_LEN = 256;

const char* lid_close="lid_close";
#define LID_CLOSE 1
const char* lid_open="lid_open";
#define LID_OPEN 2
const char* charger_connected="charger_connected";
#define CHARGER_CONNECTED 3
const char* charger_disconnected="charger_disconnected";
#define CHARGER_DISCONNECTED 4
const char* suspend_sleep="suspend_sleep";
#define SUSPEND_SLEEP 5
const char* suspend_wake="suspend_wake";
#define SUSPEND_WAKE 6
const char* hibernate_sleep="hibernate_sleep";
#define HIBERNATE_SLEEP 7
const char* hibernate_wake="hibernate_wake";
#define HIBERNATE_WAKE 8

const char* client_idle_action="client_idle_action";
#define CLIENT_IDLE_ACTION 30
const char* client_notify_daemon_about_idle="client_notify_daemon_about_idle";
#define CLIENT_NOTIFY_DAEMON_ABOUT_IDLE 31
const char* client_ask_for_lock_failed="client_ask_for_lock_failed";
#define CLIENT_ASK_FOR_LOCK_FAILED 32

const char* client_suspend_ask="client_suspend_ask";
#define CLIENT_SUSPEND_ASK 100
const char* client_hibernate_ask="client_hibernate_ask";
#define CLIENT_HIBERNATE_ASK 101
const char* daemon_suspend_refuse="daemon_suspend_refuse";
#define DAEMON_SUSPEND_REFUSE 102
const char* daemon_hibernate_refuse="daemon_hibernate_refuse";
#define DAEMON_HIBERNATE_REFUSE 103


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

