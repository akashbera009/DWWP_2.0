// it only have a demo sim sms function , other wise it is working great

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <ArduinoJson.h>
#include <vector>
// #include <NTPClient.h>
#include "time.h"
// #include <WiFiUdp.h>

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

// -------------------- OBJECTS --------------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// -------------------- SMS QUEUE STRUCTURE --------------------
struct SmsTask
{
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
void sendSMS(SmsTask &task);
void updateSmsStatus(String docId, String status, String error = "");
void printStatus();
void printLine(String symbol = "=", int count = 60);
String getCurrentTimestamp();

// ====================================================
// SETUP
// ====================================================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  printLine("=");
  Serial.println("🚀 ESP32 SMS QUEUE PROCESSOR v1.0");
  Serial.println("📱 Admin Device - Waterflow Dashboard");
  printLine("=");
  Serial.println();

  setupWiFi();

  setupFirebase();

  // syncNTPTime();
  if (!syncNTPTimeWithRetry()){
    Serial.println("⚠️ Using fallback: internal clock until NTP available");
  }

  Serial.println();
  printLine("=");
  Serial.println(" 👍 SYSTEM READY - Monitoring smsQueue Collection");
  printLine("=");
  Serial.println("📠 Status: Waiting for SMS tasks...");
  Serial.print("🔄 Fetch Interval (firebase fetch interval): ");
  Serial.print(FETCH_INTERVAL);
  Serial.println(" seconds");

  Serial.print("⚡ Process Interval (message queue interval) : ");
  Serial.print(PROCESS_INTERVAL);
  Serial.println(" seconds");
  printLine("=");
  Serial.println();
}

// ====================================================
// MAIN LOOP
// ====================================================
void loop()
{
  unsigned long currentTime = millis();

  // Job 1: Fetch pending SMS from Firestore (every 30 seconds)
  if (currentTime - lastFetchTime >= FETCH_INTERVAL)
  {
    lastFetchTime = currentTime;
    fetchPendingSMS();
  }

  // Job 2: Process local SMS queue (every 5 seconds)
  if (currentTime - lastProcessTime >= PROCESS_INTERVAL && !smsQueue.empty())
  {
    lastProcessTime = currentTime;
    processSmsQueue();
  }

  // Job 3: Print status update (every 10 seconds)
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
void syncNTPTime()
{
  Serial.println("⏰ Syncing time with NTP server...");
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    Serial.println("⌚ NTP time synced");
  }
  else
  {
    Serial.println("❌ Failed to sync NTP");
  }
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

  // Wait for authentication
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
    // Serial.println("🔐 Authentication: OK");
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
  // Serial.print("⏰ Uptime: ");
  // Serial.print(millis() / 1000);
  // Serial.println(" seconds");
  Serial.print("📥 Queue Size: ");
  Serial.println(smsQueue.size());
  Serial.print("☑️ Total Sent: ");
  Serial.println(totalProcessed);
  Serial.print("➖ Total Failed: ");
  Serial.println(totalFailed);
  // Serial.print("📶 WiFi: ");
  // Serial.print(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  // Serial.print(" (");
  // Serial.print(WiFi.RSSI());
  // Serial.println(" dBm)");
  // Serial.print("🔥 Firebase: ");
  // Serial.println(Firebase.ready() ? "Connected" : "Disconnected");
  printLine("-");
  Serial.println();
}

// ====================================================
// PRINT LINE
// ====================================================
void printLine(String symbol, int count)
{
  for (int i = 0; i < count; i++)
  {
    Serial.print(symbol);
  }
  Serial.println();
}

// ====================================================
// FETCH PENDING SMS FROM FIRESTORE
// ====================================================
void fetchPendingSMS()
{
  printLine("=");
  Serial.println("🔍 FETCHING PENDING SMS FROM FIRESTORE");
  printLine("=");
  Serial.println("📂 Collection: smsQueue");
  // Serial.println("🔎 Filtering: status='pending' OR (status='failed' AND attempts<3)");
  // Serial.println();

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
        DynamicJsonDocument jsonDoc(2048);
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

        // Only queue pending or failed (with attempts < 3)
        if (status == "pending" || (status == "failed" && attempts < 3))
        {

          // Check if already in queue
          bool alreadyQueued = false;
          for (const auto &task : smsQueue)
          {
            if (task.docId == docId)
            {
              alreadyQueued = true;
              break;
            }
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
      Serial.println("   (Either empty or all SMS already processed)");
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
  if (smsQueue.empty())
  {
    return;
  }

  // Get first task
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

  sendSMS(task);
}

// ====================================================
// SEND SMS (DEMO - Serial Monitor Output)
// ====================================================
void sendSMS(SmsTask &task)
{
  printLine("*", 60);
  Serial.println("*           SMS SENDING SIMULATOR (DEMO MODE)              *");
  printLine("*", 60);
  Serial.println();
  Serial.println("📱 TO: " + task.mobileNo);
  Serial.println();
  Serial.println("📝 MESSAGE:");
  printLine("-");
  Serial.println(task.message);
  printLine("-");
  Serial.println();
  Serial.println("📊 METADATA:");
  Serial.println("   User ID: " + task.userId);
  Serial.println("   Message Type: " + task.messageType);
  Serial.println("   Document ID: " + task.docId);
  Serial.println("   Attempt: " + String(task.attempts + 1) + "/3");
  Serial.println();
  printLine("-");
  Serial.println("⏳ Simulating SMS sending (network delay)...");

  // Simulate sending (random success/failure for demo)
  delay(2000);

  bool success = (random(0, 10) > 2); // 80% success rate

  Serial.println();
  printLine("-");

  if (success)
  {
    Serial.println("✅ SMS SENT SUCCESSFULLY!");
    Serial.println();
    Serial.println("📝 Updating Firestore...");
    Serial.println("   Setting status: 'sent'");
    Serial.println("   Setting sentAt: " + getCurrentTimestamp());

    updateSmsStatus(task.docId, "sent", "");
    totalProcessed++;

    Serial.println(" ☑️ Firestore updated successfully");
    Serial.println();
    Serial.println("🎉 SUCCESS! SMS delivered to " + task.mobileNo);
  }
  else
  {
    Serial.println("❌ SMS SENDING FAILED!");
    Serial.println();
    task.attempts++;

    String error = "Network timeout";
    Serial.println("📝 Updating Firestore...");
    Serial.println("   Setting status: 'failed'");
    Serial.println("   Setting lastError: '" + error + "'");
    Serial.println("   Incrementing attempts to: " + String(task.attempts));

    updateSmsStatus(task.docId, "failed", error);
    totalFailed++;

    Serial.println(" ☑️ Firestore updated successfully");
    Serial.println();

    if (task.attempts < 3)
    {
      Serial.println("🔄 Will retry in next fetch cycle");
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
  {
    content.set("fields/sentAt/timestampValue", getCurrentTimestamp());
  }

  if (status == "failed" && error != "")
  {
    content.set("fields/lastError/stringValue", error);
  }

  String documentPath = "smsQueue/" + docId;

  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "status,sentAt,lastError"))
  {
    // Success - already logged in sendSMS()
  }
  else
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
