  //                                ---- tested overlappiing audio doing work , need more optimization ------

  // facing sometimes delay .. after reconnecting system reboots (need to think is it good or bad ) ||  also some internal noise leads to increase water flow sensor disturbences 

/*
 **Updated Summary (Shortened)**  

1. **Display & WiFi**: Shows connection status; retries every 5s; exits failed loops.  
2. **Firebase Optimization**: Calls only when online; caches `limit`, `totalUsage`, `servoState`.  
3. **Servo & Sensor**: Runs independently; water flow sensor updates `totalUsage`.  
4. **Servo State Logic**:  
   - **Offline**: Uses local cache for control.  
   - **Online**: Respects Firebase if `totalUsage < limit`, else forces OFF.  
   - **Sync**: Enforced state updates Firebase when online.  
5. **System Robustness**: Tracks `currentServoAngle` to avoid redundant movements.  
6. **Safety & Transparency**: Servo turns OFF if `totalUsage >= limit`; manual overrides only if within limit. 🚀

7. audio (wifi connected and disconneted is integrated  || wate rsupply sound added   )

8 . in between code freezing stopped ( need more tests )
*/



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
#include <esp_task_wdt.h>



// -------------------- CONFIGURATION --------------------
#define WIFI_SSID         "AB"
#define WIFI_PASSWORD     "akashbera"
#define FIREBASE_PROJECT_ID "waterflow-dashboard"
#define API_KEY           "AIzaSyCaQ4-CXMn-2HB0RdMKZvDmVm6SGtXPnk8"
#define USER_EMAIL        "ab@gmail.com"
#define USER_PASSWORD     "123456"
#define NTP_SERVER        "pool.ntp.org"
#define GMT_OFFSET        19800      // UTC+5:30 (India)
#define DAYLIGHT_OFFSET   0

#define FLOW_SENSOR_PIN   5
#define SERVO_PIN 18
// display 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22

#define QUEUE_SIZE 15

Servo myServo;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

HardwareSerial mySerial(1); // Use UART1 on ESP32
DFRobotDFPlayerMini dfPlayer;



portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE audioMux = portMUX_INITIALIZER_UNLOCKED; // audio mux 

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
const unsigned long LIMIT_FETCH_INTERVAL = 5000; // e.g., every 5 seconds
 static unsigned long lastServoCheck = 0;

float totalUsage = 0.0;  // in Liters
float flowRate = 0.0;    // Liters per second
float limit = 0.0;       // New variable "limit"
bool servoState = false;  // Default state of servo
bool isOnline = false;
bool wasOffline = false;  // Track previous WiFi state

bool firstTime = true; // for display 
int currentServoAngle = -1;
bool effectiveServoState;

// Define mapping between names and file numbers
struct AudioFile {
    int fileNumber;
    const char* fileName;
};

int audioQueue[QUEUE_SIZE];
int front = 0, rear = 0;
bool isPlaying = false;

void enqueue(int fileNumber) {
    if ((rear + 1) % QUEUE_SIZE == front) {
        Serial.println("Queue is full!");
        return;
    }
    audioQueue[rear] = fileNumber;
    rear = (rear + 1) % QUEUE_SIZE;
}
int dequeue() {
    if (front == rear) return -1;  // Queue empty
    int fileNumber = audioQueue[front];
    front = (front + 1) % QUEUE_SIZE;
    return fileNumber;
}
AudioFile audioFiles[] = {
    {1, "NINTY_ALERT_ENGLISH.wav"},
    {2, "NINTY_ALERT_HINDI.wav"},
    {3, "PAYMENT_DUE_ENGLISH.wav"},
    {4, "PAYMENT_DUE_HINDI.wav"},
    {5, "WIFI_CONNECTED.wav"},
    {6, "WIFI_DISCONNECT.wav"},
    {7, "WATER_SUPPLY_ACTIVATED.wav"},
    {8, "WATER_SUPPLY_STOPPED.wav"}
};
// Function to get file number by name
int getFileNumber(const char* fileName) {
    for (int i = 0; i < sizeof(audioFiles) / sizeof(audioFiles[0]); i++) {
        if (strcmp(audioFiles[i].fileName, fileName) == 0) {
            return audioFiles[i].fileNumber;
        }
    }
    return -1; // Return -1 if file not found
}

