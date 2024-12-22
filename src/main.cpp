#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Hotspot credentials
const char* ssid = "ESP32_Hotspot";
const char* password = "12345678";

// Create a WebServer object on port 80
WebServer server(80);

// Variables
int DRYRUN_TIMEOUT = 30;
float MOTOR_START_MIN_VOLTAGE_CUTOFF = 10.5;
float MOTOR_START_MAX_VOLTAGE_CUTOFF = 14.0;
float MOTOR_RUN_MIN_VOLTAGE_CUTOFF = 11.0;
int VOLTAGE_CUTOFF_RETRY_TIME = 60;
int STARTER_SWITCH_DURATION = 5;

// HTML Content
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Variable Editor</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f9; }
        .container { width: 90%; max-width: 600px; margin: 0 auto; padding: 20px; background: white; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        h1 { font-size: 22px; color: #333; }
        label { display: block; margin: 10px 0 5px; }
        input { width: 100%; padding: 8px; margin-bottom: 10px; border: 1px solid #ddd; border-radius: 4px; }
        button { padding: 10px 15px; background-color: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; }
        button:hover { background-color: #45a049; }
        #status { color: green; margin-top: 10px; font-size: 14px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Variable Editor</h1>
        <form id="variableForm">
            <label for="DRYRUN_TIMEOUT">DRYRUN_TIMEOUT (s):</label>
            <input type="number" id="DRYRUN_TIMEOUT" step="1">

            <label for="MOTOR_START_MIN_VOLTAGE_CUTOFF">Motor Start Min Voltage Cutoff (V):</label>
            <input type="number" id="MOTOR_START_MIN_VOLTAGE_CUTOFF" step="0.1">

            <label for="MOTOR_START_MAX_VOLTAGE_CUTOFF">Motor Start Max Voltage Cutoff (V):</label>
            <input type="number" id="MOTOR_START_MAX_VOLTAGE_CUTOFF" step="0.1">

            <label for="MOTOR_RUN_MIN_VOLTAGE_CUTOFF">Motor Run Min Voltage Cutoff (V):</label>
            <input type="number" id="MOTOR_RUN_MIN_VOLTAGE_CUTOFF" step="0.1">

            <label for="VOLTAGE_CUTOFF_RETRY_TIME">Voltage Cutoff Retry Time (s):</label>
            <input type="number" id="VOLTAGE_CUTOFF_RETRY_TIME" step="1">

            <label for="STARTER_SWITCH_DURATION">Starter Switch Duration (s):</label>
            <input type="number" id="STARTER_SWITCH_DURATION" step="1">

            <button type="button" onclick="saveData()">Save</button>
        </form>
        <p id="status"></p>
    </div>

    <script>
        function loadVariables() {
            fetch('/variables')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('DRYRUN_TIMEOUT').value = data.DRYRUN_TIMEOUT;
                    document.getElementById('MOTOR_START_MIN_VOLTAGE_CUTOFF').value = data.MOTOR_START_MIN_VOLTAGE_CUTOFF;
                    document.getElementById('MOTOR_START_MAX_VOLTAGE_CUTOFF').value = data.MOTOR_START_MAX_VOLTAGE_CUTOFF;
                    document.getElementById('MOTOR_RUN_MIN_VOLTAGE_CUTOFF').value = data.MOTOR_RUN_MIN_VOLTAGE_CUTOFF;
                    document.getElementById('VOLTAGE_CUTOFF_RETRY_TIME').value = data.VOLTAGE_CUTOFF_RETRY_TIME;
                    document.getElementById('STARTER_SWITCH_DURATION').value = data.STARTER_SWITCH_DURATION;
                });
        }

        function saveData() {
            const formData = {
                DRYRUN_TIMEOUT: parseInt(document.getElementById('DRYRUN_TIMEOUT').value),
                MOTOR_START_MIN_VOLTAGE_CUTOFF: parseFloat(document.getElementById('MOTOR_START_MIN_VOLTAGE_CUTOFF').value),
                MOTOR_START_MAX_VOLTAGE_CUTOFF: parseFloat(document.getElementById('MOTOR_START_MAX_VOLTAGE_CUTOFF').value),
                MOTOR_RUN_MIN_VOLTAGE_CUTOFF: parseFloat(document.getElementById('MOTOR_RUN_MIN_VOLTAGE_CUTOFF').value),
                VOLTAGE_CUTOFF_RETRY_TIME: parseInt(document.getElementById('VOLTAGE_CUTOFF_RETRY_TIME').value),
                STARTER_SWITCH_DURATION: parseInt(document.getElementById('STARTER_SWITCH_DURATION').value),
            };

            fetch('/save', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(formData)
            })
            .then(response => response.text())
            .then(message => {
                document.getElementById('status').textContent = message;
                loadVariables(); // Reload variables
            });
        }

        window.onload = loadVariables;
    </script>
</body>
</html>
)rawliteral";

void setup() {
  // Start Serial communication
  Serial.begin(115200);

  // Start WiFi as an Access Point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Serve the main webpage
  server.on("/", HTTP_GET, [](WebServer* server) {
    server->send_P(200, "text/html", htmlPage);
  });

  // Serve the current variable values in JSON format
  server.on("/variables", HTTP_GET, []() {
    DynamicJsonDocument doc(1024);
    doc["DRYRUN_TIMEOUT"] = DRYRUN_TIMEOUT;
    doc["MOTOR_START_MIN_VOLTAGE_CUTOFF"] = MOTOR_START_MIN_VOLTAGE_CUTOFF;
    doc["MOTOR_START_MAX_VOLTAGE_CUTOFF"] = MOTOR_START_MAX_VOLTAGE_CUTOFF;
    doc["MOTOR_RUN_MIN_VOLTAGE_CUTOFF"] = MOTOR_RUN_MIN_VOLTAGE_CUTOFF;
    doc["VOLTAGE_CUTOFF_RETRY_TIME"] = VOLTAGE_CUTOFF_RETRY_TIME;
    doc["STARTER_SWITCH_DURATION"] = STARTER_SWITCH_DURATION;

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });

  // Save updated variables
  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      String json = server.arg("plain");
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, json);

      DRYRUN_TIMEOUT = doc["DRYRUN_TIMEOUT"];
      MOTOR_START_MIN_VOLTAGE_CUTOFF = doc["MOTOR_START_MIN_VOLTAGE_CUTOFF"];
      MOTOR_START_MAX_VOLTAGE_CUTOFF = doc["MOTOR_START_MAX_VOLTAGE_CUTOFF"];
      MOTOR_RUN_MIN_VOLTAGE_CUTOFF = doc["MOTOR_RUN_MIN_VOLTAGE_CUTOFF"];
      VOLTAGE_CUTOFF_RETRY_TIME = doc["VOLTAGE_CUTOFF_RETRY_TIME"];
      STARTER_SWITCH_DURATION = doc["STARTER_SWITCH_DURATION"];

      server.send(200, "text/plain", "Variables saved successfully!");
    } else {
      server.send(400, "text/plain", "Invalid Request");
    }
  });

  // Start the server
  server.begin();
}

void loop() {
  server.handleClient();
}
