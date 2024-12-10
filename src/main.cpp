#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Define your hotspot credentials
const char* ssid = "ESP32_Hotspot";
const char* password = "12345678";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Variables to store data
String var1 = "Variable 1";
String var2 = "Variable 2";
String var3 = "Variable 3";
String var4 = "Variable 4";
String var5 = "Variable 5";
String var6 = "Variable 6";

// HTML content for the webpage
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Variable Editor</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            background: #f4f4f9;
        }
        .container {
            width: 90%;
            max-width: 400px;
            background: white;
            border-radius: 10px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2);
            padding: 20px;
        }
        h1 {
            text-align: center;
            font-size: 24px;
            margin-bottom: 20px;
        }
        label {
            display: block;
            font-size: 14px;
            margin-top: 10px;
        }
        input[type="text"] {
            width: 100%;
            padding: 10px;
            margin-top: 5px;
            border: 1px solid #ddd;
            border-radius: 5px;
        }
        button {
            width: 100%;
            padding: 10px;
            margin-top: 20px;
            background: #4CAF50;
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
        }
        button:hover {
            background: #45a049;
        }
        #status {
            text-align: center;
            margin-top: 10px;
            font-size: 14px;
            color: green;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Edit Variables</h1>
        <form id="variableForm">
            <label for="var1">Variable 1:</label>
            <input type="text" id="var1" value="%VAR1%">

            <label for="var2">Variable 2:</label>
            <input type="text" id="var2" value="%VAR2%">

            <label for="var3">Variable 3:</label>
            <input type="text" id="var3" value="%VAR3%">

            <label for="var4">Variable 4:</label>
            <input type="text" id="var4" value="%VAR4%">

            <label for="var5">Variable 5:</label>
            <input type="text" id="var5" value="%VAR5%">

            <label for="var6">Variable 6:</label>
            <input type="text" id="var6" value="%VAR6%">

            <button type="button" onclick="saveData()">Save</button>
        </form>
        <p id="status"></p>
    </div>
    <script>
        function saveData() {
            const formData = new FormData(document.getElementById('variableForm'));
            const data = {};
            formData.forEach((value, key) => data[key] = value);

            fetch('/save', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            })
            .then(response => response.text())
            .then(message => {
                document.getElementById('status').textContent = message;
            });
        }
    </script>
</body>
</html>
)rawliteral";

void setup() {
  // Start Serial communication
  Serial.begin(115200);

  // Configure ESP32 as an Access Point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Serve the webpage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = htmlPage;
    html.replace("%VAR1%", var1);
    html.replace("%VAR2%", var2);
    html.replace("%VAR3%", var3);
    html.replace("%VAR4%", var4);
    html.replace("%VAR5%", var5);
    html.replace("%VAR6%", var6);
    request->send(200, "text/html", html);
  });

  // Save the variables
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String json = String((char*)data);
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, json);

    var1 = doc["var1"].as<String>();
    var2 = doc["var2"].as<String>();
    var3 = doc["var3"].as<String>();
    var4 = doc["var4"].as<String>();
    var5 = doc["var5"].as<String>();
    var6 = doc["var6"].as<String>();

    request->send(200, "text/plain", "Variables saved successfully!");
  });

  // Start the server
  server.begin();
}

void loop() {
  // Nothing to do here
}
