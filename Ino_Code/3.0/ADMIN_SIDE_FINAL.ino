/* Integrated: Firebase SMS Queue + SIM800L (ESP32)
   - Replaces demo send with real SIM800L sending
   - Keeps Firebase fetch/queue/process logic unchanged
   - Author: integrated for your setup
*/

#include <HardwareSerial.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <ArduinoJson.h>
#include <vector>
#include "time.h"

// -------------------- CONFIGURATION --------------------
#define WIFI_SSID "AB"
#define WIFI_PASSWORD "akashbera"
#define FIREBASE_PROJECT_ID "waterflow-dashboard"
#define API_KEY "AIzaSyCaQ4-CXMn-2HB0RdMKZvDmVm6SGtXPnk8"
#define USER_EMAIL "ab@gmail.com"
#define USER_PASSWORD "123456"
#define GMT_OFFSET 19800
#define DAYLIGHT_OFFSET 0
#define NTP_SERVER "pool.ntp.org"

// SIM800L UART (UART2)
#define SIM800_RX 16   // ESP32 RX2 (connect to SIM800 TXD)
#define SIM800_TX 17   // ESP32 TX2 (connect to SIM800 RXD)
HardwareSerial sim800(2); // UART2 for SIM800L

// -------------------- FIREBASE OBJECTS --------------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// -------------------- SMS QUEUE STRUCTURE --------------------
struct SmsTask {
  String docId;
  String userId;
  String mobileNo;
  String messageType;
  String message;
  int attempts;
};

std::vector<SmsTask> smsQueue;

// -------------------- STATE VARIABLES --------------------
unsigned long lastFetchTime = 0;
unsigned long lastProcessTime = 0;
unsigned long lastStatusTime = 0;
const unsigned long FETCH_INTERVAL = 30000;  // 30 seconds
const unsigned long PROCESS_INTERVAL = 5000; // 5 seconds
const unsigned long STATUS_INTERVAL = 10000; // 10 seconds
int totalProcessed = 0;
int totalFailed = 0;

// -------------------- FUNCTION DECLARATIONS --------------------
void setupWiFi();
void setupFirebase();
void fetchPendingSMS();
void processSmsQueue();
void sendSMSUsingSIM800(SmsTask &task); // replaces demo sender
void updateSmsStatus(String docId, String status, String error = "");
void printStatus();
void printLine(String symbol = "=", int count = 60);
String getCurrentTimestamp();

// SIM800 helper functions (from your working diagnostic)
String sendAndGetSIM(const char* cmd, uint32_t timeoutMs);
bool waitForContainsSIM(const char* expected, uint32_t timeoutMs);
String waitForAnyResponseSIM(uint32_t timeoutMs);
void flushSim();
int parseCSQ(const String& resp);
int parseCBCVoltage(const String& resp);
bool sendSMSViaSIM800(const char* number, const char* message);
bool ensureSimNetworkReady(int minCsq = 5, uint8_t cregRetries = 6, uint32_t delayMs = 2000);

// ====================================================
// SETUP
// ====================================================
void setup() {
  Serial.begin(115200);
  delay(20);

  // init SIM UART
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);
  delay(1200);

  printLine("=");
  Serial.println("🚀 ESP32 Firebase SMS Queue + SIM800L Integrated");
  printLine("=");
  Serial.println();

  setupWiFi();
  setupFirebase();

  // if (!syncNTPTimeWithRetry()){
  //   Serial.println("⚠️ Using fallback: internal clock until NTP available");
  // }

  Serial.println();
  printLine("=");
  Serial.println(" 👍 SYSTEM READY - Monitoring smsQueue Collection");
  printLine("=");
  Serial.println("📠 Status: Waiting for SMS tasks...");
  Serial.print("🔄 Fetch Interval (firebase fetch interval): ");
  Serial.print(FETCH_INTERVAL / 1000);
  Serial.println(" seconds");

  Serial.print("⚡ Process Interval (message queue interval) : ");
  Serial.print(PROCESS_INTERVAL / 1000);
  Serial.println(" seconds");
  printLine("=");
  Serial.println();
}

