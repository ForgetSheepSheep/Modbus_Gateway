#ifndef _PORT_SERIAL_H_
#define _PORT_SERIAL_H_

#include "port_macro.h"

void ModbusMasterSerialInit(UCHAR port, ULONG baudRate, UCHAR dataBits, UCHAR parity);
void ModbusMasterPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable);
void ModbusMasterPortSerialGetByte(CHAR *Byte);
void ModbusMasterSendFrame(UCHAR *frame, USHORT len);

#endif /* _PORT_SERIAL_H_ */
