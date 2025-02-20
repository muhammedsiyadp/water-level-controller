#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
// Pinout
#define LED_MOTOR_ON_PIN        16
#define LED_DRY_RUN_CUTOFF_PIN  14
#define LED_LOW_HIGH_CUTOFF_PIN 2

#define RELAY_MOTOR_PIN         13
#define RELAY_STARTER_PIN       10

#define WATERLEVEL_LOW_PIN      5
#define WATERLEVEL_FULL_PIN     4
#define WATERLEVEL_DRYRUN_PIN   12

#define MANUAL_START_BUTTON_PIN  9
//#define SETUP_BUTTON_PIN         10

#define VOLTAGE_SENSOR_PIN      A0

#define EEPROM_SIZE 64


// Constants give the default values here
int DRYRUN_TIMOUT = 20; // seconds
int MOTOR_START_MIN_VOLTAGE_CUTOFF = 185;
int MOTOR_START_MAX_VOLTAGE_CUTOFF = 260;
int MOTOR_RUN_MIN_VOLTAGE_CUTOFF = 165;
int VOLTAGE_CUTOFF_RETRY_TIME = 600; // seconds
int STARTER_SWITCH_DURATION = 5; // seconds
float VOLTAGE_CALIBRATION = 2.1;
int SAFETY_TIMEOUT = 30; // minutes
int WATER_FULL_DETECTED_BUFFER = 10000; // milliseconds
int VOLTAGE_ABNORMALITY_BUFFER = 10000; // milliseconds
int DRYRUN_DETECTION_BUFFER = 120000; // milliseconds // 2 minutes

// Variables
volatile bool manual_start_button_pressed = false;
bool motor_running = false;
int motor_running_status = 0; // 0 for not running, 1 for just started, 2 running phase
unsigned long motor_starting_millis;
unsigned long voltage_cutoff_millis;
bool dryrun_cutoff_status = false;
bool safety_timeout_status = false;
bool voltage_cutoff_status = false;
bool starter_status = false;
float live_voltage = 0;
float live_voltage_raw = 0;
bool water_low = false;  // global status of water level in low pin
bool water_full = false;
bool motor_dry_run = false;
bool water_full_detected = false;
unsigned long water_full_detected_millis;
bool voltage_abnormality_detected = false;
unsigned long voltage_abnormality_detected_millis;
bool motor_dry_run_detected = false;
unsigned long motor_dry_run_detected_millis;


bool setup_mode = false;

const char* ssid = "Lucent water level controller";
const char* password = "12345678";

ESP8266WebServer server(80);

void saveToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, DRYRUN_TIMOUT);
  EEPROM.put(4, MOTOR_START_MIN_VOLTAGE_CUTOFF);
  EEPROM.put(8, MOTOR_START_MAX_VOLTAGE_CUTOFF);
  EEPROM.put(12, MOTOR_RUN_MIN_VOLTAGE_CUTOFF);
  EEPROM.put(16, VOLTAGE_CUTOFF_RETRY_TIME);
  EEPROM.put(20, STARTER_SWITCH_DURATION);
  EEPROM.put(24, VOLTAGE_CALIBRATION);
  EEPROM.put(28, SAFETY_TIMEOUT);
  EEPROM.put(32, 7); // Reserved for future use
  EEPROM.commit();
}

// Helper function to load variables from EEPROM
void loadFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(32) != 7) {
    saveToEEPROM();
  }
  EEPROM.get(0, DRYRUN_TIMOUT);
  EEPROM.get(4, MOTOR_START_MIN_VOLTAGE_CUTOFF);
  EEPROM.get(8, MOTOR_START_MAX_VOLTAGE_CUTOFF);
  EEPROM.get(12, MOTOR_RUN_MIN_VOLTAGE_CUTOFF);
  EEPROM.get(16, VOLTAGE_CUTOFF_RETRY_TIME);
  EEPROM.get(20, STARTER_SWITCH_DURATION);
  EEPROM.get(24, VOLTAGE_CALIBRATION);
  EEPROM.get(28, SAFETY_TIMEOUT);
}