// Keep your NTP helper from earlier
void syncNTPTime()
{
  Serial.println("⏰ Syncing time with NTP server...");
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
    Serial.println("⌚ NTP time synced");
  else
    Serial.println("❌ Failed to sync NTP");
}

bool syncNTPTimeWithRetry(int maxRetries = 5)
{
  Serial.println("⏰ Syncing time with NTP server...");
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER);

  struct tm timeinfo;
  int attempts = 0;

  while (attempts < maxRetries)
  {
    if (getLocalTime(&timeinfo))
    {
      Serial.println("📅 ⌚ NTP time synced successfully");
      return true;
    }
    Serial.print("⏳ Retry ");
    Serial.println(attempts + 1);
    delay(2000);
    attempts++;
  }

  Serial.println("❌ NTP sync failed after retries");
  return false;
}

// ====================================================
// MAIN LOOP
// ====================================================
void loop()
{
  unsigned long currentTime = millis();

  if (currentTime - lastFetchTime >= FETCH_INTERVAL)
  {
    lastFetchTime = currentTime;
    fetchPendingSMS();
  }

  if (currentTime - lastProcessTime >= PROCESS_INTERVAL && !smsQueue.empty())
  {
    lastProcessTime = currentTime;
    processSmsQueue();
  }

  if (currentTime - lastStatusTime >= STATUS_INTERVAL)
  {
    lastStatusTime = currentTime;
    printStatus();
  }

  delay(100);
}

// ====================================================
// WIFI SETUP
// ====================================================
void setupWiFi()
{
  Serial.println("📡 Connecting to : " + String(WIFI_SSID));

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(" ☑️ WiFi Connected Successfully!");
    Serial.println();
  }
  else
  {
    Serial.println("❌ WiFi Connection Failed!");
    Serial.println("🔄 Restarting ESP32 in 3 seconds...");
    delay(3000);
    ESP.restart();
  }
}

// ====================================================
// FIREBASE SETUP
// ====================================================
void setupFirebase()
{
  Serial.println("🔥 Connecting to Firebase Firestore...");

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  int attempts = 0;
  while (!Firebase.ready() && attempts < 30)
  {
    Serial.print(".");
    delay(500);
    attempts++;
  }
  Serial.println();

  if (Firebase.ready())
  {
    Serial.println("☑️ Firebase Connected Successfully! at  🙍‍♂️User: " + String(USER_EMAIL));
    Serial.println();
  }
  else
  {
    Serial.println("❌ Firebase Connection Failed!");
    Serial.println("🔄 Restarting ESP32 in 3 seconds...");
    delay(3000);
    ESP.restart();
  }
}

// ====================================================
// PRINT STATUS
// ====================================================
void printStatus()
{
  printLine("-");
  Serial.println("📊 SYSTEM STATUS");
  printLine("-");
  Serial.print("📥 Queue Size: ");
  Serial.println(smsQueue.size());
  Serial.print("☑️ Total Sent: ");
  Serial.println(totalProcessed);
  Serial.print("➖ Total Failed: ");
  Serial.println(totalFailed);
  printLine("-");
  Serial.println();
}

void printLine(String symbol, int count)
{
  for (int i = 0; i < count; i++) Serial.print(symbol);
  Serial.println();
}

