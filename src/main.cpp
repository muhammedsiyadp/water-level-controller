#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Replace these with your WiFi credentials
const char* ssid = "Your_SSID";
const char* password = "Your_PASSWORD";

WebServer server(80);

// Parameters for the water controller
int lowVoltage = 500;      // Low voltage threshold
int highVoltage = 800;     // High voltage threshold
int dryRunTimeout = 10;    // Dry-run timeout in seconds
int timerCutoff = 300;     // Maximum pump run time in seconds

// HTML content (stored in a raw literal for simplicity)
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Water Controller Settings</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 0;
      display: flex;
      flex-direction: column;
      align-items: center;
      background-color: #f4f4f9;
      color: #333;
    }
    .container {
      max-width: 600px;
      width: 90%;
      margin-top: 20px;
      padding: 20px;
      background: white;
      border-radius: 10px;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
    }
    h1 {
      text-align: center;
      color: #0078D7;
    }
    label {
      font-weight: bold;
      display: block;
      margin-top: 15px;
    }
    input[type="number"], button {
      width: 100%;
      padding: 10px;
      margin-top: 5px;
      border: 1px solid #ccc;
      border-radius: 5px;
      font-size: 16px;
    }
    button {
      background-color: #0078D7;
      color: white;
      border: none;
      cursor: pointer;
      margin-top: 20px;
    }
    button:hover {
      background-color: #005BB5;
    }
    .status {
      margin-top: 15px;
      text-align: center;
      color: green;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Water Controller Settings</h1>
    <label for="lowVoltage">Low Voltage Threshold</label>
    <input type="number" id="lowVoltage" placeholder="Enter Low Voltage" />

    <label for="highVoltage">High Voltage Threshold</label>
    <input type="number" id="highVoltage" placeholder="Enter High Voltage" />

    <label for="dryRunTime">Dry-Run Timeout (seconds)</label>
    <input type="number" id="dryRunTime" placeholder="Enter Dry-Run Timeout" />

    <label for="timerCutoff">Timer Cutoff (seconds)</label>
    <input type="number" id="timerCutoff" placeholder="Enter Timer Cutoff" />

    <button onclick="saveSettings()">Save Settings</button>
    <div id="status" class="status"></div>
  </div>

  <script>
    function saveSettings() {
      const lowVoltage = document.getElementById("lowVoltage").value;
      const highVoltage = document.getElementById("highVoltage").value;
      const dryRunTime = document.getElementById("dryRunTime").value;
      const timerCutoff = document.getElementById("timerCutoff").value;

      const xhr = new XMLHttpRequest();
      xhr.open("POST", "/save", true);
      xhr.setRequestHeader("Content-Type", "application/json;charset=UTF-8");
      xhr.onreadystatechange = function () {
        if (xhr.readyState === 4 && xhr.status === 200) {
          document.getElementById("status").textContent = "Settings Saved!";
        }
      };
      xhr.send(JSON.stringify({
        lowVoltage,
        highVoltage,
        dryRunTime,
        timerCutoff
      }));
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleSave() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(512);
    deserializeJson(doc, server.arg("plain"));
    lowVoltage = doc["lowVoltage"].as<int>();
    highVoltage = doc["highVoltage"].as<int>();
    dryRunTimeout = doc["dryRunTime"].as<int>();
    timerCutoff = doc["timerCutoff"].as<int>();

    // Log updated values to the Serial Monitor
    Serial.println("Updated Settings:");
    Serial.print("Low Voltage: "); Serial.println(lowVoltage);
    Serial.print("High Voltage: "); Serial.println(highVoltage);
    Serial.print("Dry-Run Timeout: "); Serial.println(dryRunTimeout);
    Serial.print("Timer Cutoff: "); Serial.println(timerCutoff);

    server.send(200, "application/json", "{\"status\": \"ok\"}");
  } else {
    server.send(400, "application/json", "{\"status\": \"error\"}");
  }
}

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Set up server routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);

  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient(); // Handle incoming client requests
}


