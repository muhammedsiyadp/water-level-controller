#include <Arduino.h>
//Pinout
#define LED_MOTOR_ON_PIN        14
#define LED_DRY_RUN_CUTOFF_PIN  16
#define LED_LOW_HIGH_CUTOFF_PIN 12

#define RELAY_MOTOR_PIN         13
#define RELAY_STARTER_PIN       9

#define WATERLEVEL_LOW_PIN      4
#define WATERLEVEL_FULL_PIN     2
#define WATERLEVEL_DRYRUN_PIN   5

#define MANUAL_START_BUTTON_PIN  9
#define SETUP_BUTTON_PIN         10

#define VOLTAGE_SENSOR_PIN      A0

bool motor_running = false; //global variable to check the motor if it is currently pumping or not

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
void start_motor(){
  motor_running = true;
  digitalWrite(RELAY_MOTOR_PIN,HIGH);
}
void stop_motor(){
  motor_running = false;
  digitalWrite(RELAY_MOTOR_PIN,LOW);
}
int check_voltage(){
  return analogRead(VOLTAGE_SENSOR_PIN);
}
void loop(){
  bool water_low = digitalRead(WATERLEVEL_LOW_PIN);
  bool water_full = digitalRead(WATERLEVEL_FULL_PIN);
  bool motor_dry_run = digitalRead(WATERLEVEL_DRYRUN_PIN);
  
  if (!water_low && !water_full && !motor_running){
    start_motor();
  }
  else if (water_full && motor_running) {
    stop_motor();
  }
}