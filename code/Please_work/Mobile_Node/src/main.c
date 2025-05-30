/*
 * Copyright (c) 2018 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/shell/shell.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include <zephyr/data/json.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/net_buf.h>

/* Function Declarations*/
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);
static bool extract_rssi_values(struct net_buf_simple *ad, int8_t *rssi_values);



#define DEVICE_NAME "Mobile_Node_4703632"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)


static struct bt_conn *current_conn;
static struct bt_nus_cb nus_cb;


#define MAX_PATIENT_ID_LEN 4    
#define MAX_TIME_STR_LEN   6    
#define MAX_SAMPLES_PER_DAY 72




struct patient_sample {
    char patient_id[MAX_PATIENT_ID_LEN];  
    uint8_t heart_rate;                   
    float temperature;                    
    float co_level;
    uint8_t evoc;                     
    int count;          
};





static struct patient_sample samples_1[MAX_SAMPLES_PER_DAY];
static struct patient_sample samples_2[MAX_SAMPLES_PER_DAY];
static int current_sample_index = 0;



static bool is_our_patient1(struct net_buf_simple *ad)
{
    uint8_t len, type;
    while (ad->len > 1) {
        len = net_buf_simple_pull_u8(ad);
        if (len == 0) {
            break;
        }
        type = net_buf_simple_pull_u8(ad);
        
        if (type == BT_DATA_MANUFACTURER_DATA && len >= 4) {
            if (ad->data[0] == 0x4D && ad->data[1] == 0x00) { // 0x4D is our unqiue identifier for patient 1
                if (ad->data[2] == 0x02 && ad->data[3] == 0x15) {
                    return true;
                }
            }
        }
        net_buf_simple_pull(ad, len - 1);
    }
    return false;
}

static bool is_our_patient2(struct net_buf_simple *ad)
{
    uint8_t len, type;
    while (ad->len > 1) {
        len = net_buf_simple_pull_u8(ad);
        if (len == 0) {
            break;
        }
        type = net_buf_simple_pull_u8(ad);
        
        if (type == BT_DATA_MANUFACTURER_DATA && len >= 4) {
            if (ad->data[0] == 0x5D && ad->data[1] == 0x00) { // 0x5D is our unqiue identifier for patient 2
                if (ad->data[2] == 0x02 && ad->data[3] == 0x15) {
                    return true;
                }
            }
        }
        net_buf_simple_pull(ad, len - 1);
    }
    return false;
}


static int patient2_idx = 0;

static void parse_patient2(const uint8_t *mdata)
{
    if (patient2_idx >= MAX_SAMPLES_PER_DAY) {
        return;
    }
    struct patient_sample *s = &samples_2[patient2_idx++];
    strncpy(s->patient_id, "P02", MAX_PATIENT_ID_LEN);
    s->heart_rate  = mdata[4];
    s->temperature = (float)mdata[5];
    s->co_level    = (float)mdata[6];
    s->evoc        = mdata[7];
    s->count       = mdata[8];
    printk("mdata: %d, %d, %d, %d\n", mdata[4], mdata[5], mdata[6], mdata[7]);
    if (patient2_idx == MAX_SAMPLES_PER_DAY) {
        printk("✅ patient2 just reached full capacity (%d samples)\n", patient2_idx);
    }
}

static void print_all_samples(void)
{
    printk("=== All %d Samples Collected ===\n", MAX_SAMPLES_PER_DAY);
    for (int i = 0; i < MAX_SAMPLES_PER_DAY; i++) {
        struct patient_sample *s = &samples_2[i];
        
        printk("Sample %2d: ID=%s  HR=%3u  T=%.1f°C  CO=%.2f  EVOC=%3u  idx=%2d\n",
           i + 1,
           s->patient_id,
           s->heart_rate,
           s->temperature,
           s->co_level,
           s->evoc,
           s->count);
    }
}



