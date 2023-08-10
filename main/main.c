#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2c_bus.h"
#include "adpd188.h"
#include "at24cs0x.h"
#include "bsec2.h"
#include "button.h"
#include "shtc3.h"
#include "tpl5010.h"
#include "mics6814.h"
#include "esp_buzzer.h"
#include "esp_rgb_led.h"

static i2c_bus_t i2c_bus;
static at24cs0x_t at24cs01;
static bsec2_t bsec2;
static button_t button;
static tpl5010_t tpl5010;
static shtc3_t shtc3;
static mics6814_t mics6814;
static esp_buzzer_t buzzer;
static esp_rgb_led_t led;

static float temp = 0.0f, hum = 0.0f;

static const char *TAG = "test";

static void bsec_check_status(bsec2_t * const bsec) {
	if (bsec->status < BSEC_OK) {
		ESP_LOGE(TAG, "BSEC error code: %d", bsec->status);
	}
	else if (bsec->status > BSEC_OK) {
		ESP_LOGW(TAG, "BSEC warning code: %d", bsec->status);
	}
	else if (bsec->sensor.status < BME68X_OK) {
		ESP_LOGE(TAG, "BME68x error code: %d", bsec->sensor.status);
	}
	else if (bsec->sensor.status > BME68X_OK) {
		ESP_LOGW(TAG, "BME68x warning code: %d", bsec->sensor.status);
	}
}

static void bsec_callback(const bme68x_data_t bme68x_data, const bsec_outputs_t outputs, bsec2_t bsec) {
	if (!outputs.n_outputs) {
		return;
	}

	printf("FSM_BSEC_DATA_EVENT\r\n");

	for (uint8_t i = 0; i < outputs.n_outputs; i++) {
		const bsec_data_t output = outputs.output[i];

		switch (output.sensor_id) {
			case BSEC_OUTPUT_RAW_TEMPERATURE:
				printf("\ttemp: %f", output.signal);
				break;
			case BSEC_OUTPUT_RAW_HUMIDITY:
				printf("\thum: %f", output.signal);
				break;
			case BSEC_OUTPUT_RAW_PRESSURE:
				printf("\tpres: %f", output.signal);
				break;
			case BSEC_OUTPUT_IAQ:
				printf("\tiaq: %d", (int)output.signal);
				break;
			case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
				printf("\tvoc: %f", output.signal);
				break;
			case BSEC_OUTPUT_CO2_EQUIVALENT:
				printf("\tco2: %f", output.signal);
				break;
			default:
				break;
		}
		printf("\n");
	}
	printf("\r\n");
}

static esp_err_t bsec_lib_init(void) {
	esp_err_t ret = ESP_OK;

  /* Desired subscription list of BSEC2 outputs */
	bsec_sensor_t sensor_list[] = {
			BSEC_OUTPUT_IAQ,
			BSEC_OUTPUT_CO2_EQUIVALENT,
			BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_HUMIDITY,
	};


  /* Initialize the library and interfaces */
	if (!bsec2_init(&bsec2, (void *)&i2c_bus, BME68X_I2C_INTF)) {
		bsec_check_status(&bsec2);
	}

	if (!bsec2_update_subscription(&bsec2, sensor_list, ARRAY_LEN(sensor_list), BSEC_SAMPLE_RATE_LP)) {
		bsec_check_status(&bsec2);
	}

	bsec2_attach_callback(&bsec2, bsec_callback);

	ESP_LOGI(TAG,"\bBSEC library version %d.%d.%d.%d", bsec2.version.major, bsec2.version.minor, bsec2.version.major_bugfix, bsec2.version.minor_bugfix);

	return ret;
}

void bsec_task(void *arg) {
	for (;;) {
		if (!bsec2_run(&bsec2)) {
			bsec_check_status(&bsec2);
		}
		vTaskDelay(pdMS_TO_TICKS(20));
	}
}

void shtc3_task(void *arg) {
	for (;;) {
		shtc3_get_temp_and_hum(&shtc3, &temp, &hum);
		printf("temp: %f, hum: %f\r\n", temp, hum);

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void at24cs0x_task(void *arg) {
	for (;;) {
		at24cs0x_read_serial_number(&at24cs01);
		printf("serial number: ");
		for (uint8_t i = 0; i < AT24CS0X_SN_SIZE; i++) {
			printf("%02X", at24cs01.serial_number[i]);
		}
		printf("\r\n");

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void mics6814_task(void *arg) {
	for (;;) {
		for (uint8_t i = CO_GAS; i < C2H5OH_GAS; i++) {
			printf("gas %d: %f\r\n", i, mics6814_get_gas(&mics6814, i));
		}
		printf("\r\n");

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void button_task(void *arg) {
	printf("%s\r\n", (char*)arg);
}

void app_main(void) {
	ESP_ERROR_CHECK(mics6814_init(&mics6814,
			ADC_CHANNEL_3, /* NH3 */
			ADC_CHANNEL_4, /* CO */
			ADC_CHANNEL_5)); /* NO2 */
	ESP_ERROR_CHECK(tpl5010_init(&tpl5010, GPIO_NUM_37, GPIO_NUM_38));
	ESP_ERROR_CHECK(i2c_bus_init(&i2c_bus, I2C_NUM_0, GPIO_NUM_33, GPIO_NUM_34, true, true, 400000));
	ESP_ERROR_CHECK(at24cs0x_init(&at24cs01, &i2c_bus, AT24CS0X_I2C_ADDRESS, NULL, NULL));
	ESP_ERROR_CHECK(shtc3_init(&shtc3, &i2c_bus, SHTC3_I2C_ADDR, NULL, NULL));
	ESP_ERROR_CHECK(bsec_lib_init());
	ESP_ERROR_CHECK(esp_rgb_led_init(&led, GPIO_NUM_9, 1));
	ESP_ERROR_CHECK(esp_buzzer_init(&buzzer, GPIO_NUM_21));
	ESP_ERROR_CHECK(button_init(&button, GPIO_NUM_0, tskIDLE_PRIORITY + 6, configMINIMAL_STACK_SIZE * 4));
	button_register_cb(&button, SHORT_TIME, button_task, "Hello World!");

	esp_rgb_led_blink_start(&led, 200, 120, 63, 32);
	esp_buzzer_start(&buzzer, 100, 300, 0);

	xTaskCreate(shtc3_task, "shtc3 task", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 3, NULL);
	xTaskCreate(bsec_task, "bsec task", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 5, NULL);
	xTaskCreate(at24cs0x_task, "at24cs0x task", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 2, NULL);
	xTaskCreate(mics6814_task, "mics6814 task", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 1, NULL);
}