// Modified playAudio function with queue protection
void playAudio(const char* fileName) {
    if ((rear + 1) % QUEUE_SIZE == front) {
        Serial.println("Audio queue full - discarding request");
        return;
    }
    
    int fileNumber = getFileNumber(fileName);
    if (fileNumber != -1) {
        enqueue(fileNumber);
        Serial.print("Queued: ");
        Serial.println(fileName);
    }
}
#define MAX_TRACK_DURATION 10000 // 10 seconds (adjust based on your longest audio file)
unsigned long trackStartTime = 0;

// Modified processAudioQueue function
void processAudioQueue() {
    // Check if current track should be considered finished
    if (isPlaying && (millis() - trackStartTime > MAX_TRACK_DURATION)) {
        isPlaying = false;
        Serial.println("Track timeout - assuming finished");
    }

    // Play next track if not playing and queue not empty
    if (!isPlaying) {
        int fileNumber = dequeue();
        if (fileNumber != -1) {
            dfPlayer.stop();
            delay(150); // Required stabilization delay
            dfPlayer.play(fileNumber);
            Serial.print("Now playing: ");
            Serial.println(fileNumber);
            trackStartTime = millis();
            isPlaying = true;
        }
    }

    // Always handle serial messages to prevent buffer overflow
    while (mySerial.available()) {
        dfPlayer.handleMessage(mySerial.read());
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
  const unsigned long timeout = 10000; // 10 seconds timeout
  
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

  // Use modern certificate configuration
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

// Get current date as "YYYY-MM-DD"
String getCurrentDate() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char dateStr[11];
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  return String(dateStr);
}

/// --------------------------------limit  fetch 
void limitFetch() {
  String docPath = getDocumentPath(); // e.g., "users/ab@gmail.com/monthlyUsages/2025-02"
  
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
    // Check if limit is stored as doubleValue or integerValue:
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

// 🔹 Fetch servoState separately                                                                                       ---- SERVO SECTION🌲
// Updated fetchServoState(): simply retrieves servoState from Firestore

void fetchServoState() {
  if (!Firebase.ready()) return;

  String documentPath = "users/" + String(USER_EMAIL); // Firestore path for servoState

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

  // ✅ Fetch "servoState"
  if (fields.containsKey("servoState")) {
    bool fetchedServoState = fields["servoState"]["booleanValue"].as<bool>();
    preferences.putBool("servoState", fetchedServoState); // Store in local storage
    Serial.print("🚺 Fetched servoState: ");
    Serial.println(fetchedServoState ? "true" : "false");
  } else {
    Serial.println("❌ servoState not found in Firestore");
  }
}

void updateServoState() {
  // Get the latest values from local storage (updated by Firebase fetches)
  bool storedServoState = preferences.getBool("servoState", false);
  float storedLimit = preferences.getFloat("limit", 0.0);

  // Determine the effective servo state based on usage and limit

  if (totalUsage >= storedLimit) {
    effectiveServoState = false; // Force OFF if limit exceeded
    Serial.println("⚠️ Limit exceeded: Forcing servo OFF");
  } else {
    effectiveServoState = storedServoState; // Respect Firebase value
    Serial.printf("✅ Within limit: Servo state = %s\n", 
                  effectiveServoState ? "ON" : "OFF");
  }

  // Calculate target angle
  int targetAngle = effectiveServoState ? 0 : 90;

  // Move servo only if the target angle changes
  if (targetAngle != currentServoAngle) {
    myServo.write(targetAngle);
    currentServoAngle = targetAngle;
    Serial.printf("🔄 Servo moved to %d°\n", targetAngle);
          delay(100);
        // Add audio command protection
      if (audioQueueCount() < QUEUE_SIZE-2) { // Keep 2 slots free
          const char* sound = targetAngle ? "WATER_SUPPLY_STOPPED.wav" 
                                          : "WATER_SUPPLY_ACTIVATED.wav";
          playAudio(sound);
      }
      // delay(50);


    // Sync state back to Firebase (if online and state changed)
    if (isOnline && effectiveServoState != storedServoState) {
      FirebaseJson json;
      json.set("fields/servoState/booleanValue", effectiveServoState);
      String path = "users/" + String(USER_EMAIL);
      if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", 
                                          path.c_str(), json.raw(), "servoState")) {
        preferences.putBool("servoState", effectiveServoState); // Update local copy
        Serial.println("📤 Synced servo state to Firebase");
      }
    }
  }
}

    int audioQueueCount() {
        return (rear >= front) ? (rear - front) : (QUEUE_SIZE - front + rear);
    }

