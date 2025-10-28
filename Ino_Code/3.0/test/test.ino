
#include <WiFi.h> 
#include <Firebase_ESP_Client.h>
#include "time.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DFRobotDFPlayerMini.h"
#include "HardwareSerial.h"

#define QUEUE_SIZE 10 
int audioQueue[QUEUE_SIZE];
int front = 0, rear = 0;
bool isPlaying = false;
bool dfPlayerAvailable = false; // Track if DFPlayer is working

void enqueue(int fileNumber) {
    if (!dfPlayerAvailable) return; // Skip if no audio module
    
    if ((rear + 1) % QUEUE_SIZE == front) {
        Serial.println("Queue is full!");
        return;
    }
    audioQueue[rear] = fileNumber;
    rear = (rear + 1) % QUEUE_SIZE;
}

int dequeue() {
    if (front == rear) return -1;
    int fileNumber = audioQueue[front];
    front = (front + 1) % QUEUE_SIZE;
    return fileNumber;
}

// -------------------- CONFIGURATION --------------------
#define WIFI_SSID         "AB"
#define WIFI_PASSWORD     "akashbera"
#define FIREBASE_PROJECT_ID "waterflow-dashboard"
#define API_KEY           "AIzaSyCaQ4-CXMn-2HB0RdMKZvDmVm6SGtXPnk8"
#define USER_EMAIL        "ab@gmail.com"
#define USER_PASSWORD     "123456"
#define NTP_SERVER        "pool.ntp.org"
#define GMT_OFFSET        19800
#define DAYLIGHT_OFFSET   0

#define FLOW_SENSOR_PIN   5
#define SERVO_PIN 18
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 21
#define OLED_SCL 22

Servo myServo;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
HardwareSerial mySerial(1);
DFRobotDFPlayerMini dfPlayer;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// -------------------- GLOBAL OBJECTS --------------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
Preferences preferences;

// -------------------- GLOBAL VARIABLES --------------------
volatile int pulseCount = 0;
unsigned long lastFlowUpdate = 0;
unsigned long lastFirebaseUpdate = 0;
unsigned long lastLimitFetch = 0;
const unsigned long LIMIT_FETCH_INTERVAL = 5000;
static unsigned long lastServoCheck = 0;

float totalUsage = 0.0;
float flowRate = 0.0;
float limit = 0.0;
bool servoState = false;
bool isOnline = false;
bool wasOffline = false;
bool firstTime = true;
int currentServoAngle = -1;
bool effectiveServoState;

// Define mapping between names and file numbers
struct AudioFile {
    int fileNumber;
    const char* fileName;
};
AudioFile audioFiles[] = {
    {1, "Ninty_Limit_English.mp3"},
    {2, "Ninty_Limit_Hindi.mp3"},
    {3, "Water_supply_started.mp3"},
    {4, "Water_supply_started_hindi.mp3"},
    {5, "Water_supply_stopped.mp3"},
    {6, "Water_supply_stopped_hindi.mp3"},
    {7, "Wifi_Connected.mp3"},
    {8, "Wifi_Connected_hindi.mp3"},
    {9, "Wifi_Disconnected.mp3"},
    {10, "Wifi_Disconnected_hindi.mp3"}
};

int getFileNumber(const char* fileName) {
    for (int i = 0; i < sizeof(audioFiles) / sizeof(audioFiles[0]); i++) {
        if (strcmp(audioFiles[i].fileName, fileName) == 0) {
            return audioFiles[i].fileNumber;
        }
    }
    return -1;
}

void playAudioDual(const char* englishFile, const char* hindiFile) {
    if (!dfPlayerAvailable) return;

    int engFileNum = getFileNumber(englishFile);
    int hinFileNum = getFileNumber(hindiFile);

    if (engFileNum != -1) enqueue(engFileNum);
    if (hinFileNum != -1) enqueue(hinFileNum);

    Serial.print("🎧 Queued bilingual message: ");
    Serial.print(englishFile);
    Serial.print(" + ");
    Serial.println(hindiFile);
}

