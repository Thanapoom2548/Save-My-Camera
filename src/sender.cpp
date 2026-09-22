#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>

/***********************************************************************
 *                         HARDWARE PIN MAPPING                        *
 * ส่วนกำหนดขา (Pins) สำหรับเชื่อมต่ออุปกรณ์อิเล็กทรอนิกส์ต่างๆ กับ ESP32         *
 ***********************************************************************/
#define BTN_MODE_PIN 18
#define BTN_SAVE_PIN 19
#define POT_PIN 35
#define TRIG_PIN 33
#define ECHO_PIN 34
#define DHT_PIN 4
#define LED_RED 25
#define LED_YELLOW 26
#define LED_GREEN 27
#define BUZZER_PIN 14

/***********************************************************************
 *                   OBJECTS & NETWORK CONFIGURATION                   *
 * ส่วนประกาศอ็อบเจกต์ของจอ OLED, เซนเซอร์ DHT, ระบบเก็บข้อมูล และตั้งค่า Wi-Fi  *
 ***********************************************************************/
//--- ตั้งค่าจอ OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//--- ตั้งค่าเซนเซอร์ DHT และ Preferences (หน่วยความจำ) ---
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);
Preferences preferences;

// --- ตั้งค่า WiFi AP & WebServer ---
const char *ssid = "ESP32_Main_AP";
const char *password = "87654321"; 
WebServer server(80);

/***********************************************************************
 *                     GLOBAL VARIABLES & STATES                       *
 * ตัวแปรที่เก็บสถานะของระบบ, ค่าที่ตั้งไว้, ตัวนับเวลา และตัวแปรส่งค่าผ่าน API      *
 ***********************************************************************/
//--- สถานะโหมดการทำงานของหน้าจอ (State Machine) ---
enum State { NORMAL, SET_DIST, SET_TEMP, SET_HUM_MIN, SET_HUM_MAX, SET_DELAY };
State currentState = NORMAL;

//--- ตัวแปร Global สำหรับแชร์ค่าไปยังบอร์ดตัวรับ (API) ---
float global_t = 0.0;
float global_h = 0.0;
String systemStatus = "NO CAMERA"; // สถานะ: SAFE, WARNING, ALARM, DELAY, NO CAMERA, SENSOR ERROR

//--- ตัวแปรสำหรับเก็บค่า Setting จาก Preferences ---
int savedDist, savedTemp, savedHumMin, savedHumMax, savedDelayMins;

//--- ตัวแปรจัดการเวลาและเซนเซอร์ ---
unsigned long cameraDetectedTime = 0;
unsigned long cameraLostTime = 0;
bool isCameraInCabinet = false;
bool isMuted = false;

//--- ตัวแปรสำหรับการกดปุ่ม (Debounce) ---
unsigned long lastDebounceTime_Mode = 0;
unsigned long lastDebounceTime_Save = 0;
bool lastModeState = HIGH;
bool lastSaveState = HIGH;
const int debounceDelay = 50;

//--- ตัวแปรสถานะแจ้งเตือนและการอ่านค่า ---
unsigned long lastAlarmBlink = 0;
bool alarmBlinkState = false;
unsigned long lastPingTime = 0;
long currentDist = 999;

/***********************************************************************
 *                         HELPER FUNCTIONS                            *
 * ฟังก์ชันย่อยสำหรับจัดการ API (ส่ง JSON ออกไป) และอ่านค่าระยะทางจากเซนเซอร์    *
 ***********************************************************************/