// -------------------- FIREBASE UPDATE --------------------
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

  // Use today's date as the update mask so that only that field gets updated.
  String mask = "`" + today + "`";

  if (Firebase.Firestore.patchDocument(
        &fbdo, 
        FIREBASE_PROJECT_ID, 
        "(default)",  // Firestore database ID
        docPath.c_str(), 
        json.raw(), 
        mask.c_str(),  // Update mask: update only the field named for today
        "",  // Transaction (leave empty if not needed)
        "",  // New Transaction (leave empty if not needed)
        ""   // ETag (leave empty if not needed)
    )) {
    Serial.println("✅ Firestore update successful!");
    return true;
  } else {
    Serial.print("❌ Firestore update failed: ");
    Serial.println(fbdo.errorReason());
    return false;
  }
}


// -------------------- FIREBASE FETCH --------------------
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

        // ✅ Only update if a valid value is received
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

// -------------------- WATER FLOW CALCULATION --------------------
void calculateWaterFlow() {
  // noInterrupts();
  portENTER_CRITICAL(&mux);
  int pulses = pulseCount;
  pulseCount = 0;
  // interrupts();
  portEXIT_CRITICAL(&mux);

  // Conversion factor: pulses/7.5 equals liters per second (adjust as needed)
  flowRate = pulses / 7.5;
  totalUsage += flowRate;

  Serial.printf("🚰 live Total Usage: %.2f L\n", totalUsage);

  // Save totalUsage to local storage
  preferences.putFloat("totalUsage", totalUsage);
}

/// -------------------- LAST SEEN UPDATE --------------------
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

