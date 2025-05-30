#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/sys/util.h>

#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#include <zephyr/drivers/sensor.h>

#include <stdlib.h>

#include <zephyr/drivers/sensor/ccs811.h>
#include <stdio.h>

#define DT_DRV_COMPAT maxim_max30101

#define MAX30101_REG_INT_STS1		0x00
#define MAX30101_REG_INT_STS2		0x01
#define MAX30101_REG_INT_EN1		0x02
#define MAX30101_REG_INT_EN2		0x03
#define MAX30101_REG_FIFO_WR		0x04
#define MAX30101_REG_FIFO_OVF		0x05
#define MAX30101_REG_FIFO_RD		0x06
#define MAX30101_REG_FIFO_DATA		0x07
#define MAX30101_REG_FIFO_CFG		0x08
#define MAX30101_REG_MODE_CFG		0x09
#define MAX30101_REG_SPO2_CFG		0x0a
#define MAX30101_REG_LED1_PA		0x0c
#define MAX30101_REG_LED2_PA		0x0d
#define MAX30101_REG_LED3_PA		0x0e
#define MAX30101_REG_PILOT_PA		0x10
#define MAX30101_REG_MULTI_LED		0x11
#define MAX30101_REG_TINT		0x1f
#define MAX30101_REG_TFRAC		0x20
#define MAX30101_REG_TEMP_CFG		0x21
#define MAX30101_REG_PROX_INT		0x30
#define MAX30101_REG_REV_ID		0xfe
#define MAX30101_REG_PART_ID		0xff

#define MAX30101_INT_PPG_MASK		(1 << 6)

#define MAX30101_FIFO_CFG_SMP_AVE_SHIFT		5
#define MAX30101_FIFO_CFG_FIFO_FULL_SHIFT	0
#define MAX30101_FIFO_CFG_ROLLOVER_EN_MASK	(1 << 4)

#define MAX30101_MODE_CFG_SHDN_MASK	(1 << 7)
#define MAX30101_MODE_CFG_RESET_MASK	(1 << 6)

#define MAX30101_SPO2_ADC_RGE_SHIFT	5
#define MAX30101_SPO2_SR_SHIFT		2
#define MAX30101_SPO2_PW_SHIFT		0

#define MAX30101_PART_ID		0x15

#define MAX30101_BYTES_PER_CHANNEL	3
#define MAX30101_MAX_NUM_CHANNELS	3
#define MAX30101_MAX_BYTES_PER_SAMPLE	(MAX30101_MAX_NUM_CHANNELS * \
					 MAX30101_BYTES_PER_CHANNEL)

#define MAX30101_SLOT_LED_MASK		0x03

#define MAX30101_FIFO_DATA_BITS		18
#define MAX30101_FIFO_DATA_MASK		((1 << MAX30101_FIFO_DATA_BITS) - 1)

#define MAX30101_SPO2_PW_MASK (0x3 << MAX30101_SPO2_PW_SHIFT)

LOG_MODULE_REGISTER(max30102_logger, LOG_LEVEL_DBG); 

#define MAX30102_I2C_ADDR        0x57 // verified 

const struct device *const max30101 = DEVICE_DT_GET_ANY(maxim_max30101);

enum max30101_mode {
	MAX30101_MODE_HEART_RATE	= 2,
	MAX30101_MODE_SPO2		= 3,
	MAX30101_MODE_MULTI_LED		= 7,
};

enum max30101_slot {
	MAX30101_SLOT_DISABLED		= 0,
	MAX30101_SLOT_RED_LED1_PA,
	MAX30101_SLOT_IR_LED2_PA,
	MAX30101_SLOT_GREEN_LED3_PA,
	MAX30101_SLOT_RED_PILOT_PA,
	MAX30101_SLOT_IR_PILOT_PA,
	MAX30101_SLOT_GREEN_PILOT_PA,
};

enum max30101_led_channel {
	MAX30101_LED_CHANNEL_RED	= 0,
	MAX30101_LED_CHANNEL_IR,
	MAX30101_LED_CHANNEL_GREEN,
};

enum max30101_pw {
	MAX30101_PW_15BITS		= 0,
	MAX30101_PW_16BITS      = 1,
	MAX30101_PW_17BITS      = 2,
	MAX30101_PW_18BITS      = 3,
};

