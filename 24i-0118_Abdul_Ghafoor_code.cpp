#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <MFRC522.h>

// ==========================================
// 1. SETTINGS
// ==========================================
const char* ssid = "";
const char* password = ""; 
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbwgg-URroK90rq6HM8e79DnuYu5sgyjIWwHeiWiSZQlRNR3zd2gTbn1kJqSnBaZC8GsBw/exec"; 

// Timing Settings (Milliseconds)
const int MOVE_TIME = 3000;    // 6 Seconds Move
const int STOP_TIME = 4000;    // 4 Seconds Stop/Scan
const int GAP_MOVE_TIME = 3000;// 3 Seconds (The middle bridge)
const int TURN_TIME = 400;     // Time to rotate 90 degrees

// ==========================================
// 2. PINS
// ==========================================
// Motors
const int ENa=14;
const int ENb=32;
const int IN1 = 27; const int IN2 = 26; // Left
const int IN3 = 25; const int IN4 = 23; // Right

// RFID
const int RFID_SS_PIN = 5;
const int RFID_SCK_PIN = 18;
const int RFID_MOSI_PIN = 19;
const int RFID_MISO_PIN = 22;
const int RFID_RST_PIN = 23;

// Buzzer
const int BUZZER_PIN = 4;




MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
bool routeComplete = false; // Flag to stop after finishing
void setup() {
  Serial.begin(115200);
  
  analogWrite(ENa, 200);
  analogWrite(ENb, 255);
  // Init Hardware
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
  rfid.PCD_Init();

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.println(".");
  }
  Serial.println("\nWiFi Connected!");
  
  // Signal Start
  beep(3);
  delay(2000); // 2 second delay before the mission starts
}

void loop() {
  if (routeComplete) {
    // If mission is done, just stay stopped forever
    return;
  }

  // --- LEG 1: The First 3 Stops ---
  Serial.println("--- STARTING LEG 1 ---");
  for (int i = 1; i <= 3; i++) {
    Serial.print("Segment "); Serial.println(i);
    
    // 1. Move Forward (6s)
   moveForward();
   delay(4000);
    
    // 2. Stop & Scan (4s)
    stopAndScan(STOP_TIME);
  }

  // --- THE U-TURN MANEUVER ---
  Serial.println("--- PERFORMING TURN ---");
  
  // Rotate Right 90
  rotateRight();
  delay(TURN_TIME);
  
  // Move "Across" (3s)
  moveForward();
  delay(GAP_MOVE_TIME);
  
  // Rotate Right 90 (Now facing back)
  rotateRight();
  delay(TURN_TIME);

  // --- LEG 2: The Return 3 Stops ---
  Serial.println("--- STARTING LEG 2 ---");
  for (int i = 1; i <= 3; i++) {
    Serial.print("Segment "); Serial.println(i);
    
    // 1. Move Forward (6s)
   moveForward();
   delay(4000);
    
    // 2. Stop & Scan (4s)
    stopAndScan(STOP_TIME);
  }

  // --- MISSION COMPLETE ---
  Serial.println("--- ROUTE FINISHED ---");
  stopCar();
  beep(5); // Long success beep
  routeComplete = true; // Stop the loop
}

// ==========================================
// CUSTOM FUNCTIONS
// ==========================================

// This function stops the car BUT keeps the RFID reader active
void stopAndScan(int duration) {
  stopCar();
  beep(1); // Notify "Ready to Scan"
  
  unsigned long startTime = millis();
  
  // While time is not up...
  while (millis() - startTime < duration) {
    // Try to read card
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      
      // We found a card! Process it.
      String uidString = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        uidString += String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        uidString += String(rfid.uid.uidByte[i], HEX);
      }
      uidString.toUpperCase();
      
      Serial.println("Scanned: " + uidString);
      
      // Feedback
      digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
      
      // Upload
      sendData(uidString);
      
      // Stop reading this specific card
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }
  // When loop finishes, function ends and car moves again
}

void sendData(String uid) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, GOOGLE_SCRIPT_URL);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");
    
    String payload = "{\"uid\": \"" + uid + "\"}";
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
      Serial.println("Data Uploaded.");
    }
    http.end();
  }
}

// Movement Helpers
void moveForward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void rotateRight() {
  // Left wheels forward, Right wheels backward
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void stopCar() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void beep(int times) {
  for(int i=0; i<times; i++){
    digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW); delay(100);
  }
}