// -------------------- SETUP FUNCTION --------------------
void setup() {
  Serial.begin(115200);
  initializeLocalStorage();  // Load local data

  // ✅ Initialize OLED display BEFORE connecting WiFi
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(WHITE);
  String message = "DWWP";
  int16_t x = (SCREEN_WIDTH - (message.length() * 24)) / 2;
  int16_t y = (SCREEN_HEIGHT - 32) / 2;
  display.setCursor(x, y);
  display.print(message);
  display.display();


  //  mySerial.begin(9600, SERIAL_8N1, 16, 17); // RX = GPIO16, TX = GPIO17
  mySerial.begin(9600, SERIAL_8N1, 16, 17, false, 20000UL);  // Add timeout

  if (!dfPlayer.begin(mySerial)) {
        Serial.println("DFPlayer Mini not detected!");
        while (true);
    }
   Serial.println("DFPlayer Mini Ready!");
   dfPlayer.volume(29); // Set volume (0 to 30)


  // ✅ Attempt WiFi connection and keep "DWWP" displayed
  Serial.print("🔄 Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    // 🔹 Keep refreshing "DWWP" every 5 iterations
    
    if (retryCount % 1 == 0) {  // Update on every retry for a fast effect
      display.clearDisplay();
      display.setTextSize(1);
  
      int16_t centerX = 64, centerY = 32;  // Center point
      int16_t radius = 10;  // Distance from the center
      uint8_t spinnerState = retryCount % 8;  // Moves every retry count
  
      // Predefined 8 positions in a circular motion
      int8_t pointsX[8] = {0, 7, 10,  7,  0, -7, -10, -7};
      int8_t pointsY[8] = {-10, -7,  0,  7, 10,  7,   0, -7};
  
      // Draw 4 dots in a circular pattern
      for (int i = 0; i < 4; i++) {
          int index = (spinnerState + (i * 2)) % 8;  // Offset each dot
          display.fillCircle(centerX + pointsX[index], centerY + pointsY[index], 2, WHITE);
      }
  
      // Show the connecting text
      display.setCursor(30, 50);
      display.println("Connecting...");
  
      display.display();
  }
  
    retryCount++;
    if (retryCount > 20) {  // Optional timeout (20 sec)
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
      playAudio("WIFI_CONNECTED.wav");
    isOnline = true;
  } else {
    isOnline = false;
  }

  // ✅ Update display based on WiFi status
  display.clearDisplay();

  int16_t x1, y1;
  uint16_t w, h;
  
  display.clearDisplay();  // Clear the screen before updating
  
  // Draw WiFi icon instead of text
  drawWiFiIcon(64, 20);  // Centered WiFi icon at (64, 20)
  
  // Get text bounds for "Syncing..." replacement
  // const char* syncText = isOnline ? "Syncing with server.." : "Syncing with local..";
  // display.getTextBounds(syncText, 0, 0, &x1, &y1, &w, &h);
  // int16_t xPos2 = (128 - w) / 2;  // Center horizontally
  
  // Draw horizontal loader below WiFi icon
  drawLoader(34, 50, 60, 8, retryCount);
  
  display.display();  // Refresh the display
  
  // Function to draw the WiFi icon
  void drawWiFiIcon(int cx, int cy) {
      display.fillCircle(cx, cy, 2, SSD1306_WHITE);  // Base dot
      drawArc(cx, cy, 5, 200, 340);  // Small arc
      drawArc(cx, cy, 8, 200, 340);  // Middle arc
      drawArc(cx, cy, 11, 200, 340);  // Large arc
  }
  
  // Function to draw animated horizontal loader
  void drawLoader(int x, int y, int width, int height, int progress) {
      display.drawRect(x, y, width, height, SSD1306_WHITE);  // Loader border
      int fillWidth = map(progress % 10, 0, 9, 0, width - 2);  // Moving fill effect
      display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
  }
  


  // ✅ Proceed with Firebase & other setups only if online
  if (isOnline) {
    initializeFirebase();
    syncNTPTime();
    limitFetch();
    fetchServoState();
    updateServoState();
  }
  
  // ✅ Attach servo irrespective of online/offline mode
  myServo.attach(SERVO_PIN);
  myServo.write(servoState ? 0 : 90);

  // ✅ Setup water flow sensor
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);

  Serial.println("✅ Setup complete");

  // ✅ Fetch previous data from Firestore only if online
  if (isOnline) {
    totalUsage = fetchFirebaseData();
  }


    // Enable watchdog timer (5 second timeout)
     // Define WDT configuration structure
    esp_task_wdt_config_t wdtConfig = {
        .timeout_ms = 5000,  // 5 seconds timeout
        .idle_core_mask = (1 << 0) | (1 << 1),  // Enable on both cores
        .trigger_panic = false,  // Don't trigger panic; just reset
    };

    // Initialize the watchdog timer with the correct structure
    esp_task_wdt_init(&wdtConfig);  // ✅ Fixed

    // Subscribe the main loop task to the watchdog
    esp_task_wdt_add(NULL);  // NULL means "current task"

    
  vTaskPrioritySet(NULL, 1);   // Set main loop priority
}