static uint8_t last_count = -1;
static bool _adv_parse(struct bt_data *data, void *user_data)
{

    if (data->type == BT_DATA_MANUFACTURER_DATA && data->data_len >= 8) {
        const uint8_t *m = data->data;

        
        if (m[0] == 0x5D && m[1] == 0x00 && m[2] == 0x02 && m[3] == 0x15) {
            if (m[4] == 128) {
                return false;
            }
            if (patient2_idx < MAX_SAMPLES_PER_DAY) {
                int8_t count = m[8];
                printk("count: %d\n", count);
                if (count != last_count) {
                    last_count = count;
                    struct patient_sample *s = &samples_2[patient2_idx++];
                    memcpy(s->patient_id, "P02", 4);
                    s->heart_rate = m[4];
                    s->temperature= (float)m[5];
                    s->co_level   = (float)m[6  ];
                    
                    s->evoc       = m[7];
                    s->count      = count;

                    printk("Parsed #%d: HR=%u  CO2=%.0f  T=%.0f  EVOC=%u  CNT=%d\n",
                           patient2_idx,
                           s->heart_rate,
                           s->co_level,
                           s->temperature,
                           s->evoc,
                           s->count);
                }
                if (count == 71) {
                    printk("✅ patient2 buffer full (%d samples)\n",
                           patient2_idx);
                    print_all_samples();
                }
            }
            return false; 
        }
    }
    return true;  
}




static void device_found(const bt_addr_le_t *addr,
                         int8_t rssi, uint8_t type,
                         struct net_buf_simple *recv_ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    
    if (rssi < -50) {
        return;
    }
    
    bt_data_parse(recv_ad, _adv_parse, NULL);
}





static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed (err %u)\n", err);
    } else {
        printk("Connected\n");
        current_conn = bt_conn_ref(conn);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected (reason %u)\n", reason);
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};


static void nus_received(struct bt_conn *conn, const uint8_t *const data,
                         uint16_t len)
{
    printk("NUS received data: %.*s\n", len, data);
}

static void nus_sent(struct bt_conn *conn)
{
    printk("NUS data sent\n");
}

static void notif_enabled(bool enabled, void *ctx)
{
    printk("Notifications %s\n", enabled ? "enabled" : "disabled");
}






static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

bool sending_started = false;


static void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
    printk("MTU updated: TX %u, RX %u\n", tx, rx);
}


int send_patient_data(uint8_t patient_id,
                      uint16_t hr,
                      float temperature,
                      float co_level,
                      uint8_t evoc,
                      int count)
{

    int len;
    char msg[17];
    len = snprintf(msg, sizeof(msg),
                       "%1d%03u%04.1f%04.1f%02u%02d",
                       patient_id,
                       hr,
                       temperature,
                       co_level,
                       evoc,
                       count);

    int err = bt_nus_send(NULL, msg, strlen(msg));
    if (err) {
        printk("❌ Failed to send data: %d\n", err);
    } else {
        printk("✅ Sent: %s\n", msg);
    }

    return err;
}



void main(void)
{
    int err;
    int send_count = 0;
    bool done_sending = false;
    bt_addr_le_t addr = {0};
    size_t id_count = 1;
    char addr_str[BT_ADDR_LE_STR_LEN];
    bool delay_done = false;

    printk("Starting Mobile Node\n");

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");

    bt_conn_cb_register(&conn_callbacks);

    
    nus_cb.received = nus_received;
    nus_cb.notif_enabled = notif_enabled;
    bt_nus_cb_register(&nus_cb, NULL);

    
    struct bt_le_adv_param adv_params = {
        .options = BT_LE_ADV_OPT_CONN,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
        .id = BT_ID_DEFAULT,
    };

    err = bt_le_adv_start(&adv_params, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }
    

    err = observer_start();
    if (err) {
        printk("Observer start failed (err %d)\n", err);
        return;
    }
    bt_id_get(&addr, &id_count);
    bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
    printk("Advertising successfully started as %s\n", addr_str);
    

    
    
    
    static int send_index = 0;
    // Keep running
    while (1) {
        k_sleep(K_SECONDS(0.1));

        if (!current_conn) {
            continue;
        }
        
        if (!delay_done) {
            printk("Connected — waiting 5 s before sending samples…\n");
            k_sleep(K_SECONDS(5));
            delay_done = true;
        }

     
         if (!done_sending && send_count < MAX_SAMPLES_PER_DAY) {
            struct patient_sample *s = &samples_2[send_count];
            err = send_patient_data(2,        
                                  s->heart_rate,
                                  s->co_level,
                                  s->temperature,
                                  s->evoc,
                                  s->count);
            if (err >= 0) {
                send_count++;
            }
        } else if (!done_sending) {
        
            const char *empty = "done";
            err = bt_nus_send(NULL, empty, strlen(empty));
            if (err >= 0) {
                printk("✅ Sending complete. Sent %d samples.\n", send_count);
                done_sending = true;
            } else {
                printk("❌ Failed to send empty end-of-data packet: %d\n", err);
            }
        }
    }
}

