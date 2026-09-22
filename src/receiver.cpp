#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/***********************************************************************
 *                         HARDWARE PIN MAPPING                        *
 * กำหนดขาใช้งานอุปกรณ์ต่อพ่วงของบอร์ด (จอ OLED I2C และ Buzzer)            *
 ***********************************************************************/
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define BUZZER_PIN 25 //ขาควบคุม Buzzer

/***********************************************************************
 *                   NETWORK & ENDPOINT CONFIGURATION                  *
 * ตั้งค่าการเชื่อมต่อไปยังบอร์ดส่ง และ URL ปลายทาง                             *
 ***********************************************************************/
const char *ssid = "ESP32_Main_AP";
const char *password = "87654321";
const char *serverUrl = "http://192.168.4.1/data"; //ดึงข้อมูล JSON ผ่าน path /data

/***********************************************************************
 *                     GLOBAL VARIABLES & TIMING                       *
 * ตัวแปรจัดการเวลาการยิง Request เพื่อไม่ให้บล็อกการทำงานของระบบ               *
 ***********************************************************************/
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 1000; // ยิงขอข้อมูลจากบอร์ดส่งทุก 1 วิ

/***********************************************************************
 *                         HELPER FUNCTIONS                            *
 *             ฟังก์ชันสำหรับเคลียร์จอและแสดงข้อความแจ้งเตือนสั้นๆ               *
 ***********************************************************************/
//ใช้แสดงสถานะระหว่างรอเชื่อมต่อ Wi-Fi หรือเมื่อเกิด Error
void showMessage(String line1, String line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(5, 20);
  display.println(line1);
  if (line2 != "") {
    display.setCursor(5, 35);
    display.println(line2);
  }
  display.display();
}

/***********************************************************************
 *                           SETUP FUNCTION                            *
 * ตั้งค่าเริ่มต้นระบบ: ขา IO, หน้าจอ OLED และเชื่อมต่อไปยัง Wi-Fi ของฝั่งรับ        *
 ***********************************************************************/
void setup() {
  Serial.begin(115200);

  //ตั้งค่าขา Buzzer ปิดเสียงไว้ก่อน
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // เริ่มต้นจอ OLED 
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for(;;); //จอไม่ติด ไม่ต้องทำงานต่อ
  }

  showMessage("Connecting to", "ESP32_Main_AP...");

  //เริ่มเกาะ Wi-Fi AP ที่ปล่อยมาจากบอร์ดฝั่งส่ง
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConnected to Cabinet successfully!");
  showMessage("Connected!", "Ready to receive...");
  delay(1000);
}

/***********************************************************************
 *                             MAIN LOOP                               *
 * ลูปหลัก: ดึงข้อมูล HTTP GET -> ถอดรหัส JSON -> แสดงผล OLED -> สั่ง Buzzer *
 ***********************************************************************/
void loop() {
  //เช็คจับเวลาตามรอบ requestInterval 
  if (millis() - lastRequestTime >= requestInterval) {
    lastRequestTime = millis();

    //ตรวจสอบว่าสัญญาณ Wi-Fi ยังเชื่อมต่ออยู่หรือไม่
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.setTimeout(1000); //timeout กันระบบค้างถ้าสัญญาณสะดุด
      
      int httpCode = http.GET();

      //รับส่งข้อมูลสำเร็จ 
      if (httpCode == 200) {
        String payload = http.getString();
        
        // แปลงข้อความ JSON String เป็นข้อมูลตัวแปร
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          //ดึงค่าออกมาจาก JSON
          float temp = doc["temp"];
          float hum = doc["hum"];
          long dist = doc["dist"];
          String status = doc["status"].as<String>();
          bool hasCamera = doc["camera"];

          /**************************************************
           *             OLED DISPLAY RENDERING             *
           **************************************************/
          display.clearDisplay();
          display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
          display.setTextSize(1);
          display.setTextColor(WHITE);

          //อุณหภูมิ
          display.setCursor(8, 8);
          display.print("Temp : "); display.print(temp, 1); display.println(" C");

          //ความชื้น
          display.setCursor(8, 20);
          display.print("Hum  : "); display.print(hum, 1); display.println(" %");

          //สถานะของกล้องในตู้
          display.setCursor(8, 32);
          display.print("Cam  : "); 
          display.println(hasCamera ? "IN CABINET" : "NO CAMERA");

          //สถานะระบบรวม (SAFE, WARNING, ALARM, DELAY)
          display.setCursor(8, 46);
          display.print("Stat : "); 
          display.println(status);
          
          display.display();

          /**************************************************
           *             BUZZER ALARM LOGIC                 *
           **************************************************/
          if (status == "ALARM") {
            //อุณหภูมิหรือความชื้นสูงกว่าที่กำหนด: ส่งเสียงเตือน
            tone(BUZZER_PIN, 1500, 200);
          } else if (status == "WARNING") {
            //เริ่มเข้าใกล้ขีดอันตราย: ส่งเสียงสั้นๆ 
            tone(BUZZER_PIN, 800, 50);
          } else {
            //สถานะปกติ หรือกำลัง Delay: ปิดเสียง
            noTone(BUZZER_PIN);
            digitalWrite(BUZZER_PIN, LOW);
          }

        } else {
          Serial.println("JSON parse failed");
        }
      } else {
        //ยิง Request ไม่สำเร็จ 
        Serial.printf("HTTP GET failed, error: %d\n", httpCode);
        showMessage("HTTP Request Failed", "Code: " + String(httpCode));
        noTone(BUZZER_PIN);
      }
      http.end();

    } else {
      //สัญญาณหลุดระหว่างทำงาน พยายามรอการเชื่อมต่อใหม่
      Serial.println("WiFi Disconnected, reconnecting...");
      showMessage("WiFi Disconnected", "Reconnecting...");
      noTone(BUZZER_PIN);
    }
  }
}