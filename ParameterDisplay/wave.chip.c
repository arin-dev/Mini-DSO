// MIT License
// Copyright (c) 2023 MicroBeaut
// 01-Jun-23

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static float period;

typedef struct {
  pin_t pin_out_sine;
  pin_t pin_out_triangle;
  pin_t pin_out_square;
  float amplitude_attr;
  float frequency_attr;
  float reference_attr;
} chip_state_t;

static void chip_timer_event(void *user_data);

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));

  chip->pin_out_sine = pin_init("Sine", ANALOG);
  chip->pin_out_triangle = pin_init("Triangle", ANALOG);
  chip->pin_out_square = pin_init("Square", ANALOG);
  chip->amplitude_attr = attr_init_float("amplitude", 2.5f);
  chip->frequency_attr = attr_init_float("frequency", 1.0f);
  chip->reference_attr = attr_init_float("reference", 2.5f);

  const timer_config_t timer_config = {
    .callback = chip_timer_event,
    .user_data = chip,
  };
  timer_t timer_id = timer_init(&timer_config);
  timer_start(timer_id, 1, true);
}

void chip_timer_event(void *user_data) {
  chip_state_t *chip = (chip_state_t*)user_data;
  float amplitude = attr_read_float(chip->amplitude_attr);
  float frequency = attr_read_float(chip->frequency_attr);
  float reference = attr_read_float(chip->reference_attr);
  period += frequency / 1000000.0f;
  if (period > 1.0f) period = 0.0f;
  float sineValue = sin(period * 2.0f * M_PI);
  pin_dac_write(chip->pin_out_sine, amplitude * sineValue + reference);
  pin_dac_write(chip->pin_out_triangle, 2 * amplitude / M_PI * asin(sineValue) + reference);
  if (sineValue >= 0.0f) {
    pin_dac_write(chip->pin_out_square, reference + amplitude);
  } else {
    pin_dac_write(chip->pin_out_square, reference -amplitude);
  }
}