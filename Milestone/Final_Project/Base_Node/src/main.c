#include <stdbool.h>
#include <zephyr/kernel.h>
#include <sys/_stdint.h>
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


#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>

#define MAX_PATIENT_ID_LEN 4    // e.g. "P01"
#define MAX_TIME_STR_LEN   6    // e.g. "12:30"
#define MAX_SAMPLES 72
#define TARGET_NAME "Mobile_Node_4703632"

#define MAX_DATA_LEN 64
static uint8_t data_buf[MAX_DATA_LEN];
static size_t data_len = 0;

#define BT_UUID_NUS_RX_CHAR_VAL \
    BT_UUID_128_ENCODE(0x6e400003, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)


static struct bt_uuid_128 nus_rx_char_uuid = BT_UUID_INIT_128(BT_UUID_NUS_RX_CHAR_VAL);


struct patient_sample {
    char patient_id[MAX_PATIENT_ID_LEN];  // null-terminated
    uint8_t heart_rate;                   // e.g. 75                    
    float temperature;                    // e.g. 36.7
    float co_level;                       // e.g. 10
    uint8_t evoc;                        // e.g. 100
    int count;                          // 50
};


static struct patient_sample samples_1[MAX_SAMPLES];
static struct patient_sample samples_2[MAX_SAMPLES];
static struct patient_sample samples[MAX_SAMPLES];
static int sample_count = 0;
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;
static struct bt_conn *default_conn;


// Function Declarations
static void start_scan(void);
static int sample1_count = 0;
static int sample2_count = 0;


void print_all_samples_json(void)
{
    for (int i = 0; i < sample1_count; i++) {
        struct patient_sample *s = &samples_1[i];
        printk("{\"patient_id\":\"1\",\"hr\":%u,\"temp\":%.1f,"
               "\"co\":%.1f,\"evoc\":%u,\"count\":%d}\n",
               s->heart_rate, s->temperature,
               s->co_level,    s->evoc,       s->count);
        k_sleep(K_MSEC(10));
    }

    for (int i = 0; i < sample2_count; i++) {
        struct patient_sample *s = &samples_2[i];
        printk("{\"patient_id\":\"2\",\"hr\":%u,\"temp\":%.1f,"
               "\"co\":%.1f,\"evoc\":%u,\"count\":%d}\n",
                s->heart_rate, s->temperature,
               s->co_level,    s->evoc,       s->count);
        k_sleep(K_MSEC(10));
    }
}


static void parse_and_store(const char *data, size_t len)
{
    char pid_str[4] = {0};
    if (len < 3) {
        printk("⚠ Too short\n");
        return;
    }
   
    memcpy(pid_str, data, 3);  /* extract “Pxx” */

    /* pick destination array & counter */
    struct patient_sample *target;
    int *cnt;

    printk("pid = %s\n", pid_str);
    if (strcmp(pid_str, "209") == 0) {
        target = samples_2;
        cnt    = &sample2_count;
    } else {
        target = samples_1;
        cnt    = &sample1_count;
    }
    if (*cnt >= MAX_SAMPLES) {
        return;
    }

    struct patient_sample *s = &target[*cnt];

    /* now parse the five numeric fields: HR (3d), Temp (4.1), CO (4.1), Evoc (2d), Count (d) */
    int    hr, evoc, count_val;
    float  tmp, co;
    int fields = sscanf(data + 1, "%3d%4f%4f%2d%3d",
                        &hr, &tmp, &co, &evoc, &count_val);

    /* store */
    strncpy(s->patient_id, pid_str, sizeof(s->patient_id));
    s->heart_rate = (uint8_t)hr;
    s->temperature = tmp;
    s->co_level    = co;
    s->evoc        = (uint8_t)evoc;
    s->count       = count_val;
    (*cnt)++;
    printk("Stored %s sample %d (count = %d)\n", pid_str, *cnt, s->count);

    printk("data = %d\n", data[0]); 

    if (data[0] != 2) { 

        printk("All P02 samples in, printing JSON"); 
        print_all_samples_json();

    }
}


