#include "bluetooth.h"
#include "common.h"
#include "probe.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <time.h>

#if defined(CONFIG_BT_ENABLED)

#include "uuids.c"

esp_err_t gravity_bt_discover_services(app_gap_cb_t *dev);

const char *BT_TAG = "bt@GRAVITY";

app_gap_cb_t **gravity_bt_devices = NULL;
uint8_t gravity_bt_dev_count = 0;
app_gap_cb_t **gravity_selected_bt = NULL;
uint8_t gravity_sel_bt_count = 0;
app_gap_cb_t **gravity_svc_disc_q = NULL;
uint8_t gravity_svc_disc_count = 0;
app_gap_state_t state;
static bool btInitialised = false;
static bool bleInitialised = false;
static bool btServiceDiscoveryActive = false;

enum bt_device_parameters {
    BT_PARAM_COD = 0,
    BT_PARAM_RSSI,
    BT_PARAM_EIR,
    BT_PARAM_BDNAME,
    BT_PARAM_COUNT
};

//static const char remote_device_name[] = "ESP_GATTS_DEMO";
static bool connect    = false;
static bool get_server = false;
static esp_gattc_char_elem_t *char_elem_result   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;

#define REMOTE_SERVICE_UUID        0x00FF
#define REMOTE_NOTIFY_CHAR_UUID    0xFF01
#define PROFILE_NUM      1

static int bt_comparator(const void *varOne, const void *varTwo);

/* cod2deviceStr
   Converts a uint32_t representing a Bluetooth Class Of Device (COD)'s major
   device code into a useful string descriptor of the specified COD major device.
   Returns an error indicator. Input 'cod' is the COD to be converted.
   char *string is a pointer to a character array of sufficient size to hold
   the results. The maximum possible number of characters required by this
   function, including the trailing NULL, is 59.
   uint8_t *stringLen is a pointer to a uint8_t variable in the calling code.
   This variable will be overwritten with the length of string.
*/
esp_err_t cod2deviceStr(uint32_t cod, char *string, uint8_t *stringLen) {
    esp_err_t err = ESP_OK;
    char temp[59] = "";
    esp_bt_cod_major_dev_t devType = esp_bt_gap_get_cod_major_dev(cod);
    switch (devType) {
        case ESP_BT_COD_MAJOR_DEV_MISC:
            strcpy(temp, "Miscellaneous");
            break;
        case ESP_BT_COD_MAJOR_DEV_COMPUTER:
            strcpy(temp, "Computer");
            break;
        case ESP_BT_COD_MAJOR_DEV_PHONE:
            strcpy(temp, "Phone (cellular, cordless, pay phone, modem)");
            break;
        case ESP_BT_COD_MAJOR_DEV_LAN_NAP:
            strcpy(temp, "LAN, Network Access Point");
            break;
        case ESP_BT_COD_MAJOR_DEV_AV:
            strcpy(temp, "Audio/Video (headset, speaker, stereo, video display, VCR)");
            break;
        case ESP_BT_COD_MAJOR_DEV_PERIPHERAL:
            strcpy(temp, "Peripheral (mouse, joystick, keyboard)");
            break;
        case ESP_BT_COD_MAJOR_DEV_IMAGING:
            strcpy(temp, "Imaging (printer, scanner, camera, display)");
            break;
        case ESP_BT_COD_MAJOR_DEV_WEARABLE:
            strcpy(temp, "Wearable");
            break;
        case ESP_BT_COD_MAJOR_DEV_TOY:
            strcpy(temp, "Toy");
            break;
        case ESP_BT_COD_MAJOR_DEV_HEALTH:
            strcpy(temp, "Health");
            break;
        case ESP_BT_COD_MAJOR_DEV_UNCATEGORIZED:
            strcpy(temp, "Uncategorised: Device not specified");
            break;
        default:
            strcpy(temp, "ERROR: Invalid Major Device Type");
            break;
    }
    strcpy(string, temp);
    *stringLen = strlen(string);

    return err;
}

/* As above, but returning a shortened string suitable for display by VIEW */
esp_err_t cod2shortStr(uint32_t cod, char *string, uint8_t *stringLen) {
    esp_err_t err = ESP_OK;
    char temp[11] = "";
    esp_bt_cod_major_dev_t devType = esp_bt_gap_get_cod_major_dev(cod);
    switch (devType) {
        case ESP_BT_COD_MAJOR_DEV_MISC:
            strcpy(temp, "Misc.");
            break;
        case ESP_BT_COD_MAJOR_DEV_COMPUTER:
            strcpy(temp, "PC");
            break;
        case ESP_BT_COD_MAJOR_DEV_PHONE:
            strcpy(temp, "Phone");
            break;
        case ESP_BT_COD_MAJOR_DEV_LAN_NAP:
            strcpy(temp, "LAN");
            break;
        case ESP_BT_COD_MAJOR_DEV_AV:
            strcpy(temp, "A/V");
            break;
        case ESP_BT_COD_MAJOR_DEV_PERIPHERAL:
            #ifdef CONFIG_FLIPPER
                strcpy(temp, "Periph.");
            #else
                strcpy(temp, "Peripheral");
            #endif
            break;
        case ESP_BT_COD_MAJOR_DEV_IMAGING:
            strcpy(temp, "Imaging");
            break;
        case ESP_BT_COD_MAJOR_DEV_WEARABLE:
            strcpy(temp, "Wearable");
            #ifdef CONFIG_FLIPPER
                temp[7] = '\0'; /* Truncate the last character if necessary */
            #endif
            break;
        case ESP_BT_COD_MAJOR_DEV_TOY:
            strcpy(temp, "Toy");
            break;
        case ESP_BT_COD_MAJOR_DEV_HEALTH:
            strcpy(temp, "Health");
            break;
        case ESP_BT_COD_MAJOR_DEV_UNCATEGORIZED:
            strcpy(temp, "Uncat.");
            break;
        default:
            strcpy(temp, "Invalid");
            break;
    }
    strcpy(string, temp);
    *stringLen = strlen(string);

    return err;
}

static char *bda2str(esp_bd_addr_t bda, char *str, size_t size) {
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

static char *uuid2str(esp_bt_uuid_t *uuid, char *str, size_t size) {
    if (uuid == NULL || str == NULL) {
        return NULL;
    }

    if (uuid->len == 2 && size >= 5) {
        sprintf(str, "%04x", uuid->uuid.uuid16);
    } else if (uuid->len == 4 && size >= 9) {
        sprintf(str, "%08"PRIx32, uuid->uuid.uuid32);
    } else if (uuid->len == 16 && size >= 37) {
        uint8_t *p = uuid->uuid.uuid128;
        sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                p[15], p[14], p[13], p[12], p[11], p[10], p[9], p[8], p[7], p[6], p[5], p[4],
                p[3], p[2], p[1], p[0]);
    } else {
        return NULL;
    }
    return str;
}

static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len)
{
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;

    if (!eir) {
        return false;
    }

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname) {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len) {
            *bdname_len = rmt_bdname_len;
        }
        return true;
    }

    return false;
}

static bool get_string_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len) {
    bool retVal = true;
    uint8_t bdname_bytes[ESP_BT_GAP_MAX_BDNAME_LEN + 1]; // TODO: Remove +1?
    retVal = get_name_from_eir(eir, bdname_bytes, bdname_len);

    if (*bdname_len > 0) {
        memcpy(bdname, bdname_bytes, *bdname_len);
        bdname[*bdname_len] = '\0';
    }
    return retVal;
}

static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = 0x00FF,},
};

