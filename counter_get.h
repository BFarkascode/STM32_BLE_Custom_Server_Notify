/*
 *  Created on: June 10, 2025
 *  Author: BalazsFarkas
 *  Project: STM32_BLE_Custom_Server_Notification
 *  Processor: STM32WB5MMG
 *  Program version: 1.0
 *  File: bmp280_get.h
 *  Change history:
 */


#ifndef APP_COUNTER_GET_H_
#define APP_COUNTER_GET_H_

#include "stdint.h"
#include "custom_stm.h"

//LOCAL CONSTANT

//LOCAL VARIABLE
static uint32_t counter = 0;

//EXTERNAL VARIABLE

//FUNCTION PROTOTYPES
void Counter_Update(void);

#endif /* APP_COUNTER_GET_H_ */
