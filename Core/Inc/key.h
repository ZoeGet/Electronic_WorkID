/**
  ********************************************************************************
  * @file      key.h
  * @brief     按键操作头文件
  * @author    Jia Yasen
  * @date      2022-6-20
  * @version   V0.0.1
  * @copyright @Etherealize
  ********************************************************************************
  * @attention 1.本文件的函数均基于HAL库,按键KEY的初始化基于GPIO的初始化,GPIO对应引脚
  *            初始化为输入模式即可
  *            2.由于不同板子的对应引脚不同，使用前需先对照原理图修改KEY对应的引脚
  ********************************************************************************
  */
#ifndef __KEY_H__
#define __KEY_H__
/*-----------------------------Includes---------------------------------------*/
//根据使用的芯片型号进行修改
#include "stm32l4xx_hal.h"
/*-----------------------------Define-----------------------------------------*/
//将KEYx定义为对应引脚的读取值
#define KEY0 HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_7)
#define KEY1 HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_6)
#define KEY2 HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_5)

// #define KEY0 HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13)
// #define KEY1 HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0)


//给不同的KEY设定不同的对应数字，表示对应KEY按下;也表示不同的优先级,数字越小优先级越高;不用0，因为0是函数的默认返回值
#define KEY0_PRES 1
#define KEY1_PRES 2
#define KEY2_PRES 3

// 按键电平定义（按原理图：按下短接到 GND，因此按下=低电平）
#define KEY0_ON GPIO_PIN_RESET
#define KEY0_OFF GPIO_PIN_SET
#define KEY1_ON GPIO_PIN_RESET
#define KEY1_OFF GPIO_PIN_SET
#define KEY2_ON GPIO_PIN_RESET
#define KEY2_OFF GPIO_PIN_SET
/*-----------------------------Function---------------------------------------*/
uint8_t KEY_Scan1(void);
uint8_t KEY_Scan2(void);
uint8_t KEY_Scan(uint8_t mode);

#endif
/************************ (C) COPYRIGHT Etherealize*****END OF FILE***********/

