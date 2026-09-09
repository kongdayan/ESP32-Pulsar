/*
 * ble_usage.h — BLE 用量接收端（GATT server）。
 *
 * 设备以 "ESP32-Pulsar" 广播一个自定义服务；电脑端客户端（client/）扫描连接后，
 * 把一行扁平 JSON 写入 usage 特征值，固件解析进 core/usage_model 的共享存储。
 * 只做 I/O 与解析转交，不含业务判断。
 */
#ifndef HAL_BLE_USAGE_H
#define HAL_BLE_USAGE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ble_usage_init(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_BLE_USAGE_H */