// ====================================================
// FETCH PENDING SMS FROM FIRESTORE
// (kept mostly identical to your original)
// ====================================================
void fetchPendingSMS()
{
  printLine("=");
  Serial.println("🔍 FETCHING PENDING SMS FROM FIRESTORE");
  printLine("=");
  Serial.println("📂 Collection: smsQueue");

  if (!Firebase.ready())
  {
    Serial.println("❌ Firebase not ready! Skipping fetch...");
    printLine("=");
    Serial.println();
    return;
  }
  String collectionPath = "smsQueue";

  if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", collectionPath.c_str(), 100, "", "", "", false))
  {
    Serial.println(" ☑️ Successfully fetched smsQueue documents");
    Serial.println();

    FirebaseJson json;
    json.setJsonData(fbdo.payload());

    FirebaseJsonData result;
    json.get(result, "documents");

    if (result.success && result.type == "array")
    {
      FirebaseJsonArray arr;
      result.getArray(arr);

      int totalDocs = arr.size();
      int newTasks = 0;
      int skippedTasks = 0;

      Serial.println("  📄 Total Documents Found: " + String(totalDocs));
      Serial.println();

      for (size_t i = 0; i < totalDocs; i++)
      {
        FirebaseJsonData doc;
        arr.get(doc, i);
        String docData = doc.to<String>();
        DynamicJsonDocument jsonDoc(4096);
        deserializeJson(jsonDoc, docData);

        String fullPath = jsonDoc["name"].as<String>();
        String docId = fullPath.substring(fullPath.lastIndexOf("/") + 1);

        String status = jsonDoc["fields"]["status"]["stringValue"] | "unknown";
        int attempts = jsonDoc["fields"]["attempts"]["integerValue"] | 0;

        Serial.print("     [" + String(i + 1) + "] Doc ID: ");
        Serial.println(docId);
        Serial.print("          Status: ");
        Serial.print(status);
        Serial.print(" | Attempts: ");
        Serial.print(attempts);

        if (status == "pending" || (status == "failed" && attempts < 3))
        {
          bool alreadyQueued = false;
          for (const auto &task : smsQueue)
          {
            if (task.docId == docId) { alreadyQueued = true; break; }
          }

          if (!alreadyQueued)
          {
            SmsTask task;
            task.docId = docId;
            task.userId = jsonDoc["fields"]["userId"]["stringValue"] | "";
            task.mobileNo = jsonDoc["fields"]["mobileNo"]["stringValue"] | "";
            task.messageType = jsonDoc["fields"]["messageType"]["stringValue"] | "";
            task.message = jsonDoc["fields"]["message"]["stringValue"] | "";
            task.attempts = attempts;

            smsQueue.push_back(task);
            newTasks++;

            Serial.println("      --- ☑️ Added to local queue");
          }
          else
          {
            skippedTasks++;
            Serial.println("      --- ⏭️  Already in queue - skipped");
          }
        }
        else
        {
          skippedTasks++;
          Serial.println("      --- ⏭️ skipped");
        }
        Serial.println();
      }

      printLine("-");
      Serial.println("💁 FETCH SUMMARY:");
      Serial.print("   ➕ New tasks added: -------- ");
      Serial.println(newTasks);
      Serial.print("   ⏭️ Tasks skipped: ---------- ");
      Serial.println(skippedTasks);
      Serial.print("   📥 Total in local queue: --- ");
      Serial.println(smsQueue.size());
      printLine("-");
    }
    else
    {
      Serial.println("ℹ️  No documents found in smsQueue collection");
    }
  }
  else
  {
    Serial.println("❌ Failed to fetch smsQueue:");
    Serial.println("   Error: " + fbdo.errorReason());
  }

  printLine("=");
  Serial.println();
}

// ====================================================
// PROCESS SMS QUEUE (One at a time)
// ====================================================
void processSmsQueue()
{
  if (smsQueue.empty()) return;

  SmsTask task = smsQueue.front();
  smsQueue.erase(smsQueue.begin());

  printLine("=");
  Serial.println("📤 PROCESSING SMS FROM QUEUE");
  printLine("=");
  Serial.println("📄 Document ID: " + task.docId);
  Serial.println("👤 User ID: " + task.userId);
  Serial.println("📱 Mobile Number: " + task.mobileNo);
  Serial.println("📋 Message Type: " + task.messageType);
  Serial.println("🔄 Attempt Number: " + String(task.attempts + 1) + "/3");
  Serial.println("📥 Remaining in Queue: " + String(smsQueue.size()));
  printLine("=");
  Serial.println();

  sendSMSUsingSIM800(task);
}

