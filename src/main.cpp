#include <Arduino.h>
//constants
unsigned long dryrun_timeout = 60; //seconds
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
int motor_running_status = 0; // 0 for not running, 1 for just started( in this case dryrun is not checked), 2 running phase (check for dryrun)

unsigned long current_millis;
unsigned long starting_millis;

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
  motor_running_status = 1;
  digitalWrite(RELAY_MOTOR_PIN,HIGH);
}
void stop_motor(){
  motor_running = false;
  motor_running_status = 0;
  starting_millis = millis();
  digitalWrite(RELAY_MOTOR_PIN,LOW);
}
int check_voltage(){
  return analogRead(VOLTAGE_SENSOR_PIN);
}
void loop(){
  bool water_low = digitalRead(WATERLEVEL_LOW_PIN);
  bool water_full = digitalRead(WATERLEVEL_FULL_PIN);
  bool motor_dry_run = digitalRead(WATERLEVEL_DRYRUN_PIN);


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
    else if (motor_running_status == 1) {
      if (millis() - starting_millis >= dryrun_timeout){
        motor_running_status = 2;
      }
    }
    else if (motor_running_status == 2){
      if (!motor_dry_run){
        stop_motor();
      }
    }
  }
  

}