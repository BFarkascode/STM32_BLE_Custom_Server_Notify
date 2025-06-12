
/*
 *  Created on: June 10, 2025
 *  Author: BalazsFarkas
 *  Project: STM32_BLE_Custom_Server_Notification
 *  Processor: STM32WB5MMG
 *  Program version: 1.0
 *  File: bmp280_get.c
 *  Change history:
 */


#include "counter_get.h"


void Counter_Update(void){

	//the BLE_sensor code first extracts the temp value and stores it in a struct. That struct is then formed into the value that is sent over as the char
	//here we skip that since value length for the char is just 1 byte of value

//	temp = bmp280_get_temp();

	Custom_STM_App_Update_Char(CUSTOM_STM_COUNTER, (uint8_t *)&counter);

	char str[2];

	sprintf(str, "%x", counter);

	ssd1315_advance_clear();

	ssd1315_advance_string(0, 0, str, 2, 1, 0x10);

	counter++;

}
