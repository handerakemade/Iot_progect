#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "DHT.h"
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h> // 使用ArduinoJson库处理JSON数据

// =================== 硬件配置 ===================
#define DHTPIN 6
#define DHTTYPE DHT11
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_I2C_ADDR 0x3C

// =================== WiFi和MQTT配置 ===================
const char* ssid = "513";
const char* password = "lysmwhwys0912";
const char* mqtt_server = "ra61176f.ala.eu-central-1.emqxsl.com"; // 注意EMQX的SSL地址格式
const int mqtt_port = 8883;
const char* mqtt_topic_pub = "esp32/pub/dht11";
const char* mqtt_topic_sub = "esp32/sub/dht11";
const char* mqtt_user = "test1";
const char* mqtt_password = "123456";

// =================== FreeRTOS句柄和全局对象 ===================
TaskHandle_t sensor_task_handle = NULL;
TaskHandle_t oled_task_handle = NULL;
TaskHandle_t mqtt_task_handle = NULL;

QueueHandle_t sensor_data_queue = NULL;
struct SensorData {
  float temperature;
  float humidity;
  bool valid;
};
const int queue_len = 5;

//用于检测wifi和mqtt连接情况
EventGroupHandle_t mqtt_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int MQTT_CONNECTED_BIT = BIT1;
//初始化对象
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);

// =================== CA证书 ===================
const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \
"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \
"TLtm8KPigxvD17I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPigxvD17I90VUw\n" \
"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgTlEkJoyQY/Esrh\n" \
"hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n" \
"06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n" \
"PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n" \
"YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n" \
"CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n" \
"-----END CERTIFICATE-----\n";

// =================== 函数声明 ===================
void connectToWiFi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();

// =================== 任务函数实现 ===================

// 任务1: 传感器数据读取任务 (优先级2)
void sensorTask(void *pvParameters) {
  SensorData data;
  Serial.println("传感器任务启动...");
  for (;;) {
    data.humidity = dht.readHumidity();
    data.temperature = dht.readTemperature();
    data.valid = !isnan(data.humidity) && !isnan(data.temperature);

    if (data.valid) {
      if (xQueueSend(sensor_data_queue, (void *)&data, (TickType_t)10) != pdPASS) {
        Serial.println("警告: 传感器数据队列已满，数据可能丢失!");
      }
    } else {
      Serial.println("错误: 从DHT传感器读取数据失败!");
    }
    vTaskDelay(pdMS_TO_TICKS(3000)); // 每3秒读取一次
  }
}

// 任务2: OLED显示任务 (优先级1 - 绑定到核心1)
void oledTask(void *pvParameters) {
  SensorData received_data;
  Serial.println("OLED starting...");
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println(F("SSD1306 wrong"));
    for(;;);//无限循环
  }
  display.display();
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("System starting...");
  display.display();

  for (;;) {
    if (xQueueReceive(sensor_data_queue, &received_data, portMAX_DELAY) == pdPASS) {//永久等待拿数据
      if (received_data.valid) {
        display.clearDisplay();

        display.setCursor(0, 0);
        display.printf("Tem: %.1f C", received_data.temperature);

        display.setCursor(0, 15);
        display.printf("Hum: %.1f %%", received_data.humidity);

        display.setCursor(0, 30);
        //用事件组检测wifi和mqtt连接情况并显示在oled屏幕上
        EventBits_t bits = xEventGroupGetBits(mqtt_event_group);
        if (bits & MQTT_CONNECTED_BIT) {
            display.print("MQTT: Connected");
        } else if (bits & WIFI_CONNECTED_BIT) {
            display.print("MQTT: Disconnected");
        } else {
            display.print("WiFi: Connecting");
        }
        display.display();
      }
    }
  }
}

// 任务3: MQTT任务 (优先级1)
void mqttTask(void *pvParameters) {
  SensorData received_data;
  Serial.println("MQTT任务启动...");

  // 设置CA证书
  espClient.setCACert(root_ca);

  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(callback);

  //检测wifi和mqtt连接情况
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      xEventGroupClearBits(mqtt_event_group, WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT);
      connectToWiFi();
      xEventGroupSetBits(mqtt_event_group, WIFI_CONNECTED_BIT);
    }

    if (!mqtt_client.connected()) {
      xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
      reconnectMQTT();
      xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
    }
    mqtt_client.loop();

    //永久等待拿取温湿度数据
    if (xQueueReceive(sensor_data_queue, &received_data, portMAX_DELAY) == pdPASS) {
      if (received_data.valid) {
        // 使用ArduinoJson库创建JSON文档
        JsonDocument doc;
        doc["temperature"] = received_data.temperature;
        doc["humidity"] = received_data.humidity;
        doc["device"] = "esp32-s3";

        // 序列化JSON到字符串
        char json_buffer[200];
        serializeJson(doc, json_buffer, sizeof(json_buffer));

        // 发布到MQTT主题
        if (mqtt_client.publish(mqtt_topic_pub, json_buffer)) {
          Serial.printf("MQTT消息发布成功: %s\n", json_buffer);
        } else {
          Serial.println("错误: MQTT消息发布失败!");
        }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// =================== MQTT相关辅助函数 ===================

// 连接到WiFi
void connectToWiFi() {
  Serial.printf("正在连接到WiFi: %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    Serial.print(".");
  }
  Serial.println("\nWiFi已连接!");
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());
}

// MQTT收到消息的回调函数
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("收到消息 [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// 连接/重连到MQTT服务器
void reconnectMQTT() {
  while (!mqtt_client.connected()) {
    Serial.print("尝试连接到MQTT服务器...");
    String client_id = "esp32-client-" + String(WiFi.macAddress());
    if (mqtt_client.connect(client_id.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("MQTT已连接!");
      if (mqtt_client.subscribe(mqtt_topic_sub)) {
        Serial.printf("成功订阅主题: %s\n", mqtt_topic_sub);
      }
    } else {
      Serial.printf("连接失败, rc=%d 将在5秒后重试\n", mqtt_client.state());
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
  }
}

// =================== Arduino Setup & Loop ===================

void setup() {
  Serial.begin(9600);

  dht.begin();

  //创建事件组和队列
  mqtt_event_group = xEventGroupCreate();

  sensor_data_queue = xQueueCreate(queue_len, sizeof(SensorData));
  if (sensor_data_queue == NULL) {
    Serial.println("错误: 创建队列失败!");
    while(1);
  }

  // 创建传感器任务
  xTaskCreate(
    sensorTask,
    "Sensor Task",
    4096,
    NULL,
    2,
    &sensor_task_handle
  );

  // 创建OLED显示任务 (绑定到核心1)
  xTaskCreatePinnedToCore(
    oledTask,
    "OLED Task",
    4096,
    NULL,
    1,
    &oled_task_handle,
    1
  );

  // 创建MQTT任务
  xTaskCreate(
    mqttTask,
    "MQTT Task",
    8192,
    NULL,
    1,
    &mqtt_task_handle
  );

  Serial.println("所有任务已创建，FreeRTOS调度器即将启动...");
}

void loop() {
  vTaskDelete(NULL);
}