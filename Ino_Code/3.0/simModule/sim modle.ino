/* SIM800L Diagnostic + Auto-SMS for ESP32
   - Uses UART2 (GPIO16 RX, GPIO17 TX)
   - Checks AT, SIM, CSQ, CBC, CREG
   - Retries registration and sends SMS only when registered
   - Author: ChatGPT (adapted for your setup)
*/

#include <HardwareSerial.h>
HardwareSerial sim800(2); // UART2 for SIM800L

// --- CONFIG ---
#define SIM800_RX 16   // ESP32 RX2 (connect to SIM800 TXD)
#define SIM800_TX 17   // ESP32 TX2 (connect to SIM800 RXD)
// const char* TARGET_NUMBER = "9649635140"; // <-- change if needed
const char* TARGET_NUMBER = "7586068924"; // <-- change if needed
const uint8_t MAX_REG_RETRIES = 20;        // how many times to check registration
const uint32_t REG_RETRY_DELAY_MS = 3000;  // delay between registration checks (ms)
// ----------------

String lastResponse = "";

void setup() {
  Serial.begin(115200);
  delay(20);
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);
  delay(1200);

  Serial.println("\n🔍 SIM800L Diagnostic + Auto-SMS (ESP32)\n");

  // Basic checks
  bool commOK = checkCommandContains("AT", "OK", 1500, "Module Communication");
  bool simOK  = checkCommandContains("AT+CPIN?", "READY", 2000, "SIM Card Detection");

  // Signal (CSQ)
  String csqResp = sendAndGet("AT+CSQ", 1500);
  Serial.println(csqResp);
  int csq = parseCSQ(csqResp);
  if (csq == 99) {
    Serial.println("⚠️ CSQ: Unknown (99) → No signal detected");
  } else {
    Serial.print("📶 Signal (CSQ) = ");
    Serial.println(csq);
  }

  // Power (CBC)
  String cbcResp = sendAndGet("AT+CBC", 1500);
  Serial.println(cbcResp);
  int voltageMv = parseCBCVoltage(cbcResp);
  if (voltageMv > 0) {
    Serial.print("🔋 Vbat (mV) = ");
    Serial.println(voltageMv);
    if (voltageMv < 3700) Serial.println("⚠️ Voltage below 3.7V — power may be unstable");
  } else {
    Serial.println("⚠️ Could not read voltage (AT+CBC response)");
  }

  // Registration (retry loop)
  bool registered = false;
  for (uint8_t i = 0; i < MAX_REG_RETRIES; ++i) {
    String creg = sendAndGet("AT+CREG?", 1500);
    Serial.println(creg);
    if (creg.indexOf("+CREG:") != -1) {
      if (creg.indexOf("+CREG: 0,1") != -1 || creg.indexOf("+CREG: 0,5") != -1) {
        registered = true;
        Serial.println("✅ Network Registered (CREG: 0,1 or 0,5)");
        break;
      } else if (creg.indexOf("+CREG: 0,3") != -1) {
        Serial.println("🚫 Registration denied (CREG: 0,3)");
        break;
      } else {
        Serial.println("📶 Searching for network (CREG not registered yet). Retrying...");
      }
    } else {
      Serial.println("⚠️ Unexpected CREG response. Retrying...");
    }
    delay(REG_RETRY_DELAY_MS);
  }

  // Analyze & human-readable advice
  analyzeNetworkStatus();

  // If everything is fine, send SMS
  if (commOK && simOK && voltageMv >= 3700 && csq > 9 && registered) {
    Serial.println("\n📤 Conditions met — sending SMS now...");
    bool ok = sendSMS(TARGET_NUMBER, "Hello from ESP32 + SIM800L — Diagnostic success!");
    if (ok) Serial.println("✅ SMS send confirmed by module");
    else      Serial.println("❌ SMS send failed (module returned ERROR or no response)");
  } else {
    Serial.println("\n⚠️ One or more checks failed. SMS not sent.");
    Serial.println("Summary:");
    Serial.print(" - Comm: "); Serial.println(commOK ? "OK" : "FAIL");
    Serial.print(" - SIM: ");  Serial.println(simOK  ? "OK" : "FAIL");
    Serial.print(" - Vbat: "); Serial.println(voltageMv > 0 ? String(voltageMv) + " mV" : "Unknown");
    Serial.print(" - CSQ: ");  Serial.println(csq);
    Serial.print(" - Registered: "); Serial.println(registered ? "YES" : "NO");
  }

  Serial.println("\n🔚 Setup complete. Module responses will continue to print below.");
}

void loop() {
  // Forward any SIM800 output to Serial monitor in realtime
  while (sim800.available()) {
    char c = (char)sim800.read();
    Serial.write(c);
  }
  // Nothing else in loop (non-blocking) — add tasks here if required
}

// ------------------ Helper Functions ------------------

String sendAndGet(const char* cmd, uint32_t timeoutMs) {
  // send command and read all chars for timeoutMs
  flushSim(); // clear any residual data
  sim800.println(cmd);
  return waitForAnyResponse(timeoutMs);
}

