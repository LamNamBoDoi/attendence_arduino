#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <WiFi.h>

// ==== Cấu hình LCD I2C ====
int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

// ==== Cấu hình RFID ====
#define SS_PIN 5
#define RST_PIN 4
MFRC522 rfid(SS_PIN, RST_PIN);

// ==== Cấu hình LED, Buzzer ====
// #define PIN_SG90 14
const int buzzer = 13;
const int ledPin = 12;
// Servo sg90;

// ==== Cấu hình WiFi và Server ====
const char *ssid = "Po";
const char *password = "29120303";
const char *device_token = "8f19e31055c56b05";
String URL = "http://192.168.33.201/webdiemdanh/getdata.php";

// ==== FreeRTOS Queue ====
QueueHandle_t cardQueue;
QueueHandle_t lcdQueue;

// ==== Cấu trúc tin nhắn cho LCD ====
typedef struct {
  String line1;
  String line2;
} LCDMessage;

// ==== Hàm kết nối WiFi ====
void connectToWiFi() {
  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
}

// ==== Task: đọc RFID ====
// Cho phép quét lại cùng thẻ sau 3 giây
void RFIDTask(void *pvParameters) {
  String oldCard = "";
  unsigned long lastCardTime = 0;

  for (;;) {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
      vTaskDelay(200 / portTICK_PERIOD_MS);
      continue;
    }

    String cardID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      cardID += String(rfid.uid.uidByte[i]);
    }

    unsigned long now = millis();
    Serial.println(now);
    if (cardID != oldCard || (now - lastCardTime > 3000)) {
      oldCard = cardID;
      lastCardTime = now;
      // portMAX_DELAY: chờ không giới hạn
      xQueueSend(cardQueue, &cardID, portMAX_DELAY);
    }
    // hủy bỏ phiên làm việc
    rfid.PICC_HaltA();
  }
}

// ==== Task: gửi ID đến server và phản hồi LCD, hiệu ứng ==== 
void SenderTask(void *pvParameters) {
  String cardID;
  LCDMessage msg;

  for (;;) {
    // xQueueReceive: chờ cho đến khi có dữ liệu trong hàng đợi
    if (xQueueReceive(cardQueue, &cardID, portMAX_DELAY)) {
      if (!WiFi.isConnected()) connectToWiFi();
      // tạo link liên kết đến server
      HTTPClient http;
      String getData = "?card_uid=" + cardID + "&device_token=" + device_token;
      String link = URL + getData;
      Serial.println(link);
      http.begin(link);
      int httpCode = http.GET();
      String payload = http.getString();
      http.end();

      Serial.println(httpCode);
      Serial.println(payload);

      // Gửi nội dung phù hợp cho LCD
      if (httpCode == 200) {
        if (payload.startsWith("login")) {
          msg.line1 = "Welcome!";
          msg.line2 = payload.substring(5);  // Tên người dùng
          // sg90.write(90);  // mở cửa
        } else if (payload.startsWith("logout")) {
          msg.line1 = "Goodbye!";
          msg.line2 = payload.substring(6);
          // sg90.write(0);   // đóng lại
        } else if (payload == "succesful") {
          msg.line1 = "New card added!";
          msg.line2 = "";
        } else if (payload == "available") {
          msg.line1 = "Card exists!";
          msg.line2 = "";
        } else {
          msg.line1 = "Unknown card!";
          msg.line2 = "";
        }
      } else {
        msg.line1 = "HTTP ERROR";
        msg.line2 = String(httpCode);
      }

      // Gửi nội dung ra LCD
      xQueueSend(lcdQueue, &msg, portMAX_DELAY);

      // Buzzer + LED
      digitalWrite(buzzer, HIGH);
      digitalWrite(ledPin, HIGH);
      delay(200);
      digitalWrite(buzzer, LOW);
      digitalWrite(ledPin, LOW);
    }
  }
}

// ==== Task: Hiển thị LCD ====
void LCDTask(void *pvParameters) {
  LCDMessage msg;
  for (;;) {
    if (xQueueReceive(lcdQueue, &msg, portMAX_DELAY)) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(msg.line1);
      lcd.setCursor(0, 1);
      lcd.print(msg.line2);

      // Hiển thị trong 3 giây
      vTaskDelay(3000 / portTICK_PERIOD_MS);

      // Quay về màn hình chính
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ATTENDANCE");
      lcd.setCursor(0, 1);
      lcd.print("SYSTEM");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ==== setup() ====
void setup() {
  Serial.begin(9600);
  connectToWiFi();

  // I/O
  pinMode(buzzer, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // RFID + LCD
  SPI.begin();
  rfid.PCD_Init();
  lcd.init();
  lcd.backlight();

  // sg90.setPeriodHertz(50);
  // sg90.attach(PIN_SG90, 500, 2400);
  // sg90.write(0); 

  lcd.setCursor(0, 0);
  lcd.print("ATTENDANCE");
  lcd.setCursor(0, 1);
  lcd.print("SYSTEM");

  // Tạo hàng đợi
  cardQueue = xQueueCreate(5, sizeof(String));
  lcdQueue = xQueueCreate(5, sizeof(LCDMessage));

  // Tạo các task
  xTaskCreatePinnedToCore(RFIDTask, "RFID Reader", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(SenderTask, "Sender", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(LCDTask, "LCD Display", 4096, NULL, 1, NULL, 1);
}

void loop() {
}
