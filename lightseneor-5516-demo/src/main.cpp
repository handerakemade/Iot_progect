#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int LIGHT_PIN = A0;          // 光敏电阻接 GPIO1 (A0)
const int OLED_ADDR = 0x3C;        // 地址
const int SCREEN_W  = 128;
const int SCREEN_H  = 64;

// OLED 对象
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);

// 把 ADC 读到的 0~4095 映射到 0~255 亮度
int mapBrightness(int adc) {
  return constrain(map(adc, 0, 4095, 0, 255), 0, 255);
}

// 把 ADC 映射到 0~100% 用于显示
int mapPercent(int adc) {
  return constrain(map(adc, 0, 4095, 0, 100), 0, 100);
}

void setup() {
  Serial.begin(9600);
  pinMode(LIGHT_PIN, INPUT);   // 模拟脚其实可以不设，但写上更清晰

  // 初始化 I²C（ESP32-S3 默认引脚 8/9，可以省去，但是为了严谨加上）
  Wire.begin(8, 9);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED 初始化失败，请检查连线"));
    for (;;)
      ;
  }

  oled.clearDisplay();
  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  oled.println(F("Light->OLED"));
  oled.display();
  delay(1000);
}

void loop() {
  int lightRaw = 4095 - analogRead(LIGHT_PIN);          // 0~4095，反转一下
  int brightness = mapBrightness(lightRaw);      // 0~255
  int percent    = mapPercent(lightRaw);         // 0~100

  // 1) 调 OLED 硬件亮度
  oled.ssd1306_command(SSD1306_SETCONTRAST);     // 0x81
  oled.ssd1306_command(brightness);

  // 2) 在屏幕上画一条简单进度条 + 百分比
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.print(F("Light: "));
  oled.print(percent);
  oled.println(F("%"));

  // 画 100 像素长的进度条
  int barWidth = map(percent, 0, 100, 0, 100);
  oled.fillRect(0, 16, barWidth, 8, WHITE);      // 亮部
  oled.drawRect(0, 16, 100, 8, WHITE);           // 边框

  oled.display();

  Serial.printf("ADC:%4d  Bright:%3d  Percent:%3d%%\n",
                lightRaw, brightness, percent);

  delay(200);   // 200 ms 刷新一次，足够流畅
}