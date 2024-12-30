#include <Arduino.h>
#include <EEPROM.h>
//Pinout
#define LED_MOTOR_ON_PIN        16
#define LED_DRY_RUN_CUTOFF_PIN  14
#define LED_LOW_HIGH_CUTOFF_PIN 2

#define RELAY_MOTOR_PIN         13
#define RELAY_STARTER_PIN       10

#define WATERLEVEL_LOW_PIN      12
#define WATERLEVEL_FULL_PIN     4
#define WATERLEVEL_DRYRUN_PIN   5

#define MANUAL_START_BUTTON_PIN  9
#define SETUP_BUTTON_PIN         10

#define VOLTAGE_SENSOR_PIN      A0
struct configs {  //struct to control he 
  int dryrun_timout;
  int starter_switch_duration;
  int voltage_cutoff_retry_time;
  int motor_start_min_volt;
  int motor_start_max_volt;
  int motor_run_min_volt;
};
//constants give the default values here
int DRYRUN_TIMOUT = 60; //millli seconds
int MOTOR_START_MIN_VOLTAGE_CUTOFF = 185;
int MOTOR_START_MAX_VOLTAGE_CUTOFF = 245;
int MOTOR_RUN_MIN_VOLTAGE_CUTOFF = 165;
int VOLTAGE_CUTOFF_RETRY_TIME = 600;   // milliseconds
int STARTER_SWITCH_DURATION = 5000; //millis secondsduration in which the starter button is pressed, give 0 to disable

//variables
bool motor_running = false; //global variable to check the motor if it is currently pumping or not
int motor_running_status = 0; // 0 for not running, 1 for just started( in this case dryrun is not checked), 2 running phase (check for dryrun)
unsigned long starting_millis;
unsigned long voltage_cutoff_millis;
bool dryrun_cutoff_status = false;
bool voltage_cutoff_status = false;
bool starter_status = false;


