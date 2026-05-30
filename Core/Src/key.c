/**
  ********************************************************************************
  * @file      key.c
  * @brief     按键操作源文件
  * @author    Jia Yasen
  * @date      2022-6-20
  * @version   V0.0.1
  * @copyright @Etherealize
  ********************************************************************************
  */
/*-----------------------------Includes---------------------------------------*/
#include "key.h"
/*-----------------------------Function---------------------------------------*/

/**
  * @brief      按键扫描（持续触发）
  * @param[in]  None
  * @return     uint8_t 对应按键值
  * @retval     0       默认值,无操作
  *             1       KEY0对应值,表示KEY0按下,可在循环中查询是否为1来进行对应操作
  *             2       KEY1对应值
  * @details    持续触发，则确认按下后的那部分会在按下的时间里执行很多次，因为按下
  *             的时间有零点几秒，但是程序每几毫秒循环一次。所以在按下的时间里会执行多次
  */
uint8_t KEY_Scan1(void)
{
  if(KEY0 == KEY0_ON || KEY1 == KEY1_ON || KEY2 == KEY2_ON)
  {
    HAL_Delay(10);//延时10-20ms，软件消抖
    if(KEY0 == KEY0_ON)
    {
      return KEY0_PRES;//此处的操作会多次执行
    }
    else if(KEY1 == KEY1_ON)
    {
      return KEY1_PRES;//此处的操作会多次执行
    }
    else if(KEY2 == KEY2_ON)
    {
      return KEY2_PRES;//此处的操作会多次执行
    }
    //一般是按下触发事件,松开返回正常状态，不触发
    else
      return 0;
  }
  return 0;//没有按下默认返回0
}

/**
  * @brief      按键扫描（单次触发）
  * @param[in]  None
  * @return     uint8_t 对应按键值
  * @retval     0       默认值,无操作
  *             1       KEY0对应值,表示KEY0按下,可在循环中查询是否为1来进行对应操作
  *             2       KEY1对应值
  * @details    单次触发:用key_up作为制约,按下后只会触发一次操作,直到松开才会使下一次按下发挥作用
  */
uint8_t KEY_Scan2(void)
{
  //静态变量存储在静态存储区,只会在程序一开始的时候初始化一次，将局部变量静态化可以使变量独立于函数，而不是每次调用函数时重新初始化一次
  static uint8_t key_up = 1; //按键松开标志
  //用key_up作为约束,在第一次检测到按键按下并执行操作之后key_up变为0，在持续按住按键的之后几次扫描中都不会触发
  if(key_up && (KEY0 == KEY0_ON || KEY1 == KEY1_ON || KEY2 == KEY2_ON))
  {
    HAL_Delay(10);
    key_up = 0;
    if(KEY0 == KEY0_ON)
    {
      return KEY0_PRES;//
    }
    else if(KEY1 == KEY1_ON)
    {
      return KEY1_PRES;//操作只会触发一次
    }
    else if(KEY2 == KEY2_ON)
    {
      return KEY2_PRES;//操作只会触发一次
    }
    else
      return 0;
  }
  else if(KEY0 == KEY0_OFF && KEY1 == KEY1_OFF && KEY2 == KEY2_OFF) key_up = 1;//按键松开时key_up重置，下次再按下才会触发
  return 0;
}

/**
  * @brief      按键扫描
  * @param[in]  mode 模式转换. 0:单次触发 1:持续触发
  * @return     uint8_t 对应按键值
  * @retval     0       默认值,无操作
  *             1       KEY0对应值,表示KEY0按下,可在循环中查询是否为1来进行对应操作
  *             2       KEY1对应值
  * @details    若持续触发，则确认按下后的那部分会在按下的时间里执行很多次,因为按下
  *             的时间有零点几秒,但是程序每几毫秒循环一次,所以在按下的时间里会执行多次.
  *             若单次触发:用key_up作为制约,按下后只会触发一次操作,直到松开才会
  *             使下一次按下发挥作用.
  */
uint8_t KEY_Scan(uint8_t mode)
{
  static uint8_t key_up = 1; //按键松开标志
  static uint32_t last_press_time = 0; // 上次按键时间
  static uint8_t debounce_count = 0; // 消抖计数器
  
  if(mode) key_up = 1; //mode为1时,每次扫描都会重置key_up,操作就可以不断触发,持续触发
  
  if(key_up == 1 && (KEY0 == KEY0_ON || KEY1 == KEY1_ON || KEY2 == KEY2_ON))
  {
    // 非阻塞式消抖：检查按键状态是否稳定
    if (HAL_GetTick() - last_press_time > 5) { // 5ms检查一次
      last_press_time = HAL_GetTick();
      debounce_count++;
      
      // 连续检测到5次按键按下（约25ms），认为按键稳定
      if (debounce_count >= 5) {
        debounce_count = 0;
        
        // 再次检查按键状态
        if(KEY0 == KEY0_ON)
        {
          key_up = 0;
          return KEY0_PRES;
        }
        else if(KEY1 == KEY1_ON)
        {
          key_up = 0;
          return KEY1_PRES;
        }
        else if(KEY2 == KEY2_ON)
        {
          key_up = 0;
          return KEY2_PRES;
        }
      }
    }
  }
  else if(KEY0 == KEY0_OFF && KEY1 == KEY1_OFF && KEY2 == KEY2_OFF) {
    key_up = 1; //松开则刷新key_up
    last_press_time = 0;
    debounce_count = 0;
  }
  return 0;
}

/************************ (C) COPYRIGHT Etherealize*****END OF FILE***********/