void processAudioQueue() {
    if (!dfPlayerAvailable) return; // Skip if no audio module
    
    static unsigned long lastPlayTime = 0;

    if (!isPlaying && millis() - lastPlayTime > 2000) {
        int fileNumber = dequeue();
        
        if (fileNumber != -1) {
            dfPlayer.play(fileNumber);
            Serial.print("Playing: ");
            Serial.println(fileNumber);
            lastPlayTime = millis();
            isPlaying = true;
        }
    }

    if (millis() - lastPlayTime > 2000) {
        isPlaying = false;
    }
}

// -------------------- INTERRUPT SERVICE ROUTINE --------------------
void IRAM_ATTR pulseCounter() {
  portENTER_CRITICAL_ISR(&mux);
  pulseCount++;
  portEXIT_CRITICAL_ISR(&mux);
}

// -------------------- WIFI CONNECTION --------------------
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 10000;
  
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    delay(500);
    Serial.print(".");
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    isOnline = true;
    Serial.println("\n🛜 WiFi connected: " + WiFi.localIP().toString());
  } else {
    isOnline = false;
    Serial.println("\n⚠️ WiFi connection failed. Proceeding in offline mode.");
  }
}

// -------------------- FIREBASE INITIALIZATION --------------------
void initializeFirebase() {
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.cert.file = true;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  Serial.println("🎗️ Firebase initialized");
}

// -------------------- NTP TIME SYNC --------------------
void syncNTPTime() {
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.println("📅 ⌚ NTP time synced");
  } else {
    Serial.println("❌ Failed to sync NTP");
  }
}

// -------------------- LOCAL STORAGE INITIALIZATION --------------------
void initializeLocalStorage() {
  preferences.begin("storage", false);
  totalUsage = preferences.getFloat("totalUsage", 0.0);
  limit = preferences.getFloat("limit", 0.0);
  servoState = preferences.getBool("servoState", false);
  Serial.printf("📥 Loaded data from local storage: %.2f L , limit : %.2f", totalUsage , limit );
  Serial.printf(" servoState : %s\n", servoState ? "true" : "false");
}

// -------------------- GET FIRESTORE DOCUMENT PATH --------------------
String getDocumentPath() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Failed to get local time");
    return "";
  }
  char yearMonth[8];
  snprintf(yearMonth, sizeof(yearMonth), "%04d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1);
  return "users/" + String(USER_EMAIL) + "/monthlyUsages/" + String(yearMonth);
}

String getCurrentDate() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char dateStr[11];
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  return String(dateStr);
}

void limitFetch() {
  String docPath = getDocumentPath();
  
  if (!Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "(default)", docPath.c_str())) {
    Serial.print("❌ limit fetch failed: ");
    Serial.println(fbdo.errorReason());
    return;
  }
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, fbdo.payload());
  if (err) {
    Serial.print("❌ JSON parse error in limit fetch: ");
    Serial.println(err.c_str());
    return;
  }
  
  JsonObject fields = doc["fields"];
  if (fields.containsKey("limit")) {
    float fetchedLimit = 0;
    if (fields["limit"].containsKey("doubleValue")) {
      fetchedLimit = fields["limit"]["doubleValue"].as<float>();
    } else if (fields["limit"].containsKey("integerValue")) {
      fetchedLimit = fields["limit"]["integerValue"].as<float>();
    }
    limit = fetchedLimit;
    preferences.putFloat("limit", limit);
    Serial.print("✅ Fetched limit: ");
    Serial.println(limit);
  } else {
    Serial.println("❌ Limit field not found in Firestore");
  }
}

