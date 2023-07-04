idf_component_register(SRCS "deauth.c" "scan.c" "probe.c" "beacon.c" "gravity.c"
                    INCLUDE_DIRS ".")
target_link_libraries(${COMPONENT_LIB} -Wl,-zmuldefs)