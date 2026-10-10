

#include "VoltageProtector.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>


static const uint16_t MAGIC = 0xA55A;
static const float SQRT2 = 1.414;

static LiquidCrystal_I2C lcd(0x27, 16, 2);
static Settings cfg;

static FaultType activeFault = F_NONE;
static bool relayOn = false;
static bool streaming = false;
static bool settingsDirty = false;

static float volts = 0, amps = 0, peakAmps = 0;
static unsigned long faultSince = 0;   
static unsigned long okSince = 0;     
static unsigned long lastLcd = 0, lastStream = 0;
static unsigned long buzzUntil = 0;
static uint8_t screen = 0;             



void vpBegin() {
  Serial.begin(9600);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  loadSettings();
  setRelay(false);                       

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Voltage Protector");
  lcd.setCursor(0, 1); lcd.print("Starting...");
  Serial.println(F("Voltage Protector ready. Type HELP."));
  delay(1500);
  lcd.clear();

  okSince = millis();                    
}

void vpUpdate() {
  acquireMeasurements();
  protectionService();
  buzzerService();
  handleButtons();
  handleSerial();
  updateLcd();
  streamService();
}



void accumulate(AcAccum &a, uint16_t sample) {
  a.n++;
  a.sum += sample;
  a.sumSq += (uint32_t)sample * sample;
  if (sample < a.mn) a.mn = sample;
  if (sample > a.mx) a.mx = sample;
}


static float rmsCounts(const AcAccum &a) {
  float mean = (float)a.sum / a.n;
  float var = (float)a.sumSq / a.n - mean * mean;
  return sqrt(var > 0 ? var : 0);
}

float calcVoltageRms(const AcAccum &a) {
  if (a.n == 0) return 0;
#if SIM_DC_MODE
  return ((float)a.sum / a.n) * V_FULL_SCALE / 1023.0;
#else
  float v = rmsCounts(a) * V_PER_COUNT;
  return (v < V_DEADBAND) ? 0 : v;
#endif
}

float calcCurrentRms(const AcAccum &a) {
  if (a.n == 0) return 0;
#if SIM_DC_MODE
  return ((float)a.sum / a.n) * I_FULL_SCALE / 1023.0;
#else
  float i = rmsCounts(a) * A_PER_COUNT;
  return (i < I_DEADBAND) ? 0 : i;
#endif
}

float calcPeakCurrent(const AcAccum &a) {
  if (a.n == 0) return 0;
#if SIM_DC_MODE
  return calcCurrentRms(a);
#else
  return (a.mx - a.mn) * 0.5 * A_PER_COUNT;
#endif
}

void acquireMeasurements() {
  AcAccum av = {0, 0, 0, 1023, 0};
  AcAccum ai = {0, 0, 0, 1023, 0};

  unsigned long t = micros();
  for (uint16_t k = 0; k < AC_SAMPLES; k++) {
    while ((unsigned long)(micros() - t) < AC_SAMPLE_US) { }   
    t += AC_SAMPLE_US;
    accumulate(av, analogRead(PIN_V));
    accumulate(ai, analogRead(PIN_I));
  }

  volts    = calcVoltageRms(av);
  amps     = calcCurrentRms(ai);
  peakAmps = calcPeakCurrent(ai);
}



FaultType checkFault() {
  if (amps > cfg.overI || peakAmps > cfg.overI * SQRT2) return F_OVERI;
  if (volts > cfg.overV)  return F_OVERV;
  if (volts < cfg.underV) return F_UNDERV;
  return F_NONE;
}

const char* faultName(FaultType f) {
  switch (f) {
    case F_OVERV:  return "OVER-VOLT";
    case F_UNDERV: return "UNDER-VOLT";
    case F_OVERI:  return "OVER-CURR";
    default:       return "NONE";
  }
}

void setRelay(bool on) {
  relayOn = on;
  digitalWrite(PIN_RELAY, (on == RELAY_ACTIVE_HIGH) ? HIGH : LOW);
  digitalWrite(PIN_LED_G, on);
  digitalWrite(PIN_LED_R, !on);
}

void protectionService() {
  FaultType f = checkFault();

  if (relayOn) {
 
    if (f != F_NONE) {
      if (faultSince == 0) faultSince = millis();
      if (millis() - faultSince >= TRIP_DELAY_MS) {
        setRelay(false);                       
        activeFault = f;
        faultSince = 0;
        okSince = 0;
        Serial.print(F("!! TRIP: ")); Serial.println(faultName(f));
        printStatus();
        buzzerPulse(300);
      }
    } else {
      faultSince = 0;
    }
  } else {

    if (f == F_NONE) {
      if (okSince == 0) okSince = millis();
      if (millis() - okSince >= (unsigned long)cfg.reconnect * 1000UL) {
        activeFault = F_NONE;
        setRelay(true);                       
        Serial.println(F("Supply normal - load reconnected"));
      }
    } else {
      okSince = 0;
      activeFault = f;
    }
  }
}



void loadSettings() {
  EEPROM.get(0, cfg);
  if (cfg.magic != MAGIC) {
    cfg = {MAGIC, 250.0, 180.0, 10.0, 5};
    EEPROM.put(0, cfg);
  }
}

void saveSettings() {
  EEPROM.put(0, cfg);
  settingsDirty = false;
}



void buzzerPulse(uint16_t ms) {
  buzzUntil = millis() + ms;
}

void buzzerService() {
  bool on = false;
  if ((long)(buzzUntil - millis()) > 0) on = true;                      // beep pulse
  else if (!relayOn && activeFault != F_NONE) on = (millis() / 500) % 2; // fault alarm
  digitalWrite(PIN_BUZZER, on);
}