void saveSettingsToEEPROM(configs currentSettings) { //save settings to config
  EEPROM.put(0, currentSettings);
  EEPROM.commit();
}
void loadSettingsFromEEPROM() {
  configs current_config;
  EEPROM.get(0, current_config);
  // Ensure default settings if EEPROM data is invalid
  if (isnan(current_config.motor_start_min_volt) || isnan(current_config.motor_start_max_volt) || isnan(current_config.motor_run_min_volt) || 
      current_config.dryrun_timout <= 0 || current_config.voltage_cutoff_retry_time <= 0) {
        //to add later when data is invalid
  }
  else {
    DRYRUN_TIMOUT = current_config.dryrun_timout * 1000; //seconds
    MOTOR_START_MIN_VOLTAGE_CUTOFF = current_config.motor_start_min_volt;
    MOTOR_START_MAX_VOLTAGE_CUTOFF = current_config.motor_start_max_volt;
    MOTOR_RUN_MIN_VOLTAGE_CUTOFF = current_config.motor_run_min_volt;
    VOLTAGE_CUTOFF_RETRY_TIME = current_config.voltage_cutoff_retry_time * 1000;  
    STARTER_SWITCH_DURATION = current_config.starter_switch_duration * 1000;
  }
}
void setup(){
  //setting up Pin Modes
  pinMode(LED_MOTOR_ON_PIN, OUTPUT);
  pinMode(LED_DRY_RUN_CUTOFF_PIN, OUTPUT);
  pinMode(LED_LOW_HIGH_CUTOFF_PIN, OUTPUT);
  pinMode(RELAY_MOTOR_PIN, OUTPUT);
  pinMode(RELAY_STARTER_PIN, OUTPUT);

  pinMode(WATERLEVEL_LOW_PIN, INPUT);
  pinMode(WATERLEVEL_FULL_PIN, INPUT);
  pinMode(WATERLEVEL_DRYRUN_PIN, INPUT);

  pinMode(MANUAL_START_BUTTON_PIN, INPUT);
  pinMode(SETUP_BUTTON_PIN, INPUT);

  //ensuring all pins are low
  digitalWrite(RELAY_MOTOR_PIN, LOW);
  digitalWrite(RELAY_STARTER_PIN, LOW);
  digitalWrite(LED_MOTOR_ON_PIN, LOW);
  digitalWrite(LED_DRY_RUN_CUTOFF_PIN, LOW);
  digitalWrite(LED_LOW_HIGH_CUTOFF_PIN, LOW);

  


}
int check_voltage(int numReadings = 10){
  long accumulated_voltage = 0;
  for (int i = 0; i < numReadings; i++) {
    accumulated_voltage += analogRead(VOLTAGE_SENSOR_PIN);
    delay(50); 
  }
  return accumulated_voltage / numReadings;
}
void start_motor(){
  if (dryrun_cutoff_status) return;
  if (voltage_cutoff_status) {
    if(millis() - voltage_cutoff_millis < VOLTAGE_CUTOFF_RETRY_TIME){
      return;
    }
    else {
      voltage_cutoff_status = false;
    }
  }
  if (check_voltage() >= MOTOR_START_MIN_VOLTAGE_CUTOFF && check_voltage() <= MOTOR_START_MAX_VOLTAGE_CUTOFF){
    motor_running = true;
    motor_running_status = 1;
    starting_millis = millis();
    digitalWrite(RELAY_MOTOR_PIN,HIGH);
    digitalWrite(LED_MOTOR_ON_PIN, HIGH);
    digitalWrite(LED_LOW_HIGH_CUTOFF_PIN,LOW);
    if (STARTER_SWITCH_DURATION != 0){
      digitalWrite(RELAY_STARTER_PIN,HIGH);
      starter_status = true;
    }
    
  }
  else {
    digitalWrite(LED_LOW_HIGH_CUTOFF_PIN,HIGH);
  }
}
void stop_motor(){
  motor_running = false;
  motor_running_status = 0;
  digitalWrite(LED_MOTOR_ON_PIN, LOW);
  digitalWrite(RELAY_MOTOR_PIN,LOW);
  digitalWrite(RELAY_STARTER_PIN,LOW);
  starter_status = false;
}
bool check_input_pin(int pin, int timeout = 1){ //retrun the input after some filtering . 1 means there is water on the pin, 0 means no water, timout is in seconds
  for (int i = 0; i <= timeout * 100 ;i++){
    if (!digitalRead(pin)) {
      break;
    } 
    delay(10);
  }
  for (int i = 0; i <= timeout * 100;i++){
    if (!digitalRead(pin)) {
      return true;
    } 
    delay(10);
    
  }
  return false;
}
void loop(){
  bool water_low = check_input_pin(WATERLEVEL_LOW_PIN);
  bool water_full = check_input_pin(WATERLEVEL_FULL_PIN);
  bool motor_dry_run = check_input_pin(WATERLEVEL_DRYRUN_PIN);


  if (!motor_running){
    if (!water_low && !water_full){
      start_motor();
    }
    else if (!water_low && water_full){
      //error condition
    }
  }
  else {  //motor is running
    if (water_full){
      stop_motor();
    }
    else if (check_voltage() < MOTOR_RUN_MIN_VOLTAGE_CUTOFF){
      digitalWrite(LED_LOW_HIGH_CUTOFF_PIN,HIGH);
      voltage_cutoff_millis = millis();
      voltage_cutoff_status = true;
      stop_motor();
    }
    else if (motor_running_status == 1) {
      if (millis() - starting_millis >= DRYRUN_TIMOUT){
        motor_running_status = 2;
      }
    }
    else if (motor_running_status == 2){
      if (!motor_dry_run){
        stop_motor();
        digitalWrite(LED_DRY_RUN_CUTOFF_PIN,HIGH);
        dryrun_cutoff_status = true;
      }
    }
    if (starter_status && (millis() - starting_millis >= STARTER_SWITCH_DURATION)){
      starter_status = false;
      digitalWrite(RELAY_STARTER_PIN,LOW);
    }
  }

  delay(300);
  if (!motor_running && digitalRead(MANUAL_START_BUTTON_PIN)){
    voltage_cutoff_status = false;
    dryrun_cutoff_status = false;
    start_motor();
  }
  

}