// ====================================================
// SEND SMS USING SIM800L (replaces demo function)
// - Checks CSQ & CREG just before sending
// - Updates Firestore using updateSmsStatus()
// ====================================================
void sendSMSUsingSIM800(SmsTask &task)
{
  printLine("*", 60);
  Serial.println("*           SMS SENDING (SIM800L MODE)                 *");
  printLine("*", 60);
  Serial.println();
  Serial.println("📱 TO: " + task.mobileNo);
  Serial.println();
  Serial.println("📝 MESSAGE:");
  printLine("-");
  Serial.println(task.message);
  printLine("-");
  Serial.println();

  // Minimum network check (adjust threshold if needed)
  const int MIN_CSQ = 5;

  Serial.println("⏳ Checking SIM network (CSQ >= " + String(MIN_CSQ) + ") ...");
  bool networkOk = ensureSimNetworkReady(MIN_CSQ, 6, 2000);

  if (!networkOk)
  {
    Serial.println("❌ Network not ready. Marking attempt as failed and will retry later.");
    task.attempts++;
    updateSmsStatus(task.docId, "failed", "Network not ready (CSQ/CREG)");
    totalFailed++;
    if (task.attempts < 3) {
      // requeue for later retry
      smsQueue.push_back(task);
      Serial.println("🔁 Requeued for retry.");
    } else {
      Serial.println("🛑 Max attempts reached.");
    }
    printLine("*", 60);
    Serial.println();
    return;
  }

  // Attempt actual send
  bool ok = sendSMSViaSIM800(task.mobileNo.c_str(), task.message.c_str());

  if (ok)
  {
    Serial.println("✅ SMS SENT SUCCESSFULLY!");
    Serial.println("📝 Updating Firestore: status=sent, sentAt=" + getCurrentTimestamp());
    updateSmsStatus(task.docId, "sent", "");
    totalProcessed++;
  }
  else
  {
    Serial.println("❌ SMS SENDING FAILED!");
    task.attempts++;
    String error = "SIM800 send failed";
    updateSmsStatus(task.docId, "failed", error);
    totalFailed++;

    if (task.attempts < 3)
    {
      Serial.println("🔄 Will retry in next fetch cycle");
      smsQueue.push_back(task);
    }
    else
    {
      Serial.println("🛑 Max attempts (3) reached - giving up");
    }
  }

  printLine("*", 60);
  Serial.println();
}

// ====================================================
// UPDATE SMS STATUS IN FIRESTORE
// ====================================================
void updateSmsStatus(String docId, String status, String error)
{
  FirebaseJson content;
  content.set("fields/status/stringValue", status);

  if (status == "sent")
    content.set("fields/sentAt/timestampValue", getCurrentTimestamp());

  if (status == "failed" && error != "")
    content.set("fields/lastError/stringValue", error);

  String documentPath = "smsQueue/" + docId;

  if (!Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "status,sentAt,lastError"))
  {
    Serial.println("❌ Failed to update Firestore:");
    Serial.println("   Error: " + fbdo.errorReason());
  }
}

// ====================================================
// GET CURRENT TIMESTAMP (ISO 8601)
// ====================================================
String getCurrentTimestamp()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    return "1970-01-01T00:00:00Z";
  }

  char buffer[30];
  sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02dZ",
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec);
  return String(buffer);
}

// ====================================================
// ---------- SIM800 HELPER IMPLEMENTATIONS ------------
// (adapted from your diagnostic file)
// ====================================================
String waitForAnyResponseSIM(uint32_t timeoutMs) {
  uint32_t start = millis();
  String resp = "";
  while (millis() - start < timeoutMs) {
    while (sim800.available()) resp += (char)sim800.read();
  }
  return resp;
}

String sendAndGetSIM(const char* cmd, uint32_t timeoutMs) {
  flushSim();
  sim800.println(cmd);
  return waitForAnyResponseSIM(timeoutMs);
}

