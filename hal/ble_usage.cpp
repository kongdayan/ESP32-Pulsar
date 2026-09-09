/*
 * ble_usage.cpp — 用 Arduino-ESP32 自带的 BLE（Bluedroid）实现 GATT server。
 *
 * 服务/特征值 UUID 与 client/pulsar_ble_client.py 保持一致，改一端必须改另一端。
 * onWrite 回调运行在 BLE 任务里，通过 usage_store_set() 的 seqlock 交给 UI 主循环。
 */
#include "ble_usage.h"

#include <Arduino.h>

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#include "usage_model.h"
#include "balance_model.h"

namespace {

const char *const kDeviceName    = "ESP32-Pulsar";
const char *const kServiceUuid   = "0b1e5a10-7e3d-4f1a-9c2b-1a2b3c4d5e60";
const char *const kUsageUuid     = "0b1e5a11-7e3d-4f1a-9c2b-1a2b3c4d5e60";
const char *const kStatusUuid    = "0b1e5a12-7e3d-4f1a-9c2b-1a2b3c4d5e60";
const char *const kBalanceUuid   = "0b1e5a13-7e3d-4f1a-9c2b-1a2b3c4d5e60";

const uint16_t    kPreferredMtu  = 512;
const char *const kStatusBoot    = "boot";
const char *const kStatusOk      = "ok";
const char *const kStatusError   = "err";
const uint16_t    kAdvMinInterval = 0x06;
const uint16_t    kAdvMaxInterval = 0x12;

BLECharacteristic *g_status = nullptr;

void publish_status(const char *text)
{
    if (g_status == nullptr) return;
    g_status->setValue(String(text));
    g_status->notify();
}

class UsageWriteCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *characteristic, esp_ble_gatts_cb_param_t *param) override
    {
        (void)param;
        const String value = characteristic->getValue();

        usage_data_t data;
        if (!usage_parse_json(value.c_str(), &data)) {
            publish_status(kStatusError);
            return;
        }

        data.rx_ms = (uint32_t)millis();
        usage_store_set(&data);
        publish_status(kStatusOk);
    }
};

class BalanceWriteCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *characteristic, esp_ble_gatts_cb_param_t *param) override
    {
        (void)param;
        const String value = characteristic->getValue();

        balance_data_t data;
        if (!balance_parse_json(value.c_str(), &data)) {
            publish_status(kStatusError);
            return;
        }

        data.rx_ms = (uint32_t)millis();
        balance_store_set(&data);
        publish_status(kStatusOk);
    }
};

class ServerCallbacks : public BLEServerCallbacks {
    void onDisconnect(BLEServer *server) override
    {
        server->startAdvertising();      /* 断开后继续广播，客户端可重连 */
    }
};

}  // namespace

void ble_usage_init(void)
{
    BLEDevice::init(kDeviceName);
    BLEDevice::setMTU(kPreferredMtu);

    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    BLEService *service = server->createService(kServiceUuid);

    BLECharacteristic *usage = service->createCharacteristic(
        kUsageUuid, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    usage->setCallbacks(new UsageWriteCallbacks());

    BLECharacteristic *balance = service->createCharacteristic(
        kBalanceUuid, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    balance->setCallbacks(new BalanceWriteCallbacks());

    g_status = service->createCharacteristic(
        kStatusUuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    g_status->addDescriptor(new BLE2902());
    g_status->setValue(String(kStatusBoot));

    service->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(kServiceUuid);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(kAdvMinInterval);
    advertising->setMaxPreferred(kAdvMaxInterval);
    BLEDevice::startAdvertising();
}
