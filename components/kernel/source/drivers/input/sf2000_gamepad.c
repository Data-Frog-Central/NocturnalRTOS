#define LOG_TAG "sf2000_gamepad"
//#define ELOG_OUTPUT_LVL ELOG_LVL_DEBUG

#include <kernel/module.h>
#include <sys/unistd.h>
#include <errno.h>
#include <kernel/lib/fdt_api.h>
#include <kernel/ld.h>
#include <kernel/elog.h>
#include <kernel/drivers/input.h>
#include <hcuapi/input-event-codes.h>
#include <hcuapi/input.h>
#include <hcuapi/gpio.h>
#include <kernel/delay.h>
#include <stdio.h>

#define KEY_SHIFTER_CLK_PIN PINPAD_L24 // the clock pin is shared
//#define KEY_SHIFTER_PL1_PIN PINPAD_L25 // X60
#define KEY_SHIFTER_PL1_PIN PINPAD_L23 // SF2000
#define KEY_SHIFTER_PL2_PIN PINPAD_L26 // currently unimplemented

// TODO: these only used debug log. leave it here for now.
#if ELOG_OUTPUT_LVL >= ELOG_LVL_INFO
static const char * const key_names[] = {
	"L", "Y", "X", "R", "A", "B", "Select", "Start", "Up", "Down", "Left", "Right"
};
#endif

// map the button by their name and not by position as described here
// https://www.kernel.org/doc/html/v4.13/input/gamepad.html
static const int key_codes[] = {
	BTN_TL, BTN_Y, BTN_X, BTN_TR, BTN_A, BTN_B, BTN_SELECT, BTN_START, BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT
};


static TaskHandle_t sf2000_gamepad_thread = NULL;
static int register_num = 0;

// TODO: cleanup this struct to include only what needed
struct sf2000_gamepad_priv {
	struct input_dev	*input;
	struct device		*dev;
	int					keymap_len;
	int					*key_map;
	int					*pin_map;
	struct gpio_descs	*gpiods;
	int					enable;
};


static void sf2000_gamepad_kthread(void *pvParameters);

static int sf2000_gamepad_open(struct input_dev *dev)
{
	struct sf2000_gamepad_priv *priv = input_get_drvdata(dev);

	priv->enable = 1;

	if (register_num == 0) {
		xTaskCreate(sf2000_gamepad_kthread, (const char *)"sf2000_gamepad_kthread", configTASK_STACK_DEPTH,
				priv, portPRI_TASK_NORMAL, &sf2000_gamepad_thread);
	}

	register_num++;
	return 0;
}

static void sf2000_gamepad_close(struct input_dev *dev)
{
	struct sf2000_gamepad_priv *priv = input_get_drvdata(dev);

	if (register_num == 0) {
		priv->enable = 0;
		vTaskDelete(sf2000_gamepad_thread);
	} else
		register_num--;

	return;
}

static void input_init(struct sf2000_gamepad_priv *priv)
{
	priv->input->open = sf2000_gamepad_open;
	priv->input->close = sf2000_gamepad_close;
}

static void sf2000_gamepad_kthread(void *pvParameters)
{
	struct sf2000_gamepad_priv *priv = (struct sf2000_gamepad_priv *)pvParameters;

	int i, prev[12], state[12];

	for (i = 0; i < 12; i++)
		prev[i] = 1;	// 1 means button not pressed (ie. released)

	gpio_configure(KEY_SHIFTER_CLK_PIN, GPIO_DIR_OUTPUT);
	gpio_set_output(KEY_SHIFTER_CLK_PIN, 1); // shifts on 1->0 transition

	while (1) {
		if (priv->enable == 0)
			usleep(100000);
		else {
			// probably latches the state while the pin is actively driven
			// setup the shifter to collect the inputs
			gpio_configure(KEY_SHIFTER_PL1_PIN, GPIO_DIR_OUTPUT);
			gpio_set_output(KEY_SHIFTER_PL1_PIN, 0);

			usleep(4);

			// setup the shifter for serial reading of the inputs it collected
			gpio_configure(KEY_SHIFTER_PL1_PIN, GPIO_DIR_INPUT);

			for (i = 0; i < 12; i++) {
				// read one input bit
				state[i] = gpio_get_input(KEY_SHIFTER_PL1_PIN);

				// pulse the shifter clock to prepare reading the next bit
				gpio_set_output(KEY_SHIFTER_CLK_PIN, 0);
				usleep(2);
				gpio_set_output(KEY_SHIFTER_CLK_PIN, 1);
			}

			// check which input changed
			for (i = 0; i < 12; i++) {
				if (prev[i] != state[i]) {
					log_d("button %s %s\n", key_names[i], state[i] ? "released" : "pressed");

					prev[i] = state[i];

					// TODO: I don't think that for our needs it is necessary to call
					// "input_event(...EV_MSC..." or "input_sync" like other input drivers do
					// because EV_SYN is used to sync multi events into one - like mouse (X,Y) pos
					// and EV_MSC is mostly used for repeating events - like when holding a key
					if (state[i]) {		// button released
						input_report_key(priv->input, key_codes[i], 0);
						//input_sync(priv->input);
					} else {			// button pressed
						//input_event(priv->input, EV_MSC, MSC_SCAN, key_codes[i]);
						input_report_key(priv->input, key_codes[i], 1);
						//input_sync(priv->input);
					}
				}
			}

			// original software has 4 but scans every 4th iteration
			// a software key debounce, or the MCU can't handle faster rate?
			msleep(16);
		}
	}

	return;
}

// TODO: maybe add some functionality to read dynamic configuration from dts
// for example the KEY_SHIFTER_PL1_PIN define is different for X60 and SF2000
// so maybe to read its value from dts instead
static int hc_gpiokey_probe(const char *node)
{
	// int np, ret;
	// u32 i, pin, num_pins;
	int ret;
	struct sf2000_gamepad_priv *priv;

	// np = fdt_node_probe_by_path(node);
	// if (np < 0)
		// return 0;

	priv = kzalloc(sizeof(struct sf2000_gamepad_priv), GFP_KERNEL);
	if (!priv)
		return 0;

	priv->input = input_allocate_device();
	if (!priv->input)
		goto err;

	input_init(priv);

	input_set_drvdata(priv->input, priv);

	ret = input_register_device(priv->input);

	return ret;

err:
	input_free_device(priv->input);
	kfree(priv);
	return 0;
}

static int sf2000_gamepad_init(void)
{
	// TODO: why this doesn't appear in log.txt when fileuart is configured?
	log_i("sf2000_gamepad_init\n");

	int rc = 0;

	rc |= hc_gpiokey_probe("/hcrtos/sf2000-gamepad");

	return rc;
}

// TODO: the last param is "priority", but im not sure is 0 is the lowest priority or the highest
module_driver(sf2000_gamepad, sf2000_gamepad_init, NULL, 0)
