/*
 * Copyright (c) 2018 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/shell/shell.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <stdint.h>   
#include <math.h>   

#define DEVICE_NAME "PatientNode_02"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define NAME_LEN 30
#define NUM_BEACONS (sizeof(target_addresses) / sizeof(target_addresses[0]))

static bool in_range = false;


#define ZEPHYR_USER_NODE DT_PATH(zephyr_user) // https://docs.zephyrproject.org/latest/build/dts/zephyr-user-node.html

#define LED_ON 0
#define LED_OFF 1

static const struct gpio_dt_spec led_1 = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, clock_gpios);

/* parameters */
#define HR_MIN       90
#define HR_MAX      100
#define ANOMALY_HR  200   /* whatever “very high” means */
#define ANOMALY_CHANCE 50 /* 1 / 50 chance */

static uint8_t sample_hr(void)
{
    /* 1-in-ANOMALY_CHANCE => inject anomaly */
    if ((rand() % ANOMALY_CHANCE) == 0) {
        return ANOMALY_HR;
    }
    /* else pick uniform in [HR_MIN..HR_MAX] */
    return HR_MIN + (rand() % (HR_MAX - HR_MIN + 1));
}




#define WINDOW_SIZE 32
#define THRESHOLD   3.0f

static float window[WINDOW_SIZE];
static int   win_idx;

/* insert new sample and update idx */
static void push_sample(float hr)
{
    window[win_idx++] = hr;
    if (win_idx == WINDOW_SIZE) {
        win_idx = 0;
    }
}

/* compute mean & stddev over window[] */
static void compute_stats(float *mean, float *stddev)
{
    float sum = 0, sum2 = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        sum  += window[i];
        sum2 += window[i] * window[i];
    }
    *mean   = sum / WINDOW_SIZE;
    *stddev = sqrtf((sum2 / WINDOW_SIZE) - (*mean)*(*mean));
}

/* returns true if hr is more than THRESHOLD * stddev away from mean */
static bool is_anomaly(float hr)
{
    float m, s;
    compute_stats(&m, &s);
    return (s > 0) && (fabsf(hr - m) > THRESHOLD * s);
}










// iBeacon specific data
static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
        0x5D, 0x00,              // Company ID (Apple)
        0x02, 0x15,              // iBeacon Type
        0x80, 0x80, 0x80, 0x80, // hr, co2, temp, evoc (gas)
        0x80, 0x80, 0x80, 0x80) // 
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};


// List of MAC addresses to detect
static const char *target_addresses[] = {
    "F5:75:FE:85:34:67",
    "E5:73:87:06:1E:86",
    "CA:99:9E:FD:98:B1",
    "CB:1B:89:82:FF:FE",
    "D4:D2:A0:A4:5C:AC",
    "C1:13:27:E9:B7:7C",
    "F1:04:48:06:39:A0",
    "CA:0C:E0:DB:CE:60"
};


static atomic_t connected_flag = ATOMIC_INIT(0);

// static void connected_cb(struct bt_conn *conn, uint8_t err)
// {
//     if (!err) {
//         printk("Central connected, starting sampling\n");
//         atomic_set(&connected_flag, 1);
//     }
// }

// static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
// {
//     printk("Central disconnected (reason %u)\n", reason);
//     atomic_set(&connected_flag, 0);
// }

// /* Register connection callbacks */
// static struct bt_conn_cb conn_callbacks = {
//     .connected    = connected_cb,
//     .disconnected = disconnected_cb,
// };


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




/* This thread does the 72 updates at 100 ms intervals */
#define SAMPLE_COUNT    72
static void sample_thread(void *a, void *b, void *c)
{
    uint8_t *m = (uint8_t *)ad[1].data;
    int count = 0;
    bool start_delay_done = false;

    while (1) {
        if (in_range && count < SAMPLE_COUNT) {
            /* 1) On first detection, wait 5 seconds before sending anything */
            if (!start_delay_done) {
                start_delay_done = true;
                printk("Mobile detected – waiting 5 s before streaming samples\n");
                k_sleep(K_SECONDS(5));
            }

            /* 2) Fill in your dummy sensor values once per sample */
            m[4] = sample_hr();            /* heart rate */
            m[5] = 20  + (rand() % 10);   /* temp */
            m[6] = 400 + (rand() % 100);  /* CO₂ */
            m[7] = rand() % 200;          /* gas/eVOC */
            m[8] = count;                 /* sequence/count */

            /* 3) Send *five* copies of this same sample */
            for (int rep = 0; rep < 5; rep++) {
                bool anomaly = is_anomaly(m[4]);
                //gpio_pin_set_dt(&led_1, anomaly ? LED_ON : LED_OFF);
                if (anomaly) {
                    gpio_pin_set_dt(&led_1, LED_ON);
                } else {
                    gpio_pin_set_dt(&led_1, LED_OFF);
                }
                int err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad),
                                                sd, ARRAY_SIZE(sd));
                if (err == 0) {
                    printk("Sample %2d (copy %d): HR=%u T=%u CO2=%u GAS=%u cnt=%u\n",
                           count + 1, rep + 1,
                           m[4], m[5], m[6], m[7], m[8]);
                } else {
                    printk("⚠ adv_update failed: %d\n", err);
                }
                /* small spacing so the stack actually has time to re-advertise */
                k_sleep(K_MSEC(200));
            }

            /* 4) Move on to the next sample only after 5 copies sent */
            count++;
        }
        /* poll at 1 Hz if nothing to do (or change to a smaller tick) */
        k_sleep(K_MSEC(100));
    }
}