// Generate HTML page
String generateHTML() {
  String html = "<html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f9; color: #333; }";
  html += "header { background: #6200ea; color: #fff; padding: 1rem 0; text-align: center; }";
  html += "h1, h2 { margin: 0.5rem; }";
  html += "form { max-width: 600px; margin: 2rem auto; padding: 1rem; background: #fff; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
  html += "input[type='number'], input[type='text'], input[type='submit'], button { width: calc(100% - 22px); margin: 0.5rem 0; padding: 0.5rem; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }";
  html += "input[type='submit'], button { background: #6200ea; color: #fff; border: none; cursor: pointer; }";
  html += "input[type='submit']:hover, button:hover { background: #4500b0; }";
  html += "footer { text-align: center; margin: 1rem; font-size: 0.9rem; color: #666; }";
  html += "</style></head><body>";
  html += "<header><h1>Smart Water Level Controller</h1><h2>by Lucent Technologies</h2></header>";
  html += "<h2 style='text-align:center;'>Voltage: <span id='timer'>" + String(live_voltage_raw/VOLTAGE_CALIBRATION) + "</span> seconds</h2>";
  html += "<h3 style='text-align:center;'>Status:</h3>";
  html += "<ul style='list-style-type: none; text-align: center; padding: 0;'>";
  html += "<li>Water Level Low: " + String(water_low ? "Yes" : "No") + "</li>";
  html += "<li>Water Level High: " + String(water_full ? "Yes" : "No") + "</li>";
  html += "<li>Pump Dry Run: " + String(motor_dry_run ? "Yes" : "No") + "</li>";
  html += "</ul>";
  html += "<form method='POST' action='/save'>";
  html += "<label>DRYRUN_TIMEOUT (seconds):</label><input type='number' name='DRYRUN_TIMOUT' value='" + String(DRYRUN_TIMOUT) + "'><br>";
  html += "<label>MOTOR_START_MIN_VOLTAGE_CUTOFF:</label><input type='number' name='MOTOR_START_MIN_VOLTAGE_CUTOFF' value='" + String(MOTOR_START_MIN_VOLTAGE_CUTOFF) + "'><br>";
  html += "<label>SAFETY_TIMEOUT (minutes):</label><input type='number' name='SAFETY_TIMEOUT' value='" + String(SAFETY_TIMEOUT) + "'><br>";
  html += "<label>MOTOR_START_MAX_VOLTAGE_CUTOFF:</label><input type='number' name='MOTOR_START_MAX_VOLTAGE_CUTOFF' value='" + String(MOTOR_START_MAX_VOLTAGE_CUTOFF) + "'><br>";
  html += "<label>MOTOR_RUN_MIN_VOLTAGE_CUTOFF:</label><input type='number' name='MOTOR_RUN_MIN_VOLTAGE_CUTOFF' value='" + String(MOTOR_RUN_MIN_VOLTAGE_CUTOFF) + "'><br>";
  html += "<label>VOLTAGE_CUTOFF_RETRY_TIME (seconds):</label><input type='number' name='VOLTAGE_CUTOFF_RETRY_TIME' value='" + String(VOLTAGE_CUTOFF_RETRY_TIME) + "'><br>";
  html += "<label>STARTER_SWITCH_DURATION (seconds):</label><input type='number' name='STARTER_SWITCH_DURATION' value='" + String(STARTER_SWITCH_DURATION) + "'><br>";
  html += "<label>VOLTAGE_CALIBRATION:</label><input type='text' name='VOLTAGE_CALIBRATION' id='timer1' value='0'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "<button onclick='exitAndRestart()'>Exit</button>";
  html += "<script>";
  html += "function exitAndRestart() {";
  html += "  fetch('/exit').then(() => {";
  html += "    window.close();";
  html += "  });";
  html += "}";
  html += "setInterval(() => {";
  html += "  fetch('/timer').then(response => response.text()).then(data => {";
  html += "    document.getElementById('timer').innerText = data;";
  html += "  });";
  html += "}, 1000);";
  html += "</script>";
  html += "<footer>&copy; 2024 Lucent Technologies</footer>";
  html += "</body></html>";
  return html;
}
void update_pin_statuses_and_voltage(float update_delay_pin = 0.4,float update_delay_voltage = 0.05) {
  static unsigned long last_update_millis_pins = 0;
  static unsigned long last_update_millis_voltage = 0;
  static bool water_low_buffer[5] = {false};
  static bool water_full_buffer[5] = {false};
  static bool motor_dry_run_buffer[5] = {false};
  static float voltage_buffer[5] = {0};
  static int buffer_index_pins = 0;
  static int buffer_index_voltage = 0;
  if (millis() - last_update_millis_pins >= update_delay_pin * 1000) {
    last_update_millis_pins = millis();

    // Update buffers
    water_low_buffer[buffer_index_pins] = !digitalRead(WATERLEVEL_LOW_PIN);
    water_full_buffer[buffer_index_pins] = !digitalRead(WATERLEVEL_FULL_PIN);
    motor_dry_run_buffer[buffer_index_pins] = !digitalRead(WATERLEVEL_DRYRUN_PIN);
    

    // Increment buffer index
    buffer_index_pins = (buffer_index_pins + 1) % 5;

    // Check occurrences in buffers
    int water_low_count = 0;
    int water_full_count = 0;
    int motor_dry_run_count = 0;

    for (int i = 0; i < 5; i++) {
      if (water_low_buffer[i]) water_low_count++;
      if (water_full_buffer[i]) water_full_count++;
      if (motor_dry_run_buffer[i]) motor_dry_run_count++;
    }

    water_low = (water_low_count >= 1);
    water_full = (water_full_count >= 1);
    motor_dry_run = (motor_dry_run_count >= 1);
  }
  if (millis() - last_update_millis_voltage >= update_delay_voltage * 1000) {
    last_update_millis_voltage = millis();

    // Update buffers
    voltage_buffer[buffer_index_voltage] = analogRead(VOLTAGE_SENSOR_PIN);

    // Increment buffer index
    buffer_index_voltage = (buffer_index_voltage + 1) % 5;

    // Check occurrences in buffers
    float voltage_sum = 0;

    for (int i = 0; i < 5; i++) {
      voltage_sum += voltage_buffer[i];
    }
    live_voltage_raw = voltage_sum / 5;
    if (VOLTAGE_CALIBRATION != 0) {
      live_voltage = live_voltage_raw / VOLTAGE_CALIBRATION;
    }
    else {
      live_voltage = live_voltage_raw;
    }

  }
}
// Handle root page
void handleRoot() {
  setup_mode = true;
  //Serial.println("Root page requested");
  server.send(200, "text/html", generateHTML());
}
void handleExit() {
  server.send(200, "text/plain", "Restarting...");
  delay(1000);
  ESP.restart();
}
// Handle saving variables
void handleSave() {
  if (server.hasArg("DRYRUN_TIMOUT")) DRYRUN_TIMOUT = server.arg("DRYRUN_TIMOUT").toInt();
  if (server.hasArg("MOTOR_START_MIN_VOLTAGE_CUTOFF")) MOTOR_START_MIN_VOLTAGE_CUTOFF = server.arg("MOTOR_START_MIN_VOLTAGE_CUTOFF").toInt();
  if (server.hasArg("MOTOR_START_MAX_VOLTAGE_CUTOFF")) MOTOR_START_MAX_VOLTAGE_CUTOFF = server.arg("MOTOR_START_MAX_VOLTAGE_CUTOFF").toInt();
  if (server.hasArg("MOTOR_RUN_MIN_VOLTAGE_CUTOFF")) MOTOR_RUN_MIN_VOLTAGE_CUTOFF = server.arg("MOTOR_RUN_MIN_VOLTAGE_CUTOFF").toInt();
  if (server.hasArg("VOLTAGE_CUTOFF_RETRY_TIME")) VOLTAGE_CUTOFF_RETRY_TIME = server.arg("VOLTAGE_CUTOFF_RETRY_TIME").toInt();
  if (server.hasArg("STARTER_SWITCH_DURATION")) STARTER_SWITCH_DURATION = server.arg("STARTER_SWITCH_DURATION").toInt();
  if (server.hasArg("VOLTAGE_CALIBRATION")) 
  {
    if (server.arg("VOLTAGE_CALIBRATION").toFloat() != 0){
      VOLTAGE_CALIBRATION = live_voltage_raw / server.arg("VOLTAGE_CALIBRATION").toFloat();
    }
  }
  if (server.hasArg("SAFETY_TIMEOUT")) SAFETY_TIMEOUT = server.arg("SAFETY_TIMEOUT").toInt();
  

  saveToEEPROM();
  server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Settings Saved</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; padding: 20px; }
        h1 { margin-bottom: 20px; }
        a { display: inline-block; padding: 10px 15px; text-decoration: none; color: white; background-color: #6200ea; border-radius: 4px; }
        a:hover { background-color: #6200ea; }
    </style>
</head>
<body>
    <h1>Settings Saved!</h1>
    <a href="/">Go Back</a>
</body>
</html>
)rawliteral");
}
void handleTimer() {
  server.send(200, "text/plain", String(live_voltage_raw/VOLTAGE_CALIBRATION));
}
void IRAM_ATTR manual_start_button_isr() {
  manual_start_button_pressed = true;
  digitalWrite(LED_MOTOR_ON_PIN, HIGH);
}
void booting(int duration_sec = 20) { //duration to wait for system to start. during this time all leds will be blinking to indicate system is booting
  for (int i = 0; i < duration_sec *2; i++){
    digitalWrite(LED_MOTOR_ON_PIN, !(i % 2));
    digitalWrite(LED_DRY_RUN_CUTOFF_PIN, !(i % 2));
    digitalWrite(LED_LOW_HIGH_CUTOFF_PIN, !(i % 2));
    int last_local_time = millis();
    while(millis() - last_local_time < 500){
      server.handleClient();
      delay(1);
    }
  }
}