// -------------------- MAIN LOOP --------------------
void loop() {
  // Check WiFi connection status
      processAudioQueue(); 
if (dfPlayer.available()) {  // ✅ Check if DFPlayer has sent a response
    int messageType = dfPlayer.readType();  // ✅ Get message type

    switch (messageType) {
        case DFPlayerPlayFinished:
            Serial.println("Track finished playing.");
            break;
        case DFPlayerError:
            Serial.print("Error: ");
            Serial.println(dfPlayer.read());  // ✅ Read error code
            break;
        default:
            Serial.print("Message Type: ");
            Serial.println(messageType);
            break;
    }
}

  if (WiFi.status() != WL_CONNECTED) {
    if (isOnline) {  // Only update on change
      isOnline = false;
      wasOffline = true;
      playAudio("WIFI_DISCONNECT.wav");
      Serial.println("⚠️ WiFi Disconnected! Working in offline mode.");
    }
        // ✅ Attempt to reconnect every 5 seconds
    static unsigned long lastWiFiAttempt = 0;
    if (millis() - lastWiFiAttempt >= 5000) {  
      lastWiFiAttempt = millis();
      Serial.println("🔄 Attempting to reconnect WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
  } else {
    if (!isOnline) {  // WiFi reconnected
      isOnline = true;
       playAudio("WIFI_CONNECTED.wav");
      Serial.println("✅ WiFi Reconnected! isOnline = true");
      
      // Wait for connection stabilization
      delay(5000);
      
      // Reinitialize Firebase to recover from potential SSL issues
      initializeFirebase();
      
      // Fetch usage data if we were offline
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
  if (millis() - lastServoCheck >= 1000) { // Check every 1 second
  
    updateServoState();
    lastServoCheck = millis();
  }
  // Periodically fetch "limit" and "servoState" from Firestore if online
  if (isOnline && millis() - lastLimitFetch >= LIMIT_FETCH_INTERVAL) {
    lastLimitFetch = millis();
    limitFetch();
    fetchServoState();
  }

  // Display update: Show welcome message once, then continuously update display
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
    delay(1000); // Show welcome message for 2 seconds
    display.clearDisplay();
    firstTime = false;
  }
  
  // Update main display with current sensor readings and status
  FINAL_DISPLAY();
      esp_task_wdt_reset();
  delay(10);
}


// Helper function to draw an arc (a portion of a circle)
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

  drawArc(cx, cy, 5, 200, 340);   // Small arc
  drawArc(cx, cy, 8, 200, 340);   // Middle arc
  drawArc(cx, cy, 11, 200, 340);  // Large arc
  for (int i = 0; i < 3; i++) {  // Increase the number to make it thicker
      display.drawLine(cx - 12, cy + 5 + i, cx + 12, cy - 11 + i, SSD1306_WHITE);
  }
}

// Simplified WiFi icon function with one base dot and three arcs
void drawWiFiIcon(int cx, int cy) {
    // Base dot (signal source)
    display.fillCircle(cx, cy, 2, SSD1306_WHITE);
    // Three arcs that form the WiFi signal (drawn as partial circles)
    drawArc(cx, cy, 5, 200, 340);   // Small arc
    drawArc(cx, cy, 8, 200, 340);   // Middle arc
    drawArc(cx, cy, 11, 200, 340);  // Large arc
}
void FINAL_DISPLAY() {
    display.clearDisplay();

    // Display WiFi icon in the top-right corner if online
    if (isOnline) {
      drawWiFiIcon(110, 15);
    } else {
        drawNoWiFiIcon(110, 15);
    }

    // Display Total Usage
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.print(String(totalUsage, 0));


    // Draw a separator line
    display.drawLine(0, 30, SCREEN_WIDTH, 30, SSD1306_WHITE);

    // Display Limit
    display.setCursor(10, 40);
    display.print(String(limit, 0));

    // servo state 
    display.setTextSize(2);
    display.setCursor(10 + (6 * 12) + 10, 40);
    display.print( effectiveServoState ? "ON" : "OFF");

    display.display();
}