static bool pressed(uint8_t pin, bool &last) {
  bool now = !digitalRead(pin);          
  bool edge = now && !last;
  last = now;
  return edge;
}

void handleButtons() {
  static bool lm = false, lu = false, ld = false;
  static unsigned long lastBtn = 0;
  if (millis() - lastBtn < 40) return;    
  lastBtn = millis();

  if (pressed(BTN_MENU, lm)) {
    screen = (screen + 1) % 5;
    if (screen == 0 && settingsDirty) saveSettings();
    lcd.clear();
  }
  bool up = pressed(BTN_UP, lu);
  bool dn = pressed(BTN_DOWN, ld);
  if (!up && !dn) return;
  int8_t d = up ? 1 : -1;
  settingsDirty = true;

  switch (screen) {
    case 1: cfg.overV  = constrain(cfg.overV + 5 * d, cfg.underV + 10, 300); break;
    case 2: cfg.underV = constrain(cfg.underV + 5 * d, 100, cfg.overV - 10); break;
    case 3: cfg.overI  = constrain(cfg.overI + 0.5 * d, 1, 20); break;
    case 4: cfg.reconnect = constrain((int)cfg.reconnect + d, 1, 60); break;
    default: settingsDirty = false; break;
  }
}

void updateLcd() {
  if (millis() - lastLcd < 250) return;
  lastLcd = millis();
  char l0[17], l1[17];
  char a[8], b[8];

  switch (screen) {
    case 0:
      dtostrf(volts, 5, 1, a); dtostrf(amps, 4, 1, b);
      snprintf(l0, 17, "V:%s I:%s", a, b);
      if (relayOn) snprintf(l1, 17, "LOAD ON  OK     ");
      else if (activeFault != F_NONE) snprintf(l1, 17, "TRIP:%-11s", faultName(activeFault));
      else snprintf(l1, 17, "RECONNECTING... ");
      break;
    case 1:
      snprintf(l0, 17, "Set Over-Voltage");
      dtostrf(cfg.overV, 3, 0, a);  snprintf(l1, 17, "%s V          ", a); break;
    case 2:
      snprintf(l0, 17, "Set Under-Volt  ");
      dtostrf(cfg.underV, 3, 0, a); snprintf(l1, 17, "%s V          ", a); break;
    case 3:
      snprintf(l0, 17, "Set Over-Current");
      dtostrf(cfg.overI, 4, 1, a);  snprintf(l1, 17, "%s A         ", a); break;
    default:
      snprintf(l0, 17, "Reconnect Delay ");
      snprintf(l1, 17, "%2u s           ", cfg.reconnect); break;
  }
  lcd.setCursor(0, 0); lcd.print(l0);
  lcd.setCursor(0, 1); lcd.print(l1);
}



void printSettings() {
  Serial.print(F("OV="));  Serial.print(cfg.overV, 0);
  Serial.print(F(" V, UV=")); Serial.print(cfg.underV, 0);
  Serial.print(F(" V, OC=")); Serial.print(cfg.overI, 1);
  Serial.print(F(" A, DLY=")); Serial.print(cfg.reconnect);
  Serial.println(F(" s"));
}

void printStatus() {
  Serial.print(F("V=")); Serial.print(volts, 1);
  Serial.print(F(" I=")); Serial.print(amps, 2);
  Serial.print(F(" RELAY=")); Serial.print(relayOn ? F("ON") : F("OFF"));
  Serial.print(F(" FAULT=")); Serial.println(faultName(activeFault));
}

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  line.toUpperCase();
  if (line.length() == 0) return;

  if (line == "HELP") {
    Serial.println(F("Commands: GET | STATUS | SET OV x | SET UV x | SET OC x | SET DLY x | STREAM ON/OFF | RESET"));
  } else if (line == "GET") {
    printSettings();
  } else if (line == "STATUS") {
    printStatus();
  } else if (line == "STREAM ON") {
    streaming = true;  Serial.println(F("OK stream on"));
  } else if (line == "STREAM OFF") {
    streaming = false; Serial.println(F("OK stream off"));
  } else if (line == "RESET") {
    if (checkFault() == F_NONE) {
      activeFault = F_NONE; setRelay(true);
      Serial.println(F("OK reconnected"));
    } else {
      Serial.println(F("ERR fault still present"));
    }
  } else if (line.startsWith("SET ")) {
    int sp = line.indexOf(' ', 4);
    if (sp < 0) { Serial.println(F("ERR format")); return; }
    String key = line.substring(4, sp);
    float val = line.substring(sp + 1).toFloat();
    bool ok = true;
    if      (key == "OV"  && val >= 200 && val <= 300 && val > cfg.underV) cfg.overV = val;
    else if (key == "UV"  && val >= 100 && val <= 220 && val < cfg.overV)  cfg.underV = val;
    else if (key == "OC"  && val >= 1   && val <= 20)  cfg.overI = val;
    else if (key == "DLY" && val >= 1   && val <= 60)  cfg.reconnect = (uint8_t)val;
    else ok = false;
    if (ok) { saveSettings(); Serial.print(F("OK ")); printSettings(); }
    else    Serial.println(F("ERR invalid key or range"));
  } else {
    Serial.println(F("ERR unknown command (type HELP)"));
  }
}

void streamService() {
  if (!streaming || millis() - lastStream < 1000) return;
  lastStream = millis();
  Serial.print(F("DATA,")); Serial.print(volts, 1); Serial.print(',');
  Serial.print(amps, 2);    Serial.print(',');
  Serial.print(relayOn ? 1 : 0); Serial.print(',');
  Serial.println(faultName(activeFault));
}
