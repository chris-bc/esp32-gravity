set(reqs
    "esp_wifi"
    "console"
    "fatfs"
    "nvs_flash"
    "cmd_system"
    "cmd_wifi"
    "cmd_nvs"
)

idf_component_register(SRCS "beacon.c" "main.c"
                    INCLUDE_DIRS "." REQUIRES ${reqs})
