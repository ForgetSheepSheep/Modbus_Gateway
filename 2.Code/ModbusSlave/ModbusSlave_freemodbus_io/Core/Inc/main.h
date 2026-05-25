/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f0xx_hal.h"

void Error_Handler(void);
#define KEY1_Pin GPIO_PIN_3
#define KEY1_GPIO_Port GPIOA
#define KEY2_Pin GPIO_PIN_4
#define KEY2_GPIO_Port GPIOA
#define KEY3_Pin GPIO_PIN_5
#define KEY3_GPIO_Port GPIOA
#define LED1_Pin GPIO_PIN_11
#define LED1_GPIO_Port GPIOB
#define LED2_Pin GPIO_PIN_12
#define LED2_GPIO_Port GPIOB
#define LED3_Pin GPIO_PIN_13
#define LED3_GPIO_Port GPIOB
#define RS485_CTRL_Pin GPIO_PIN_8
#define RS485_CTRL_GPIO_Port GPIOA
#define RELAY1_Pin GPIO_PIN_4
#define RELAY1_GPIO_Port GPIOB
#define RELAY2_Pin GPIO_PIN_5
#define RELAY2_GPIO_Port GPIOB
#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