void fetchServoState() {
  if (!Firebase.ready()) return;

  String documentPath = "users/" + String(USER_EMAIL);

  if (!Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "(default)", documentPath.c_str())) {
    Serial.print("❌ Firestore fetch failed: ");
    Serial.println(fbdo.errorReason());
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, fbdo.payload());
  if (err) {
    Serial.print("❌ JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  JsonObject fields = doc["fields"];

  if (fields.containsKey("servoState")) {
    bool fetchedServoState = fields["servoState"]["booleanValue"].as<bool>();
    preferences.putBool("servoState", fetchedServoState);
    Serial.print("🚺 Fetched servoState: ");
    Serial.println(fetchedServoState ? "true" : "false");
  } else {
    Serial.println("❌ servoState not found in Firestore");
  }
}

void updateServoState() {
  bool storedServoState = preferences.getBool("servoState", false);
  float storedLimit = preferences.getFloat("limit", 0.0);

  if (totalUsage >= storedLimit) {
    effectiveServoState = false;
    Serial.println("⚠️ Limit exceeded: Forcing servo OFF");
  } else {
    effectiveServoState = storedServoState;
    Serial.printf("✅ Within limit: Servo state = %s\n", 
                  effectiveServoState ? "ON" : "OFF");
  }

  int targetAngle = effectiveServoState ? 0 : 90;

  if (targetAngle != currentServoAngle) {
    myServo.write(targetAngle);
    currentServoAngle = targetAngle;
    Serial.printf("🔄 Servo moved to %d°\n", targetAngle);
    
    if(targetAngle == 0 ){
      playAudioDual("Water_supply_started.mp3", "Water_supply_started_hindi.mp3");
    } else {
      playAudioDual("Water_supply_stopped.mp3", "Water_supply_stopped_hindi.mp3");
    }
    
    if (isOnline && effectiveServoState != storedServoState) {
      FirebaseJson json;
      json.set("fields/servoState/booleanValue", effectiveServoState);
      String path = "users/" + String(USER_EMAIL);
      if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", 
                                          path.c_str(), json.raw(), "servoState")) {
        preferences.putBool("servoState", effectiveServoState);
        Serial.println("📤 Synced servo state to Firebase");
      }
    }
  }
}

bool updateFirebase(float totalUsage) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi not connected, skipping Firebase update.");
    return false;
  }

  String docPath = getDocumentPath();
  String today = getCurrentDate();

  FirebaseJson json;
  json.set("fields/" + today + "/doubleValue", totalUsage);

  Serial.print("🔄 Updating Firestore: ");
  Serial.println(docPath);

  String mask = "`" + today + "`";

  if (Firebase.Firestore.patchDocument(
        &fbdo, 
        FIREBASE_PROJECT_ID, 
        "(default)",
        docPath.c_str(), 
        json.raw(), 
        mask.c_str(),
        "", "", ""
    )) {
    Serial.println("✅ Firestore update successful!");
    return true;
  } else {
    Serial.print("❌ Firestore update failed: ");
    Serial.println(fbdo.errorReason());
    return false;
  }
}

float fetchFirebaseData() {
  if (!Firebase.ready()) return -1;

  String docPath = getDocumentPath();
  String today = getCurrentDate();

  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", docPath.c_str())) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, fbdo.payload());

    if (!error) {
      JsonObject fields = doc["fields"];
      if (fields.containsKey(today)) {
        float usage = fields[today]["doubleValue"].as<float>();
        Serial.printf("📥 Fetched Data → %s: %.2fL\n", today.c_str(), usage);

        if (usage >= 0) {
          totalUsage = usage;
        }
        return usage;
      }
    }
  }

  Serial.printf("❌ Failed to fetch Firestore data: %s\n", fbdo.errorReason().c_str());
  return -1;
}

void calculateWaterFlow() {
  portENTER_CRITICAL(&mux);
  int pulses = pulseCount;
  pulseCount = 0;
  portEXIT_CRITICAL(&mux);

  flowRate = pulses / 7.5;
  totalUsage += flowRate;

  Serial.printf("🚰 live Total Usage: %.2f L\n", totalUsage);

  preferences.putFloat("totalUsage", totalUsage);
}

