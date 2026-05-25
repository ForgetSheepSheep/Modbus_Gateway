#ifndef __COMPONENT_4G_H
#define __COMPONENT_4G_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 4G模块操作返回值 */
#define FOUR_G_OK           0u
#define FOUR_G_ERROR        1u
#define FOUR_G_TIMEOUT      2u

void FourGInit(void);
void FourGClearRecvBuf(void);

uint8_t FourGSendCmd(const char *cmd);
uint8_t FourGWaitResp(const char *expect, uint32_t timeout_ms);
uint8_t FourGSendCmdAndWait(const char *cmd, const char *expect, uint32_t timeout_ms);

/* 基础AT检测接口 */
uint8_t FourGCheckAT(void);
uint8_t FourGCloseEcho(void);
uint8_t FourGCheckSim(void);
uint8_t FourGCheckSignal(void);
uint8_t FourGCheckNetAttach(void);

/* MQTT最小闭环接口 */
uint8_t FourGMqttConnect(void);
uint8_t FourGMqttSubscribe(void);
uint8_t FourGMqttPublishHello(void);
void FourGMqttReceiveProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* __COMPONENT_4G_H */