void setup() {
  Serial.begin(115200); // Initialize serial communication at 9600 baud rate
  EEPROM.begin(EEPROM_SIZE);
  loadFromEEPROM();

  //pinMode(MANUAL_START_BUTTON_PIN, INPUT);
  Serial.println("Starting setup");
  Serial.println("Checking for setup switch");
  pinMode(LED_MOTOR_ON_PIN, OUTPUT);
  pinMode(LED_DRY_RUN_CUTOFF_PIN, OUTPUT);
  pinMode(LED_LOW_HIGH_CUTOFF_PIN, OUTPUT);
  pinMode(RELAY_MOTOR_PIN, OUTPUT);
  pinMode(RELAY_STARTER_PIN, OUTPUT);

  pinMode(WATERLEVEL_LOW_PIN, INPUT);
  pinMode(WATERLEVEL_FULL_PIN, INPUT);
  pinMode(WATERLEVEL_DRYRUN_PIN, INPUT);

    WiFi.softAP(ssid, password);
    Serial.println("WiFi started");
  // Setup web server routes
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/timer", handleTimer);
    server.on("/exit", handleExit);

    server.begin();
    Serial.println("Web server started");
  
 
  
    pinMode(MANUAL_START_BUTTON_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(MANUAL_START_BUTTON_PIN), manual_start_button_isr, FALLING);
    Serial.println("Booting");
    booting();
    Serial.println("Booting complete");

  
  // Setting up Pin Modes
  pinMode(LED_MOTOR_ON_PIN, OUTPUT);
  pinMode(LED_DRY_RUN_CUTOFF_PIN, OUTPUT);
  pinMode(LED_LOW_HIGH_CUTOFF_PIN, OUTPUT);
  pinMode(RELAY_MOTOR_PIN, OUTPUT);
  pinMode(RELAY_STARTER_PIN, OUTPUT);

  pinMode(WATERLEVEL_LOW_PIN, INPUT);
  pinMode(WATERLEVEL_FULL_PIN, INPUT);
  pinMode(WATERLEVEL_DRYRUN_PIN, INPUT);

  //pinMode(SETUP_BUTTON_PIN, INPUT);

  // Ensuring all pins are low
  digitalWrite(RELAY_MOTOR_PIN, LOW);
  digitalWrite(RELAY_STARTER_PIN, LOW);
  digitalWrite(LED_MOTOR_ON_PIN, LOW);
  digitalWrite(LED_DRY_RUN_CUTOFF_PIN, LOW);
  digitalWrite(LED_LOW_HIGH_CUTOFF_PIN, LOW);

  Serial.println("Setup complete");
}