bool checkCommandContains(const char* cmd, const char* expected, uint32_t timeoutMs, const char* label) {
  String resp = sendAndGet(cmd, timeoutMs);
  bool ok = (resp.indexOf(expected) != -1);
  Serial.print(ok ? "✅ " : "❌ ");
  Serial.println(label);
  return ok;
}

String waitForAnyResponse(uint32_t timeoutMs) {
  uint32_t start = millis();
  String resp = "";
  while (millis() - start < timeoutMs) {
    while (sim800.available()) {
      resp += (char)sim800.read();
    }
  }
  lastResponse = resp;
  return resp;
}

bool waitForContains(const char* expected, uint32_t timeoutMs) {
  uint32_t start = millis();
  String resp = "";
  while (millis() - start < timeoutMs) {
    while (sim800.available()) resp += (char)sim800.read();
    if (resp.indexOf(expected) != -1) {
      lastResponse = resp;
      return true;
    }
  }
  lastResponse = resp;
  return false;
}

void flushSim() {
  // clear buffer from previous runs
  while (sim800.available()) sim800.read();
  lastResponse = "";
}

int parseCSQ(const String& resp) {
  // resp example: "\r\n+CSQ: 19,0\r\n\r\nOK\r\n"
  int idx = resp.indexOf("+CSQ:");
  if (idx == -1) return 99; // unknown
  int comma = resp.indexOf(',', idx);
  if (comma == -1) return 99;
  String s = resp.substring(idx + 6, comma); // 6 to skip "+CSQ: "
  s.trim();
  return s.toInt(); // 0..31, 99 unknown
}

int parseCBCVoltage(const String& resp) {
  // resp example: "\r\n+CBC: 0,69,3955\r\n\r\nOK\r\n" -> voltage in mV = 3955
  int idx = resp.indexOf("+CBC:");
  if (idx == -1) return -1;
  int lastComma = resp.lastIndexOf(',');
  if (lastComma == -1) return -1;
  String s = resp.substring(lastComma + 1);
  s.trim();
  return s.toInt();
}

bool sendSMS(const char* number, const char* message) {
  // returns true if module accepted and reported success (best-effort)
  flushSim();

  sim800.println("AT+CMGF=1"); // text mode
  if (!waitForContains("OK", 2000)) {
    Serial.println("⚠️ AT+CMGF failed or no response");
    return false;
  }

  // Start SMS
  sim800.print("AT+CMGS=\"");
  sim800.print(number);
  sim800.println("\"");

  // Wait for '>' prompt
  if (!waitForContains(">", 4000)) {
    Serial.println("⚠️ No '>' prompt — can't enter message (module returned:)");
    Serial.println(lastResponse);
    return false;
  }

  // Send message + Ctrl+Z
  sim800.print(message);
  sim800.write(26); // Ctrl+Z

  // Wait for CMGS or OK / or ERROR
  uint32_t start = millis();
  String resp = "";
  while (millis() - start < 10000) {
    while (sim800.available()) resp += (char)sim800.read();
    // check for success
    if (resp.indexOf("+CMGS:") != -1 || resp.indexOf("OK") != -1) {
      Serial.println(resp);
      lastResponse = resp;
      return true;
    }
    if (resp.indexOf("ERROR") != -1) {
      Serial.println(resp);
      lastResponse = resp;
      return false;
    }
  }
  Serial.println("⚠️ No definitive send result (timeout). Module response:");
  Serial.println(resp);
  lastResponse = resp;
  return false;
}

// Network analysis helper — prints human-readable diagnosis using lastResponse
void analyzeNetworkStatus() {
  Serial.println("\n🧠 Network Analysis:");

  // We look at lastResponse which after CREG/CSQ/CBC calls holds last data; get fresh checks too
  String csqResp = sendAndGet("AT+CSQ", 1200);
  String cregResp = sendAndGet("AT+CREG?", 1200);

  // Print raw
  Serial.println(csqResp);
  Serial.println(cregResp);

  if (csqResp.indexOf("+CSQ: 0,0") != -1) {
    Serial.println("⚠️ No Signal (CSQ: 0,0) — likely no 2G coverage or SIM is 4G-only (e.g., Jio).");
  } else {
    int csqVal = parseCSQ(csqResp);
    if (csqVal == 99) {
      Serial.println("⚠️ Signal unknown (CSQ: 99).");
    } else if (csqVal < 10) {
      Serial.println("⚠️ Weak signal (CSQ < 10). Try moving near a window or using external antenna.");
    } else {
      Serial.println("✅ Signal OK.");
    }
  }

  if (cregResp.indexOf("+CREG: 0,1") != -1 || cregResp.indexOf("+CREG: 0,5") != -1) {
    Serial.println("✅ Registered on network (CREG: 0,1 or 0,5).");
  } else if (cregResp.indexOf("+CREG: 0,2") != -1) {
    Serial.println("📶 Searching for network (CREG: 0,2).");
    Serial.println("→ If you're using Jio, note: SIM800L is 2G-only and Jio is 4G-only (won't register).");
  } else if (cregResp.indexOf("+CREG: 0,3") != -1) {
    Serial.println("🚫 Registration denied (CREG: 0,3) — SIM might be blocked or not allowed on network.");
  } else {
    Serial.println("🤔 Unknown registration state — check SIM, antenna, and power.");
  }
}
