/**
 * @file bt_spp.c
 * @brief BLE GATT UART Service using NimBLE for ESP32-C6 (ESP-IDF 5.5)
 */

#include "bt_spp.h"
#include "msg_queue.h"

#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

/* NimBLE headers for ESP-IDF 5.x */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BT_BLE";

/* NUS (Nordic UART Service) UUIDs */
static const ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

static const ble_uuid128_t gatt_svr_chr_rx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

static const ble_uuid128_t gatt_svr_chr_tx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

/* 连接状态 */
typedef struct {
    bool connected;
    uint16_t conn_handle;
    uint16_t tx_attr_handle;
    bool notify_enabled;
} bt_ble_state_t;

/* 指令缓冲区 */
typedef struct {
    char buffer[BT_CMD_MAX_LEN];
    uint8_t len;
} bt_cmd_buffer_t;

static bt_ble_state_t s_ble_state = {0};
static bt_cmd_buffer_t s_cmd_buffer = {0};
static uint8_t own_addr_type;

/* 前向声明 */
static void handle_odtc_command(void);
static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static void ble_advertise(void);
static int ble_gap_event(struct ble_gap_event *event, void *arg);

/**
 * @brief 解析接收到的BLE数据
 */
static void parse_command(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return;
    }

    for (uint16_t i = 0; i < len; i++) {
        char c = (char)data[i];
        
        if (s_cmd_buffer.len >= BT_CMD_MAX_LEN - 1) {
            ESP_LOGW(TAG, "Buffer overflow, resetting");
            s_cmd_buffer.len = 0;
            memset(s_cmd_buffer.buffer, 0, sizeof(s_cmd_buffer.buffer));
        }
        
        s_cmd_buffer.buffer[s_cmd_buffer.len++] = c;
        s_cmd_buffer.buffer[s_cmd_buffer.len] = '\0';
        
        if (s_cmd_buffer.len >= 4) {
            char *cmd_start = s_cmd_buffer.buffer + s_cmd_buffer.len - 4;
            if (strncmp(cmd_start, BT_CMD_OPEN_DOOR, 4) == 0) {
                ESP_LOGI(TAG, "ODTC command detected");
                handle_odtc_command();
                s_cmd_buffer.len = 0;
                memset(s_cmd_buffer.buffer, 0, sizeof(s_cmd_buffer.buffer));
            }
        }
    }
}

/**
 * @brief 处理ODTC开门指令
 */
static void handle_odtc_command(void)
{
    bool sent = msg_send_key_event(QUEUE_PWM, 0, KEY_EVENT_DOUBLE_CLICK);
    
    if (sent) {
        ESP_LOGI(TAG, "ODTC executed, door opening");
        bt_spp_send(BT_RSP_OK, strlen(BT_RSP_OK));
    } else {
        ESP_LOGE(TAG, "Failed to send to PWM queue");
        bt_spp_send(BT_RSP_ERROR, strlen(BT_RSP_ERROR));
    }
}

/* GATT 服务定义 */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &gatt_svr_chr_rx_uuid.u,
                .access_cb = gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &gatt_svr_chr_tx_uuid.u,
                .access_cb = gatt_svr_chr_access,
                .val_handle = &s_ble_state.tx_attr_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 },
        },
    },
    { 0 },
};

/**
 * @brief GATT 特征访问回调
 */
static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    
    if (ble_uuid_cmp(uuid, &gatt_svr_chr_rx_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            uint8_t buf[BT_CMD_MAX_LEN];
            
            if (len > sizeof(buf)) len = sizeof(buf);
            
            if (ble_hs_mbuf_to_flat(ctxt->om, buf, len, NULL) == 0) {
                ESP_LOGI(TAG, "RX data, len=%d", len);
                parse_command(buf, len);
            }
        }
        return 0;
    }
    
    if (ble_uuid_cmp(uuid, &gatt_svr_chr_tx_uuid.u) == 0) {
        return 0;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief 开始广播
 */
static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)BT_DEVICE_NAME;
    fields.name_len = strlen(BT_DEVICE_NAME);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Set adv fields failed: rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 0x20;
    adv_params.itvl_max = 0x40;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start adv failed: rc=%d", rc);
        return;
    }
    
    ESP_LOGI(TAG, "Advertising started");
}

/**
 * @brief GAP 事件回调
 */
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                s_ble_state.connected = true;
                s_ble_state.conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "Connected, handle=%d", event->connect.conn_handle);
                
                /* 获取连接信息 */
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                if (rc == 0) {
                    ESP_LOGI(TAG, "Conn params: itvl=%d latency=%d timeout=%d",
                             desc.conn_itvl, desc.conn_latency, desc.supervision_timeout);
                }
                
                /* 更新连接参数以提高稳定性 */
                struct ble_gap_upd_params params = {
                    .itvl_min = 24,   /* 30ms (24 * 1.25ms) */
                    .itvl_max = 40,   /* 50ms (40 * 1.25ms) */
                    .latency = 0,
                    .supervision_timeout = 400,  /* 4s (400 * 10ms) */
                    .min_ce_len = 0,
                    .max_ce_len = 0,
                };
                ble_gap_update_params(event->connect.conn_handle, &params);
            } else {
                ESP_LOGE(TAG, "Connect failed, status=%d", event->connect.status);
                ble_advertise();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected, reason=%d", event->disconnect.reason);
            s_ble_state.connected = false;
            s_ble_state.conn_handle = 0;
            s_ble_state.notify_enabled = false;
            memset(&s_cmd_buffer, 0, sizeof(s_cmd_buffer));
            ble_advertise();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ble_advertise();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            s_ble_state.notify_enabled = event->subscribe.cur_notify;
            ESP_LOGI(TAG, "Notify %s", s_ble_state.notify_enabled ? "enabled" : "disabled");
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU=%d", event->mtu.value);
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            ESP_LOGI(TAG, "Conn params updated, status=%d", event->conn_update.status);
            break;

        default:
            break;
    }
    return 0;
}

static void ble_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Ensure addr failed: rc=%d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Infer addr type failed: rc=%d", rc);
        return;
    }

    ble_advertise();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset, reason=%d", reason);
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/**
 * @brief 初始化BLE服务
 */
esp_err_t bt_spp_init(void)
{
    int rc;

    ESP_LOGI(TAG, "Initializing BLE (NimBLE)...");

    /* 初始化NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化NimBLE (ESP-IDF 5.x) */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NimBLE init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置Host */
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* 初始化GATT服务 */
    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT count failed: rc=%d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT add svcs failed: rc=%d", rc);
        return ESP_FAIL;
    }

    ble_svc_gap_device_name_set(BT_DEVICE_NAME);

    /* 启动NimBLE Host任务 */
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE initialized, device: %s", BT_DEVICE_NAME);
    return ESP_OK;
}

bool bt_spp_is_connected(void)
{
    return s_ble_state.connected;
}

esp_err_t bt_spp_send(const char *data, size_t len)
{
    if (!s_ble_state.connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_ble_state.notify_enabled) {
        ESP_LOGW(TAG, "Notify not enabled");
        return ESP_ERR_INVALID_STATE;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int rc = ble_gatts_notify_custom(s_ble_state.conn_handle,
                                      s_ble_state.tx_attr_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Notify failed: rc=%d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}
