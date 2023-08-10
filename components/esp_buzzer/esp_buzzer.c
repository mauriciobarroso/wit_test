/**
  ******************************************************************************
  * @file           : esp_buzzer.c
  * @author         : Mauricio Barroso Benavides
  * @date           : Feb 27, 2023
  * @brief          : todo: write brief
  ******************************************************************************
  * @attention
  *
  * MIT License
  *
  * Copyright (c) 2023 Mauricio Barroso Benavides
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to
  * deal in the Software without restriction, including without limitation the
  * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
  * sell copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in
  * all copies or substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  * IN THE SOFTWARE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "esp_buzzer.h"
#include "esp_log.h"

/* Private macro -------------------------------------------------------------*/

/* External variables --------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* Tag for debug */
static const char * TAG = "esp_buzzer";

/* Private function prototypes -----------------------------------------------*/
static void buzzer_timer_handler(TimerHandle_t timer);
static void buzzer_pause_timer_handler(TimerHandle_t timer);

/* Exported functions --------------------------------------------------------*/
/**
  * @brief Initialize a buzzer instance
  */
esp_err_t esp_buzzer_init(esp_buzzer_t *const me, gpio_num_t gpio) {
	ESP_LOGI(TAG, "Initializing buzzer...");

	esp_err_t ret = ESP_OK;

	/* Fill the members structure with their default values*/
	me->on_time = 0;
	me->off_time = 0;
	me->level = true;
	me->gpio = gpio;
	me->state = BUZZER_STOP_STATE;

	/* Configure a GPIO to drive the buzzer */
	gpio_config_t gpio_conf;
	gpio_conf.intr_type = GPIO_INTR_DISABLE;
	gpio_conf.mode = GPIO_MODE_OUTPUT;
	gpio_conf.pin_bit_mask = 1ULL << me->gpio;
	gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;

	ret = gpio_config(&gpio_conf);

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to configure buzzer GPIO");
		return ret;
	}

	/* Turn off the buzzer */
	ret = gpio_set_level(me->gpio, false);

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to turn off the buzzer");
		return ret;
	}

	/* Create a timer to control the buzzer function */
	me->timer_handle = xTimerCreate("Buzzer timer",
			100, /* fixme: define better */
			pdTRUE,
			(void *)me,
			buzzer_timer_handler);

	if (me->timer_handle == NULL) {
		ESP_LOGE(TAG, "Failed to create buzzer timer");
		return ESP_FAIL;
	}

	/* Create a timer to pause the buzzer funciton */
	me->pause_timer_handle = xTimerCreate("Buzzer pause timer",
			100, /* fixme: define better */
			pdFALSE,
			(void *)me,
			buzzer_pause_timer_handler);

	if (me->pause_timer_handle == NULL) {
		ESP_LOGE(TAG, "Failed to create buzzer timer");
		return ESP_FAIL;
	}

	/* Return ESP_OK */
	return ret;
}

/**
  * @brief Start a buzzer instance
  */
void esp_buzzer_start(esp_buzzer_t *const me, uint16_t on_time, uint16_t off_time, uint8_t times) {
	/* Store the new buzzer parameters */
	me->on_time = pdMS_TO_TICKS(on_time);
	me->off_time = pdMS_TO_TICKS(off_time);
	me->times = times;
	me->state = BUZZER_RUN_STATE;

	/* Change the timer period and start */
	xTimerChangePeriod(me->timer_handle, pdMS_TO_TICKS(me->on_time), 0);

	/* Turn on the buzzer */
	gpio_set_level(me->gpio, true);
}

/**
  * @brief Stop a buzzer instance
  */
void esp_buzzer_stop(esp_buzzer_t * const me) {
	me->state = BUZZER_STOP_STATE;

	/* Stop the buzzer timer to stop the buzzer */
	xTimerStop(me->timer_handle, 0);
	gpio_set_level(me->gpio, false);
}

/**
  * @brief Pause a buzzer instance
  */
void esp_buzzer_pause(esp_buzzer_t *const me, uint32_t time) {
	me->pause_time = pdMS_TO_TICKS(time);
	me->state = BUZZER_PAUSE_STATE;

	/* Turn off the buzzer */
	gpio_set_level(me->gpio, false);

	xTimerChangePeriod(me->pause_timer_handle, me->pause_time, 0);
}

/**
  * @brief Pause a buzzer instance
  */
buzzer_state_e esp_buzzer_get_state(esp_buzzer_t *const me) {
	return me->state;
}

/* Private functions ---------------------------------------------------------*/
static void buzzer_timer_handler(TimerHandle_t timer) {
	/* Get the buzzer instance parameters */
	esp_buzzer_t *buzzer = (esp_buzzer_t *)pvTimerGetTimerID(timer);

	/* Check if the buzzer is active */
	if (buzzer->state == BUZZER_RUN_STATE) {
		/* If the buzzer off time is not equal to zero, do the toggle function */
		if (buzzer->off_time) {
			/* Toggle the buzzer level */
			buzzer->level = !buzzer->level;

			/* Change the buzzer timer period according its level */
			xTimerChangePeriod(buzzer->timer_handle,
					buzzer->level ? buzzer->on_time : buzzer->off_time,
					0);

			/* Turn on or turn off the buzzer */
			gpio_set_level(buzzer->gpio, buzzer->level);
		}

		/* Count the times the buzzer must be toggled */
		static uint16_t times_counter = 0;

		if (buzzer->level && buzzer->times != 0) {
			times_counter++;

			if (times_counter >= buzzer->times) {
				times_counter = 0;
				esp_buzzer_stop(buzzer);
			}
		}
	}
}

static void buzzer_pause_timer_handler(TimerHandle_t timer) {
	/* Get the buzzer instance parameters */
	esp_buzzer_t *buzzer = (esp_buzzer_t *)pvTimerGetTimerID(timer);

	buzzer->state = BUZZER_RUN_STATE;
}

/***************************** END OF FILE ************************************/