//API สำหรับส่งข้อมูลให้บอร์ดรับ เมื่อมีการเรียก /data
void handleGetData() {
  String json = "{";
  json += "\"temp\":" + String(global_t, 1) + ",";
  json += "\"hum\":" + String(global_h, 1) + ",";
  json += "\"dist\":" + String(currentDist) + ",";
  json += "\"status\":\"" + systemStatus + "\",";
  json += "\"camera\":" + String(isCameraInCabinet ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

//ฟังก์ชันอ่านค่าระยะทางจาก Ultrasonic Sensor
long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
  if (duration == 0) return 999; 
  return duration * 0.034 / 2;
}

/***********************************************************************
 *                           SETUP FUNCTION                            *
 * ฟังก์ชันตั้งค่าเริ่มต้น ทำงานครั้งแรกตอนเปิดเครื่อง (Pins, จอ, เซิร์ฟเวอร์, อ่านค่า)   *
 ***********************************************************************/
void setup() {
  Serial.begin(115200);

  //ตั้งค่า Pin Modes
  pinMode(BTN_MODE_PIN, INPUT_PULLUP);
  pinMode(BTN_SAVE_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  //เริ่มการทำงานเซนเซอร์และจอ
  dht.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); // ถ้าจอไม่ติดให้หยุดการทำงาน
  }

  //--- เริ่มต้นปล่อย Wi-Fi AP และรัน WebServer ---
  WiFi.softAP(ssid, password);
  Serial.println("\n--- WiFi AP Started ---");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("AP IP Address: "); Serial.println(WiFi.softAPIP());

  server.on("/data", HTTP_GET, handleGetData);
  server.begin();
  Serial.println("HTTP Server Ready at /data");

  //ดึงข้อมูลที่เคยเซฟไว้ขึ้นมาจากหน่วยความจำ ESP32
  preferences.begin("cabinet", false);
  savedDist = preferences.getInt("dist", 50); 
  savedTemp = preferences.getInt("temp", 38);     
  savedHumMin = preferences.getInt("humMin", 20); 
  savedHumMax = preferences.getInt("humMax", 60); 
  savedDelayMins = preferences.getInt("delay", 1); 
}

/***********************************************************************
 *                           MAIN LOOP & INPUTS                        *
 * ลูปหลักของโปรแกรม: รับค่าเว็บเซิร์ฟเวอร์, อ่านปุ่มกด (Debounce), อ่านเซนเซอร์    *
 ***********************************************************************/