LOG_MODULE_REGISTER(simple_ble, LOG_LEVEL_INF);








#if defined(CONFIG_BT_EXT_ADV)
static bool data_cb(struct bt_data *data, void *user_data)
{
	char *name = user_data;
	uint8_t len;

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		len = MIN(data->data_len, NAME_LEN - 1);
		(void)memcpy(name, data->data, len);
		name[len] = '\0';
		return false;
	default:
		return true;
	}
}

static const char *phy2str(uint8_t phy)
{
	switch (phy) {
	case BT_GAP_LE_PHY_NONE: return "No packets";
	case BT_GAP_LE_PHY_1M: return "LE 1M";
	case BT_GAP_LE_PHY_2M: return "LE 2M";
	case BT_GAP_LE_PHY_CODED: return "LE Coded";
	default: return "Unknown";
	}
}

static void scan_recv(const struct bt_le_scan_recv_info *info,
		      struct net_buf_simple *buf)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
	char name[NAME_LEN];
	uint8_t data_status;
	uint16_t data_len;
static void generate_dummy_samples(void)
	(void)memset(name, 0, sizeof(name));

	data_len = buf->len;
	bt_data_parse(buf, data_cb, name);

	data_status = BT_HCI_LE_ADV_EVT_TYPE_DATA_STATUS(info->adv_props);

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
	printk("[DEVICE]: %s, AD evt type %u, Tx Pwr: %i, RSSI %i "
	       "Data status: %u, AD data len: %u Name: %s "
	       "C:%u S:%u D:%u SR:%u E:%u Pri PHY: %s, Sec PHY: %s, "
	       "Interval: 0x%04x (%u ms), SID: %u\n",
	       le_addr, info->adv_type, info->tx_power, info->rssi,
	       data_status, data_len, name,
	       (info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0,
	       (info->adv_props & BT_GAP_ADV_PROP_SCANNABLE) != 0,
	       (info->adv_props & BT_GAP_ADV_PROP_DIRECTED) != 0,
	       (info->adv_props & BT_GAP_ADV_PROP_SCAN_RESPONSE) != 0,
	       (info->adv_props & BT_GAP_ADV_PROP_EXT_ADV) != 0,
	       phy2str(info->primary_phy), phy2str(info->secondary_phy),
	       info->interval, info->interval * 5 / 4, info->sid);
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,
};
#endif /* CONFIG_BT_EXT_ADV */

int observer_start(void)
{
	// struct bt_le_scan_param scan_param = {
	// 	.type       = BT_LE_SCAN_TYPE_PASSIVE,
	// 	// .options    = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
    //     .options    = 0,
	// 	.interval   = BT_GAP_SCAN_FAST_INTERVAL,
	// 	.window     = BT_GAP_SCAN_FAST_WINDOW,
	// };

    struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_ACTIVE,
		// .options    = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
        .options    = 0,
		.interval   = BT_GAP_SCAN_FAST_WINDOW,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};
	int err;

#if defined(CONFIG_BT_EXT_ADV)
	bt_le_scan_cb_register(&scan_callbacks);
	printk("Registered scan callbacks\n");
#endif /* CONFIG_BT_EXT_ADV */

	err = bt_le_scan_start(&scan_param, device_found);
	if (err) {
		printk("Start scanning failed (err %d)\n", err);
		return err;
	}
	printk("Started scanning...\n");

	return 0;
}