struct max30101_config {
	struct i2c_dt_spec i2c;
	uint8_t fifo;
	uint8_t spo2;
	uint8_t led_pa[MAX30101_MAX_NUM_CHANNELS];
	enum max30101_mode mode;
	enum max30101_slot slot[4];
};

struct max30101_data {
	uint32_t raw[MAX30101_MAX_NUM_CHANNELS];
	uint8_t map[MAX30101_MAX_NUM_CHANNELS];
	uint8_t num_channels;
};

static int max30101_sample_fetch(const struct device *dev,
    enum sensor_channel chan)
{
struct max30101_data *data = dev->data;
const struct max30101_config *config = dev->config;
uint8_t buffer[MAX30101_MAX_BYTES_PER_SAMPLE];
uint32_t fifo_data;
int fifo_chan;
int num_bytes;
int i;

/* Read all the active channels for one sample */
num_bytes = data->num_channels * MAX30101_BYTES_PER_CHANNEL;
if (i2c_burst_read_dt(&config->i2c, MAX30101_REG_FIFO_DATA, buffer,
     num_bytes)) {
LOG_ERR("Could not fetch sample");
return -EIO;
}

fifo_chan = 0;
for (i = 0; i < num_bytes; i += 3) {
/* Each channel is 18-bits */
fifo_data = (buffer[i] << 16) | (buffer[i + 1] << 8) |
   (buffer[i + 2]);
fifo_data &= MAX30101_FIFO_DATA_MASK;

/* Save the raw data */
data->raw[fifo_chan++] = fifo_data;
}

return 0;
}

static int max30101_channel_get(const struct device *dev,
    enum sensor_channel chan,
    struct sensor_value *val)
{
struct max30101_data *data = dev->data;
enum max30101_led_channel led_chan;
int fifo_chan;

switch (chan) {
case SENSOR_CHAN_RED:
led_chan = MAX30101_LED_CHANNEL_RED;
break;

case SENSOR_CHAN_IR:
led_chan = MAX30101_LED_CHANNEL_IR;
break;

case SENSOR_CHAN_GREEN:
led_chan = MAX30101_LED_CHANNEL_GREEN;
break;

default:
LOG_ERR("Unsupported sensor channel");
return -ENOTSUP;
}

/* Check if the led channel is active by looking up the associated fifo
* channel. If the fifo channel isn't valid, then the led channel
* isn't active.
*/
fifo_chan = data->map[led_chan];
if (fifo_chan >= MAX30101_MAX_NUM_CHANNELS) {
LOG_ERR("Inactive sensor channel");
return -ENOTSUP;
}

/* TODO: Scale the raw data to standard units */
val->val1 = data->raw[fifo_chan];
val->val2 = 0;

return 0;
}

static DEVICE_API(sensor, max30101_driver_api) = {
.sample_fetch = max30101_sample_fetch,
.channel_get = max30101_channel_get,
};

static int max30101_init(const struct device *dev)
{
	const struct max30101_config *config = dev->config;
	struct max30101_data *data = dev->data;
	uint8_t part_id;
	uint8_t mode_cfg;
	uint32_t led_chan;
	int fifo_chan;

	if (!device_is_ready(config->i2c.bus)) {
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}

	/* Check the part id to make sure this is MAX30101 */
	if (i2c_reg_read_byte_dt(&config->i2c, MAX30101_REG_PART_ID,
				 &part_id)) {
		LOG_ERR("Could not get Part ID");
		return -EIO;
	}
	if (part_id != MAX30101_PART_ID) {
		LOG_ERR("Got Part ID 0x%02x, expected 0x%02x",
			    part_id, MAX30101_PART_ID);
		return -EIO;
	}

	/* Reset the sensor */
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30101_REG_MODE_CFG,
				  MAX30101_MODE_CFG_RESET_MASK)) {
		return -EIO;
	}

	/* Wait for reset to be cleared */
	do {
		if (i2c_reg_read_byte_dt(&config->i2c, MAX30101_REG_MODE_CFG,
					 &mode_cfg)) {
			LOG_ERR("Could read mode cfg after reset");
			return -EIO;
		}
	} while (mode_cfg & MAX30101_MODE_CFG_RESET_MASK);

	/* Write the FIFO configuration register */
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30101_REG_FIFO_CFG,
				  config->fifo)) {
		return -EIO;
	}

	/* Write the mode configuration register */
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30101_REG_MODE_CFG,
				  config->mode)) {
		return -EIO;
	}

	/* Write the SpO2 configuration register */
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30101_REG_SPO2_CFG,
        config->spo2)) {
		return -EIO;
	}

	/* Write the LED pulse amplitude registers */
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30101_REG_LED1_PA,
				  config->led_pa[0])) {
		return -EIO;
	}
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30101_REG_LED2_PA,
				  config->led_pa[1])) {
		return -EIO;
	}
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30101_REG_LED3_PA,
				  config->led_pa[2])) {
		return -EIO;
	}

