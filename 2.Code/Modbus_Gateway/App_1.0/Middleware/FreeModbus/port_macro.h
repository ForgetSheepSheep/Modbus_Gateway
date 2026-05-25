/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

#ifndef _PORT_MACRO_H_
#define _PORT_MACRO_H_

#include <inttypes.h>


typedef unsigned char UCHAR;
typedef char CHAR;

typedef uint16_t USHORT;
typedef int16_t SHORT;

typedef uint32_t ULONG;
typedef int32_t LONG;

typedef uint8_t BOOL;

#ifndef TRUE
#define TRUE            1
#endif

#ifndef FALSE
#define FALSE           0
#endif

#define MODBUS_RX_MAX_TIMEOUT		           1000   // 单位3.5T，9600波特率，3.5T为4ms左右


//#define DBGLOG

#if defined(DBGLOG)
#include <stdio.h>
#define DBG_log(format, ...) printf("[log]"format"["__FILE__ ":" __S_LINE__ "]\r\n",##__VA_ARGS__)
#else
#define DBG_log(format, ...)
#endif

//#define  USE_FULL_ASSERT

#ifdef  USE_FULL_ASSERT
  #define AssertParam(expr)    do { \
									if ((expr) == 0)  \
									{              \
										DBG_log("Wrong parameters value\n"); \
										while (1); \
									}              \
								} while (0)
#else
  #define AssertParam(expr) ((void)0U)
	  
#endif
  
#endif
