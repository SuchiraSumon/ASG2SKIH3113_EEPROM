#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>

// Define the EEPROM addresses for each parameter
const int WIFI_SSID_ADDR = 0;
const int WIFI_PASS_ADDR = 32;
const int DEVICE_ID_ADDR = 64;
const int LED_STATUS_ADDR = 96;

// Define the maximum size for each parameter
const int MAX_SSID_SIZE = 32;
const int MAX_PASS_SIZE = 32;
const int MAX_DEVICE_ID_SIZE = 32;

// Define the default values for the parameters
const String DEFAULT_SSID = "default_ssid";
const String DEFAULT_PASS = "default_pass";
const String DEFAULT_DEVICE_ID = "default_device_id";
const bool DEFAULT_LED_STATUS = false; // LED is OFF by default

// Server name
const char* serverName = "chira_ssgkm";

// Access Point details
const char* ssidAP = serverName;
const char* passwordAP = "";

// Create an instance of the server
ESP8266WebServer server(80);

// Variables to store the current configuration
String ssid, pass, deviceID;
bool ledStatus;

// LED pin configuration
const int LED_PIN = 14; // GPIO D5 on ESP8266

// Wi-Fi connection timeout in milliseconds
const int WIFI_TIMEOUT = 15000; // 15 seconds

// ----- Part 1: Setup -----

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  pinMode(LED_PIN, OUTPUT);

  // Read the configuration from EEPROM
  ssid = readStringFromEEPROM(WIFI_SSID_ADDR, MAX_SSID_SIZE);
  pass = readStringFromEEPROM(WIFI_PASS_ADDR, MAX_PASS_SIZE);
  deviceID = readStringFromEEPROM(DEVICE_ID_ADDR, MAX_DEVICE_ID_SIZE);
  ledStatus = EEPROM.read(LED_STATUS_ADDR) == 1 ? true : false; // 1 means LED is ON

  // Always start in AP mode
  startAPMode();
}

// ----- Part 2: Main Loop -----

void loop() {
  server.handleClient();

  // Check if in AP mode and toggle LED after a second
  if (WiFi.getMode() == WIFI_AP) {
    static unsigned long previousMillis = 0;
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 1000) {
      previousMillis = currentMillis;
      ledStatus = !ledStatus; // Toggle LED status
      applyLEDStatus(ledStatus);
    }
  }
}

// ----- Part 3: Access Point Mode -----

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssidAP, passwordAP);
  Serial.print("AP Mode. Connect to '");
  Serial.print(ssidAP);
  Serial.println("' and visit http://192.168.4.1 to configure");
  createWebServer();
  server.begin();
}

void createWebServer() {
  // Serve the main page
  server.on("/", [] {
    server.send(200, "text/html", getMainPage());
  });

  // Handle the form submission
  server.on("/submit", [] {
    ssid = server.arg("ssid");
    pass = server.arg("password");
    deviceID = server.arg("deviceid");
    ledStatus = server.arg("ledstatus") == "on" ? true : false;

    // Save the new configuration to EEPROM
    writeStringToEEPROM(WIFI_SSID_ADDR, ssid, MAX_SSID_SIZE);
    writeStringToEEPROM(WIFI_PASS_ADDR, pass, MAX_PASS_SIZE);
    writeStringToEEPROM(DEVICE_ID_ADDR, deviceID, MAX_DEVICE_ID_SIZE);
    EEPROM.write(LED_STATUS_ADDR, ledStatus ? 1 : 0); // 1 means LED is ON
    EEPROM.commit();

    // Try to connect to the Wi-Fi network
    connectToWiFi();

    // Apply the LED status and restart
    applyLEDStatus(ledStatus);
    server.send(200, "text/html", "<p>Configuration saved. The device will now restart.</p>");
    delay(2000);
    ESP.restart();
  });
}

// ----- Part 4: Web Interface -----

String getMainPage() {
  String page = "<html><head><title>ESP8266 Configuration</title></head><body>";
  page += "<h1>ESP8266 Configuration</h1>";
  page += "<form action='/submit' method='POST'>";
  page += "SSID: <input type='text' name='ssid' value='" + ssid + "'><br>";
  page += "Password: <input type='password' name='password' value='" + pass + "'><br>";
  page += "Device ID: <input type='text' name='deviceid' value='" + deviceID + "'><br>";
  page += "LED Status: <select name='ledstatus'>";
  page += "<option value='off'" + String(ledStatus ? "" : " selected") + ">Off</option>";
  page += "<option value='on'" + String(ledStatus ? " selected" : "") + ">On</option>";
  page += "</select><br>";
  page += "<input type='submit' value='Save'>";
  page += "</form></body></html>";
  return page;
}

// ----- Part 5: Helper Functions -----

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Connecting to Wi-Fi");
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
    applyLEDStatus(true); // Turn on the LED when connected
  } else {
    Serial.println("\nFailed to connect within the timeout period, remaining in AP mode");
    startAPMode(); // Stay in AP mode if failed to connect
  }
}

void applyLEDStatus(bool status) {
  // Invert the status because LOW turns the LED on and HIGH turns it off
  digitalWrite(LED_PIN, status ? LOW : HIGH);
}

String readStringFromEEPROM(int startAddr, int maxSize) {
  String result;
  for (int i = startAddr; i < startAddr + maxSize; i++) {
    char c = EEPROM.read(i);
    if (c == 0 || c == 255) {
      break;
    }
    result += c;
  }
  return result;
}

void writeStringToEEPROM(int startAddr, String stringToWrite, int maxSize) {
  for (int i = 0; i < maxSize; i++) {
    if (i < stringToWrite.length()) {
      EEPROM.write(startAddr + i, stringToWrite[i]);
    } else {
      // Write null character to remaining positions
      EEPROM.write(startAddr + i, 0);
    }
  }
}
