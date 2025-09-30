#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// ===== PIN CONFIGURATION =====
const int RELAY_PINS[4] = {4, 5, 18, 19}; // GPIO untuk 4 relay

// ===== WiFi Configuration =====
const char* ap_ssid = "Alarm_Timer_Setup";
const char* ap_password = "12345678";

// ===== WEB SERVER =====
WebServer server(80);
Preferences preferences;

// ===== TIMER STRUCTURE =====
struct Timer {
  bool running;
  unsigned long startTime;
  unsigned long duration;
  unsigned long remainingTime;
  bool warningTriggered;
  bool alarmTriggered;
  String status;
};

Timer timers[4];

// ===== CONFIGURATION =====
struct Config {
  int testDuration;
  int warningDuration;
  int timeupDuration;
  int warningTime;
  int repeatCount;
  int repeatInterval;
  bool enableWarning;
  bool enableRepeating;
} config;

// ===== ALARM STATE =====
struct AlarmState {
  bool active;
  unsigned long startTime;
  int duration;
  int repeatCount;
  int currentRepeat;
  unsigned long lastRepeatTime;
} alarmStates[4];

// ===== WiFi Credentials =====
String saved_ssid = "";
String saved_password = "";
bool wifi_connected = false;

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== Futsal Timer System v2.0 ===");

  // Initialize relay pins
  for (int i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW); // Relay OFF (active HIGH)
  }
  Serial.println("Relay pins initialized");

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  // Initialize Preferences
  preferences.begin("futsal-timer", false);
  
  // Load configuration
  loadConfiguration();
  loadWiFiCredentials();

  // Initialize timers
  for (int i = 0; i < 4; i++) {
    timers[i].running = false;
    timers[i].startTime = 0;
    timers[i].duration = 0;
    timers[i].remainingTime = 0;
    timers[i].warningTriggered = false;
    timers[i].alarmTriggered = false;
    timers[i].status = "Idle";
    
    alarmStates[i].active = false;
    alarmStates[i].startTime = 0;
    alarmStates[i].duration = 0;
    alarmStates[i].repeatCount = 0;
    alarmStates[i].currentRepeat = 0;
    alarmStates[i].lastRepeatTime = 0;
  }

  // Try to connect to saved WiFi
  if (saved_ssid.length() > 0) {
    Serial.println("Attempting to connect to saved WiFi: " + saved_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      Serial.println("\nWiFi Connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to connect to saved WiFi");
      wifi_connected = false;
    }
  }

  // If not connected, start AP mode
  if (!wifi_connected) {
    Serial.println("Starting Access Point mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
  }

  // Setup web server routes
  setupServerRoutes();
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("================================\n");
}

// ===== LOOP =====
void loop() {
  server.handleClient();
  updateTimers();
  updateAlarms();
  delay(10);
}

// ===== SERVER ROUTES SETUP =====
void setupServerRoutes() {
  // Serve HTML page
  server.on("/", HTTP_GET, handleRoot);
  
  // API endpoints
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/alarm", HTTP_GET, handleAlarm);
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/save-config", HTTP_POST, handleSaveConfig);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/wifi-scan", HTTP_GET, handleWiFiScan);
  server.on("/wifi-save", HTTP_POST, handleWiFiSave);
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });
}

// ===== HANDLER: Root (HTML Page) =====
void handleRoot() {
  File file = LittleFS.open("/gabungan2.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to open HTML file");
    Serial.println("ERROR: Failed to open /gabungan2.html");
    return;
  }
  
  server.streamFile(file, "text/html");
  file.close();
}