void start_motor() {
  if (dryrun_cutoff_status) return;
  if (safety_timeout_status) return;
  if (voltage_cutoff_status) {
    if ((millis() - voltage_cutoff_millis) < VOLTAGE_CUTOFF_RETRY_TIME * 1000) return;
    else voltage_cutoff_status = false;
  }
  if (live_voltage >= MOTOR_START_MIN_VOLTAGE_CUTOFF && live_voltage <= MOTOR_START_MAX_VOLTAGE_CUTOFF) {
    motor_running = true;
    motor_starting_millis = millis();
    digitalWrite(RELAY_MOTOR_PIN, HIGH);
    digitalWrite(LED_MOTOR_ON_PIN, HIGH);
    digitalWrite(LED_LOW_HIGH_CUTOFF_PIN, LOW);
    Serial.println("Motor started");
    if (STARTER_SWITCH_DURATION != 0) {
      digitalWrite(RELAY_STARTER_PIN, HIGH);
      starter_status = true;
      Serial.println("Starter relay activated");
    }
  } 
  else {
    digitalWrite(LED_LOW_HIGH_CUTOFF_PIN, HIGH);
    Serial.println("Voltage out of range, motor not started");
  }
  motor_running_status = 1;
}

void stop_motor() {
  motor_running = false;
  motor_running_status = 0;
  digitalWrite(LED_MOTOR_ON_PIN, LOW);
  digitalWrite(RELAY_MOTOR_PIN, LOW);
  digitalWrite(RELAY_STARTER_PIN, LOW);
  starter_status = false;
  Serial.println("Motor stopped");
}



