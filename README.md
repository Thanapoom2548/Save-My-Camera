# 🛡️ ระบบเฝ้าระวังและแจ้งเตือนตู้กันชื้น (Save My Camera)

ระบบตรวจสอบตู้กันชื้นอัจฉริยะที่สร้างด้วย **ESP32** โปรเจกต์นี้ช่วยให้ตรวจสอบอุณหภูมิและความชื้นของตู้เก็บกล้อง ตรวจจับว่ามีกล้องวางอยู่ข้างในหรือไม่ด้วยเซ็นเซอร์อัลตราโซนิก และแจ้งเตือนดสถานะด้วย (ไฟ LED) (จอ OLED)และเสียง (Buzzer) เมื่อสภาพแวดล้อมเกินค่าความปลอดภัยที่ตั้งไว้

## ✨ ฟีเจอร์หลัก (Features)

- **🌡️ การตรวจสอบสภาพแวดล้อม:** ติดตามอุณหภูมิและความชื้นแบบเรียลไทม์ผ่านเซ็นเซอร์ DHT11
- **📷 การตรวจจับกล้อง:** ใช้เซ็นเซอร์ Ultrasonic เพื่อเช็คว่ากล้องถูกเก็บไว้ในตู้หรือไม่
- **🎛️ หน้าจออินเตอร์เฟซ (UI):** เลื่อนดูและตั้งค่าต่างๆ ผ่านปุ่ม (Potentiometer) และจอ OLED
- **💾 ระบบบันทึกค่าถาวร:** การตั้งค่าของผู้ใช้ (ระยะเซ็นเซอร์, ขีดจำกัดอุณหภูมิ/ความชื้น, เวลาหน่วง) จะถูกบันทึกลงในหน่วยความจำแฟลช NVS ของ ESP32 อย่างถาวร (`Preferences.h`) ปิดเครื่องหรือไฟดับค่าก็ไม่หาย!
- **🚨 ระบบแจ้งเตือนอัจฉริยะ:** 
  - ไฟแสดงสถานะ 3 สี (เขียว = ปลอดภัย, เหลือง = ขัดข้อง, แดง = แจ้งเตือน)
  - เสียงเตือนผ่าน Buzzer (พร้อมฟังก์ชันกดปุ่มเพื่อ Mute ปิดเสียงชั่วคราว)

## 📌 Block Diagram 

```mermaid
flowchart LR
    subgraph Inputs ["Inputs (ส่วนตรวจจับและรับค่า)"]
        direction TB
        DHT["DHT11<br>(อุณหภูมิและความชื้น)"]
        US["HC-SR04<br>(วัดระยะเช็คกล้อง)"]
        POT["Potentiometer<br>(ปรับค่าตัวเลข)"]
        BTN["Push Buttons<br>(Mode & Save/Mute)"]
    end

    subgraph Controller ["Processing Unit"]
        ESP["ESP32 Core"]
        NVS[("NVS Flash Memory<br>(เก็บค่าการตั้งค่า)")]
        ESP <--> NVS
    end

    subgraph Outputs ["Outputs (ส่วนแสดงผลและแจ้งเตือน)"]
        direction TB
        OLED["OLED Display 0.96 inch<br>(แสดงผลสถานะ / เมนู)"]
        LED["LED Indicators<br>(แดง / เหลือง / เขียว)"]
        BUZZ["Buzzer Driver<br>(ส่งเสียงเตือนฉุกเฉิน)"]
    end

    Inputs --> Controller
    Controller --> Outputs

    classDef mcu fill:#1f77b4,stroke:#fff,stroke-width:2px,color:#fff;
    classDef sensor fill:#2ca02c,stroke:#fff,stroke-width:1px,color:#fff;
    classDef output fill:#ff7f0e,stroke:#fff,stroke-width:1px,color:#fff;
    classDef storage fill:#6c757d,stroke:#fff,stroke-width:1px,color:#fff;

    class ESP mcu;
    class DHT,US,POT,BTN sensor;
    class OLED,LED,BUZZ output;
    class NVS storage;
```

