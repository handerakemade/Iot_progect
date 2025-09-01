#include <Arduino.h>
#include <esp_sleep.h>
#include <WiFi.h>

#define SLEEP_TIME_US 10 * 1000 * 1000  // 10秒 = 10,000,000 微秒

void setup() {
  Serial.begin(9600);
  delay(1000);  // 等待串口稳定

  Serial.println("准备进入 Deep-Sleep 模式...");
  delay(1000);

  // 设置 RTC 定时器10秒后唤醒
  esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);

  // 可选：关闭蓝牙/WiFi 以进一步降低功耗
  btStop();
  WiFi.mode(WIFI_OFF);

  Serial.println("进入 Deep-Sleep...");
  esp_deep_sleep_start();  // 进入深度休眠，唤醒后会从头开始执行
}

void loop() {
  // 不会执行
}