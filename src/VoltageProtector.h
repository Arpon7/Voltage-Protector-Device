

#ifndef VOLTAGE_PROTECTOR_H
#define VOLTAGE_PROTECTOR_H

#include <Arduino.h>

#define SIM_DC_MODE 1

const uint8_t PIN_V      = A0;   
const uint8_t PIN_I      = A1;   
const uint8_t PIN_RELAY  = 7;    
const uint8_t PIN_BUZZER = 6;
const uint8_t PIN_LED_G  = 8;    
const uint8_t PIN_LED_R  = 9;   
const uint8_t BTN_MENU   = 2;    
const uint8_t BTN_UP     = 3;
const uint8_t BTN_DOWN   = 4;


const bool RELAY_ACTIVE_HIGH = true;


const float V_FULL_SCALE = 300.0;   
const float I_FULL_SCALE = 20.0;  


const float ADC_VOLTS_PER_COUNT = 5.0 / 1023.0;               
const float A_PER_COUNT = ADC_VOLTS_PER_COUNT / 0.100;       
const float V_PER_COUNT = 0.76;     
                                    
const float I_DEADBAND  = 0.10;     
const float V_DEADBAND  = 5.0;      

#if SIM_DC_MODE
  const uint16_t AC_SAMPLES   = 10;     
#else
  const uint16_t AC_SAMPLES   = 400;    
#endif
const uint16_t AC_SAMPLE_US = 250;  


const unsigned long TRIP_DELAY_MS = 50; 


struct Settings {
  uint16_t magic;     
  float overV;        
  float underV;       
  float overI;      
  uint8_t reconnect;  
};


enum FaultType { F_NONE, F_OVERV, F_UNDERV, F_OVERI };


struct AcAccum {
  uint16_t n;       
  uint32_t sum;     
  uint32_t sumSq;   
  uint16_t mn;      
  uint16_t mx;      
};


void vpBegin();


void vpUpdate();

void accumulate(AcAccum &a, uint16_t sample);


float calcVoltageRms(const AcAccum &a);

float calcCurrentRms(const AcAccum &a);



float calcPeakCurrent(const AcAccum &a);


void acquireMeasurements();


FaultType checkFault();

const char* faultName(FaultType f);

void setRelay(bool on);

void protectionService();


void loadSettings();

void saveSettings();

void buzzerPulse(uint16_t ms);


void buzzerService();

void handleButtons();


void updateLcd();


void handleSerial();

void printSettings();


void printStatus();

void streamService();

#endif  