void updateLastSeen() {
  if (!Firebase.ready()) return;

  struct timeval tv;
  gettimeofday(&tv, NULL);
  unsigned long long timestampMillis = ((unsigned long long)tv.tv_sec * 1000) + (tv.tv_usec / 1000);

  FirebaseJson json;
  json.set("fields/lastSeen/integerValue", String(timestampMillis));

  String documentPath = "users/" + String(USER_EMAIL);
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), json.raw(), "lastSeen")) {
    Serial.println("🟢 Last seen updated: " + String(timestampMillis));
  } else {
    Serial.println("❌ Failed to update last seen: " + fbdo.errorReason());
  }
}


// Helper function to draw an arc
void drawArc(int cx, int cy, int r, float startAngle, float endAngle) {
  for (float a = startAngle; a <= endAngle; a += 2.0) {
    float rad = a * PI / 180.0;
    int x = cx + r * cos(rad);
    int y = cy + r * sin(rad);
    display.drawPixel(x, y, SSD1306_WHITE);
  }
}

void drawNoWiFiIcon(int cx, int cy) {
  display.fillCircle(cx, cy, 2, SSD1306_WHITE);
  drawArc(cx, cy, 5, 200, 340);
  drawArc(cx, cy, 8, 200, 340);
  drawArc(cx, cy, 11, 200, 340);
  for (int i = 0; i < 3; i++) {
    display.drawLine(cx - 12, cy + 5 + i, cx + 12, cy - 11 + i, SSD1306_WHITE);
  }
}

void drawWiFiIcon(int cx, int cy) {
  display.fillCircle(cx, cy, 2, SSD1306_WHITE);
  drawArc(cx, cy, 5, 200, 340);
  drawArc(cx, cy, 8, 200, 340);
  drawArc(cx, cy, 11, 200, 340);
}

void FINAL_DISPLAY() {
  display.clearDisplay();

  if (isOnline) {
    drawWiFiIcon(110, 15);
  } else {
    drawNoWiFiIcon(110, 15);
  }

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.print(String(totalUsage, 0));

  display.drawLine(0, 30, SCREEN_WIDTH, 30, SSD1306_WHITE);

  display.setCursor(10, 40);
  display.print(String(limit, 0));

  display.setTextSize(2);
  display.setCursor(10 + (6 * 12) + 10, 40);
  display.print(effectiveServoState ? "ON" : "OFF");

  display.display();
}

// -------------------- SETUP FUNCTION --------------------
void setup() {
  Serial.begin(115200);
  initializeLocalStorage();

  // Initialize OLED display
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Save Water"); 
  display.display();

  // ============ DFPlayer Initialization (Optional) ============
  mySerial.begin(9600, SERIAL_8N1, 16, 17);
  
  Serial.println("⏳ Initializing DFPlayer Mini...");
  delay(1000);

  // Try to initialize DFPlayer with timeout
  bool dfReady = false;
  for (int i = 0; i < 3; i++) {
    if (dfPlayer.begin(mySerial)) {
      dfReady = true;
      break;
    }
    Serial.printf("⚠️ DFPlayer not detected (attempt %d/3)\n", i + 1);
    delay(1000);
  }

  if (dfReady) {
    dfPlayerAvailable = true;
    Serial.println("✅ DFPlayer Mini Ready!");
    dfPlayer.volume(29);
    delay(500);
  } else {
    dfPlayerAvailable = false;
    Serial.println("⚠️ DFPlayer Mini not available - continuing without audio");
  }
  // ============================================================

  // WiFi connection
  Serial.print("🔄 Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    if (retryCount % 5 == 0) {
      display.clearDisplay();
      display.setTextSize(1);
      int16_t x1, y1;
      uint16_t w, h;
      const char* wifiConnectText = "Connecting to WIFI...";
      display.getTextBounds(wifiConnectText, 0, 0, &x1, &y1, &w, &h);
      int16_t xPos = (128 - w) / 2;
      display.setCursor(xPos, 30); 
      display.println(wifiConnectText);
      display.display();
    }
    
    retryCount++;
    if (retryCount > 20) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Offline mode");
      display.display();
      Serial.println("\n❌ WiFi Connection Failed!");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Online");
    display.display();
    Serial.println("\n✅ WiFi Connected!");
    playAudioDual("Wifi_Connected.mp3", "Wifi_Connected_hindi.mp3");
    isOnline = true;
  } else {
    isOnline = false;
  }

  // Update display based on WiFi status
  display.clearDisplay();
  int16_t x1, y1;
  uint16_t w, h;

  const char* wifiText = isOnline ? "WiFi connected" : "Offline Mode";
  display.getTextBounds(wifiText, 0, 0, &x1, &y1, &w, &h);
  int16_t xPos1 = (128 - w) / 2;

  const char* syncText = isOnline ? "Syncing with server.." : "Syncing with local..";
  display.getTextBounds(syncText, 0, 0, &x1, &y1, &w, &h);
  int16_t xPos2 = (128 - w) / 2;

  display.setCursor(xPos1, 20);
  display.println(wifiText);
  display.setCursor(xPos2, 40);
  display.println(syncText);
  display.display();

  // Proceed with Firebase & other setups only if online
  if (isOnline) {
    initializeFirebase();
    syncNTPTime();
    limitFetch();
    fetchServoState();
    updateServoState();
  }
  
  // Attach servo
  myServo.attach(SERVO_PIN);
  myServo.write(servoState ? 0 : 90);

  // Setup water flow sensor
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);

  Serial.println("✅ Setup complete");

  if (isOnline) {
    totalUsage = fetchFirebaseData();
  }

  vTaskPrioritySet(NULL, 1);
}

