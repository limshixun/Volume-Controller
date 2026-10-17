#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/task.h"
#include "soc/adc_channel.h"

#define BIT_WIDTH 12
#define V_REF 3300
float ALPHA = 0.2f;

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
    // ADC1_GPIO32_CHANNEL,
    // ADC1_GPIO33_CHANNEL,
    ADC1_GPIO34_CHANNEL,
    ADC1_GPIO35_CHANNEL,
    // ADC1_GPIO36_CHANNEL,
    // ADC1_GPIO37_CHANNEL,
    // ADC1_GPIO38_CHANNEL,
    // ADC1_GPIO39_CHANNEL,
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
      .bitwidth = ADC_BITWIDTH_12,
      .atten = ADC_ATTEN_DB_12,
  };

  for (int i = 0; i < num_of_gpios; i++) {
    ESP_ERROR_CHECK(adc_oneshot_config_channel(*adc1_handle, gpios[i], config));
  };
}

void initialize_cali(adc_cali_handle_t *cali_handle,
                     adc_cali_line_fitting_config_t *cali_config,
                     esp_err_t *cali_ret) {

  // 2. Configure the scheme. This tells the driver to read the eFuses
  //    associated with ADC Unit 1 at 12dB Attenuation.
  *cali_config = (adc_cali_line_fitting_config_t){
      .unit_id = ADC_UNIT_1,
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_12,
  };

  // 3. This is WHERE you load it. This function queries the hardware eFuses
  //    and loads the correction factors into your 'cali_handle'.
  *cali_ret = adc_cali_create_scheme_line_fitting(cali_config, cali_handle);

  bool use_efuse = (*cali_ret == ESP_OK);

  if (use_efuse) {
    printf("Successfully initialized eFuse calibration scheme!\n");

    // Check which specific eFuse value type is being used by the driver
    adc_cali_line_fitting_efuse_val_t efuse_val_type;
    esp_err_t check_ret =
        adc_cali_scheme_line_fitting_check_efuse(&efuse_val_type);

    if (check_ret == ESP_OK) {
      if (efuse_val_type == ADC_CALI_LINE_FITTING_EFUSE_VAL_EFUSE_TP) {
        printf(">>> eFuse Calibration Source: [Two-Point Calibration V2]\n");
      } else if (efuse_val_type == ADC_CALI_LINE_FITTING_EFUSE_VAL_EFUSE_VREF) {
        printf(">>> eFuse Calibration Source: [Factory Reference Voltage "
               "(Vref)]\n");
      } else {
        printf(">>> eFuse Calibration Source: None (Using default fallback "
               "software parameters)\n");
      }
    }
  } else {
    printf("Warning: Factory eFuse calibration data not found or not "
           "supported!\n");
  }
}

int map_voltage_to_percentage(int voltage) {
  // Adjust these to match your hardware limits if necessary
  int min_mv = 150;  // Anything 150mV or lower becomes 0%
  int max_mv = 3100; // Anything 3100mV or higher becomes 100%

  if (voltage <= min_mv)
    return 0;
  if (voltage >= max_mv)
    return 100;

  // Standard linear interpolation formula
  return ((voltage - min_mv) * 100) / (max_mv - min_mv);
}

void app_main(void) {
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1;
  adc_oneshot_chan_cfg_t config;

  adc_cali_handle_t cali_handle;
  adc_cali_line_fitting_config_t cali_config;
  esp_err_t cali_ret;

  int num_of_gpios = sizeof(gpios) / sizeof(int);
  gpio_channels *chan =
      (gpio_channels *)malloc(num_of_gpios * sizeof(gpio_channels));

  initialize_adc(&adc1_handle, &init_config1, &config, num_of_gpios);
  initialize_gpios(chan, num_of_gpios);
  initialize_cali(&cali_handle, &cali_config, &cali_ret);

  bool use_efuse = (cali_ret == ESP_OK);

  while (true) {
    for (int i = 0; i < num_of_gpios; i++) {
      int raw_adc_output;
      ESP_ERROR_CHECK(
          adc_oneshot_read(adc1_handle, chan[i].channel, &raw_adc_output));

      int v_data;
      if (use_efuse) {
        adc_cali_raw_to_voltage(cali_handle, raw_adc_output, &v_data);
      } else {
        v_data = calc_v_data(raw_adc_output);
      }

      if (!chan[i].initialized) {
        chan[i].ema = (float)v_data;
        chan[i].initialized = true;
      } else {
        chan[i].ema = calc_ema_filtered(ALPHA, v_data, chan[i].ema);
      }

      int smoothed_v = (int)(chan[i].ema + 0.5f);

      int volume_percentage = map_voltage_to_percentage(smoothed_v);

      printf("| GPIO %2d %3d | ", i, volume_percentage);
    };

    vTaskDelay(pdMS_TO_TICKS(50));
    printf("\n");
  }

  free(chan);
}
