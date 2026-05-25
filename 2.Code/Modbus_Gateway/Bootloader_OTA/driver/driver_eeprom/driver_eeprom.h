#ifndef __DRIVER_EEPROM_H
#define __DRIVER_EEPROM_H

#include "config.h"

void     DrvEepromInit(void);
uint8_t  DrvEepromRead(uint8_t readAddr, uint8_t *pBuffer, uint16_t numToRead);
uint8_t  DrvEepromWrite(uint8_t writeAddr, uint8_t *pBuffer, uint16_t numToWrite);

#endif /* __DRIVER_EEPROM_H */