## 🔀 ผังงานการทำงาน (Flowchart)

![Flowchart ของระบบ](https://i.postimg.cc/Wp7kSD4Z/Microcontroller-Flowchart-drawio.png)

## 🛠️ อุปกรณ์ที่ต้องใช้ (Hardware Requirements)

- **ไมโครคอนโทรลเลอร์:** ESP32 DOIT DevKit V1
- **หน้าจอ:** 0.96" OLED Display (SSD1306, I2C)
- **เซ็นเซอร์:** 
  - DHT11 (เซ็นเซอร์วัดอุณหภูมิและความชื้น)
  - HC-SR04 (เซ็นเซอร์วัดระยะทางอัลตราโซนิก)
- **อินพุต (Inputs):**
  - 1x โพเทนชิโอมิเตอร์ 
  - 2x ปุ่มกด (ปุ่ม Mode และปุ่ม Save)
- **เอาต์พุต (Outputs):**
  - 3x หลอด LED (แดง, เหลือง, เขียว)
  - 1x Buzzer

### 📋 รายการเอกสารทางเทคนิค (Component Datasheets)

| อุปกรณ์ (Component) | เอกสารอ้างอิง (Datasheet) |
| :--- | :--- |
| **ESP32 DOIT DevKit V1** | [ESP32 Datasheet](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp-dev-kits-en-master-esp32.pdf) |
| **0.96" OLED Display** | [SSD1306 Datasheet](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/791/SSD1306-Datasheet-for-096-OLED-_2800_1_2900_.pdf) |
| **DHT11 Sensor** | [DHT11 Datasheet](https://moodle.upm.es/en-abierto/pluginfile.php/47163/mod_page/content/10/DHT11.PDF) |
| **HC-SR04 Ultrasonic** | [HC-SR04 Datasheet](https://www.alldatasheet.com/datasheet-pdf/view/1132204/ETC2/HCSR04.html) |

## 🔌 การต่อสาย (Pin Configuration)

| อุปกรณ์ (Component) | ขา ESP32 (Pin) | หมายเหตุ (Note) |
| :--- | :--- | :--- |
| **OLED SDA** | GPIO 21 | มาตรฐาน I2C |
| **OLED SCL** | GPIO 22 | มาตรฐาน I2C |
| **DHT11 Data** | GPIO 4 | |
| **HC-SR04 Trig** | GPIO 33 | |
| **HC-SR04 Echo** | GPIO 34 | |
| **Potentiometer**| GPIO 35 | พอร์ต Analog Input |
| **ปุ่ม Mode** | GPIO 18 | เปิด `INPUT_PULLUP` (ต่อขาเข้า GND) |
| **ปุ่ม Save** | GPIO 19 | เปิด `INPUT_PULLUP` (ต่อขาเข้า GND) |
| **ไฟ LED แดง** | GPIO 25 | ต่อผ่านตัวต้านทาน 330Ω |
| **ไฟ LED เหลือง** | GPIO 26 | ต่อผ่านตัวต้านทาน 330Ω |
| **ไฟ LED เขียว** | GPIO 27 | ต่อผ่านตัวต้านทาน 330 |
| **Buzzer** | GPIO 14 | |

### 📐 แผนภาพการต่อวงจร (Circuit Diagram)

![แผนภาพการต่อวงจร Circuit Diagram](https://i.postimg.cc/3xhpZgXn/Add-a-subheading.png)

## 💻 ซอฟต์แวร์และไลบรารี (Software & Libraries)

โปรเจกต์นี้พัฒนาด้วย (**PlatformIO**) อย่าลืมติดตั้งไลบรารีต่อไปนี้ก่อนทำการคอมไพล์โค้ด:

- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `DHT sensor library` (โดย Adafruit)

## 📖 คู่มือการใช้งาน (How to Use)

### 🎛️ การควบคุม (Controls)
- **ปุ่มซ้าย (Mode):** ใช้สำหรับกดเข้าสู่โหมดการตั้งค่า เลื่อนเปลี่ยนหน้าเมนู หรือกดเพื่อ "ข้าม" ไปหน้าถัดไปในกรณีที่ไม่อยากเปลี่ยนค่าในหน้านั้น
- **ปุ่มขวา (Save / Mute):** ในหน้าต่างตั้งค่าจะใช้สำหรับ **บันทึก (Save)** ค่าใหม่ และในกรณีที่ตู้เกิดความผิดปกติจนเสียงบัซเซอร์ร้องเตือน คุณสามารถกดปุ่มนี้เพื่อ **ปิดเสียงเตือน (Mute)** ได้
- **ปุ่มหมุน (Potentiometer):** ใช้หมุนเพื่อปรับเพิ่มหรือลดค่าตัวเลขเมื่ออยู่ในหน้าต่างการตั้งค่า

### ⚙️ ลำดับหน้าจอ (Menu & Settings)
ระบบจะเริ่มทำงานที่หน้าหลัก หากต้องการตั้งค่า ให้กด **ปุ่มซ้าย (Mode)** เพื่อวนเข้าสู่หน้าจอต่างๆ ตามลำดับ ดังนี้:

- **หน้าหลัก (Normal Mode):** หน้าจอแสดงสถานะการทำงานปกติ จะบอกอุณหภูมิและความชื้นแบบเรียลไทม์ หากมีกล้องหรือเลนส์วางอยู่ในตู้ ระบบจะเริ่มจับเวลาและแสดงระยะเวลาที่เก็บไว้ แต่ถ้าตู้ว่างเปล่า หน้าจอจะขึ้นสถานะว่า "No Camera"
- **หน้าที่ 1 - ตั้งระยะตรวจจับ (Set Distance):** หมุนปรับระยะความลึกของตู้ เพื่อให้เซ็นเซอร์ Ultrasonic ทราบว่าระยะไหนคือผนังตู้ และระยะไหนคือระยะที่มีกล้องวางอยู่
- **หน้าที่ 2 - ตั้งอุณหภูมิสูงสุด (Set Temp Limit):** หมุนปรับค่าอุณหภูมิสูงสุด (องศาเซลเซียส) หากความร้อนเกินค่านี้ระบบจะแจ้งเตือน
- **หน้าที่ 3 - ตั้งความชื้นต่ำสุด (Set Hum Min):** หมุนปรับค่าความชื้นขั้นต่ำ (%) เพื่อป้องกันตู้แห้งเกินไป ซึ่งอาจทำให้ชิ้นส่วนยางของเลนส์หรือตัวกล้องเสื่อมสภาพ
- **หน้าที่ 4 - ตั้งความชื้นสูงสุด (Set Hum Max):** หมุนปรับค่าความชื้นสูงสุด (%) เพื่อป้องกันความชื้นสะสมที่อาจก่อให้เกิดเชื้อรา
- **หน้าที่ 5 - ตั้งเวลาหน่วง (Set Delay):** หมุนปรับเวลา (นาที) เพื่อชะลอการแจ้งเตือนหลังจากเพิ่งนำกล้องใส่ตู้ เพื่อให้ระบบดูดความชื้นได้ทำงานและปรับสภาพอากาศให้เข้าที่ก่อนเริ่มเฝ้าระวัง

*💡 **ข้อควรระวัง:** เมื่อหมุนปรับตัวเลขจนได้ค่าที่ต้องการแล้ว **ต้องกดปุ่มขวา (Save) 1 ครั้ง** เพื่อบันทึกค่าลงหน่วยความจำด้วย หากกดปุ่มซ้ายผ่านไปเลย ระบบจะไม่จำค่าใหม่และกลับไปใช้ค่าเดิม*

## 📑 เอกสารและคู่มือการใช้งาน (Documentation & Manual)

- 📖 [คลิกเพื่อดูคู่มือการใช้งาน (Save My Camera - Microsoft Sway)](https://sway.cloud.microsoft/RhiMDbVuny8xtQOC?ref=Link)