// -------------------- MAIN LOOP --------------------
void loop() {
  processAudioQueue(); 
  
  // Check WiFi connection status
  if (WiFi.status() != WL_CONNECTED) {
    if (isOnline) {
      isOnline = false;
      wasOffline = true;
      playAudioDual("Wifi_Disconnected.mp3", "Wifi_Disconnected_hindi.mp3");
      Serial.println("⚠️ WiFi Disconnected! Working in offline mode.");
    }
    
    static unsigned long lastWiFiAttempt = 0;
    if (millis() - lastWiFiAttempt >= 5000) {  
      lastWiFiAttempt = millis();
      Serial.println("🔄 Attempting to reconnect WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
  } else {
    if (!isOnline) {
      isOnline = true;
      playAudioDual("Wifi_Connected.mp3", "Wifi_Connected_hindi.mp3");
      Serial.println("✅ WiFi Reconnected! isOnline = true");
      
      delay(5000);
      initializeFirebase();
      
      if (wasOffline) {
        float fetchedUsage = fetchFirebaseData();
        if (fetchedUsage >= 0) {
          totalUsage = fetchedUsage;
        }
        wasOffline = false;
      }
    }
  }

  // Update water flow sensor every second
  if (millis() - lastFlowUpdate >= 1000) {
    lastFlowUpdate = millis();
    calculateWaterFlow();
  }

  // Update Firebase data every 10 seconds, only if online
  if (isOnline && millis() - lastFirebaseUpdate >= 10000) {
    lastFirebaseUpdate = millis();
    updateFirebase(totalUsage);
    updateLastSeen();
  }
  
  if (millis() - lastServoCheck >= 1000) {
    updateServoState();
    lastServoCheck = millis();
  }
  
  // Periodically fetch "limit" and "servoState" from Firestore if online
  if (isOnline && millis() - lastLimitFetch >= LIMIT_FETCH_INTERVAL) {
    lastLimitFetch = millis();
    limitFetch();
    fetchServoState();
  }

  // Display update
  if (firstTime) {
    String message = "DWWP";
    display.clearDisplay();
    display.setTextSize(4);
    display.setTextColor(SSD1306_WHITE);
    int16_t x = (SCREEN_WIDTH - (message.length() * 24)) / 2;
    int16_t y = (SCREEN_HEIGHT - 32) / 2;
    display.setCursor(x, y);
    display.print(message);
    display.display();
    delay(2000);
    display.clearDisplay();
    firstTime = false;
  }
  
  FINAL_DISPLAY();
  delay(10);
}