// ===== HANDLER: Status =====
void handleStatus() {
  StaticJsonDocument<1024> doc;
  
  for (int i = 0; i < 4; i++) {
    JsonObject lap = doc.createNestedObject("lapangan" + String(i + 1));
    lap["status"] = timers[i].status;
    lap["timeRemaining"] = formatTime(timers[i].remainingTime);
    
    String statusClass = "idle";
    if (timers[i].running) {
      if (timers[i].remainingTime <= config.warningTime && timers[i].remainingTime > 0) {
        statusClass = "warning";
      } else if (timers[i].remainingTime <= 0) {
        statusClass = "finished";
      } else {
        statusClass = "running";
      }
    }
    lap["statusClass"] = statusClass;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ===== HANDLER: Start Timer =====
void handleStart() {
  if (!server.hasArg("lap") || !server.hasArg("durasi")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  
  int lap = server.arg("lap").toInt();
  int durasi = server.arg("durasi").toInt();
  
  if (lap < 0 || lap >= 4) {
    server.send(400, "text/plain", "Invalid lapangan");
    return;
  }
  
  timers[lap].running = true;
  timers[lap].startTime = millis();
  timers[lap].duration = durasi * 1000UL; // Convert to milliseconds
  timers[lap].remainingTime = durasi;
  timers[lap].warningTriggered = false;
  timers[lap].alarmTriggered = false;
  timers[lap].status = "Running";
  
  Serial.println("Field " + String(lap + 1) + " started - Duration: " + String(durasi) + " seconds");
  
  server.send(200, "text/plain", "Timer started");
}

// ===== HANDLER: Stop Timer =====
void handleStop() {
  if (!server.hasArg("lap")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  
  int lap = server.arg("lap").toInt();
  
  if (lap < 0 || lap >= 4) {
    server.send(400, "text/plain", "Invalid lapangan");
    return;
  }
  
  timers[lap].running = false;
  timers[lap].status = "Idle";
  timers[lap].remainingTime = 0;
  deactivateRelay(lap);
  
  Serial.println("Field " + String(lap + 1) + " stopped");
  
  server.send(200, "text/plain", "Timer stopped");
}

// ===== HANDLER: Test Alarm =====
void handleAlarm() {
  if (!server.hasArg("lap")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  
  int lap = server.arg("lap").toInt();
  
  if (lap < 0 || lap >= 4) {
    server.send(400, "text/plain", "Invalid lapangan");
    return;
  }
  
  // Activate test alarm
  alarmStates[lap].active = true;
  alarmStates[lap].startTime = millis();
  alarmStates[lap].duration = config.testDuration * 1000;
  alarmStates[lap].repeatCount = config.enableRepeating ? config.repeatCount : 1;
  alarmStates[lap].currentRepeat = 0;
  alarmStates[lap].lastRepeatTime = millis();
  
  activateRelay(lap);
  
  Serial.println("Test alarm Field " + String(lap + 1));
  
  server.send(200, "text/plain", "Test alarm activated");
}

// ===== HANDLER: Get Config =====
void handleConfig() {
  StaticJsonDocument<512> doc;
  
  doc["testDuration"] = config.testDuration;
  doc["warningDuration"] = config.warningDuration;
  doc["timeupDuration"] = config.timeupDuration;
  doc["warningTime"] = config.warningTime;
  doc["repeatCount"] = config.repeatCount;
  doc["repeatInterval"] = config.repeatInterval;
  doc["enableWarning"] = config.enableWarning;
  doc["enableRepeating"] = config.enableRepeating;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ===== HANDLER: Save Config =====
void handleSaveConfig() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    StaticJsonDocument<512> doc;
    
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }
    
    config.testDuration = doc["testDuration"] | 3;
    config.warningDuration = doc["warningDuration"] | 2;
    config.timeupDuration = doc["timeupDuration"] | 8;
    config.warningTime = doc["warningTime"] | 300;
    config.repeatCount = doc["repeatCount"] | 3;
    config.repeatInterval = doc["repeatInterval"] | 2;
    config.enableWarning = doc["enableWarning"] | true;
    config.enableRepeating = doc["enableRepeating"] | true;
    
    saveConfiguration();
    
    Serial.println("Configuration saved");
    server.send(200, "text/plain", "Configuration saved");
  } else {
    server.send(400, "text/plain", "No data received");
  }
}

// ===== HANDLER: System Info =====
void handleInfo() {
  StaticJsonDocument<256> doc;
  
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["totalHeap"] = ESP.getHeapSize();
  doc["connectedClients"] = WiFi.softAPgetStationNum();
  doc["uptime"] = millis() / 1000;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ===== HANDLER: WiFi Scan =====
void handleWiFiScan() {
  Serial.println("Scanning WiFi networks...");
  int n = WiFi.scanNetworks();
  
  StaticJsonDocument<2048> doc;
  JsonArray networks = doc.to<JsonArray>();
  
  for (int i = 0; i < n; i++) {
    networks.add(WiFi.SSID(i));
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  Serial.println("Found " + String(n) + " networks");
}

// ===== HANDLER: WiFi Save =====
void handleWiFiSave() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    saveWiFiCredentials(ssid, password);
    
    server.send(200, "text/plain", "WiFi credentials saved. Restarting...");
    
    Serial.println("WiFi credentials saved: " + ssid);
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

// ===== UPDATE TIMERS =====
void updateTimers() {
  unsigned long currentMillis = millis();
  
  for (int i = 0; i < 4; i++) {
    if (!timers[i].running) continue;
    
    unsigned long elapsed = currentMillis - timers[i].startTime;
    long remaining = (timers[i].duration - elapsed) / 1000;
    
    if (remaining < 0) remaining = 0;
    
    timers[i].remainingTime = remaining;
    
    // Check for warning alarm
    if (config.enableWarning && !timers[i].warningTriggered && 
        remaining <= config.warningTime && remaining > 0) {
      timers[i].warningTriggered = true;
      timers[i].status = "Warning";
      triggerAlarm(i, config.warningDuration, "warning");
      Serial.println("Warning alarm Field " + String(i + 1));
    }
    
    // Check for time up alarm
    if (!timers[i].alarmTriggered && remaining <= 0) {
      timers[i].alarmTriggered = true;
      timers[i].status = "Finished";
      timers[i].running = false;
      triggerAlarm(i, config.timeupDuration, "timeup");
      Serial.println("Time up alarm Field " + String(i + 1));
    }
    
    // Update status
    if (timers[i].running) {
      if (remaining <= config.warningTime && remaining > 0) {
        timers[i].status = "Warning";
      } else if (remaining > 0) {
        timers[i].status = "Running";
      }
    }
  }
}

// ===== UPDATE ALARMS =====
void updateAlarms() {
  unsigned long currentMillis = millis();
  
  for (int i = 0; i < 4; i++) {
    if (!alarmStates[i].active) continue;
    
    unsigned long elapsed = currentMillis - alarmStates[i].lastRepeatTime;
    
    // Check if current alarm duration is over
    if (elapsed >= alarmStates[i].duration) {
      deactivateRelay(i);
      alarmStates[i].currentRepeat++;
      
      // Check if we need to repeat
      if (alarmStates[i].currentRepeat < alarmStates[i].repeatCount) {
        // Wait for interval before next repeat
        if (elapsed >= (alarmStates[i].duration + config.repeatInterval * 1000UL)) {
          activateRelay(i);
          alarmStates[i].lastRepeatTime = currentMillis;
        }
      } else {
        // All repeats done
        alarmStates[i].active = false;
        alarmStates[i].currentRepeat = 0;
        deactivateRelay(i);
      }
    }
  }
}

// ===== TRIGGER ALARM =====
void triggerAlarm(int index, int durationSeconds, String type) {
  alarmStates[index].active = true;
  alarmStates[index].startTime = millis();
  alarmStates[index].duration = durationSeconds * 1000UL;
  alarmStates[index].repeatCount = config.enableRepeating ? config.repeatCount : 1;
  alarmStates[index].currentRepeat = 0;
  alarmStates[index].lastRepeatTime = millis();
  
  activateRelay(index);
}

// ===== RELAY CONTROL =====
void activateRelay(int index) {
  if (index >= 0 && index < 4) {
    digitalWrite(RELAY_PINS[index], HIGH);
    Serial.println("Relay " + String(index + 1) + " ON");
  }
}

void deactivateRelay(int index) {
  if (index >= 0 && index < 4) {
    digitalWrite(RELAY_PINS[index], LOW);
    Serial.println("Relay " + String(index + 1) + " OFF");
  }
}

// ===== LOAD CONFIGURATION =====
void loadConfiguration() {
  config.testDuration = preferences.getInt("testDur", 3);
  config.warningDuration = preferences.getInt("warnDur", 2);
  config.timeupDuration = preferences.getInt("timeupDur", 8);
  config.warningTime = preferences.getInt("warnTime", 300);
  config.repeatCount = preferences.getInt("repeatCnt", 3);
  config.repeatInterval = preferences.getInt("repeatInt", 2);
  config.enableWarning = preferences.getBool("enableWarn", true);
  config.enableRepeating = preferences.getBool("enableRep", true);
  
  Serial.println("Configuration loaded");
}

// ===== SAVE CONFIGURATION =====
void saveConfiguration() {
  preferences.putInt("testDur", config.testDuration);
  preferences.putInt("warnDur", config.warningDuration);
  preferences.putInt("timeupDur", config.timeupDuration);
  preferences.putInt("warnTime", config.warningTime);
  preferences.putInt("repeatCnt", config.repeatCount);
  preferences.putInt("repeatInt", config.repeatInterval);
  preferences.putBool("enableWarn", config.enableWarning);
  preferences.putBool("enableRep", config.enableRepeating);
  
  Serial.println("Configuration saved to preferences");
}

// ===== LOAD WiFi CREDENTIALS =====
void loadWiFiCredentials() {
  saved_ssid = preferences.getString("wifi_ssid", "");
  saved_password = preferences.getString("wifi_pass", "");
  
  if (saved_ssid.length() > 0) {
    Serial.println("WiFi credentials loaded: " + saved_ssid);
  } else {
    Serial.println("No saved WiFi credentials");
  }
}

// ===== SAVE WiFi CREDENTIALS =====
void saveWiFiCredentials(String ssid, String password) {
  preferences.putString("wifi_ssid", ssid);
  preferences.putString("wifi_pass", password);
  saved_ssid = ssid;
  saved_password = password;
  
  Serial.println("WiFi credentials saved: " + ssid);
}

// ===== FORMAT TIME =====
String formatTime(unsigned long seconds) {
  int hours = seconds / 3600;
  int minutes = (seconds % 3600) / 60;
  int secs = seconds % 60;
  
  char buffer[10];
  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, secs);
  return String(buffer);
}