void loop() {
  update_pin_statuses_and_voltage();
  if (setup_mode) {
    server.handleClient();
    
  }
  else { // Normal operation
    server.handleClient();

    Serial.print("Water Low:\t");
    Serial.print(water_low);
    Serial.print("\tWater Full:\t");
    Serial.print(water_full);
    Serial.print("\tMotor Dry Run:\t");
    Serial.print(motor_dry_run);
    Serial.print("\tVoltage:\t");
    Serial.print(live_voltage);
    Serial.print("\tMotor Running:\t");
    Serial.print(motor_running);
    Serial.print("\tMotor Running Status:\t");
    Serial.println(motor_running_status);

    if (!motor_running) {
      if (dryrun_cutoff_status){
        digitalWrite(LED_DRY_RUN_CUTOFF_PIN, millis()/500 % 2);
      }
      if (!water_low && !water_full) {
        start_motor();
      } 
      else if (!water_low && water_full) {
        Serial.println("Error condition: Water low and full at the same time");
      }
    } 
    else { // Motor is running
      if (water_full) {
        if (!water_full_detected){
          water_full_detected = true;
          water_full_detected_millis = millis();
        }
        else if (millis() - water_full_detected_millis >= WATER_FULL_DETECTED_BUFFER) {
          stop_motor();
          Serial.println("Water full detected, motor stopped");
          water_full_detected = false;
        }
        
      } 
      else {
        water_full_detected = false;
      }

      if (live_voltage < MOTOR_RUN_MIN_VOLTAGE_CUTOFF || live_voltage > MOTOR_START_MAX_VOLTAGE_CUTOFF) {
        if (!voltage_abnormality_detected) {
          voltage_abnormality_detected = true;
          voltage_abnormality_detected_millis = millis();
        } 
        else if (millis() - voltage_abnormality_detected_millis >= VOLTAGE_ABNORMALITY_BUFFER) {
          digitalWrite(LED_LOW_HIGH_CUTOFF_PIN, HIGH);
          voltage_cutoff_millis = millis();
          voltage_cutoff_status = true;
          stop_motor();
          voltage_abnormality_detected = false;
          Serial.println("Voltage out of range, motor stopped");
        }
      }
      else {
        voltage_abnormality_detected = false;
      } 
      if (motor_running_status == 1) {
        if (motor_dry_run) {
          motor_running_status = 2;
          Serial.println("water detected detected, motor started running phase");
        }
        else if (millis() - motor_starting_millis >= DRYRUN_TIMOUT * 1000) {
          motor_running_status = 2;
        }
      } 
      else if (motor_running_status == 2) {
        if (!motor_dry_run && DRYRUN_TIMOUT != 0) {
          if (!motor_dry_run_detected) {
            motor_dry_run_detected = true;
            motor_dry_run_detected_millis = millis();
          }
          else if (millis() - motor_dry_run_detected_millis >= DRYRUN_DETECTION_BUFFER) {
            stop_motor();
            digitalWrite(LED_DRY_RUN_CUTOFF_PIN, HIGH);
            dryrun_cutoff_status = true;
            Serial.println("Dry run detected, motor stopped");
            motor_dry_run_detected = false;
          }
        }
        else {
          motor_dry_run_detected = false;
        }
      }
      if (starter_status && (millis() - motor_starting_millis >= STARTER_SWITCH_DURATION * 1000)) {
        starter_status = false;
        digitalWrite(RELAY_STARTER_PIN, LOW);
        Serial.println("Starter relay deactivated");
      }
      if (SAFETY_TIMEOUT != 0 && millis() - motor_starting_millis >= SAFETY_TIMEOUT * 60000) {
        stop_motor();
        safety_timeout_status = true;
        digitalWrite(LED_DRY_RUN_CUTOFF_PIN, HIGH);
        digitalWrite(LED_LOW_HIGH_CUTOFF_PIN, HIGH);
        Serial.println("Safety timeout reached, motor stopped");
      }
    }

    //delay(300);
    if (manual_start_button_pressed) {
      manual_start_button_pressed = false;
      digitalWrite(LED_MOTOR_ON_PIN, LOW);
      if (!motor_running) {
        voltage_cutoff_status = false;
        dryrun_cutoff_status = false;
        safety_timeout_status = false;
        digitalWrite(LED_DRY_RUN_CUTOFF_PIN, LOW);
        start_motor();
        Serial.println("Manual start button pressed, motor started");
      }
    }
  }
  delay(30);
  
}