static uint8_t notify_func(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length) {
    
    if (!data) {
        printk("Notification subscription stopped\n");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    for (int i = 0; i < length; i++) {
        printk("%02x ", ((uint8_t*)data)[i]);
    }
    printk("\n");

    char buf[65];
    memcpy(buf, data, length);
    buf[length] = '\0';

    parse_and_store(buf, length);
    return BT_GATT_ITER_CONTINUE;
}


static uint8_t discover_func(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    int err;

    if (!attr) {
        printk("Discover complete\n");
        memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    const struct bt_gatt_chrc *chrc = attr->user_data;

    char uuid_str[BT_UUID_STR_LEN];
    bt_uuid_to_str(chrc->uuid, uuid_str, sizeof(uuid_str));
    printk("[CHARACTERISTIC] handle %u, uuid: %s\n", attr->handle, uuid_str);

    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        const struct bt_gatt_chrc *chrc = attr->user_data;

        char uuid_str[BT_UUID_STR_LEN];
        bt_uuid_to_str(chrc->uuid, uuid_str, sizeof(uuid_str));
        printk("[CHARACTERISTIC] handle %u, uuid: %s\n", attr->handle, uuid_str);

        if (!bt_uuid_cmp(chrc->uuid, &nus_rx_char_uuid.uuid)) {
            printk("Found NUS RX characteristic!\n");

            subscribe_params.notify = notify_func;
            subscribe_params.value_handle = chrc->value_handle;
            subscribe_params.ccc_handle = 0;
            subscribe_params.value = BT_GATT_CCC_NOTIFY;

            // Discover the CCC descriptor next
            discover_params.uuid = BT_UUID_GATT_CCC;
            discover_params.start_handle = chrc->value_handle + 1;
            discover_params.end_handle = 0xffff;
            discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
            discover_params.func = discover_func;

            err = bt_gatt_discover(conn, &discover_params);
            if (err) {
                printk("Discover CCC failed (err %d)\n", err);
            }

            return BT_GATT_ITER_STOP;
        }
    }

    else if (params->type == BT_GATT_DISCOVER_DESCRIPTOR) {
        printk("Checking descriptor at handle %u\n", attr->handle);

        if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC)) {
            printk("Found CCC descriptor at handle %u\n", attr->handle);

            subscribe_params.ccc_handle = attr->handle;

            err = bt_gatt_subscribe(conn, &subscribe_params);
            if (err && err != -EALREADY) {
                printk("Subscribe failed (err %d)\n", err);
            } else {
                printk("Subscribed to notifications\n");
            }

            return BT_GATT_ITER_STOP;
        }
    }
    return BT_GATT_ITER_CONTINUE;
}


static void start_discovery(struct bt_conn *conn)
{
    int err;

    discover_params.uuid = &nus_rx_char_uuid.uuid;
    discover_params.func = discover_func;
    discover_params.start_handle = 0x0001;
    discover_params.end_handle = 0xffff;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

    err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("Discover failed(err %d)\n", err);
    } else {
        printk("Discover started\n");
    }
}


static bool name_found_in_adv(struct net_buf_simple *ad, const char *target_name)
{
    int i = 0;

    while (i < ad->len) {
        uint8_t len = ad->data[i];
        if (len == 0) {
            break;
        }

        uint8_t type = ad->data[i + 1];

        if (type == BT_DATA_NAME_COMPLETE || type == BT_DATA_NAME_SHORTENED) {
            // Extract the name string
            int name_len = len - 1;
            if (name_len == strlen(target_name) &&
                memcmp(&ad->data[i + 2], target_name, name_len) == 0) {
                return true;
            }
        }
        i += len + 1;
    }
    return false;
}


static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;
    if (name_found_in_adv(ad, TARGET_NAME)) {
        printk("Found target device %s, attempting to connect...\n", TARGET_NAME);


        if (rssi < -50) {
            return;
        }

        if (default_conn) {
		    return;
        }

        if (type != BT_GAP_ADV_TYPE_ADV_IND &&
            type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
            return;
        }

        bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
        printk("Device found: %s (RSSI %d)\n", addr_str, rssi);

        if (bt_le_scan_stop()) {
            return;
        }

        err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
                    BT_LE_CONN_PARAM(0x0018, 0x0028, 0, 400), &default_conn);
        
        if (err) {
            printk("Create conn to %s failed (%d)\n", addr_str, err);
            start_scan();
        }
    }
}


static void start_scan(void)
{
	int err;

	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");
}


static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s %u %s\n", addr, err, bt_hci_err_to_str(err));
		bt_conn_unref(default_conn);
		default_conn = NULL;
		start_scan();
		return;
	}

	if (conn != default_conn) {
		return;
	}

	printk("Connected: %s\n", addr);
    start_discovery(conn);

}


static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn != default_conn) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Disconnected: %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));
	bt_conn_unref(default_conn);
	default_conn = NULL;
	start_scan();
}


BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};


int main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	start_scan();
    

	return 0;
}

LOG_MODULE_REGISTER(simple_ble, LOG_LEVEL_INF);