void loop() {
  //รับ Request จากบอร์ดภายนอกตลอดเวลา
  server.handleClient();

  //--- ปุ่ม Mode (เปลี่ยนหน้าจอ) ---
  bool readingMode = digitalRead(BTN_MODE_PIN);
  if (readingMode != lastModeState) lastDebounceTime_Mode = millis();
  if ((millis() - lastDebounceTime_Mode) > debounceDelay) {
    if (readingMode == LOW) {
      currentState = (State)((currentState + 1) % 6); // วนลูป 6 สถานะ
      digitalWrite(LED_RED, LOW); digitalWrite(LED_YELLOW, LOW); digitalWrite(LED_GREEN, LOW);
      noTone(BUZZER_PIN); 
      digitalWrite(BUZZER_PIN, LOW);
      delay(200);
    }
  }
  lastModeState = readingMode;

  //--- ปุ่ม Save (บันทึกค่า / Mute เสียง) ---
  bool readingSave = digitalRead(BTN_SAVE_PIN);
  if (readingSave != lastSaveState) lastDebounceTime_Save = millis();
  bool savePressed = false;
  if ((millis() - lastDebounceTime_Save) > debounceDelay) {
    if (readingSave == LOW) {
      savePressed = true;
      delay(200);
    }
  }
  lastSaveState = readingSave;

  //--- อ่านค่าเซนเซอร์ต่างๆ ---
  //อ่าน Ultrasonic (ทุก 100ms)
  if (millis() - lastPingTime > 100) {
    currentDist = getDistance();
    lastPingTime = millis();
  }
  
  //อัปเดตลงตัวแปร Global
  global_t = dht.readTemperature();
  global_h = dht.readHumidity();
  int potValue = analogRead(POT_PIN);

  //เตรียมจอ
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

/***********************************************************************
 *                              หน้าจอต่างๆ                               *
 * เลือกว่าจะให้หน้าจอแสดงผลอะไร และทำงานระบบไหนตาม State ที่อยู่ปัจจุบัน         *
 ***********************************************************************/
  switch (currentState) {
    
    /**************************************************
     *         STATE: NORMAL (โหมดการทำงานปกติ)         *
     * แสดงสถานะกล้อง แจ้งเตือนอุณหภูมิ/ความชื้น และนับเวลาดีเลย์ *
     **************************************************/
    case NORMAL: {
      // ตรวจสอบเซนเซอร์พัง
      if (isnan(global_t) || isnan(global_h)) {
        systemStatus = "SENSOR ERROR";
        digitalWrite(LED_YELLOW, HIGH);
        display.setTextSize(1);
        display.setCursor(4, 32);
        display.println("--- SENSOR ERROR ---");
        display.display();
        return; 
      }

      int triggerDistance = savedDist - 5; 

      //ตรวจจับกล้องเข้าตู้
      if (currentDist <= triggerDistance && currentDist > 0) { 
        cameraLostTime = millis();
        if (!isCameraInCabinet) {
          isCameraInCabinet = true;
          cameraDetectedTime = millis();
          isMuted = false;
        }
      } else {
        //กล้องออกเกิน 2 วินาที รีเซ็ตระบบ
        if (isCameraInCabinet && (millis() - cameraLostTime > 2000)) {
          isCameraInCabinet = false;
          digitalWrite(LED_GREEN, LOW); digitalWrite(LED_RED, LOW); digitalWrite(LED_YELLOW, LOW);
          noTone(BUZZER_PIN); digitalWrite(BUZZER_PIN, LOW); 
        }
      }
      
      //แสดงอุณหภูมิ ความชื้น
      display.setCursor(0, 0);
      display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
      display.setTextSize(1); display.setCursor(5, 10);
      display.print("Temperature : "); display.print(global_t, 1); display.println("C");
      display.setTextSize(1); display.setCursor(5, 20);
      display.print("Humidity    : "); display.print(global_h, 1); display.println("%");
      
      //logic เวลา มีกล้องอยู่ในตู้
      if (isCameraInCabinet) {
        unsigned long elapsedMillis = millis() - cameraDetectedTime;
        unsigned long delayTargetMillis = savedDelayMins * 60000UL;
        unsigned long totalSecs = elapsedMillis / 1000;

        if (elapsedMillis < delayTargetMillis) {
           //ช่วงนับเวลาดีเลย์
           systemStatus = "DELAY";
           digitalWrite(LED_GREEN, HIGH);
           noTone(BUZZER_PIN); digitalWrite(BUZZER_PIN, LOW); 
           display.setCursor(5, 30);
           display.print("Delay       : ");
           display.print((delayTargetMillis - elapsedMillis) / 1000); display.println(" s");
        } else {
           //พ้นระยะดีเลย์ เริ่มเช็คเงื่อนไขแจ้งเตือน
           bool isAlarm = (global_t >= savedTemp || global_h >= savedHumMax || global_h <= savedHumMin);
           bool isWarning = false;
           
           if (!isAlarm) {
             if (global_t >= savedTemp - 2) isWarning = true;
             if (global_h >= savedHumMax - 5 || global_h <= savedHumMin + 5) isWarning = true;
           }
           
           if (isAlarm) {
             systemStatus = "ALARM";
             if (savePressed) isMuted = true; //กด save เพื่อ Mute เสียงเตือน

             // ทำไฟ/เสียง กระพริบ
             if (millis() - lastAlarmBlink >= 500) {
               lastAlarmBlink = millis();
               alarmBlinkState = !alarmBlinkState;
             }

             if (alarmBlinkState) {
               digitalWrite(LED_GREEN, LOW); digitalWrite(LED_YELLOW, LOW); digitalWrite(LED_RED, HIGH);
               if (!isMuted) tone(BUZZER_PIN, 1000); 
               else { noTone(BUZZER_PIN); digitalWrite(BUZZER_PIN, LOW); }
               
               display.setCursor(12, 30);
               display.println("! ALARM ACTIVE !");
             } else {
               digitalWrite(LED_GREEN, LOW); digitalWrite(LED_RED, LOW); digitalWrite(LED_YELLOW, LOW);
               noTone(BUZZER_PIN); digitalWrite(BUZZER_PIN, LOW); 
               
               display.setCursor(12, 30); 
               display.println("                "); 
             }
             
           } else if (isWarning) {
             systemStatus = "WARNING";
             digitalWrite(LED_GREEN, LOW); digitalWrite(LED_RED, LOW); digitalWrite(LED_YELLOW, HIGH);
             noTone(BUZZER_PIN); digitalWrite(BUZZER_PIN, LOW);
             display.setCursor(5, 30);
             display.println("Status    : WARNING");
             
           } else {
             systemStatus = "SAFE";
             digitalWrite(LED_GREEN, HIGH); digitalWrite(LED_RED, LOW); digitalWrite(LED_YELLOW, LOW);
             noTone(BUZZER_PIN); digitalWrite(BUZZER_PIN, LOW);
             display.setCursor(5, 30);
             display.println("Status      : SAFE");
           }          
        }
        
        //คำนวณเวลาเป็น วัน ชั่วโมง นาที วินาที
        unsigned long d_time = totalSecs / 86400;
        unsigned long h_time = (totalSecs % 86400) / 3600; 
        unsigned long m_time = (totalSecs % 3600) / 60;
        unsigned long s_time = totalSecs % 60;
        
        display.setCursor(5, 40);
        display.print("Time  : "); 
        display.printf("%lud %02lu:%02lu:%02lu\n", d_time, h_time, m_time, s_time);
        
      } else {
        //ไม่พบกล้องในตู้
        systemStatus = "NO CAMERA";
        display.setCursor(5, 30);
        display.println("Status  : NO CAMERA");
      }
      break;
    }

    /**************************************************
     *      STATE: SETTINGS (หน้าจอหมุนตั้งค่า)           *
     **************************************************/
    
    // ตั้งค่าระยะทาง
    case SET_DIST: {
      int mappedDist = map(potValue, 4095, 0, 10, 100);
      display.setCursor(7,0);
      display.println("-SET DISTANCE (CM)-");
      display.setCursor(8,15);
      display.setTextSize(2);
      display.print("New  : "); display.print(mappedDist); 
      display.setTextSize(2); 
      display.setCursor(8,35);
      display.print("Real : "); display.println(currentDist);
      
      //ไฟสีแดงติดเมื่อระยะที่วัดได้ใกล้เคียงกับค่าที่ตั้งไว้
      if (currentDist > 0 && currentDist != 999) {
        if (currentDist <= mappedDist && currentDist >= mappedDist - 3) {
          digitalWrite(LED_RED, HIGH);
        } else {
          digitalWrite(LED_RED, LOW);
        }
      } else {
        digitalWrite(LED_RED, LOW);
      }

      if (savePressed) {
        savedDist = mappedDist;
        preferences.putInt("dist", savedDist);
        display.println("SAVED!");
      }
      break;
    }
    
    //ตั้งค่าความร้อน
    case SET_TEMP: {
      int mappedTemp = map(potValue, 4095, 0, 20, 60);
      display.setCursor(10,0);
      display.println("SET TEMP LIMIT (C)");
      display.setCursor(8,15);
      display.setTextSize(2);
      display.print("Saved: "); display.println(savedTemp);
      display.setTextSize(2); 
      display.setCursor(8,35);
      display.print("New  : "); display.println(mappedTemp);
      if (savePressed) {
        savedTemp = mappedTemp;
        preferences.putInt("temp", savedTemp);
        display.println("SAVED!");
      }
      break;
    }

    //ตั้งค่าความชื้นต่ำสุด
    case SET_HUM_MIN: {
      int mappedHumMin = map(potValue, 4095, 0, 20, 50);
      display.setCursor(7,0);
      display.println("--SET HUM MIN (%)--");
      display.setCursor(8,15);
      display.setTextSize(2);
      display.print("Saved: "); display.println(savedHumMin);
      display.setTextSize(2); 
      display.setCursor(8,35);
      display.print("New  : "); display.println(mappedHumMin);
      if (savePressed) {
        savedHumMin = mappedHumMin;
        preferences.putInt("humMin", savedHumMin);
        display.println("SAVED!");
      }
      break;
    }

    //ตั้งค่าความชื้นสูงสุด
    case SET_HUM_MAX: {
      int mappedHumMax = map(potValue, 4095, 0, 40, 90);
      display.setCursor(7,0);
      display.println("--SET HUM MAX (%)--");
      display.setCursor(8,15);
      display.setTextSize(2);
      display.print("Saved: "); display.println(savedHumMax);
      display.setTextSize(2); 
      display.setCursor(8,35);
      display.print("New  : "); display.println(mappedHumMax);
      if (savePressed) {
        savedHumMax = mappedHumMax;
        preferences.putInt("humMax", savedHumMax);
        display.println("SAVED!");
      }
      break;
    }

    //ตั้งค่าเวลาหน่วงก่อนเริ่มแจ้งเตือน
    case SET_DELAY: {
      int mappedDelay = map(potValue, 4095, 0, 0, 60);
      display.setCursor(6,0);
      display.println("--SET DELAY (MINS)--");
      display.setCursor(8,15);
      display.setTextSize(2);
      display.print("Saved: "); display.println(savedDelayMins);
      display.setTextSize(2); 
      display.setCursor(8,35);
      display.print("New  : "); display.println(mappedDelay);
      if (savePressed) {
        savedDelayMins = mappedDelay;
        preferences.putInt("delay", savedDelayMins);
        display.println("SAVED!");
      }
      break;
    }
  } 

  // อัปเดตหน้าจอ OLED 
  display.display();
}