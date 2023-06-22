#include <stdbool.h>
#include "beacon.c"

/*  Globals to track module status information */
enum {
    ATTACK_BEACON,
    ATTACK_PROBE,
    ATTACK_DEAUTH,
    ATTACK_MANA,
    ATTACK_AP_DOS,
    ATTACK_AP_CLONE,
    ATTACK_SCAN,
    ATTACK_HANDSHAKE,
    ATTACKS_COUNT
};
static bool attack_status[ATTACKS_COUNT] = {false, false, false, false, false, false, false, false};

/* Console callbacks */
int cmd_beacon(int argc, char **argv);
int cmd_probe(int argc, char **argv);
int cmd_deauth(int argc, char **argv);
int cmd_mana(int argc, char **argv);
int cmd_stalk(int argc, char **argv);
int cmd_ap_dos(int argc, char **argv);
int cmd_ap_clone(int argc, char **argv);
int cmd_scan(int argc, char **argv);
int cmd_set(int argc, char **argv);
int cmd_get(int argc, char **argv);
int cmd_view(int argc, char **argv);
int cmd_select(int argc, char **argv);
int cmd_handshake(int argc, char **argv);