idf_component_register(SRCS "uuids.c" "stalk.c" "dos.c" "bluetooth.c" "hop.c" "common.c" "mana.c" "sniff.c" "fuzz.c" "deauth.c" "scan.c" "probe.c" "beacon.c" "gravity.c"
                    INCLUDE_DIRS ".")
target_link_libraries(${COMPONENT_LIB} -Wl,-zmuldefs)