bool waitForContainsSIM(const char* expected, uint32_t timeoutMs) {
  uint32_t start = millis();
  String resp = "";
  while (millis() - start < timeoutMs) {
    while (sim800.available()) resp += (char)sim800.read();
    if (resp.indexOf(expected) != -1) return true;
  }
  return false;
}

void flushSim() {
  while (sim800.available()) sim800.read();
}

int parseCSQ(const String& resp) {
  int idx = resp.indexOf("+CSQ:");
  if (idx == -1) return 99;
  int comma = resp.indexOf(',', idx);
  if (comma == -1) return 99;
  String s = resp.substring(idx + 6, comma);
  s.trim();
  return s.toInt();
}

int parseCBCVoltage(const String& resp) {
  int idx = resp.indexOf("+CBC:");
  if (idx == -1) return -1;
  int lastComma = resp.lastIndexOf(',');
  if (lastComma == -1) return -1;
  String s = resp.substring(lastComma + 1);
  s.trim();
  return s.toInt();
}

// Ensure the SIM is registered and has minimum CSQ
bool ensureSimNetworkReady(int minCsq, uint8_t cregRetries, uint32_t delayMs) {
  // check CSQ
  String csqResp = sendAndGetSIM("AT+CSQ", 1500);
  int csqVal = parseCSQ(csqResp);
  Serial.print("📶 CSQ check = ");
  Serial.println(csqVal);

  // quick loop to wait for registration if not already
  for (uint8_t i = 0; i < cregRetries; ++i) {
    String creg = sendAndGetSIM("AT+CREG?", 1200);
    Serial.println(creg);
    if (creg.indexOf("+CREG: 0,1") != -1 || creg.indexOf("+CREG: 0,5") != -1) {
      // registered
      if (csqVal == 99) {
        // re-check CSQ once
        csqResp = sendAndGetSIM("AT+CSQ", 1200);
        csqVal = parseCSQ(csqResp);
      }
      return (csqVal != 99 && csqVal >= minCsq);
    } else if (creg.indexOf("+CREG: 0,3") != -1) {
      Serial.println("🚫 Registration denied (CREG:0,3)");
      return false;
    }
    delay(delayMs);
    // refresh csqVal for next iteration
    csqResp = sendAndGetSIM("AT+CSQ", 1200);
    csqVal = parseCSQ(csqResp);
    Serial.print("📶 CSQ retry = ");
    Serial.println(csqVal);
    if (csqVal >= minCsq) {
      // do one more CREG check
      String creg2 = sendAndGetSIM("AT+CREG?", 1200);
      if (creg2.indexOf("+CREG: 0,1") != -1 || creg2.indexOf("+CREG: 0,5") != -1) return true;
    }
  }

  return false;
}

// Actual send routine using AT commands (returns true if send confirmed)
bool sendSMSViaSIM800(const char* number, const char* message) {
  flushSim();

  sim800.println("AT+CMGF=1"); // text mode
  if (!waitForContainsSIM("OK", 2000)) {
    Serial.println("⚠️ AT+CMGF failed or no response");
    return false;
  }

  sim800.print("AT+CMGS=\"");
  sim800.print(number);
  sim800.println("\"");

  // Wait for '>' prompt
  if (!waitForContainsSIM(">", 4000)) {
    Serial.println("⚠️ No '>' prompt — can't enter message (module returned:)");
    return false;
  }

  // Send message + Ctrl+Z
  sim800.print(message);
  sim800.write(26); // Ctrl+Z

  // Wait for CMGS or OK / or ERROR
  uint32_t start = millis();
  String resp = "";
  while (millis() - start < 15000) {
    while (sim800.available()) resp += (char)sim800.read();
    if (resp.indexOf("+CMGS:") != -1 || resp.indexOf("OK") != -1) {
      Serial.println(resp);
      return true;
    }
    if (resp.indexOf("ERROR") != -1) {
      Serial.println(resp);
      return false;
    }
  }
  Serial.println("⚠️ No definitive send result (timeout). Module response:");
  Serial.println(resp);
  return false;
}
