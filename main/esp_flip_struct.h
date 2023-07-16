/*  Globals to track module status information */
enum AttackMode {
    ATTACK_BEACON,
    ATTACK_PROBE,
    ATTACK_FUZZ,
    ATTACK_SNIFF,
    ATTACK_DEAUTH,
    ATTACK_MANA,
    ATTACK_MANA_VERBOSE,
    ATTACK_MANA_LOUD,
    ATTACK_AP_DOS,
    ATTACK_AP_CLONE,
    ATTACK_SCAN,
    ATTACK_HANDSHAKE,
    ATTACK_RANDOMISE_MAC, // True
    ATTACKS_COUNT
};
typedef enum AttackMode AttackMode;

enum GravityCommand {
    GRAVITY_NONE,
    GRAVITY_BEACON,
    GRAVITY_TARGET_SSIDS,
    GRAVITY_PROBE,
    GRAVITY_SNIFF,
    GRAVITY_DEAUTH,
    GRAVITY_MANA,
    GRAVITY_STALK,
    GRAVITY_AP_DOS,
    GRAVITY_AP_CLONE,
    GRAVITY_SCAN,
    GRAVITY_HOP,
    GRAVITY_SET,
    GRAVITY_GET,
    GRAVITY_VIEW,
    GRAVITY_SELECT,
    GRAVITY_SELECTED,
    GRAVITY_CLEAR,
    GRAVITY_HANDSHAKE,
    GRAVITY_COMMANDS
};
typedef enum GravityCommand GravityCommand;

/* Using binary values so we can use binary operations & and | */
typedef enum FuzzPacketType {
    FUZZ_PACKET_NONE = 0,
    FUZZ_PACKET_BEACON = 1,
    FUZZ_PACKET_PROBE_REQ = 2,
    FUZZ_PACKET_PROBE_RESP = 4,
    FUZZ_PACKET_TYPE_COUNT = 3
} FuzzPacketType;

typedef enum FuzzMode {
    FUZZ_MODE_SSID_OVERFLOW,
    FUZZ_MODE_SSID_MALFORMED,
    FUZZ_MODE_COUNT
} FuzzMode;