#ifdef CONFIG_MAX30101_MULTI_LED_MODE
	uint8_t multi_led[2];

	/* Write the multi-LED mode control registers */
	multi_led[0] = (config->slot[1] << 4) | (config->slot[0]);
	multi_led[1] = (config->slot[3] << 4) | (config->slot[2]);

	if (i2c_reg_write_byte_dt(&config->i2c, MAX30101_REG_MULTI_LED,
				  multi_led[0])) {
		return -EIO;
	}
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30101_REG_MULTI_LED + 1,
				  multi_led[1])) {
		return -EIO;
	}
#endif

	/* Initialize the channel map and active channel count */
	data->num_channels = 0U;
	for (led_chan = 0U; led_chan < MAX30101_MAX_NUM_CHANNELS; led_chan++) {
		data->map[led_chan] = MAX30101_MAX_NUM_CHANNELS;
	}

	/* Count the number of active channels and build a map that translates
	 * the LED channel number (red/ir/green) to the fifo channel number.
	 */
	for (fifo_chan = 0; fifo_chan < MAX30101_MAX_NUM_CHANNELS;
	     fifo_chan++) {
		led_chan = (config->slot[fifo_chan] & MAX30101_SLOT_LED_MASK)-1;
		if (led_chan < MAX30101_MAX_NUM_CHANNELS) {
			data->map[led_chan] = fifo_chan;
			data->num_channels++;
		}
	}


	return 0;
}


// int32_t bufferLength; //data length
// int32_t spo2; //SPO2 value
// int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
// int32_t heartRate; //heart rate value
// int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

// uint32_t irBuffer[100]; //infrared LED sensor data
// uint32_t redBuffer[100];  //red LED sensor data


bool check_for_beat(uint32_t irValue) {
    static uint32_t prev_ir = 0;
    static bool rising = false;
    static int64_t last_beat_time = 0;
    static const int MIN_BEAT_INTERVAL_MS = 300; // 200 BPM max

    int64_t now = k_uptime_get();
    bool beat_detected = false;

    if (irValue > prev_ir) {
        rising = true;
    } else if (rising && irValue < prev_ir) {
        if (now - last_beat_time > MIN_BEAT_INTERVAL_MS) {
            beat_detected = true;
            last_beat_time = now;
        }
        rising = false;
    }

    prev_ir = irValue;
    return beat_detected;
}


#define MA_SIZE 20
static uint32_t ir_buffer[MA_SIZE] = {0};
static int ir_index = 0;

uint32_t moving_avg(uint32_t new_val) {
    ir_buffer[ir_index++] = new_val;
    ir_index %= MA_SIZE;

    uint32_t sum = 0;
    for (int i = 0; i < MA_SIZE; i++) {
        sum += ir_buffer[i];
    }
    return sum / MA_SIZE;
}

#define AMBIENT_IR_VALUE 	9000
#define NUMBER_OF_SAMPLES 	4

