#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#define LED_PIN 4

const char* ssid = "513"; 
const char* password = "lysmwhwys0912"; 
// 固件更新文件的URL地址
const char* firmwareUrl = "http://192.168.1.103:8000/firmware.bin"; // 示例地址，稍后修改

// 函数声明
void connectToWiFi();
void performOTA();

void setup() {
  Serial.begin(9600); // 初始化串口通信，波特率9600
  Serial.println("设备启动...");
  pinMode(LED_PIN,OUTPUT);

  // 连接Wi-Fi
  connectToWiFi();

  // 执行OTA更新检查与升级
  performOTA();

}

void loop() {
  // 主循环，OTA升级后设备将运行新的固件代码
  // 这里我们让LED灯闪烁，表示设备在正常运行
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  Serial.println("ota更新成功，恭喜你！");
}

// 连接Wi-Fi函数
void connectToWiFi() {
  Serial.printf("正在连接至Wi-Fi: %s", ssid);
  WiFi.begin(ssid, password);
  
  // 等待连接成功，超时时间约20秒
  int tryCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tryCount++;
    if (tryCount > 40) { // 20秒后仍未连接则放弃
      Serial.println("\nWi-Fi连接失败！设备将重启...");
      delay(2000);
      ESP.restart(); // 重启设备
    }
  }
  Serial.println("\nWi-Fi连接成功!");
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());
}

// 执行OTA更新函数
void performOTA() {
  Serial.println("准备检查OTA更新...");
  
  // 检查Wi-Fi连接状态
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("网络已连接，开始OTA升级流程");
    
    // 创建HTTPClient和WiFiClient对象
    HTTPClient http;
    WiFiClient client; // 用于HTTPUpdate

    // 可选：注册OTA更新过程的回调函数，用于打印进度和状态
    // 当升级开始时
    httpUpdate.onStart([]() {
      Serial.println("OTA更新开始...");
    });
    // 当升级有进度时
    httpUpdate.onProgress([](int cur, int total) {
      Serial.printf("升级进度: %d%% (已下载 %d 字节 / 总计约 %d 字节)\r", 
                   (int)(cur * 100 / total), cur, total);
    });
    // 当升级结束时
    httpUpdate.onEnd([]() {
      Serial.println("\nOTA更新文件传输完成，设备即将重启应用新固件...");
    });
    // 当升级出错时
    httpUpdate.onError([](int err) {
      Serial.printf("OTA更新过程中出错[错误代码: %d]，详情请查阅文档\n", err);
    });

    Serial.print("正在从URL获取固件: ");
    Serial.println(firmwareUrl);
    
    // 执行OTA更新
    t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);
    
    // 根据返回结果进行处理
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP升级失败: (%d) %s\n", 
                     httpUpdate.getLastError(), 
                     httpUpdate.getLastErrorString().c_str());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("检查完毕，暂无可用更新");
        break;
      case HTTP_UPDATE_OK:
        Serial.println("OTA更新指令已接受，设备即将重启"); 
        // 如果更新成功，httpUpdate会负责重启设备，后面的代码不会执行
        delay(5000); // 稍等片刻，让串口信息发送完
        ESP.restart(); // 保险起见，如果没重启就手动重启
        break;
    }
  } else {
    Serial.println("网络未连接，跳过OTA检查");
  }
  Serial.println("OTA流程结束，即将进入主循环");
}
