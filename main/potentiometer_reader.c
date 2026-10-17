#include "freertos/FreeRTOS.h"
#include "soc/adc_channel.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "esp_adc/adc_oneshot.h"
#include "freertos/task.h"
#include <stdbool.h>

#define BIT_WIDTH 12
#define V_REF 3300
#define ADC_BITWIDTH ADC_BITWIDTH_DEFAULT

int calc_v_data(int data) { return (data * V_REF) / 4095; }

// Exponential Moving Average to smooth out potentiometer noise
float calc_ema_filtered(float alpha, int data, float previos_ema) {
  return alpha * data + (1 - alpha) * previos_ema;
}

typedef struct {
  int channel;
  float ema;
  bool initialized;
} gpio_channels;

int gpios[] = {
    ADC1_GPIO36_CHANNEL, ADC1_GPIO37_CHANNEL, ADC1_GPIO38_CHANNEL,
    ADC1_GPIO39_CHANNEL, ADC1_GPIO32_CHANNEL, ADC1_GPIO33_CHANNEL,
    ADC1_GPIO34_CHANNEL,
};

void initialize_gpios(gpio_channels channels[], int size) {
  for (int i = 0; i < size; i++) {
    channels[i] = (gpio_channels){gpios[i], 0.00, false};
  };
}

void initialize_adc(adc_oneshot_unit_handle_t *adc1_handle,
                    adc_oneshot_unit_init_cfg_t *init_config1,
                    adc_oneshot_chan_cfg_t *config, int num_of_gpios) {
  // ADC driver configurations
  *init_config1 = (adc_oneshot_unit_init_cfg_t){
      .unit_id = ADC_UNIT_1,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };

  ESP_ERROR_CHECK(adc_oneshot_new_unit(init_config1, adc1_handle));

  // ADC channel config
  *config = (adc_oneshot_chan_cfg_t){
      .bitwidth = ADC_BITWIDTH_DEFAULT,
      .atten = ADC_ATTEN_DB_12,
  };

  for (int i = 0; i < num_of_gpios; i++) {
    ESP_ERROR_CHECK(adc_oneshot_config_channel(*adc1_handle, gpios[i], config));
  };
}

void app_main(void) {
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1;
  adc_oneshot_chan_cfg_t config;

  int num_of_gpios = sizeof(gpios) / sizeof(int);

  initialize_adc(&adc1_handle, &init_config1, &config, num_of_gpios);
  gpio_channels *chan =
      (gpio_channels *)malloc(num_of_gpios * sizeof(gpio_channels));
  initialize_gpios(chan, num_of_gpios);

  float alpha = 0.2f;

  while (true) {
    for (int i = 0; i < num_of_gpios; i++) {
      int raw_adc_output;
      ESP_ERROR_CHECK(
          adc_oneshot_read(adc1_handle, chan[i].channel, &raw_adc_output));
      int v_data = calc_v_data(raw_adc_output);

      if (!chan[i].initialized) {
        chan[i].ema = (float)v_data;
        chan[i].initialized = true;
      } else {
        chan[i].ema = calc_ema_filtered(alpha, v_data, chan[i].ema);
      }

      int smoothed_v = (int)(chan[i].ema + 0.5f);

      int volume_percentage = (smoothed_v * 100) / V_REF;
      if (volume_percentage > 100)
        volume_percentage = 100;
      if (volume_percentage < 0)
        volume_percentage = 0;

      printf("GPIO %d %d\n", gpios[i], volume_percentage);
    };

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  free(chan);
}