void hr_main(void) { 

    max30101_init(max30101); // Initalise the hr monitor

    uint16_t samples[4] = {60, 60, 60, 60}; // Array to hold the bpm values (initalised to an average so that calculations can begin immediatley)

    uint64_t start_sample_time = k_uptime_get(); // Set the inital sample time

	// Values to keep track 
    int beat_count = 0;
    int sample_number = 0;
	static int64_t last_beat_time = 0;

	// Data structures to hold the sample value
	struct sensor_value ir;
    uint32_t ir_value;

    while (1) { 

		uint64_t current_time_ms = k_uptime_get();	// Set the current time 

		// Get the IR sample from the hr monitor
		if (sensor_sample_fetch(max30101) < 0) {
            LOG_ERR("Sample fetch error");
            continue;
        }
        if (sensor_channel_get(max30101, SENSOR_CHAN_IR, &ir) < 0) {
            LOG_ERR("IR channel read error");
            continue;
        }

        ir_value = ir.val1; // Extract the interger value from the sensor struct

		if (ir_value < AMBIENT_IR_VALUE) { 
			// reset the timing values
			start_sample_time = k_uptime_get();
			current_time_ms = k_uptime_get();

			beat_count = 0; // Reset the beat count so that they dont get added to next valid values
			LOG_ERR("Finger not on the sensor"); // Might implement led functionality
		}

		// If the smaple time is 15 seconds, begin sample average calculations
        if (current_time_ms - start_sample_time >= 15000) {

            int bpm_avg_sample = beat_count * 4; // Multiply the beat count by 4 to get the average over a minute

            samples[sample_number] = bpm_avg_sample; // Set that value in the array to the current avg sample
            sample_number = (sample_number + 1) % 4; // Increment the sample index

            int total_sample = 0; // Initialise to hold sample total so that average can be calculated

			// add all the smaples together (might break out into a helper function)
            for (int i = 0; i < 4; i++) {
                total_sample += samples[i];
            }

            int bmp_avg = total_sample / NUMBER_OF_SAMPLES; // Get the average of the array
            printk("\nBPM: %d [%d, %d, %d, %d]\n", bmp_avg, samples[0], samples[1], samples[2], samples[3]); // Prit to the terminal for checking

            beat_count = 0; // Reset the beat count for the next 15 sec interval
            start_sample_time = current_time_ms;	// reset the sample time 
        }

		// Check whether a beat occured
        if (check_for_beat(moving_avg(ir_value))) { // Use the moving average to eliminate more noise
            int64_t now = k_uptime_get();	// Get the starting value to ensure the beat timing is valid

    		if (now - last_beat_time > 300) {  // At most 200 BPM
        	beat_count++; // INcrease the beat count
			// printk("%d\n", beat_count); 
        	last_beat_time = now; // reset the timing 
    		}
        }

        k_sleep(K_MSEC(10)); // 100 Hz sampling
    }
}

// void main(void) { 


// }





static bool app_fw_2;

static const char *now_str(void)
{
	static char buf[16]; /* ...HH:MM:SS.MMM */
	uint32_t now = k_uptime_get_32();
	unsigned int ms = now % MSEC_PER_SEC;
	unsigned int s;
	unsigned int min;
	unsigned int h;

	now /= MSEC_PER_SEC;
	s = now % 60U;
	now /= 60U;
	min = now % 60U;
	now /= 60U;
	h = now;

	snprintf(buf, sizeof(buf), "%u:%02u:%02u.%03u",
		 h, min, s, ms);
	return buf;
}

static int do_fetch(const struct device *dev)
{
	struct sensor_value co2, tvoc, voltage, current;
	int rc = 0;
	int baseline = -1;

	if (rc == 0) {
		rc = sensor_sample_fetch(dev);
	}
	if (rc == 0) {
		const struct ccs811_result_type *rp = ccs811_result(dev);

		sensor_channel_get(dev, SENSOR_CHAN_CO2, &co2);
		sensor_channel_get(dev, SENSOR_CHAN_VOC, &tvoc);
		printk("CCS811: %u ppm eCO2; %u ppb eTVOC\n",co2.val1, tvoc.val1);
	}
	return rc;
}

static void do_main(const struct device *dev)
{
	while (true) {
		int rc = do_fetch(dev);
		k_msleep(1000);
	}
}

int air_main(void)
{
	const struct device *const dev = DEVICE_DT_GET_ONE(ams_ccs811);
	struct ccs811_configver_type cfgver;
	int rc;

	if (!device_is_ready(dev)) {
		printk("Device %s is not ready\n", dev->name);
		return 0;
	}

	printk("device is %p, name is %s\n", dev, dev->name);

	rc = ccs811_configver_fetch(dev, &cfgver);
	if (rc == 0) {
		do_main(dev);
	}
	return 0;
}

K_THREAD_DEFINE(thr1_id, 1024, air_main, NULL, NULL, NULL,
	7, 0, 0);
K_THREAD_DEFINE(thr2_id, 1024, hr_main, NULL, NULL, NULL,
	7, 0, 0);