static esp_bt_uuid_t remote_filter_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = 0xFF01,},
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static struct gattc_profile_inst gl_profile_tab[1] = {
    [0] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(BT_TAG, "Event handler registered.");
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret){
            ESP_LOGE(BT_TAG, "set scan params error, error code = %x", scan_ret);
        }
        break;
    case ESP_GATTC_CONNECT_EVT:{
        ESP_LOGI(BT_TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gl_profile_tab[0].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[0].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(BT_TAG, "REMOTE BDA:");
        esp_log_buffer_hex(BT_TAG, gl_profile_tab[0].remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        if (mtu_ret){
            ESP_LOGE(BT_TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    }
    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK){
            ESP_LOGE(BT_TAG, "open failed, status %d", p_data->open.status);
            break;
        }
        ESP_LOGI(BT_TAG, "open success");
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(BT_TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        ESP_LOGI(BT_TAG, "discover service complete conn_id %d", param->dis_srvc_cmpl.conn_id);
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remote_filter_service_uuid);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(BT_TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGI(BT_TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        ESP_LOGI(BT_TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(BT_TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID) {
            ESP_LOGI(BT_TAG, "service found");
            get_server = true;
            gl_profile_tab[0].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[0].service_end_handle = p_data->search_res.end_handle;
            ESP_LOGI(BT_TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(BT_TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
            ESP_LOGI(BT_TAG, "Get service information from remote device");
        } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
            ESP_LOGI(BT_TAG, "Get service information from flash");
        } else {
            ESP_LOGI(BT_TAG, "unknown service source");
        }
        ESP_LOGI(BT_TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        if (get_server){
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab[0].service_start_handle,
                                                                     gl_profile_tab[0].service_end_handle,
                                                                     0,
                                                                     &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(BT_TAG, "esp_ble_gattc_get_attr_count error");
            }

            if (count > 0){
                char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result){
                    ESP_LOGE(BT_TAG, "gattc no mem");
                }else{
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_tab[0].service_start_handle,
                                                             gl_profile_tab[0].service_end_handle,
                                                             remote_filter_char_uuid,
                                                             char_elem_result,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(BT_TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                        gl_profile_tab[0].char_handle = char_elem_result[0].char_handle;
                        esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[0].remote_bda, char_elem_result[0].char_handle);
                    }
                }
                /* free char_elem_result */
                free(char_elem_result);
            }else{
                ESP_LOGE(BT_TAG, "no char found");
            }
        }
         break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        ESP_LOGI(BT_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(BT_TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
        }else{
            uint16_t count = 0;
            uint16_t notify_en = 1;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                         gl_profile_tab[0].conn_id,
                                                                         ESP_GATT_DB_DESCRIPTOR,
                                                                         gl_profile_tab[0].service_start_handle,
                                                                         gl_profile_tab[0].service_end_handle,
                                                                         gl_profile_tab[0].char_handle,
                                                                         &count);
            if (ret_status != ESP_GATT_OK){
                ESP_LOGE(BT_TAG, "esp_ble_gattc_get_attr_count error");
            }
            if (count > 0){
                descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result){
                    ESP_LOGE(BT_TAG, "malloc error, gattc no mem");
                }else{
                    ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                         gl_profile_tab[0].conn_id,
                                                                         p_data->reg_for_notify.handle,
                                                                         notify_descr_uuid,
                                                                         descr_elem_result,
                                                                         &count);
                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(BT_TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                    }
                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                    if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
                        ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                     gl_profile_tab[0].conn_id,
                                                                     descr_elem_result[0].handle,
                                                                     sizeof(notify_en),
                                                                     (uint8_t *)&notify_en,
                                                                     ESP_GATT_WRITE_TYPE_RSP,
                                                                     ESP_GATT_AUTH_REQ_NONE);
                    }

                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(BT_TAG, "esp_ble_gattc_write_char_descr error");
                    }

                    /* free descr_elem_result */
                    free(descr_elem_result);
                }
            }
            else{
                ESP_LOGE(BT_TAG, "decsr not found");
            }

        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        if (p_data->notify.is_notify){
            ESP_LOGI(BT_TAG, "ESP_GATTC_NOTIFY_EVT, receive notify value:");
        }else{
            ESP_LOGI(BT_TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value:");
        }
        esp_log_buffer_hex(BT_TAG, p_data->notify.value, p_data->notify.value_len);
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(BT_TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(BT_TAG, "write descr success ");
        uint8_t write_char_data[35];
        for (int i = 0; i < sizeof(write_char_data); ++i)
        {
            write_char_data[i] = i % 256;
        }
        esp_ble_gattc_write_char( gattc_if,
                                  gl_profile_tab[0].conn_id,
                                  gl_profile_tab[0].char_handle,
                                  sizeof(write_char_data),
                                  write_char_data,
                                  ESP_GATT_WRITE_TYPE_RSP,
                                  ESP_GATT_AUTH_REQ_NONE);
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(BT_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(BT_TAG, bda, sizeof(esp_bd_addr_t));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(BT_TAG, "write char failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(BT_TAG, "write char success ");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        connect = false;
        get_server = false;
        ESP_LOGI(BT_TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
        break;
    default:
        break;
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        esp_ble_gap_start_scanning(CONFIG_BLE_SCAN_SECONDS);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BT_TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
            break;
        }
        #ifdef CONFIG_FLIPPER
            printf("BLE scan status: %s\n%d cached BT devices.\n", attack_status[ATTACK_SCAN_BLE]?"ON":"OFF", gravity_bt_dev_count);
        #else
            ESP_LOGI(BT_TAG, "Bluetooth LE scan status: %s Total Bluetooth devices (Classic + LE): %d.", attack_status[ATTACK_SCAN_BLE]?"Active":"Inactive", gravity_bt_dev_count);
        #endif

        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_DISC_RES_EVT:
            printf("discovery result\n");
            break;
        case ESP_GAP_SEARCH_DISC_BLE_RES_EVT:
            printf("discovery ble result\n");
            break;
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            /* Get device name */
            char bdNameStr[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
            memcpy(bdNameStr, adv_name, adv_name_len);
            bdNameStr[adv_name_len] = '\0';

            /* Does the BDA exist? */
            int devIdx = 0;
            for ( ; devIdx < gravity_bt_dev_count && memcmp(scan_result->scan_rst.bda, gravity_bt_devices[devIdx]->bda, ESP_BD_ADDR_LEN); ++devIdx) { }
            if (devIdx < gravity_bt_dev_count) {
                /* Found - Update */
                gravity_bt_devices[devIdx]->rssi = scan_result->scan_rst.rssi;
                if (scan_result->scan_rst.adv_data_len > 0) {
                    if (gravity_bt_devices[devIdx]->eir != NULL) {
                        free(gravity_bt_devices[devIdx]->eir);
                    }
                    gravity_bt_devices[devIdx]->eir = malloc(sizeof(uint8_t) * scan_result->scan_rst.adv_data_len);
                    if (gravity_bt_devices[devIdx]->eir == NULL) {
                        #ifdef CONFIG_FLIPPER
                            printf("%sfor EIR (len %u).\n", STRINGS_MALLOC_FAIL, scan_result->scan_rst.adv_data_len);
                        #else
                            ESP_LOGE(BT_TAG, "%sfor EIR (length %u).", STRINGS_MALLOC_FAIL, scan_result->scan_rst.adv_data_len);
                        #endif
                        return; /* Out of memory */
                    }
                    memcpy(gravity_bt_devices[devIdx]->eir, scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len);
                    gravity_bt_devices[devIdx]->eir_len = scan_result->scan_rst.adv_data_len;
                }
                if (adv_name_len > 0) {
                    gravity_bt_devices[devIdx]->bdname_len = adv_name_len;
                    if (gravity_bt_devices[devIdx]->bdName != NULL) {
                        free(gravity_bt_devices[devIdx]->bdName);
                    }
                    gravity_bt_devices[devIdx]->bdName = malloc(sizeof(char) * (adv_name_len + 1));
                    if (gravity_bt_devices[devIdx]->bdName == NULL) {
                        #ifdef CONFIG_FLIPPER
                            printf("%sfor bdName (len %u).\n", STRINGS_MALLOC_FAIL, adv_name_len);
                        #else
                            ESP_LOGE(BT_TAG, "%sfor Bluetooth Device Name (length %u).", STRINGS_MALLOC_FAIL, adv_name_len);
                        #endif
                        return; /* ESP_ERR_NO_MEM */
                    }
                    strncpy(gravity_bt_devices[devIdx]->bdName, bdNameStr, adv_name_len);
                    gravity_bt_devices[devIdx]->bdName[adv_name_len] = '\0';
                }
            } else {
                bt_dev_add_components(scan_result->scan_rst.bda, bdNameStr, adv_name_len, scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len, 0, scan_result->scan_rst.rssi, GRAVITY_BT_SCAN_BLE);
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            /* Restart the BLE scanner if it hasn't been disabled (status will be displayed on
               successful start)
            */
            if (attack_status[ATTACK_SCAN_BLE]) {
                esp_ble_gap_start_scanning(CONFIG_BLE_SCAN_SECONDS);
            }
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(BT_TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
            break;
        }
        ESP_LOGI(BT_TAG, "stop scan successfully");
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(BT_TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(BT_TAG, "stop adv successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(BT_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    case ESP_GAP_BLE_GET_DEV_NAME_COMPLETE_EVT:
        ESP_LOGI(BT_TAG, "Got a complete name %s status %d", param->get_dev_name_cmpl.name, param->get_dev_name_cmpl.status);
        break;
    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(BT_TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

esp_err_t gravity_ble_scan_start(gravity_bt_purge_strategy_t purgeStrat) {
    esp_err_t err = ESP_OK;
    purgeStrategy = purgeStrat;

    if (!btInitialised) {
        err = gravity_bt_initialise();
        if (err != ESP_OK) {
            printf("%s\n", esp_err_to_name(err));
            return err;
        }
    }

    if (!bleInitialised) {
        err = esp_ble_gap_register_callback(esp_gap_cb);
        if (err != ESP_OK) {
            printf("%s\n", esp_err_to_name(err));
            return err;
        }

        err = esp_ble_gattc_register_callback(esp_gattc_cb);
        if (err != ESP_OK) {
            printf("%s\n", esp_err_to_name(err));
            return err;
        }

        err = esp_ble_gattc_app_register(0);
        if (err != ESP_OK) {
            printf("%s\n", esp_err_to_name(err));
            //return err;
        }

        err = esp_ble_gatt_set_local_mtu(500);
        if (err != ESP_OK) {
            printf("%s\n", esp_err_to_name(err));\
            //return err;
        }
        bleInitialised = true;
        printf("BLE Initialised.\n");
    } else {
        err |= esp_ble_gap_start_scanning(CONFIG_BLE_SCAN_SECONDS);
    }
    return err;
}

/* update_device_info
   This function is called by the Bluetooth callback when a device discovered event
   is received. It will maintain gravity_bt_devices and gravity_bt_dev_count with a
   singe element per physical device.
*/
void update_device_info(esp_bt_gap_cb_param_t *param) {
    char bda_str[18];
    esp_bt_gap_dev_prop_t *p;
    char codDevType[59]; /* Placeholder to store COD major device type */
    uint8_t codDevTypeLen = 0;

    /* A bunch of extra variables now we're no longer caching them in a struct */
    esp_bd_addr_t dev_bda;
    uint32_t dev_cod = 0;
    uint32_t dev_rssi = -127;
    uint8_t dev_bdname_len = 0;
    char dev_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    uint8_t dev_eir_len = 0;
    uint8_t dev_eir[ESP_BT_GAP_EIR_DATA_LEN];

    /* Keep track of which parameters need writing to the data model */
    bool paramUpdated[BT_PARAM_COUNT]; // TODO: Come back to me
    for (int i = 0; i < BT_PARAM_COUNT; ++i) {
        paramUpdated[i] = false;
    }

    /* Is it a new BDA (i.e. device we haven't seen before)? */
    int deviceIdx = 0;
    memcpy(dev_bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
    bda2str(param->disc_res.bda, bda_str, 18);
    for (; deviceIdx < gravity_bt_dev_count && memcmp(param->disc_res.bda,
                    gravity_bt_devices[deviceIdx]->bda, ESP_BD_ADDR_LEN); ++deviceIdx) { }

    int numProp = param->disc_res.num_prop;
    #ifdef CONFIG_DEBUG_VERBOSE
        printf("BEGIN UPDATE_DEVICE_INFO() BDA: %s %sNum properties %d: ", bda_str, (deviceIdx < gravity_bt_dev_count)?"Already cached, ":"New device, ", numProp);
        /* Display info about each attached property */
        for (int i = 0; i < numProp; ++i) {
            p = param->disc_res.prop + i;
            switch (p->type) {
                case ESP_BT_GAP_DEV_PROP_COD:
                    printf("(%d, COD)  ", i);
                    break;
                case ESP_BT_GAP_DEV_PROP_RSSI:
                    printf("(%d, RSSI %d)  ", i, *(int8_t*)(p->val));
                    break;
                case ESP_BT_GAP_DEV_PROP_BDNAME:
                    printf("(%d, Len %u)   ", i, p->len);
                    break;
                case ESP_BT_GAP_DEV_PROP_EIR:
                    printf("(%d, EIR Len %u)  ", i, p->len);
                    break;
                default:
                    printf("(%d, Unknown [%d])  ", i, p->type);
            }
        }
    #endif

    for (int i = 0; i < numProp; ++i) {
        p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            dev_cod = *(uint32_t *)(p->val);
            cod2deviceStr(dev_cod, codDevType, &codDevTypeLen);
            paramUpdated[BT_PARAM_COD] = true;
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            dev_rssi = *(int8_t *)(p->val);
            paramUpdated[BT_PARAM_RSSI] = true;
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME:
            /* extract bdnameLen */
            dev_bdname_len = (p->len > ESP_BT_GAP_MAX_BDNAME_LEN) ? ESP_BT_GAP_MAX_BDNAME_LEN :
                    (uint8_t)p->len;
            memcpy(dev_bdname, p->val, dev_bdname_len);
            dev_bdname[dev_bdname_len] = '\0';
            paramUpdated[BT_PARAM_BDNAME] = true;
            break;
        case ESP_BT_GAP_DEV_PROP_EIR: {
            memcpy(dev_eir, p->val, p->len);
            dev_eir_len = p->len;
            paramUpdated[BT_PARAM_EIR] = true;
            break;
        }
        default:
            break;
        }
    }

    /* search for device with Major device type "PHONE" or "Audio/Video" in COD */
    // if (!esp_bt_gap_is_valid_cod(cod) ||
	//     (!(esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_PHONE) &&
    //          !(esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_AV))) {
    //     return;
    // }

    if (dev_bdname_len == 0) {
        get_string_name_from_eir(dev_eir, dev_bdname, &dev_bdname_len);
        if (dev_bdname_len > 0) {
            paramUpdated[BT_PARAM_BDNAME] = true;
        }
    }

    /* Is BDA already in gravity_bt_devices[]? */
    if (deviceIdx < gravity_bt_dev_count) {
        /* Found an existing device with the same BDA - Update its RSSI */
        /* YAGNI - Update bdname if it's changed */
        // TODO: Include a timestamp so we can age devices
        if (paramUpdated[BT_PARAM_BDNAME] && ((gravity_bt_devices[deviceIdx]->bdName == NULL && dev_bdname_len > 0) || strcmp(gravity_bt_devices[deviceIdx]->bdName, dev_bdname))) {
            #ifdef CONFIG_FLIPPER
                printf("BT: Update BT Device %s Name \"%s\"\n", bda_str, dev_bdname);
            #else
                ESP_LOGI(BT_TAG, "Update BT Device %s Name \"%s\"", bda_str, dev_bdname);
            #endif
        }
        if (updateDevice(paramUpdated, dev_bda, dev_cod, dev_rssi, dev_bdname_len, dev_bdname, dev_eir_len, dev_eir) != ESP_OK) {
            #ifdef CONFIG_FLIPPER
                printf("An error occurred trying to update the device in memory. Sorry about that.\n");
            #else
                ESP_LOGW(BT_TAG, "updateDevice() did not complete successfully.");
            #endif
        }
    } else {
        /* It's a new device - Add to gravity_bt_devices[] */
        char devString[41 + ESP_BT_GAP_MAX_BDNAME_LEN];
        sprintf(devString, "Found BT Device %s", bda_str);
        /* Append name if we have one */
        if (dev_bdname_len > 0) {
            strcat(devString, " (Name \"");
            strcat(devString, dev_bdname);
            strcat(devString, "\")");
        }
        // TODO: Can/do/will I ever use the state variable (hardcoded 0 here)?
        #ifdef CONFIG_FLIPPER
            printf("BT: %s\n", devString);
        #else
            ESP_LOGI(BT_TAG, "%s", devString);
        #endif
        bt_dev_add_components(dev_bda, dev_bdname, dev_bdname_len, dev_eir, dev_eir_len, dev_cod, dev_rssi, GRAVITY_BT_SCAN_CLASSIC_DISCOVERY);
    }

    state = APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE;
    //ESP_LOGI(BT_TAG, "Cancel device discovery ...");
    //esp_bt_gap_cancel_discovery();
}

/* Update the element of gravity_bt_devices[] that is specified by theBda
   If the BDA does not exist in the data model it will be created instead.
*/
esp_err_t updateDevice(bool *updatedFlags, esp_bd_addr_t theBda, int32_t theCod, int32_t theRssi, uint8_t theNameLen, char *theName, uint8_t theEirLen, uint8_t *theEir) {
    esp_err_t err = ESP_OK;
    int deviceIdx = 0;
    while (deviceIdx < gravity_bt_dev_count && memcmp(theBda, gravity_bt_devices[deviceIdx]->bda, ESP_BD_ADDR_LEN)) {
        ++deviceIdx;
    }
    if (deviceIdx < gravity_bt_dev_count) {
        /* We found a stored device with the same BDA */
        if (updatedFlags[BT_PARAM_COD]) {
            gravity_bt_devices[deviceIdx]->cod = theCod;
            updatedFlags[BT_PARAM_COD] = false;
        }
        if (updatedFlags[BT_PARAM_RSSI]) {
            gravity_bt_devices[deviceIdx]->rssi = theRssi;
            updatedFlags[BT_PARAM_RSSI] = false;
        }
        if (updatedFlags[BT_PARAM_BDNAME]) {
            if (gravity_bt_devices[deviceIdx]->bdName != NULL) {
                free(gravity_bt_devices[deviceIdx]->bdName);
            }
            gravity_bt_devices[deviceIdx]->bdName = malloc(sizeof(char) * (theNameLen + 1));
            if (gravity_bt_devices[deviceIdx]->bdName == NULL) {
                #ifdef CONFIG_FLIPPER
                    printf("%sfor BDName (len %u).\n", STRINGS_MALLOC_FAIL, theNameLen);
                #else
                    ESP_LOGE(BT_TAG, "%sfor Bluetooth Device Name (length %u).", STRINGS_MALLOC_FAIL, theNameLen);
                #endif
                return ESP_ERR_NO_MEM;
            }
            strncpy(gravity_bt_devices[deviceIdx]->bdName, theName, theNameLen);
            gravity_bt_devices[deviceIdx]->bdName[theNameLen] = '\0';
            gravity_bt_devices[deviceIdx]->bdname_len = theNameLen;
            updatedFlags[BT_PARAM_BDNAME] = false;
        }
        if (updatedFlags[BT_PARAM_EIR]) {
            if (gravity_bt_devices[deviceIdx]->eir != NULL) {
                free(gravity_bt_devices[deviceIdx]->eir);
            }
            gravity_bt_devices[deviceIdx]->eir = malloc(sizeof(uint8_t) * theEirLen);
            if (gravity_bt_devices[deviceIdx] == NULL) {
                #ifdef CONFIG_FLIPPER
                    printf("%sfor EIR (len %u).\n", STRINGS_MALLOC_FAIL, theEirLen);
                #else
                    ESP_LOGE(BT_TAG, "%sfor EIR (length %u).", STRINGS_MALLOC_FAIL, theEirLen);
                #endif
                return ESP_ERR_NO_MEM;
            }
            memcpy(gravity_bt_devices[deviceIdx]->eir, theEir, theEirLen);
            gravity_bt_devices[deviceIdx]->eir_len = theEirLen;
        }
        /* Update lastSeen and lastSeen */
        gravity_bt_devices[deviceIdx]->lastSeen = clock();
    } else {
        /* Device doesn't exist, add it instead */
        return bt_dev_add_components(theBda, theName, theNameLen, theEir, theEirLen, theCod, theRssi, GRAVITY_BT_SCAN_CLASSIC_DISCOVERY);
    }
    return err;
}

app_gap_cb_t *gravity_svc_disc_queue_pop() {
    #ifdef CONFIG_DEBUG_VERBOSE
        char bda_str[18] = "";
        bda2str(gravity_svc_disc_q[0]->bda, bda_str, 18);
        printf("Beginning of pop, first queue item is %s (%s).\n", bda_str, gravity_svc_disc_q[0]->bdName);
    #endif

    app_gap_cb_t **newQ = NULL;
    if (gravity_svc_disc_count == 0) {
        printf("queu is empty, returning NULL\n");
        return NULL;
    } else {
        /* Shrink the queue */
        if (gravity_svc_disc_count > 1) {
            newQ = malloc(sizeof(app_gap_cb_t *) * (gravity_svc_disc_count - 1));
            if (newQ == NULL) {
                #ifdef CONFIG_FLIPPER
                    printf("%s\n", STRINGS_MALLOC_FAIL);
                #else
                    ESP_LOGE(BT_TAG, "%s.", STRINGS_MALLOC_FAIL);
                #endif
                return NULL;
            }
            /* Copy relevant array elements beginning with the first */
            memcpy(newQ, &(gravity_svc_disc_q[1]), (gravity_svc_disc_count - 1));
        }
        app_gap_cb_t *retVal = gravity_svc_disc_q[0];
        --gravity_svc_disc_count;
        free(gravity_svc_disc_q);
        gravity_svc_disc_q = newQ;
        return retVal;
    }
}

/* Search the store of known UUIDs for the specified UUID */
bt_uuid *svcForUUID(uint16_t uuid) {
    uint8_t svcIdx = 0;
    for ( ; svcIdx < BT_UUID_COUNT && uuid != bt_uuids[svcIdx].uuid16; ++ svcIdx) {}
    if (svcIdx == BT_UUID_COUNT) {
        return NULL;
    }
    return &(bt_uuids[svcIdx]);
}

esp_err_t listUnknownServicesDev(app_gap_cb_t *device) {
    esp_err_t err = ESP_OK;
    uint8_t knownCount = 0;
    char bda_str[18];
    bda2str(device->bda, bda_str, 18);

    #ifdef CONFIG_FLIPPER
        printf("%u/%u Unknown for\n%s\n", (device->bt_services.num_services - device->bt_services.known_services_len),
                device->bt_services.num_services, (device->bdname_len > 0)?device->bdName:bda_str);
    #else
        ESP_LOGI(BT_TAG, "%u/%u Services are Unknown for %s", (device->bt_services.num_services - device->bt_services.known_services_len),
                device->bt_services.num_services, (device->bdname_len > 0)?device->bdName:bda_str);
    #endif

    for (int i = 0; i < device->bt_services.num_services; ++i) {
        /* Is the current device unknown? Thankfully service_uuids and known_uuids
           are ordered in the same way
        */
        if (!(device->bt_services.known_services_len > knownCount && device->bt_services.service_uuids[i].len == ESP_UUID_LEN_16 &&
                device->bt_services.known_services[knownCount]->uuid16 == device->bt_services.service_uuids[i].uuid.uuid16)) {
            /* Check UUID length */
            if (device->bt_services.service_uuids[i].len == ESP_UUID_LEN_128) {
                /* Display 16 bytes */
                char *uuidStr = malloc(sizeof(char) * (3 * ESP_UUID_LEN_128));
                if (uuidStr == NULL) {
                    #ifdef CONFIG_FLIPPER
                        printf("%s\n", STRINGS_MALLOC_FAIL);
                    #else
                        ESP_LOGE(BT_TAG, "%s", STRINGS_MALLOC_FAIL);
                    #endif
                    return ESP_ERR_NO_MEM;
                }
                if (bytes_to_string(device->bt_services.service_uuids[i].uuid.uuid128, uuidStr, ESP_UUID_LEN_128) != ESP_OK) {
                    printf ("bytes_to_string returned an error\n");
                    free(uuidStr);
                    return ESP_ERR_INVALID_RESPONSE;
                }
                #ifdef CONFIG_FLIPPER
                    printf("%s:\n", uuidStr);
                #else
                    ESP_LOGI(BT_TAG, "-- UUID Type ESP_UUID_LEN_128, UUID %s", uuidStr);
                #endif
                free(uuidStr);
            } else {
                /* 16 and 32 bit UUIDs can both be displayed */
                #ifdef CONFIG_FLIPPER
                    printf("0x%04lx:\n", (device->bt_services.service_uuids[i].len == ESP_UUID_LEN_16)?device->bt_services.service_uuids[i].uuid.uuid16:device->bt_services.service_uuids[i].uuid.uuid32);
                #else
                    ESP_LOGI(BT_TAG, "-- UUID Type %s, UUID 0x%04lx", (device->bt_services.service_uuids[i].len == ESP_UUID_LEN_16)?"ESP_UUID_LEN_16":"ESP_UUID_LEN_32", (device->bt_services.service_uuids[i].len == ESP_UUID_LEN_16)?device->bt_services.service_uuids[i].uuid.uuid16:device->bt_services.service_uuids[i].uuid.uuid32);
                #endif
            }
        } else {
            ++knownCount;
        }
    }
    return err;
}

esp_err_t listUnknownServices() {
    esp_err_t err = ESP_OK;

    for (int i = 0; i < gravity_bt_dev_count; ++i) {
        err |= listUnknownServicesDev(gravity_bt_devices[i]);
        printf("\n");
    }
    return err;
}

esp_err_t listKnownServices(app_gap_cb_t **devices, uint8_t devCount) {
    esp_err_t err = ESP_OK;
    for (uint8_t i = 0; i < devCount; ++i) {
        err |= listKnownServicesDev(devices[i]);
        printf("\n");
    }
    return err;
}

esp_err_t bt_listAllServices() {
    return bt_listAllServicesFor(gravity_bt_devices, gravity_bt_dev_count);
}

esp_err_t bt_listAllServicesFor(app_gap_cb_t **devices, uint8_t devCount) {
    esp_err_t err = ESP_OK;
    for (int i = 0; i < devCount; ++i) {
        err |= bt_listAllServicesDev(devices[i]);
        printf("\n");
    }
    return err;
}

esp_err_t bt_listAllServicesDev(app_gap_cb_t *thisDev) {
    esp_err_t err = ESP_OK;
    char bda_str[18] = "";
    uint8_t knownIdx = 0;
    uint8_t allIdx = 0;

    bda2str(thisDev->bda, bda_str, 18);
    #ifdef CONFIG_FLIPPER
        printf("%s:\n\t%u/%u known services\n", (thisDev->bdname_len > 0)?thisDev->bdName:bda_str, thisDev->bt_services.known_services_len, thisDev->bt_services.num_services);
    #else
        ESP_LOGI(BT_TAG, "%s :  %u / %u Known Services", (thisDev->bdname_len > 0)?thisDev->bdName:bda_str, thisDev->bt_services.known_services_len, thisDev->bt_services.num_services);
    #endif

    /* Iterate through bt_service.service_uuids, displaying the next known service if
       current element has a matching UUID
    */
    for (uint8_t i = 0; i < thisDev->bt_services.num_services; ++i) {
        if (knownIdx < thisDev->bt_services.known_services_len && thisDev->bt_services.service_uuids[i].len == ESP_UUID_LEN_16 && thisDev->bt_services.known_services[knownIdx]->uuid16 == thisDev->bt_services.service_uuids[i].uuid.uuid16) {
            /* There's a valid known service to display, current service is 16-bit,
               and the next known service (ordering will always be consistent) has
               the same UUID as the current service
            */
            #ifdef CONFIG_FLIPPER
                printf("0x%04x:\n%s\n", thisDev->bt_services.known_services[knownIdx]->uuid16, thisDev->bt_services.known_services[knownIdx]->name);
            #else
                ESP_LOGI(BT_TAG, "-- UUID Type ESP_UUID_LEN_16, UUID 0x%04x, Service: %s", thisDev->bt_services.known_services[knownIdx]->uuid16, thisDev->bt_services.known_services[knownIdx]->name);
            #endif
            ++knownIdx;
        } else {
            /* No name to display, just display UUID type and value */
            if (thisDev->bt_services.service_uuids[i].len == ESP_UUID_LEN_128) {
                /* Display 16 bytes */
                char *uuidStr = malloc(sizeof(char) * (3 * ESP_UUID_LEN_128));
                if (uuidStr == NULL) {
                    #ifdef CONFIG_FLIPPER
                        printf("%s\n", STRINGS_MALLOC_FAIL);
                    #else
                        ESP_LOGE(BT_TAG, "%s", STRINGS_MALLOC_FAIL);
                    #endif
                    return ESP_ERR_NO_MEM;
                }
                if (bytes_to_string(thisDev->bt_services.service_uuids[i].uuid.uuid128, uuidStr, ESP_UUID_LEN_128) != ESP_OK) {
                    printf ("bytes_to_string returned an error\n");
                    free(uuidStr);
                    return ESP_ERR_INVALID_RESPONSE;
                }
                #ifdef CONFIG_FLIPPER
                    printf("%s:\n", uuidStr);
                #else
                    ESP_LOGI(BT_TAG, "-- UUID Type ESP_UUID_LEN_128, UUID %s", uuidStr);
                #endif
                free(uuidStr);
            } else {
                /* Either 2 or 4 byte UUID but we should be able to capture both in a single block */
                #ifdef CONFIG_FLIPPER
                    printf("0x%04lx:\n", (thisDev->bt_services.service_uuids[i].len == ESP_UUID_LEN_16)?thisDev->bt_services.service_uuids[i].uuid.uuid16:thisDev->bt_services.service_uuids[i].uuid.uuid32);
                #else
                    ESP_LOGI(BT_TAG, "-- UUID Type %s, UUID 0x%04lx", (thisDev->bt_services.service_uuids[i].len == ESP_UUID_LEN_16)?"ESP_UUID_LEN_16":"ESP_UUID_LEN_32", (thisDev->bt_services.service_uuids[i].len == ESP_UUID_LEN_16)?thisDev->bt_services.service_uuids[i].uuid.uuid16:thisDev->bt_services.service_uuids[i].uuid.uuid32);
                #endif
            }
        }
    }
    return err;
}

esp_err_t listKnownServicesDev(app_gap_cb_t *thisDev) {
    esp_err_t err = ESP_OK;

    char bda_str[18] = "";
    bda2str(thisDev->bda, bda_str, 18);
    #ifdef CONFIG_FLIPPER
        printf("Known services for %s\n\t(%s)\n", bda_str, thisDev->bdName == NULL?"(No Name)":thisDev->bdName);
    #else
        ESP_LOGI(BT_TAG, "Known services for %s (%s)", bda_str, (thisDev->bdName == NULL)?"(No Name)":thisDev->bdName);
    #endif
    /* Display each service */
    for (int i = 0; i < thisDev->bt_services.known_services_len; ++i) {
        #ifdef CONFIG_FLIPPER
            printf("0x%04x\n%s\n", thisDev->bt_services.known_services[i]->uuid16, thisDev->bt_services.known_services[i]->name);
        #else
            ESP_LOGI(BT_TAG, "0x%04x :  %s", thisDev->bt_services.known_services[i]->uuid16, thisDev->bt_services.known_services[i]->name);
        #endif
    }
    return err;
}

/* Parse a list of discovered service UUIDs and collate known UUIDs for display
   num_services elements of service_uuids will be inspected to see if they are known UUIDs.
   Known UUIDs will be stored in known_services with the number in known_services_len.

   This function assumes that known_services has not been initialised - It will not free existing
   values, and it will malloc a new value. It's up to the user to manage memory appropriately.
*/
esp_err_t identifyKnownServices(app_gap_cb_t *thisDev) {
    esp_err_t err = ESP_OK;
    grav_bt_svc *bt_services = &(thisDev->bt_services);

    /* First, count the number of services that are known */
    uint8_t knownCount = 0;
    bt_uuid *thisSvc = NULL;
    for (int i = 0; i < bt_services->num_services; ++i) {
        if (bt_services->service_uuids[i].len == ESP_UUID_LEN_16) {
            /* Only 16-bit UUIDs are known */
            // TODO: What about 32?
            thisSvc = svcForUUID(bt_services->service_uuids[i].uuid.uuid16);
            if (thisSvc != NULL) {
                ++knownCount;
            }
        }
    }
    /* Display high-level results */
    char bda_str[18];
    bda2str(thisDev->bda, bda_str, 18);
    #ifdef CONFIG_FLIPPER
        printf("%u/%u Known Services\nFor %s\n", knownCount, bt_services->num_services, (thisDev->bdname_len > 0)?thisDev->bdName:bda_str);
    #else
        ESP_LOGI(BT_TAG, "%u/%u Services Identified for %s", knownCount, bt_services->num_services, (thisDev->bdname_len > 0)?thisDev->bdName:bda_str);
    #endif
    /* Begin preparing known service attributes */
    bt_services->known_services_len = knownCount;
    if (knownCount > 0) {
        bt_services->known_services = malloc(sizeof(bt_uuid *) * knownCount);
        if (bt_services->known_services == NULL) {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", STRINGS_MALLOC_FAIL);
            #else
                ESP_LOGE(BT_TAG, "%s", STRINGS_MALLOC_FAIL);
            #endif
            return ESP_ERR_NO_MEM;
        }
    }

    /* A second loop through discovered services, this time collating known services as we go */
    uint8_t curSvc = 0;
    for (int i = 0; i < bt_services->num_services; ++i) {
        if (bt_services->service_uuids[i].len == ESP_UUID_LEN_16) {
            thisSvc = svcForUUID(bt_services->service_uuids[i].uuid.uuid16);
            if (thisSvc != NULL) {
                /* Notify user of discovered service */
                #ifdef CONFIG_DEBUG
                    #ifdef CONFIG_FLIPPER
                        printf("0x%04x: %s\n", thisSvc->uuid16, thisSvc->name);
                    #else
                        ESP_LOGI(BT_TAG, "-- UUID 0x%04x: %s", thisSvc->uuid16, thisSvc->name);
                    #endif
                #endif
                /* Add thisSvc to known_uuids */
                bt_services->known_services[curSvc++] = thisSvc;
            }
        }
    }

    return err;
}

app_gap_cb_t *deviceWithBDA(esp_bd_addr_t bda) {
    uint8_t devIdx = 0;
    for ( ; devIdx < gravity_bt_dev_count && memcmp(bda, gravity_bt_devices[devIdx]->bda, 6); ++devIdx) { }
    if (devIdx < gravity_bt_dev_count) {
        return gravity_bt_devices[devIdx];
    } else {
        return NULL;
    }
}

/* This function is called by the Bluetooth callback function when a remote service event is received
   This function maintains the bt_services element of the bluetooth device data model.
   In order to do this, the function:
    - Locates the relevant device using its BDA
    - Copies service UUID details from the results to the data model
    - Calls an auxilliary function to identify and translate known UUIDs from the list
*/
static void bt_remote_service_cb(esp_bt_gap_cb_param_t *param) {
    char bda_str[18];
    ESP_LOGI(BT_TAG, "Receiving services for BDA %s", bda2str(param->rmt_srvcs.bda, bda_str, 18));
    if (state == APP_GAP_STATE_SERVICE_DISCOVERING)
        printf("state is APP_GAP_STATE_SERVICE_DISCOVERING\n");
    if (state == APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE)
        printf("state is APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE\n");

    app_gap_cb_t *thisDev = deviceWithBDA(param->rmt_srvcs.bda);
    if (thisDev == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("Device %s no found.\n", bda_str);
        #else
            ESP_LOGE(BT_TAG, "Device with BDA %s not found.", bda_str);
        #endif
        printf("TODO: Do I return here or hang out?\n");
    }

    state = APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE;
    if (param->rmt_srvcs.stat == ESP_BT_STATUS_SUCCESS) {
        if (thisDev != NULL) {
            thisDev->bt_services.lastSeen = clock();
            thisDev->bt_services.num_services = param->rmt_srvcs.num_uuids;
            /* Allocate space for UUID array */
            thisDev->bt_services.service_uuids = malloc(sizeof(esp_bt_uuid_t) * param->rmt_srvcs.num_uuids);
            if (thisDev->bt_services.service_uuids == NULL) {
                #ifdef CONFIG_FLIPPER
                    printf("%s\n", STRINGS_MALLOC_FAIL);
                #else
                    ESP_LOGE(BT_TAG, "%s", STRINGS_MALLOC_FAIL);
                #endif
            }
            /* Copy UUIDs from discovery results to thisDev */
            memcpy(thisDev->bt_services.service_uuids, param->rmt_srvcs.uuid_list, sizeof(esp_bt_uuid_t) * param->rmt_srvcs.num_uuids);
            /* Parse the UUIDs and collate known_uuids */
            identifyKnownServices(thisDev);
        }
        #ifdef CONFIG_DEBUG_VERBOSE
            for (int i = 0; i < param->rmt_srvcs.num_uuids; ++i) {
                esp_bt_uuid_t *u = param->rmt_srvcs.uuid_list + i;
                // ESP_UUID_LEN_128 is uint8_t[16]
                ESP_LOGI(BT_TAG, "-- UUID type %s, UUID: 0x%04lx", (u->len == ESP_UUID_LEN_16)?"ESP_UUID_LEN_16":(u->len == ESP_UUID_LEN_32)?"ESP_UUID_LEN_32":"ESP_UUID_LEN_128", (u->len == ESP_UUID_LEN_16)?u->uuid.uuid16:(u->len == ESP_UUID_LEN_32)?u->uuid.uuid32:0);
                if (u->len == ESP_UUID_LEN_128) {
                    char *uuidStr = malloc(sizeof(char) * (3 * ESP_UUID_LEN_128));
                    if (uuidStr == NULL) {
                        #ifdef CONFIG_FLIPPER
                            printf("%s\n", STRINGS_MALLOC_FAIL);
                        #else
                            ESP_LOGE(BT_TAG, "%s", STRINGS_MALLOC_FAIL);
                        #endif
                        return;
                    }
                    if (bytes_to_string(u->uuid.uuid128, uuidStr, ESP_UUID_LEN_128) != ESP_OK) {
                        printf ("bytes_to_string returned an error\n");
                        free(uuidStr);
                        return;
                    }
                    ESP_LOGI(BT_TAG, "UUID128: %s", uuidStr);
                    free(uuidStr);
                }
            }
        #endif
    }
    #ifdef CONFIG_DEBUG_VERBOSE
        printf("remote service callback - Outputs done, checking queue (len %u, addr %p)\n",
                gravity_svc_disc_count, gravity_svc_disc_q);
    #endif
    /* Service discovery complete - Is there a queue to take the next one from? */
    if (gravity_svc_disc_count == 0) {
        /* No queue - Finish service discovery */
        #ifdef CONFIG_DEBUG
            #ifdef CONFIG_FLIPPER
                printf("Service Discovery:\n\tNo devices left in queue.\n");
            #else
                ESP_LOGI(BT_TAG, "Service Discovery: No devices left in scan queue, finishing.");
            #endif
        #endif
        btServiceDiscoveryActive = false;
    } else {
        app_gap_cb_t *thisDev = gravity_svc_disc_queue_pop();
        #ifdef CONFIG_DEBUG
            char bda_str[18] = "";
            if (thisDev != NULL) {
                bda2str(thisDev->bda, bda_str, 18);
            }
            #ifdef CONFIG_FLIPPER
                printf("Service Discovery:\n\tPopped %s from the queue.\n", (thisDev==NULL)?"NULL":bda_str);
            #else
                ESP_LOGI(BT_TAG, "Service Discovery: Popped %s from the queue.", (thisDev == NULL)?"NULL":bda_str);
            #endif
        #endif
        if (thisDev != NULL) {
            esp_bt_gap_get_remote_services(thisDev->bda);
        }
    }
}

static void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            update_device_info(param);;
            break;
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                /* Display status & restart Discovery */
                if (attack_status[ATTACK_SCAN_BT_DISCOVERY]) {
                    state = APP_GAP_STATE_DEVICE_DISCOVERING;
                    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, CONFIG_BT_SCAN_DURATION, 0);
                }
            } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
                gravity_bt_scan_display_status();
            }
            break;
        case ESP_BT_GAP_RMT_SRVCS_EVT:
            bt_remote_service_cb(param);
            break;
        case ESP_BT_GAP_RMT_SRVC_REC_EVT:
        default:
            ESP_LOGI(BT_TAG, "event: %d", event);
            break;
    }
    return;
}

esp_err_t gravity_bt_gap_start() {
    esp_err_t err = ESP_OK;

    if (!btInitialised) {
        err |= gravity_bt_initialise();
    }
    err |= esp_bt_gap_register_callback(bt_gap_cb);
    /* Set Device name */
    char *dev_name = "GRAVITY_INQUIRY";
    err |= esp_bt_dev_set_device_name(dev_name);

    /* Set discoverable and connectable, wait to be connected */
    err |= esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    /* Start to discover nearby devices */
    state = APP_GAP_STATE_DEVICE_DISCOVERING;
    err |= esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, CONFIG_BT_SCAN_DURATION, 0);

    return err;
}

esp_err_t gravity_bt_initialise() {
    #ifdef CONFIG_BLE_PURGE_MIN_AGE
        PURGE_MIN_AGE = CONFIG_BLE_PURGE_MIN_AGE;
    #endif
    #ifdef CONFIG_BLE_PURGE_MAX_RSSI
        PURGE_MAX_RSSI = CONFIG_BLE_PURGE_MAX_RSSI;
    #endif

    esp_err_t err = ESP_OK;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode = ESP_BT_MODE_BTDM;
    //bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    err |= esp_bt_controller_init(&bt_cfg);
    /* Enable WiFi sleep mode in order for wireless coexistence to work */
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    err |= esp_bt_controller_enable(ESP_BT_MODE_BTDM); /* Was ESP_BT_MODE_CLASSIC_BT */
    err |= esp_bluedroid_init();
    err |= esp_bluedroid_enable();

    btInitialised = true;
    return err;
}

/* Add a new Bluetooth device to gravity_bt_devices[]
   Creates a new BT device struct using the specified parameters and adds it to
   gravity_bt_devices[].
   A uniqueness check will be done using BDA prior to adding the specified device.
   The most barebones, valid way to call this (i.e. the minimum info that must be
   specified for a BT device) is bt_dev_add(myValidBDA, NULL, 0, NULL, 0, myValidCOD, 0, 0);
   bdNameLen should specify the raw length of the name because it is stored in a uint8_t[],
   excluding the trailing '\0'.
*/
esp_err_t bt_dev_add_components(esp_bd_addr_t bda, char *bdName, uint8_t bdNameLen, uint8_t *eir,
                        uint8_t eirLen, uint32_t cod, int32_t rssi, gravity_bt_scan_t devScanType) {
    esp_err_t err = ESP_OK, err2 = ESP_OK;

    /* Make sure the specified BDA doesn't already exist */
    if (isBDAInArray(bda, gravity_bt_devices, gravity_bt_dev_count)) {
        char bdaStr[18] = "";
        #ifdef CONFIG_FLIPPER
            printf("Unable to add existing BT Dev:\n%25s\n", bda2str(bda, bdaStr, 18));
        #else
            ESP_LOGE(BT_TAG, "Unable to add the requested Bluetooth device to Gravity's device array; BDA %s already exists.", bda2str(bda, bdaStr, 18));
        #endif
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* Set up a replacement copy of gravity_bt_devices */
    app_gap_cb_t **newDevices = gravity_ble_purge_and_malloc(sizeof(app_gap_cb_t *) * (gravity_bt_dev_count + 1));
    if (newDevices == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("%sto add BT device\n", STRINGS_MALLOC_FAIL);
        #else
            ESP_LOGE(BT_TAG, "%sfor Bluetooth Device #%d.", STRINGS_MALLOC_FAIL, (gravity_bt_dev_count + 1));
        #endif
        return ESP_ERR_NO_MEM;
    }
    /* Copy contents from gravity_bt_devices to newDevices */
    /* This was previously a loop iterating through and calling an additional function to
       copy the struct. Luckily I stuffed up and corrupted nearby memory so I had to use my
       brain and come up with a simple solution - Copy the memory contents of the first array
       to the second.
    */
    //memcpy(newDevices, gravity_bt_devices, (gravity_bt_dev_count * sizeof(app_gap_cb_t *)));
    for (int i=0;i<gravity_bt_dev_count;++i) {
        newDevices[i] = gravity_bt_devices[i];
    }
    /* Create new device at index gravity_bt_dev_count */
    newDevices[gravity_bt_dev_count] = gravity_ble_purge_and_malloc(sizeof(app_gap_cb_t));
    if (newDevices[gravity_bt_dev_count] == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("%sfor BT device %d.\n", STRINGS_MALLOC_FAIL, gravity_bt_dev_count + 1);
        #else
            ESP_LOGE(BT_TAG, "%s(%d) for BT device #%d.", STRINGS_MALLOC_FAIL, sizeof(app_gap_cb_t), gravity_bt_dev_count + 1);
        #endif
        free(newDevices);
        return ESP_ERR_NO_MEM;
    }
    newDevices[gravity_bt_dev_count]->bt_services.known_services = NULL;
    newDevices[gravity_bt_dev_count]->bt_services.known_services_len = 0;
    newDevices[gravity_bt_dev_count]->bt_services.num_services = 0;
    newDevices[gravity_bt_dev_count]->bt_services.service_uuids = NULL;
    newDevices[gravity_bt_dev_count]->bt_services.lastSeen = 0;
    newDevices[gravity_bt_dev_count]->bdname_len = bdNameLen;
    newDevices[gravity_bt_dev_count]->eir_len = eirLen;
    newDevices[gravity_bt_dev_count]->eir = NULL;
    newDevices[gravity_bt_dev_count]->rssi = rssi;
    newDevices[gravity_bt_dev_count]->cod = cod;
    newDevices[gravity_bt_dev_count]->scanType = devScanType;
    newDevices[gravity_bt_dev_count]->lastSeen = clock();
    newDevices[gravity_bt_dev_count]->selected = false;
    newDevices[gravity_bt_dev_count]->index = gravity_bt_dev_count;
    memcpy(newDevices[gravity_bt_dev_count]->bda, bda, ESP_BD_ADDR_LEN);
    newDevices[gravity_bt_dev_count]->bdName = gravity_ble_purge_and_malloc(sizeof(char) * (bdNameLen + 1));
    if (newDevices[gravity_bt_dev_count]->bdName == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("%sfor BDName (len %u).\n", STRINGS_MALLOC_FAIL, bdNameLen);
        #else
            ESP_LOGE(BT_TAG, "%sfor Bluetooth Device Name (length %u).", STRINGS_MALLOC_FAIL, bdNameLen);
        #endif
        free(newDevices);
        return ESP_ERR_NO_MEM;
    }
    strncpy(newDevices[gravity_bt_dev_count]->bdName, bdName, bdNameLen);
    newDevices[gravity_bt_dev_count]->bdName[bdNameLen] = '\0';
    if (eirLen > 0) {
        newDevices[gravity_bt_dev_count]->eir = gravity_ble_purge_and_malloc(sizeof(uint8_t) * eirLen);
        if (newDevices[gravity_bt_dev_count]->eir == NULL) {
            #ifdef CONFIG_FLIPPER
                printf("%sfor EIR (len %u).\n", STRINGS_MALLOC_FAIL, eirLen);
            #else
                ESP_LOGE(BT_TAG, "%sfor EIR (length %u).", STRINGS_MALLOC_FAIL, eirLen);
            #endif
            if (newDevices[gravity_bt_dev_count]->bdName != NULL) {
                free(newDevices[gravity_bt_dev_count]->bdName);
            }
            free(newDevices);
            return ESP_ERR_NO_MEM;
        }
        memcpy(newDevices[gravity_bt_dev_count]->eir, eir, eirLen);
    }

    /* Finally copy new device array into place */
    if (gravity_bt_devices != NULL) {
        free(gravity_bt_devices);
    }
    gravity_bt_devices = newDevices;
    ++gravity_bt_dev_count;


    #ifdef CONFIG_DEBUG_VERBOSE
        printf("End of bt_dev_add_components(), gravity_bt_devices has %u elements:\n", gravity_bt_dev_count);
        for (int i =0; i < gravity_bt_dev_count; ++i) {
            if (i > 0) {
                printf(",\t");
            }
            printf("\"%s\"", gravity_bt_devices[i]->bdName);
        }
    #endif

    return err;
}

esp_err_t bt_dev_add(app_gap_cb_t *dev) {
    return bt_dev_add_components(dev->bda, dev->bdName, dev->bdname_len,
            dev->eir, dev->eir_len, dev->cod, dev->rssi, dev->scanType);
}

/* Is the specified bluetooth device address in the specified array, which has the specified length? */
bool isBDAInArray(esp_bd_addr_t bda, app_gap_cb_t **array, uint8_t arrayLen) {
    int i = 0;
    for (; i < arrayLen && memcmp(bda, array[i]->bda, ESP_BD_ADDR_LEN); ++i) { }

    /* If i < arrayLen then it was found */
    return (i < arrayLen);
}

/* Copy all elements of a app_gap_cb_t representing a Bluetooth device
   from one variable to another.
   If dest is a reused app_gap_cb_t, this function WILL NOT manage the
   clean release of memory from its elements. That's up to the caller.
*/
esp_err_t bt_dev_copy(app_gap_cb_t dest, app_gap_cb_t source) {
    esp_err_t err = ESP_OK;

    /* Start with the pass-by-value elements */
    dest.bdname_len = source.bdname_len;
    dest.eir_len = source.eir_len;
    dest.rssi = source.rssi;
    dest.cod = source.cod;

    /* And now the refs */
    memcpy(dest.bda, source.bda, ESP_BD_ADDR_LEN);
    /* EIR - free, malloc, copy */
    if (dest.eir != NULL) {
        free(dest.eir);
    }
    dest.eir = malloc(sizeof(uint8_t) * source.eir_len);
    if (dest.eir == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("%sfor EIR (len %u).\n", STRINGS_MALLOC_FAIL, source.eir_len);
        #else
            ESP_LOGE(BT_TAG, "%sfor EIR (length %u).", STRINGS_MALLOC_FAIL, source.eir_len);
        #endif
        return ESP_ERR_NO_MEM;
    }
    memcpy(dest.eir, source.eir, source.eir_len);
    /* bdName - free, malloc, copy */
    if (dest.bdName != NULL) {
        free(dest.bdName);
    }
    dest.bdName = malloc(sizeof(char) * (source.bdname_len + 1));
    if (dest.bdName == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("%sfor bdName (len %u).\n", STRINGS_MALLOC_FAIL, source.bdname_len);
        #else
            ESP_LOGE(BT_TAG, "%sfor Bluetooth device name (length %u).", STRINGS_MALLOC_FAIL, source.bdname_len);
        #endif
        if (dest.eir != NULL) {
            free(dest.eir);
        }
        return ESP_ERR_NO_MEM;
    }
    strncpy(dest.bdName, source.bdName, source.bdname_len);
    dest.bdName[source.bdname_len] = '\0';

    return err;
}

/* Discover services for the specified Bluetooth device
   Currently only supports Bluetooth Classic. BLE coming soon :-)
   Only a single instance of service discovery can run per Bluetooth radio,
   so a queueing strategy had to be implemented for this.
   This function will:
   - Commence service discovery if it is not currently running
   - append the specified device to the service discovery queue if it is currently running
   The queue is consumed by bt_gap_cb, with a new instance started when the
   previous one completes.
*/
esp_err_t gravity_bt_discover_services(app_gap_cb_t *dev) {
    if (btServiceDiscoveryActive) {
        /* Create new queue object */
        app_gap_cb_t **newQ = malloc(sizeof(app_gap_cb_t *) * (gravity_svc_disc_count + 1));
        if (newQ == NULL) {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", STRINGS_MALLOC_FAIL);
            #else
                ESP_LOGI(BT_TAG, "%s svc_disc 1.", STRINGS_MALLOC_FAIL);
            #endif
            return ESP_ERR_NO_MEM;
        }
        /* Copy across gravity_svc_disc_count elements */
        memcpy(newQ, gravity_svc_disc_q, sizeof(app_gap_cb_t *) * gravity_svc_disc_count);
        /* Append dev */
        newQ[gravity_svc_disc_count] = dev;
        /* Replace queue */
        if (gravity_svc_disc_count > 0) {
            free(gravity_svc_disc_q);
        }
        gravity_svc_disc_q = newQ;
        ++gravity_svc_disc_count;

        #ifdef CONFIG_DEBUG_VERBOSE
            char bda_str[18] = "";
            bda2str(gravity_svc_disc_q[gravity_svc_disc_count - 1]->bda, bda_str, 18);
            printf("BT Service discovery %s, queueing BDA %s (%s).\nDiscover queue is %u elements at address %p\n",
                    btServiceDiscoveryActive?"Starting":"Stopped", bda_str, gravity_svc_disc_q[gravity_svc_disc_count - 1]->bdName, gravity_svc_disc_count, gravity_svc_disc_q);
        #endif
    } else {
        #ifdef CONFIG_DEBUG_VERBOSE
            char bda_str[18] = "";
            bda2str(dev->bda, bda_str, 18);
            printf("Starting service discovery for %s (%s) starting.\nQueue length %u, address %p.\n",
                    bda_str, dev->bdName, gravity_svc_disc_count, gravity_svc_disc_q);
        #endif
        /* Simply start service discovery */
        btServiceDiscoveryActive = true;
        return esp_bt_gap_get_remote_services(dev->bda);
    }
    return ESP_OK;
}

/* Discover all services for gravity_bt_devices */
esp_err_t gravity_bt_discover_services_for(app_gap_cb_t **devices, uint8_t deviceCount) {
    esp_err_t res = ESP_OK;

    #ifdef CONFIG_DEBUG_VERBOSE
        printf("In discover_services_for(), bt_dev_count is %u\n", deviceCount);
        for (int i = 0; i < deviceCount; ++i) {
            char strBda[18];
            char strEir[ESP_BT_GAP_EIR_DATA_LEN + 1];
            bda2str(devices[i]->bda, strBda, 18);
            memcpy(strEir, devices[i]->eir, devices[i]->eir_len);
            strEir[devices[i]->eir_len] = '\0';
            printf("Device %d:\t\tBDA \"%s\"\tCOD: %lu\tRSSI: %ld\nName Len: %u\tEIR Len: %u\tName: \"%s\"\nEIR: \"%s\"\n",i,strBda,devices[i]->cod, devices[i]->rssi,devices[i]->bdname_len, devices[i]->eir_len, devices[i]->bdName, strEir);
        }
    #endif

    for (int i = 0; i < deviceCount; ++i) {
        char bda_str[18];
        printf("Requesting services for %s\n", devices[i]->bdname_len > 0?devices[i]->bdName:bda2str(devices[i]->bda, bda_str, 18));
        res |= gravity_bt_discover_services(devices[i]);
    }
    return res;
}

/* Discover all services for gravity_bt_devices */
esp_err_t gravity_bt_discover_all_services() {
    return gravity_bt_discover_services_for(gravity_bt_devices, gravity_bt_dev_count);
}

/* Discover all services for selected devices */
esp_err_t gravity_bt_discover_selected_services() {
    return gravity_bt_discover_services_for(gravity_selected_bt, gravity_sel_bt_count);
}

/* Discover Classic Bluetooth services offered by the specified device
   device specifies the target Bluetooth device. If creating your own rather than using scan results,
   the only relevant attribute is BDA. Note that this is a pointer.
   To discover services for all devices provide NULL for device.
*/
esp_err_t gravity_bt_gap_services_discover(app_gap_cb_t *device) {
    esp_err_t err = ESP_OK;
    if (!btInitialised) {
        gravity_bt_initialise();
    }

    /* A little validation to be user-friendly */
    bool deviceFound = false;
    if (device != NULL) {
        for (int i = 0; i < gravity_bt_dev_count && !deviceFound; ++i) {
            deviceFound = !memcmp(gravity_bt_devices[i]->bda, device->bda, ESP_BD_ADDR_LEN);
        }
    }
    /* Display a warning if there are no BT devices, or there are but the specified device is non-NULL and not found */
    if (gravity_bt_dev_count == 0 || (device != NULL && !deviceFound)) {
        char dev_bda[18];
        bda2str(device->bda, dev_bda, 18);
        #ifdef CONFIG_FLIPPER
            printf("Specified Device\n%25s\nNot in Gravity scan results. Services will not be stored.\n", dev_bda);
        #else
            ESP_LOGW(BT_TAG, "Specified Device %s not in Gravity scan results. Services will not be stored.", dev_bda);
        #endif
    }
    if (device == NULL) {
        return gravity_bt_discover_all_services();
    } else {
        return gravity_bt_discover_services(device);
    }
}

/* Display status of bluetooth scanning */
esp_err_t gravity_bt_scan_display_status() {
    esp_err_t err = ESP_OK;

    #ifdef CONFIG_FLIPPER
        printf("BT Classic %s, BLE %s\n%u Devices Discovered\n", attack_status[ATTACK_SCAN_BT_DISCOVERY]?"ON":"OFF", attack_status[ATTACK_SCAN_BLE]?"ON":"OFF", gravity_bt_dev_count);
    #else
        ESP_LOGI(BT_TAG, "Bluetooth Classic Scanning %s, BLE Scanning %s\t%u Devices Discovered", attack_status[ATTACK_SCAN_BT_DISCOVERY]?"Active":"Inactive", attack_status[ATTACK_SCAN_BLE]?"Active":"Inactive", gravity_bt_dev_count);
    #endif

    return err;
}

esp_err_t gravity_bt_list_all_devices(bool hideExpiredPackets) {
    esp_err_t err = ESP_OK;

    if (gravity_bt_dev_count > 0) {
        err |= gravity_bt_list_devices(gravity_bt_devices, gravity_bt_dev_count, hideExpiredPackets);
    } else {
        #ifdef CONFIG_FLIPPER
            printf("No HCIs in scan results\n");
        #else
            ESP_LOGI(BT_TAG, "No STAs in scan results to display. Have you run scan?");
        #endif
    }
    return err;
}

esp_err_t gravity_bt_list_devices(app_gap_cb_t **devices, uint8_t deviceCount, bool hideExpiredPackets) {
    esp_err_t err = ESP_OK;

    char strBssid[18];
    char strTime[26];
    char strName[25]; /* Hold a substring of device name */
    char strScanType[18];
    unsigned long nowTime;
    unsigned long elapsed;
    double minutes;

    // Print header
    #ifdef CONFIG_FLIPPER
        printf(" ID | RSSI |      Name      | Class | LastSeen\n");
        printf("===|====|========|====|======\n");
    #else
        printf(" ID | RSSI | Name                   | BSSID             | Class    | Scan Method       | LastSeen\n");
        printf("====|======|========================|===================|==========|===================|===========================\n");
    #endif

    /* Apply the sort to selectedAPs */
    qsort(devices, deviceCount, sizeof(app_gap_cb_t *), &bt_comparator);

    // Display devices
    for (int deviceIdx = 0; deviceIdx < deviceCount; ++deviceIdx) {
        err |= mac_bytes_to_string(devices[deviceIdx]->bda, strBssid);

        /* Stringify timestamp */
        nowTime = clock();
        elapsed = (nowTime - devices[deviceIdx]->lastSeen) / CLOCKS_PER_SEC;
        #ifdef CONFIG_DISPLAY_FRIENDLY_AGE
            if (elapsed < 60.0) {
                strcpy(strTime, "Under a minute ago");
            } else if (elapsed < 3600.0) {
                sprintf(strTime, "%d %s ago", (int)elapsed/60, (elapsed >= 120)?"minutes":"minute");
            } else {
                sprintf(strTime, "%d %s ago", (int)elapsed/3600, (elapsed >= 7200)?"hours":"hour");
            }
        #else
            /* Display precise time */
            char strTmp[16] = "";
            unsigned long tmp = elapsed;
            strcpy(strTime, "");
            if (tmp >= 3600.0) {
                sprintf(strTmp, "%2dh ", (int)tmp/3600);
                strcat(strTime, strTmp);
                tmp = tmp % 3600;
            }
            if (tmp >= 60) {
                sprintf(strTmp, "%2dm ", (int)tmp/60);
                strcat(strTime, strTmp);
                tmp = tmp % 60;
            }
            sprintf(strTmp, "%2lds", tmp);
            strcat(strTime, strTmp);
        #endif
        /* Skip the rest of this loop iteration if the packet has expired */
        minutes = (elapsed / 60.0);
        if (hideExpiredPackets && scanResultExpiry != 0 && minutes >= scanResultExpiry) {
            continue;
        }

        // Shorten device name as necessary to display
        memset(strName, '\0', 25);
        strncpy(strName, devices[deviceIdx]->bdName, 25);
        #ifdef CONFIG_FLIPPER
            strName[16] = '\0';
        #endif

        /* COD has 7chars in Flipper, 10 in Console */
        char shortCod[11];
        uint8_t shortCodLen = 0;
        err |= cod2shortStr(devices[deviceIdx]->cod, shortCod, &shortCodLen);

        /* Stringify scanType for display to console */
        err |= bt_scanTypeToString(devices[deviceIdx]->scanType, strScanType);

        /* Finally, display */
        #ifdef CONFIG_FLIPPER
            printf("%s%2d | %4ld |%-16s|%-7s| %s\n", (devices[deviceIdx]->selected?"*":" "), devices[deviceIdx]->index, devices[deviceIdx]->rssi, strName, shortCod, strTime);
        #else
            printf("%s%2d | %4ld | %-22s | %-17s |%-10s| %-17s | %s\n", (devices[deviceIdx]->selected?"*":" "), devices[deviceIdx]->index, devices[deviceIdx]->rssi, strName, strBssid, shortCod, strScanType, strTime);
        #endif

    }
    return err;
}

/* Convert a gravity_bt_scan_t enum to a string */
/* The supplied strOutput must be an initialised char array of at least 18 bytes */
esp_err_t bt_scanTypeToString(gravity_bt_scan_t scanType, char *strOutput) {
    esp_err_t err = ESP_OK;
    switch (scanType) {
        case GRAVITY_BT_SCAN_CLASSIC_DISCOVERY:
            strcpy(strOutput, "Classic Discovery");
            break;
        case GRAVITY_BT_SCAN_BLE:
            strcpy(strOutput, "Bluetooth LE");
            break;
        case GRAVITY_BT_SCAN_TYPE_ACTIVE:
            strcpy(strOutput, "Active Sniffing");
            break;
        default:
            strcpy(strOutput, "***UNKNOWN***");
            break;
    }
    return err;
}

/* Comparison function for sorting of ScanResultAPs */
/* Provides a sort function that uses sortResults
*/
static int bt_comparator(const void *varOne, const void *varTwo) {
    /* Return 0 if they're identical */
    if (sortCount == 0) {
        return 0;
    }
    app_gap_cb_t *one = (app_gap_cb_t *)varOne;
    app_gap_cb_t *two = (app_gap_cb_t *)varTwo;
    if (sortCount == 1) {
        if (sortResults[0] == GRAVITY_SORT_AGE) {
            if (one->lastSeen == two->lastSeen) {
                return 0;
            } else if (one->lastSeen < two->lastSeen) {
                return -1;
            } else {
                return 1;
            }
        } else if (sortResults[0] == GRAVITY_SORT_RSSI) {
            if (one->rssi == two->rssi) {
                return 0;
            } else if (one->rssi < two->rssi) {
                return -1;
            } else {
                return 1;
            }
        } else if (sortResults[0] == GRAVITY_SORT_SSID) {
            if (one->bdName == NULL && two->bdName == NULL) {
                return 0;
            } else if (one->bdName == NULL) {
                return -1;
            } else if (two->bdName == NULL) {
                return 1;
            } else {
                return strcmp((char *)one->bdName, (char *)two->bdName);
            }
        }
    } else if (sortCount == 2) {
        if (sortResults[0] == GRAVITY_SORT_AGE) {
            if (one->lastSeen < two->lastSeen) {
                return -1;
            } else if (one->lastSeen > two->lastSeen) {
                return 1;
            } else {
                /* Return based on sortResults[1] */
                if (sortResults[1] == GRAVITY_SORT_RSSI) {
                    if (one->rssi == two->rssi) {
                        return 0;
                    } else if (one->rssi < two->rssi) {
                        return -1;
                    } else {
                        return 1;
                    }
                } else if (sortResults[1] == GRAVITY_SORT_SSID) {
                    if (one->bdName == NULL && two->bdName == NULL) {
                        return 0;
                    } else if (one->bdName == NULL) {
                        return -1;
                    } else if (two->bdName == NULL) {
                        return 1;
                    } else {
                        return strcmp((char *)one->bdName, (char *)two->bdName);
                    }
                }
            }
        } else if (sortResults[0] == GRAVITY_SORT_RSSI) {
            if (one->rssi < two->rssi) {
                return -1;
            } else if (one->rssi > two->rssi) {
                return 1;
            } else {
                /* Return based on sortResults[1] */
                if (sortResults[1] == GRAVITY_SORT_AGE) {
                    if (one->lastSeen == two->lastSeen) {
                        return 0;
                    } else if (one->lastSeen < two->lastSeen) {
                        return -1;
                    } else {
                        return 1;
                    }
                } else if (sortResults[1] == GRAVITY_SORT_SSID) {
                    if (one->bdName == NULL && two->bdName == NULL) {
                        return 0;
                    } else if (one->bdName == NULL) {
                        return -1;
                    } else if (two->bdName == NULL) {
                        return 1;
                    } else {
                        return strcmp((char *)one->bdName, (char *)two->bdName);
                    }
                }
            }
        } else if (sortResults[0] == GRAVITY_SORT_SSID) {
            if (one->bdName == NULL && two->bdName == NULL) {
                return 0;
            } else if (one->bdName == NULL) {
                return -1;
            } else if (two->bdName == NULL) {
                return 1;
            } else if (strcmp((char *)one->bdName, (char *)two->bdName)) {
                return strcmp((char *)one->bdName, (char *)two->bdName);
            } else {
                /* Return based on sortResults[1] */
                if (sortResults[1] == GRAVITY_SORT_AGE) {
                    if (one->lastSeen == two->lastSeen) {
                        return 0;
                    } else if (one->lastSeen < two->lastSeen) {
                        return -1;
                    } else {
                        return 1;
                    }
                } else if (sortResults[1] == GRAVITY_SORT_RSSI) {
                    if (one->rssi == two->rssi) {
                        return 0;
                    } else if (one->rssi < two->rssi) {
                        return -1;
                    } else {
                        return 1;
                    }
                }
            }
        }
    } else { /* sortCount == 3 */
        if (sortResults[0] == GRAVITY_SORT_AGE) {
            if (one->lastSeen < two->lastSeen) {
                return -1;
            } else if (one->lastSeen > two->lastSeen) {
                return 1;
            }
            if (sortResults[1] == GRAVITY_SORT_RSSI) {
                if (one->rssi < two->rssi) {
                    return -1;
                } else if (one->rssi > two->rssi) {
                    return 1;
                }
            } else if (sortResults[1] == GRAVITY_SORT_SSID) {
                if (one->bdName == NULL && two->bdName == NULL) {
                    return 0;
                } else if (one->bdName == NULL) {
                    return -1;
                } else if (two->bdName == NULL) {
                    return 1;
                } else if (strcmp((char *)one->bdName, (char *)two->bdName)) {
                    return strcmp((char *)one->bdName, (char *)two->bdName);
                }
            }
            /* Third layer of comparison */
            if (sortResults[2] == GRAVITY_SORT_RSSI) {
                if (one->rssi == two->rssi) {
                    return 0;
                } else if (one->rssi < two->rssi) {
                    return -1;
                } else {
                    return 1;
                }
            } else if (sortResults[2] == GRAVITY_SORT_SSID) {
                if (one->bdName == NULL && two->bdName == NULL) {
                    return 0;
                } else if (one->bdName == NULL) {
                    return -1;
                } else if (two->bdName == NULL) {
                    return 1;
                } else {
                    return strcmp((char *)one->bdName, (char *)two->bdName);
                }
            }
        } else if (sortResults[0] == GRAVITY_SORT_RSSI) {
            if (one->rssi < two->rssi) {
                return -1;
            } else if (one->rssi > two->rssi) {
                return 1;
            }
            if (sortResults[1] == GRAVITY_SORT_AGE) {
                if (one->lastSeen < two->lastSeen) {
                    return -1;
                } else if (one->lastSeen > two->lastSeen) {
                    return 1;
                }
            } else if (sortResults[1] == GRAVITY_SORT_SSID) {
                if (one->bdName == NULL && two->bdName == NULL) {
                    return 0;
                } else if (one->bdName == NULL) {
                    return -1;
                } else if (two->bdName == NULL) {
                    return 1;
                } else if (strcmp((char *)one->bdName, (char *)two->bdName)) {
                    return strcmp((char *)one->bdName, (char *)two->bdName);
                }
            }
            /* Third layer of comparison */
            if (sortResults[2] == GRAVITY_SORT_AGE) {
                if (one->lastSeen == two->lastSeen) {
                    return 0;
                } else if (one->lastSeen < two->lastSeen) {
                    return -1;
                } else {
                    return 1;
                }
            } else if (sortResults[2] == GRAVITY_SORT_SSID) {
                if (one->bdName == NULL && two->bdName == NULL) {
                    return 0;
                } else if (one->bdName == NULL) {
                    return -1;
                } else if (two->bdName == NULL) {
                    return 1;
                } else {
                    return strcmp((char *)one->bdName, (char *)two->bdName);
                }
            }
        } else if (sortResults[0] == GRAVITY_SORT_SSID) {
            if (one->bdName == NULL && two->bdName == NULL) {
                return 0;
            } else if (one->bdName == NULL) {
                return -1; // TODO: This logic will fail if both one->bdName and two->bdName == NULL
            } else if (two->bdName == NULL) {
                return 1;
            } else if (strcmp((char *)one->bdName, (char *)two->bdName)) {
                return strcmp((char *)one->bdName, (char *)two->bdName);
            } else if (sortResults[1] == GRAVITY_SORT_AGE) {
                if (one->lastSeen < two->lastSeen) {
                    return -1;
                } else if (one->lastSeen > two->lastSeen) {
                    return 1;
                }
            } else if (sortResults[1] == GRAVITY_SORT_RSSI) {
                if (one->rssi < two->rssi) {
                    return -1;
                } else if (one->rssi > two->rssi) {
                    return 1;
                }
            }
            /* Third layer of comparison */
            if (sortResults[2] == GRAVITY_SORT_AGE) {
                if (one->lastSeen == two->lastSeen) {
                    return 0;
                } else if (one->lastSeen < two->lastSeen) {
                    return -1;
                } else {
                    return 1;
                }
            } else if (sortResults[2] == GRAVITY_SORT_RSSI) {
                if (one->rssi == two->rssi) {
                    return 0;
                } else if (one->rssi < two->rssi) {
                    return -1;
                } else {
                    return 1;
                }
            }
        }
    }
    return 0; // TODO: Confirm no gaps?
}

esp_err_t gravity_clear_bt() {
    esp_err_t err = ESP_OK;

    /* Clear selected BT devices first to avoid having pointers to free'd memory */
    if (gravity_selected_bt != NULL) {
        free(gravity_selected_bt);
        gravity_selected_bt = NULL;
        gravity_sel_bt_count = 0;
    }

    if (gravity_bt_devices != NULL) {
        for (int i = 0; i < gravity_bt_dev_count; ++i) {
            if (gravity_bt_devices[i] != NULL) {
                if (gravity_bt_devices[i]->bdName != NULL) {
                    free(gravity_bt_devices[i]->bdName);
                }
                if (gravity_bt_devices[i]->eir != NULL) {
                    free(gravity_bt_devices[i]->eir);
                }
                free(gravity_bt_devices[i]);
            }
        }
        free(gravity_bt_devices);
        gravity_bt_devices = NULL;
    }
    gravity_bt_dev_count = 0;
    return err;
}

esp_err_t gravity_clear_bt_selected() {
    uint8_t newCount = gravity_bt_dev_count - gravity_sel_bt_count;

    if (newCount == gravity_bt_dev_count) {
        /* Nothing to do */
        #ifdef CONFIG_DEBUG
            #ifdef CONFIG_FLIPPER
                printf("No selected BT devices.\n");
            #else
                ESP_LOGI(BT_TAG, "No selected BT devices. Nothing to clear.");
            #endif
        #endif
        return ESP_OK;
    }

    if (gravity_bt_devices == NULL || gravity_bt_dev_count == 0) {
        #ifdef CONFIG_FLIPPER
            printf("No BT devices discovered.\n");
        #else
            ESP_LOGI(BT_TAG, "No Bluetooth devices discovered, nothing to clear.");
        #endif
        return ESP_OK;
    }

    /* Clear the selected array to start */
    if (gravity_selected_bt != NULL) {
        free(gravity_selected_bt);
        gravity_selected_bt = NULL;
        gravity_sel_bt_count = 0;
    }

    for (int i = 0; i < gravity_bt_dev_count; ++i) {
        if (gravity_bt_devices[i] != NULL && gravity_bt_devices[i]->selected) {
            #ifdef CONFIG_DEBUG
                char bda_str[18] = "";
                bda2str(gravity_bt_devices[i]->bda, bda_str, 18);
                printf("%s (%d) is selected\n", (gravity_bt_devices[i]->bdname_len > 0)?gravity_bt_devices[i]->bdName:bda_str, i);
            #endif
            /* Element i is selected, free it */
            if (gravity_bt_devices[i]->bdName != NULL) {
                free(gravity_bt_devices[i]->bdName);
            }
            if (gravity_bt_devices[i]->eir != NULL) {
                free(gravity_bt_devices[i]->eir);
            }
            free(gravity_bt_devices[i]);
            gravity_bt_devices[i] = NULL;
        }
    }

    return gravity_bt_shrink_devices();
}

esp_err_t gravity_select_bt(uint8_t selIndex) {
    esp_err_t err = ESP_OK;
    app_gap_cb_t **newSel = NULL;

    /* Find the device */
    uint8_t devIdx = 0;
    for ( ; devIdx < gravity_bt_dev_count && gravity_bt_devices[devIdx]->index != selIndex; ++devIdx) { }
    if (devIdx < gravity_bt_dev_count) {
        /* Found the device - Invert its selected status */
        gravity_bt_devices[devIdx]->selected = !gravity_bt_devices[devIdx]->selected;
        /* Are we adding to, or removing from, gravity_selected_bt ? */
        if (gravity_bt_devices[devIdx]->selected) {
            /* Adding to gravity_selected_bt */
            if (newSel != NULL) {
                free(newSel);
            }
            newSel = malloc(sizeof(app_gap_cb_t *) * ++gravity_sel_bt_count);
            if (newSel == NULL) {
                #ifdef CONFIG_FLIPPER
                    printf("%sfor %d pointers.\n", STRINGS_MALLOC_FAIL, gravity_sel_bt_count);
                #else
                    ESP_LOGE(BT_TAG, "%sfor %d pointers.", STRINGS_MALLOC_FAIL, gravity_sel_bt_count);
                #endif
                return ESP_ERR_NO_MEM;
            }
            /* Copy across the old elements */
            memcpy(newSel, gravity_selected_bt, sizeof(app_gap_cb_t *) * (gravity_sel_bt_count - 1));
            newSel[gravity_sel_bt_count - 1] = gravity_bt_devices[devIdx];

            if (gravity_selected_bt != NULL && gravity_sel_bt_count > 1) {
                free(gravity_selected_bt);
            }
            gravity_selected_bt = newSel;
        } else {
            /* Removing device from gravity_selected_bt */
            --gravity_sel_bt_count;
            if (gravity_sel_bt_count > 0) {
                newSel = malloc(sizeof(app_gap_cb_t *) * gravity_sel_bt_count);
                if (newSel == NULL) {
                    #ifdef CONFIG_FLIPPER
                        printf("%sfor %d pointers\n", STRINGS_MALLOC_FAIL, gravity_sel_bt_count);
                    #else
                        ESP_LOGE(BT_TAG, "%sfor %d pointers.", STRINGS_MALLOC_FAIL, gravity_sel_bt_count);
                    #endif
                    return ESP_ERR_NO_MEM;
                }
                /* Copy across selected indices */
                int numSelected = 0;
                for (int i = 0; i < gravity_bt_dev_count; ++i) {
                    if (gravity_bt_devices[i]->selected) {
                        newSel[numSelected++] = gravity_bt_devices[i];
                    }
                }
                if (numSelected != gravity_sel_bt_count) {
                    #ifdef CONFIG_FLIPPER
                        printf("Expected %d selected Bluetooth devices, found %d.\n", gravity_sel_bt_count, numSelected);
                    #else
                        ESP_LOGE(BT_TAG, "Expected %d selected Bluetooth devices, found %d.", gravity_sel_bt_count, numSelected);
                    #endif
                }
                /* We're done at last */
                if (gravity_selected_bt != NULL) {
                    free(gravity_selected_bt);
                }
                gravity_selected_bt = newSel;
            } else {
                /* We probably just made it 0 */
                if (gravity_selected_bt != NULL) {
                    free(gravity_selected_bt);
                    gravity_selected_bt = NULL;
                }
            }
        }
    } else {
        /* No such device */
        #ifdef CONFIG_FLIPPER
            printf("No BT Device with index %d\n", selIndex);
        #else
            ESP_LOGE(BT_TAG, "No Bluetooth device exists with the specified index (%d).", selIndex);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    return err;
}

bool gravity_bt_isSelected(uint8_t selIndex) {
    /* Find selIndex */
    int i = 0;
    for ( ; i < gravity_bt_dev_count && gravity_bt_devices[i]->index != selIndex; ++i) { }
    if (i < gravity_bt_dev_count) {
        /* We found an item with the specified index */
        return (gravity_bt_devices[i]->selected);
    }
    return false;
}

esp_err_t gravity_bt_disable_scan() {
    esp_err_t err = ESP_OK;

    if (attack_status[ATTACK_SCAN_BT_DISCOVERY]) {
        err |= esp_bt_gap_cancel_discovery();
    }
    if (attack_status[ATTACK_SCAN_BLE]) {
        err |= esp_ble_gap_stop_scanning();
    }
    return err;
}

esp_err_t bt_service_rm_dev(app_gap_cb_t *device) {
    esp_err_t err = ESP_OK;
    grav_bt_svc *services = &(device->bt_services);
    if (services->num_services > 0) {
        free(services->service_uuids);
        services->service_uuids = NULL;
        services->num_services = 0;
    }
    if (services->known_services_len > 0) {
        free(services->known_services);
        services->known_services = NULL;
        services->known_services_len = 0;
    }
    return err;
}

/* Remove all BT services from the specified devices
   This function will release all memory reserved by services for the specified devices.
*/
esp_err_t bt_service_rm(app_gap_cb_t **devices, uint8_t devCount) {
    esp_err_t err = ESP_OK;
    for (int i = 0; i < devCount; ++i) {
        err |= bt_service_rm_dev(devices[i]);
    }
    return err;
}

esp_err_t bt_service_rm_all() {
    return bt_service_rm(gravity_bt_devices, gravity_bt_dev_count);
}

/* Purge all BLE records with the earliest lastSeen, unless lastSeen
   is > CONFIG_BLE_PURGE_MIN_AGE
*/
esp_err_t purgeAge(GravityDeviceType devType, uint16_t purge_min_age) {
    esp_err_t err = ESP_OK;
    uint8_t newCount = 0;
    gravity_bt_scan_t scanType = GRAVITY_BT_SCAN_TYPE_COUNT;

    switch (devType) {
        case GRAVITY_DEV_BT:
            scanType = GRAVITY_BT_SCAN_CLASSIC_DISCOVERY;
            break;
        case GRAVITY_DEV_BLE:
            scanType = GRAVITY_BT_SCAN_BLE;
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Bluetooth: Invalid device type %d\n", devType);
            #else
                ESP_LOGE(BT_TAG, "Invalid device type %d", devType);
            #endif
            return ESP_ERR_INVALID_ARG;
    }

    /* Find the oldest age */
    clock_t minAge = -1;
    for (int i = 0; i < gravity_bt_dev_count; ++i) {
        if (gravity_bt_devices[i]->scanType == scanType && (minAge == -1 ||
                gravity_bt_devices[i]->lastSeen < minAge)) {
            minAge = gravity_bt_devices[i]->lastSeen;
        }
    }
    if (minAge == -1) {
        /* No valid devices found. Nothing to do */
        return ESP_ERR_NOT_FOUND;
    }

    /* We're done with this purge strategy if the oldest age is less than the minimum purge age */
    /* Calculate elapsed time in seconds to test this */
    clock_t nowTime = clock();
    double elapsed = (nowTime - minAge) / CLOCKS_PER_SEC;
    if (elapsed < purge_min_age) {
        return ESP_ERR_NOT_FOUND;
    }

    /* If we're still here, remove all BLE records lastSeen at (or before, to cater for minAge dodginess) minAge */
    for (int i = 0; i < gravity_bt_dev_count; ++i) {
        if (gravity_bt_devices[i]->scanType == scanType && gravity_bt_devices[i]->lastSeen <= minAge) {
            /* Don't forget to free the name and EIR */
            if (gravity_bt_devices[i]->bdName != NULL && gravity_bt_devices[i]->bdname_len > 0) {
                free(gravity_bt_devices[i]->bdName);
            }
            if (gravity_bt_devices[i]->eir != NULL && gravity_bt_devices[i]->eir_len > 0) {
                free(gravity_bt_devices[i]->eir);
            }
            free(gravity_bt_devices[i]);
            gravity_bt_devices[i] = NULL;
        }
    }
    /* Finally, generate a new gravity_bt_devices with those elements removed */
    err = gravity_bt_shrink_devices();

    return err;
}

/* Purge all BLE records with the smallest RSSI, unless that
   is > CONFIG_GRAVITY_BLE_PURGE_MIN_RSSI, then return ESP_ERR_NOT_FOUND */
esp_err_t purgeRSSI(GravityDeviceType devType, int32_t purge_max_rssi) {
    esp_err_t err = ESP_OK;
    uint8_t newCount = 0;
    gravity_bt_scan_t scanType = GRAVITY_BT_SCAN_TYPE_COUNT;

    switch (devType) {
        case GRAVITY_DEV_BT:
            scanType = GRAVITY_BT_SCAN_CLASSIC_DISCOVERY;
            break;
        case GRAVITY_DEV_BLE:
            scanType = GRAVITY_BT_SCAN_BLE;
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Bluetooth: Invalid device type %d\n", devType);
            #else
                ESP_LOGE(BT_TAG, "Invalid device type %d", devType);
            #endif
            return ESP_ERR_INVALID_ARG;
    }

    /* Find the smallest RSSI */
    int32_t smallRSSI = 0; /* Invalid */
    for (int i = 0; i < gravity_bt_dev_count; ++i) {
        if (gravity_bt_devices[i]->rssi < smallRSSI) {
            smallRSSI = gravity_bt_devices[i]->rssi;
        }
    }

    /* Fail this purge method if smallest RSSI is larger than the min RSSI */
    if (smallRSSI > purge_max_rssi) {
        return ESP_ERR_NOT_FOUND;
    }

    /* Free any elements with the smallest RSSI */
    for (int i = 0; i < gravity_bt_dev_count; ++i) {
        if (gravity_bt_devices[i]->scanType == scanType && gravity_bt_devices[i]->rssi == smallRSSI) {
            /* Delete the object */
            if (gravity_bt_devices[i]->bdName != NULL && gravity_bt_devices[i]->bdname_len > 0) {
                free(gravity_bt_devices[i]->bdName);
            }
            if (gravity_bt_devices[i]->eir != NULL && gravity_bt_devices[i]->eir_len > 0) {
                free(gravity_bt_devices[i]->eir);
            }
            free(gravity_bt_devices[i]);
            gravity_bt_devices[i] = NULL;
        } else {
            ++newCount;
        }
    }

    if (newCount == 0) {
        #ifdef CONFIG_FLIPPER
            printf("Purging lowest RSSI.\nNo remaining %s devices.\n", (devType == GRAVITY_DEV_BLE)?"BLE":"BT");
        #else
            ESP_LOGI(BT_TAG, "Purging devices with lowest RSSI... There are no remaining %s devices, %u have been purged.", (devType == GRAVITY_DEV_BLE)?"BLE":"BT", gravity_bt_dev_count);
        #endif
        /* Free gravity_bt_devices */
        if (gravity_bt_devices != NULL) {
            free(gravity_bt_devices);
            gravity_bt_devices = NULL;
            gravity_bt_dev_count = 0;
        }
    } else {
        /* Shrink the NULLs out of gravity_bt_devices */
        err = gravity_bt_shrink_devices();
    }
    return err;
}

/* Purge all BLE devices that are not selected */
esp_err_t purgeUnselected(GravityDeviceType devType) {
    esp_err_t err = ESP_OK;
    uint8_t newCount = 0;
    gravity_bt_scan_t scanType = GRAVITY_BT_SCAN_TYPE_COUNT;

    switch (devType) {
        case GRAVITY_DEV_BT:
            scanType = GRAVITY_BT_SCAN_CLASSIC_DISCOVERY;
            break;
        case GRAVITY_DEV_BLE:
            scanType = GRAVITY_BT_SCAN_BLE;
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Bluetooth: Invalid device type %d\n", devType);
            #else
                ESP_LOGE(BT_TAG, "Invalid device type %d", devType);
            #endif
            return ESP_ERR_INVALID_ARG;
    }

    /* Take a creative, low-memory, way to remove unselected elements from gravity_bt_devices
       Loop through gravity_bt_devices. Along the way count new devices (selected or not BLE)
       and free the bdname, eir and app_gap_cb_t */
    for (int i = 0; i < gravity_bt_dev_count; ++i) {
        if (gravity_bt_devices[i]->scanType != scanType || gravity_bt_devices[i]->selected) {
            ++newCount;
        } else {
            /* Element is specified type and not selected */
            if (gravity_bt_devices[i]->bdname_len > 0 && gravity_bt_devices[i]->bdName != NULL) {
                free(gravity_bt_devices[i]->bdName);
            }
            if (gravity_bt_devices[i]->eir_len > 0 && gravity_bt_devices[i]->eir != NULL) {
                free(gravity_bt_devices[i]->eir);
            }
            free(gravity_bt_devices[i]);
            gravity_bt_devices[i] = NULL;
        }
    }

    if (newCount == 0) {
        #ifdef CONFIG_FLIPPER
            printf("Purging unselected devices.\nNo remaining %s devices.\n", (devType == GRAVITY_DEV_BLE)?"BLE":"BT");
        #else
            ESP_LOGI(BT_TAG, "Purging unselected devices... There are no selected %s devices, %u have been purged.",(devType == GRAVITY_DEV_BLE)?"BLE":"BT", gravity_bt_dev_count);
        #endif
        if (gravity_bt_devices != NULL) {
            free(gravity_bt_devices);
            gravity_bt_devices = NULL;
            gravity_bt_dev_count = 0;
        }
    } else {
        /* Shorten gravity_bt_devices */
        err = gravity_bt_shrink_devices();
    }
    return err;
}

/* Purge all BLE devices that don't have a name */
esp_err_t purgeUnnamed(GravityDeviceType devType) {
    esp_err_t err = ESP_OK;
    uint8_t newCount = 0;
    gravity_bt_scan_t scanType = GRAVITY_BT_SCAN_TYPE_COUNT;

    switch (devType) {
        case GRAVITY_DEV_BT:
            scanType = GRAVITY_BT_SCAN_CLASSIC_DISCOVERY;
            break;
        case GRAVITY_DEV_BLE:
            scanType = GRAVITY_BT_SCAN_BLE;
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Bluetooth: Invalid device type %d\n", devType);
            #else
                ESP_LOGE(BT_TAG, "Invalid device type %d", devType);
            #endif
            return ESP_ERR_INVALID_ARG;
    }

    /* To minimise memory usage, first free any app_gap_cb_t elements that can be purged */
    for (int i = 0; i < gravity_bt_dev_count; ++i) {
        if (gravity_bt_devices[i]->scanType != scanType || (gravity_bt_devices[i]->bdname_len > 0 &&
                gravity_bt_devices[i]->bdName != NULL)) {
            ++newCount;
        } else {
            /* Element is devType and has no name */
            if (gravity_bt_devices[i]->bdName != NULL) {
                free(gravity_bt_devices[i]->bdName);
            }
            free(gravity_bt_devices[i]);
            gravity_bt_devices[i] = NULL;
        }
    }
    if (newCount == 0) {
        #ifdef CONFIG_FLIPPER
            printf("Purging unnamed devices.\nNo remaining %s devices.\n", (devType == GRAVITY_DEV_BLE)?"BLE":"BT");
        #else
            ESP_LOGI(BT_TAG, "Purging unnamed devices... There are no named %s devices, %u have been purged.", (devType == GRAVITY_DEV_BLE)?"BLE":"BT", gravity_bt_dev_count);
        #endif
        if (gravity_bt_devices != NULL) {
            free(gravity_bt_devices);
            gravity_bt_devices = NULL;
            gravity_bt_dev_count = 0;
        }
    } else {
        /* Shorten gravity_bt_devices */
        err = gravity_bt_shrink_devices();
    }
    return err;
}

/* Manually execute purging functions for BLE devices */
esp_err_t purge(GravityDeviceType devType, gravity_bt_purge_strategy_t strategy, uint16_t minAge, int32_t maxRssi) {
    esp_err_t err = ESP_OK;
    if ((strategy & GRAVITY_BLE_PURGE_RSSI) == GRAVITY_BLE_PURGE_RSSI) {
        /* Re-run purge RSSI until there are no more devices with RSSIs below the cutoff */
        while (purgeRSSI(devType, maxRssi) != ESP_ERR_NOT_FOUND) { }
    }
    if ((strategy & GRAVITY_BLE_PURGE_AGE) == GRAVITY_BLE_PURGE_AGE) {
        /* Likewise for age */
        while (purgeAge(devType, minAge) != ESP_ERR_NOT_FOUND) { }
    }
    if ((strategy & GRAVITY_BLE_PURGE_UNNAMED) == GRAVITY_BLE_PURGE_UNNAMED) {
        err |= purgeUnnamed(devType);
    }
    if ((strategy & GRAVITY_BLE_PURGE_UNSELECTED) == GRAVITY_BLE_PURGE_UNSELECTED) {
        err |= purgeUnselected(devType);
    }
    return err;
}

esp_err_t purgeBLE(gravity_bt_purge_strategy_t strategy, uint16_t minAge, int32_t maxRssi) {
    return purge(GRAVITY_DEV_BLE, strategy, minAge, maxRssi);
}

/* Manually execute purging functions for Bluetooth Classic devices */
esp_err_t purgeBT(gravity_bt_purge_strategy_t strategy, uint16_t minAge, int32_t maxRssi) {
    return purge(GRAVITY_DEV_BT, strategy, minAge, maxRssi);
}

/* Purge BLE devices based on purgeStrategy until the requested malloc can be completed.
   Returns a pointer to the allocated memory. If Gravity cannot free sufficient memory
   using the specified purge strategies NULL will be returned.
   It is expected that this NULL will signal SCAN to cease BLE scanning.
   gravity_bt_purge_strategy_t purgeStrategy = GRAVITY_BLE_PURGE_NONE;
   This variable is used by the function if memory allocation for the new device fails
*/
void *gravity_ble_purge_and_malloc(uint16_t bytes) {
    void *retVal = malloc(bytes);
    gravity_bt_purge_strategy_t purgeAttempts = purgeStrategy;
    esp_err_t err = ESP_OK;
    while (retVal == NULL) {
        if ((purgeAttempts & GRAVITY_BLE_PURGE_RSSI) == GRAVITY_BLE_PURGE_RSSI) {
            /* Purge the lowest RSSI BLE devices from Gravity */
            err = purgeRSSI(GRAVITY_DEV_BLE, PURGE_MAX_RSSI);
            if (err == ESP_ERR_NOT_FOUND) {
                /* There's nothing left that we can purge */
                purgeAttempts = (purgeAttempts ^ GRAVITY_BLE_PURGE_RSSI); /* XOR out GRAVITY_BLE_PURGE_RSSI */
            }
        } else if ((purgeAttempts & GRAVITY_BLE_PURGE_AGE) == GRAVITY_BLE_PURGE_AGE) {
            /* Purge the oldest BLE devices from Gravity */
            err = purgeAge(GRAVITY_DEV_BLE, PURGE_MIN_AGE);
            if (err == ESP_ERR_NOT_FOUND) {
                /* If no more devices can be purged XOR out GRAVITY_BLE_PURGE_AGE */
                purgeAttempts = (purgeAttempts ^ GRAVITY_BLE_PURGE_AGE);
            }
        } else if ((purgeAttempts & GRAVITY_BLE_PURGE_UNNAMED) == GRAVITY_BLE_PURGE_UNNAMED) {
            /* Remove any unnamed BLE elements from bluetooth array */
            err = purgeUnnamed(GRAVITY_DEV_BLE);
            if (err != ESP_OK) {
                #ifdef CONFIG_FLIPPER
                    printf("Error purging unnamed BLE:\n%s\n", esp_err_to_name(err));
                #else
                    ESP_LOGE(BT_TAG, "An error occurred purging unnamed BLE devices: %s.", esp_err_to_name(err));
                #endif
            }
            /* This isn't an iterative process - All devices are removed at the same time
               Remove UNNAMED from the list of purge strategies to try for this function
               call. ^ == XOR */
            purgeAttempts = (purgeAttempts ^ GRAVITY_BLE_PURGE_UNNAMED);
        } else if ((purgeAttempts & GRAVITY_BLE_PURGE_UNSELECTED) == GRAVITY_BLE_PURGE_UNSELECTED) {
            /* Remove any BLE elements that are not selected */
            err = purgeUnselected(GRAVITY_DEV_BLE);
            if (err != ESP_OK) {
                #ifdef CONFIG_FLIPPER
                    printf("Error purging unselected BLE:\n%s\n", esp_err_to_name(err));
                #else
                    ESP_LOGW(BT_TAG, "An error occurred purging unselected BLE devices: %s.", esp_err_to_name(err));
                #endif
            }
            /* This strategy has been tried - Remove it from the todo list */
            purgeAttempts = (purgeAttempts ^ GRAVITY_BLE_PURGE_UNSELECTED);
        } else if ((purgeAttempts & GRAVITY_BLE_PURGE_NONE) == GRAVITY_BLE_PURGE_NONE) {
            /* Purge has been disabled - No action */
            return NULL;
        } else if (purgeAttempts == GRAVITY_BLE_PURGE_IDLE) {
            /* purgeAttempts is down to 0 - There's nothing more we can do */
            break;
        } else {
            /* What is this? */
            #ifdef CONFIG_FLIPPER
                printf("Invalid PURGE type: %u\n", purgeAttempts);
            #else
                ESP_LOGE(BT_TAG, "Gravity encountered an invalid gravity_bt_purge_strategy_t: %u.", purgeAttempts);
            #endif
            return NULL;
        }
        // If there's nothing else I can purge
        //    break;
        // Purge stuff
        retVal = malloc(bytes);
    };
    return retVal;
}

/* Shrink gravity_bt_devices to remove gaps left after purging elements
   This function will regenerate a new instance of gravity_bt_devices,
   removing any elements of gravity_bt_devices that are NULL.
*/
esp_err_t gravity_bt_shrink_devices() {
    esp_err_t err = ESP_OK;
    uint8_t targetCount = 0; /* Number of elements at end of function */
    app_gap_cb_t **newDevices = NULL;

    /* Count number of elements in result */
    for (int i = 0; i < gravity_bt_dev_count; ++i) {
        if (gravity_bt_devices[i] != NULL) {
            ++targetCount;
        }
    }
    /* Allocate memory for new gravity_bt_devices if it has any elements */
    if (targetCount > 0) {
        newDevices = malloc(sizeof(app_gap_cb_t *) * targetCount);
        if (newDevices == NULL) {
            #ifdef CONFIG_FLIPPER
                printf("Unable to allocate memory for %u pointers.\n", targetCount);
            #else
                ESP_LOGE(BT_TAG, "Unable to allocate memory for %u BT pointers.", targetCount);
            #endif
            return ESP_ERR_NO_MEM;
        }
    }
    /* Copy across pointers from gravity_bt_devices into newDevices */
    targetCount = 0;
    for (int i = 0; i < gravity_bt_dev_count; ++i) {
        if (gravity_bt_devices[i] != NULL) {
            newDevices[targetCount++] = gravity_bt_devices[i];
        }
    }
    /* Free and replace gravity_bt_devices */
    if (gravity_bt_devices != NULL && gravity_bt_dev_count > 0) {
        free(gravity_bt_devices);
    }
    gravity_bt_devices = newDevices;
    gravity_bt_dev_count = targetCount;

    return err;
}

/* purgeStrategyToString
   Convert the specified purge strategy to a string
   strategy is any valid (composited) strategy
   strOutput is at least 56 bytes of initialised memory
   The function also returns a pointer to strOutput
*/
char *purgeStrategyToString(gravity_bt_purge_strategy_t strategy, char *strOutput) {
    strcpy(strOutput, "");
    if ((strategy & GRAVITY_BLE_PURGE_RSSI) == GRAVITY_BLE_PURGE_RSSI) {
        strcat(strOutput, "PURGE_RSSI");
    }
    if ((strategy & GRAVITY_BLE_PURGE_AGE) == GRAVITY_BLE_PURGE_AGE) {
        if (strlen(strOutput) > 1) {
            strcat(strOutput, ", ");
        }
        strcat(strOutput, "PURGE_AGE");
    }
    if ((strategy & GRAVITY_BLE_PURGE_UNNAMED) == GRAVITY_BLE_PURGE_UNNAMED) {
        if (strlen(strOutput) > 1) {
            strcat(strOutput, ", ");
        }
        strcat(strOutput, "PURGE_UNNAMED");
    }
    if ((strategy & GRAVITY_BLE_PURGE_UNSELECTED) == GRAVITY_BLE_PURGE_UNSELECTED) {
        if (strlen(strOutput) > 1) {
            strcat(strOutput, ", ");
        }
        strcat(strOutput, "PURGE_UNSELECTED");
    }
    if ((strategy & GRAVITY_BLE_PURGE_NONE) == GRAVITY_BLE_PURGE_NONE) {
        if (strlen(strOutput) > 1) {
            strcat(strOutput, ", ");
        }
        strcat(strOutput, "PURGE_NONE");
    }
    return strOutput;
}

#endif
