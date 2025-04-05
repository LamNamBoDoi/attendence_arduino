#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <HTTPClient.h>
#include <WiFi.h>

int lcdColumns = 16;
int lcdRows = 2;

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
#define SS_PIN 5
#define RST_PIN 4
#define PIN_SG90 27
const int buzzer = 13;
const int ledPin = 12;

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

Servo sg90;

const char *ssid = "Po";
const char *password = "29120303";
const char* device_token  = "8f19e31055c56b05";
unsigned long previousMillis1 = 0;
unsigned long previousMillis2 = 0;
int timezone = 7 * 3600;   //Replace "x" your timezone.
int time_dst = 0;
String getData, Link;
String OldCardID = "";
String URL = "http://192.168.229.201/webdiemdanh/getdata.php";

//********************connect to the WiFi******************
void connectToWiFi(){
    WiFi.mode(WIFI_OFF);        //Prevents reconnection issue (taking too long to connect)
    delay(1000);
    WiFi.mode(WIFI_STA);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("Connected");
    
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());  //IP address assigned to your ESP
    
    delay(1000);
}

void ledandbuzzer()
{
    digitalWrite(buzzer, HIGH);
    digitalWrite(ledPin, HIGH);  // Bật đèn LED
    // sg90.write(180);
    delay(500);
    digitalWrite(ledPin, LOW);   // Tắt đèn LED
    digitalWrite(buzzer, LOW);
    // sg90.write(0);  
}

void SendCardID( String Card_uid ){
  Serial.println("Sending the Card ID");
    ledandbuzzer();
  if(WiFi.isConnected()){
    HTTPClient http;    
    getData = "?card_uid=" + String(Card_uid) + "&device_token=" + String(device_token); 
    Link = URL + getData;
    http.begin(Link); 
    int httpCode = http.GET(); 
    String payload = http.getString(); 
    Serial.println("Link: " + Link);   //Print HTTP return code
    Serial.println("httpCode: " + String(httpCode));   //Print HTTP return code
    Serial.println("Card_id: " + Card_uid);     //Print Card ID
    Serial.println("payload: " + String(payload));    //Print request response payload

    if (httpCode == 200) {
      if (payload.substring(0, 5) == "login") {
        String user_name = payload.substring(5);
         Serial.println(user_name);
         lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("LOGIN");
          lcd.setCursor(0,1);
          lcd.print(user_name);
      }
      else if (payload.substring(0, 6) == "logout") {
        String user_name = payload.substring(6);
          Serial.println(user_name);
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("LOGOUT");
          lcd.setCursor(0,1);
          lcd.print(user_name);

      }
      else if (payload == "succesful") {
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("NEW CARD");
      }
      else if (payload == "available") {
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("OLD CARD");
      }else{
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("UNDEFINED");
      }
      delay(100);
      http.end();  //Close connection
    }else{
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print(httpCode);
            Serial.println("HTTP GET Failed");
            Serial.print("Error: ");
            Serial.println(http.errorToString(httpCode));
    }
  }
}

void setup()
{
    Serial.begin(9600);
    connectToWiFi();
    pinMode(buzzer, OUTPUT);
    pinMode(ledPin, OUTPUT);
    SPI.begin();
    rfid.PCD_Init();
    lcd.init();
    lcd.backlight();
    sg90.setPeriodHertz(50);         
    sg90.attach(PIN_SG90, 500, 2400);
    lcd.setCursor(0, 0); // Đặt con trỏ tại dòng 0, cột 0.
    lcd.print("ATTENDANCE"); // In thông tin.
    lcd.setCursor(9, 1);
    lcd.print("SYSTEM");
}

void loop()
{
    if(!WiFi.isConnected()){
        connectToWiFi();
    }
    if (millis() - previousMillis1 >= 1000) {
        previousMillis1 = millis();
        if (millis() - previousMillis2 >= 5000) {
            previousMillis2 = millis();
            OldCardID="";
        }
        delay(50);
        // Kiểm tra xem có thẻ mới không
        if (!rfid.PICC_IsNewCardPresent()) {
            return; // Không có thẻ mới, thoát vòng lặp
        }

        // Đọc dữ liệu từ thẻ
        if (!rfid.PICC_ReadCardSerial()) {
            return; // Không đọc được thẻ, thoát vòng lặp
        }
        String CardID ="";
        for (byte i = 0; i < rfid.uid.size; i++) {
        CardID += rfid.uid.uidByte[i];
        }
        if( CardID == OldCardID ){
            return;
        }else{
        OldCardID = CardID;
        }
        SendCardID(CardID);
  }

}