// static void sample_thread(void *a, void *b, void *c)
// {
//     uint8_t *m = (uint8_t *)ad[1].data;
//     int count = 0;

//     while (1) {
//         if (in_range && count < SAMPLE_COUNT) {
//             /* Dummy sensor values */
//             //m[4] = 60  + (rand() % 40);   /* hr: 60–99 */
//             m[4] = sample_hr();
//             m[5] = 20  + (rand() % 10);   /* temp: 20–29°C */
//             m[6] = 400 + (rand() % 100);  /* co2: 400–499 */
            
//             m[7] = rand() % 200;          /* gas:  0–199 */
//             m[8] = count;                /* count: 0–71 */

//             push_sample(m[4]);
//             bool anomaly = is_anomaly(m[4]);
//             printk("Anomaly: %d\n", anomaly);
//             if (anomaly) {
//                 gpio_pin_set_dt(&led_1, LED_ON);
//             } else {
//                 gpio_pin_set_dt(&led_1, LED_OFF);
//             }
//             //gpio_pin_set_dt(&led_1, anomaly);
//             int err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad),
//                                             sd, ARRAY_SIZE(sd));
//             if (err == 0) {
//                 printk("Sample %2d: HR=%u CO2=%u T=%u GAS=%u count=%d\n",
//                        count+1, m[4], m[5], m[6], m[7], m[8]);
//             } else {
//                 printk("⚠ adv_update failed: %d\n", err);
//             }
//             count++;
//         }
//         k_sleep(K_MSEC(1000));
//     }
// }
K_THREAD_DEFINE(sample_tid, 512,
                sample_thread, NULL, NULL, NULL,
                K_PRIO_PREEMPT(7), 0, 0);





#define TARGET_NAME "Mobile_Node_4703632"
// Callback for when a device is found during scanning
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                        struct net_buf_simple *recv_ad) {
    
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    //printk("Device found: %s (RSSI %d)\n", addr_str, rssi);
    // if (name_found_in_adv(recv_ad, TARGET_NAME)) {
    if (strncmp(addr_str, "FE:06:E1:92:94:9B", 17) == 0) {
    //if (strncmp(addr_str, "C2:A3:89:DC:FA:CD", 17) == 0) {
        if (rssi < -60) {
            return;
        }

        in_range = true;
        char addr_str[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
        printk("Devie found: %s (RSSI: %d)\n",  addr_str, rssi);
        
    
    }
}

static void start_scanning(void) {
    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_ACTIVE,
        .options    = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
        .interval   = BT_GAP_SCAN_FAST_INTERVAL,
        .window     = BT_GAP_SCAN_FAST_WINDOW,
    };

    int err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Start scanning failed (err %d)\n", err);
        return;
    }
    printk("Started scanning for target devices...\n");
}



void main(void)
{
    int err;
    char addr_s[BT_ADDR_LE_STR_LEN];
    bt_addr_le_t addr = {0};
    size_t count = 1;
    gpio_pin_configure_dt(&led_1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_set_dt(&led_1, LED_OFF);

    for (int i = 0; i < WINDOW_SIZE; i++) {
        window[i] = 95;
    }
    win_idx = 0;
        printk("Starting Patient2 Node iBeacon...\n");

    // Initialize Bluetooth
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");



    // Start advertising
    err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, ad, ARRAY_SIZE(ad),
                         sd, ARRAY_SIZE(sd));  // Include scan response data
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    

    // Get and print the Bluetooth address
    bt_id_get(&addr, &count);
    bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));
    printk("iBeacon started, advertising as %s\n", addr_s);

    start_scanning();

    while (1) {
        k_sleep(K_SECONDS(1));
        // Update advertising data with new RSSI values
        err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
        if (err) {
            printk("Failed to update advertising data (err %d)\n", err);
        }
    }
}





















// void main(void)
// {
//     int err;
//     bt_addr_le_t id = {0};
//     size_t count = 1;

//     printk("Patient Node starting\n");

//     /* Enable Bluetooth */
//     err = bt_enable(NULL);
//     if (err) {
//         printk("bt_enable failed: %d\n", err);
//         return;
//     }

//     /* Register conn callbacks */
//     bt_conn_cb_register(&conn_callbacks);

//     /* Start connectable advertising */
//     struct bt_le_adv_param adv = {
//         .options = BT_LE_ADV_OPT_CONN,
//         .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
//         .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
//         .id = BT_ID_DEFAULT,
//     };
//     err = bt_le_adv_start(&adv, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
//     if (err) {
//         printk("bt_le_adv_start failed: %d\n", err);
//         return;
//     }

//     /* Print our address */
//     bt_id_get(&id, &count);
//     {
//         char addr[BT_ADDR_LE_STR_LEN];
//         bt_addr_le_to_str(&id, addr, sizeof(addr));
//         printk("Advertising as %s\n", addr);
//     }

//     /* Nothing else in main: sample_thread will do updates */
// }









