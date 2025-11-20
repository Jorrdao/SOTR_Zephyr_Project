/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* O Kconfig garante que esta macro está definida se o driver GPIO for ativado. 
 * A configuração do LED depende do Device Tree (DTS). */
#define LED0_NODE DT_ALIAS(led0)

/* Obter a especificação do dispositivo LED0 a partir do Device Tree */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Definição do tempo de espera em milissegundos (meio segundo) */
#define SLEEP_TIME_MS   5000

int main(void)
{
    int ret;

    // 1. Verificação da existência do LED
    if (!device_is_ready(led.port)) {
        return 0;
    }

    

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }

    // 3. Loop principal (Onde implementaria as suas threads RTOS)
    while (1) {
        // Ligar o LED
        ret = gpio_pin_set_dt(&led, 1);
        if (ret < 0) {
            return 0;
        }
        k_msleep(SLEEP_TIME_MS);

        // Desligar o LED
        ret = gpio_pin_set_dt(&led, 0);
        if (ret < 0) {
            return 0;
        }
        k_msleep(SLEEP_TIME_MS);
    }
}