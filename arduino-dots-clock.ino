/*
   Arduino Clock on MAX7219 x4 \w temp and humidity.

   @author qbbr
*/

#include "RTClib.h"
#include "LedControl.h"
#include "DHT.h"
#include "font.h"

// RTC: A4 - SDA, A5 - SCL
RTC_DS1307 RTC;
int year = 2020; // 2000-2099
int month = 1; // 1-12
int day = 1; // 1-31
int hour = 0; // 0-23
int minute = 0; // 0-59
int second = 0; // 0-59

// DHT
#define DHT_PIN 7
DHT dht(DHT_PIN, DHT11); // DHT11|DHT22

// matrix
#define DIN_PIN 11
#define CLK_PIN 13
#define CS_PIN 10
#define MAX_DEVICES 4
#define ANIMATION_TIME 50
#define SCREEN_TIMEOUT 10000
#define SCREEN_CLOCK 0
#define SCREEN_TEMPERATURE 1
#define SCREEN_HUMIDITY 2
#define SCREEN_INTENSITY 3
#define SCREEN_HOUR_SETTER 4
#define SCREEN_MINUTE_SETTER 5
#define SCREEN_QBBR 6
LedControl matrix = LedControl(DIN_PIN, CLK_PIN, CS_PIN, MAX_DEVICES); // DIN, CLK, CS, num devices
int intensity = 8; // 0-15
boolean dotsBlinkState = false;
unsigned long dotsPrevMillis = 0;
unsigned long screenPrevMillis = 0;
int screenIndex = SCREEN_CLOCK;
typedef void (*ScreenList)(void);
ScreenList screenList[] = {&setScreenClock, &setScreenTemp, &setScreenHumidity, &setScreenIntensity, &setScreenHourSetter, &setScreenMinuteSetter, &setScreenQBBR};

int _d1 = -1; // 3 addr
int _d2 = -1; // 2 addr
int _d3 = -1; // 1 addr
int _d4 = -1; // 0 addr

// buttons
#define BTN1_PIN 2 // mode
#define BTN2_PIN 3 // -
#define BTN3_PIN 4 // +
unsigned long btn1PrevMillis = 0;
unsigned long btn2PrevMillis = 0;
unsigned long btn3PrevMillis = 0;

void setup () {
  Serial.begin(9600);

  pinMode(BTN1_PIN, INPUT);
  pinMode(BTN2_PIN, INPUT);
  pinMode(BTN3_PIN, INPUT);

  dht.begin();

  while (!RTC.begin()) {
    Serial.println("Couldn't find RTC");
    delay(500);
  }

  if (!RTC.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    RTC.adjust(DateTime(year, month, day, hour, minute, second));
    //RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  for (int n = 0; n < MAX_DEVICES; n++) {
    matrix.shutdown(n, false);
    matrix.setIntensity(n, intensity); // 0-15
    matrix.clearDisplay(n);
  }

  delay(500);
}

void loop () {
  if (digitalRead(BTN1_PIN) == HIGH && millis() - btn1PrevMillis > 300) {
    Serial.println("BTN1 clicked");
    screenNext();
    btn1PrevMillis = millis();
    screenPrevMillis = millis();
  }

  if (digitalRead(BTN2_PIN) == HIGH && millis() - btn2PrevMillis > 300) {
    Serial.println("BTN2 clicked");
    btn2PrevMillis = millis();
    screenPrevMillis = millis();

    if (screenIndex == SCREEN_INTENSITY) {
      intensityDec();
    } else if (screenIndex == SCREEN_HOUR_SETTER) {
      hourDec();
    } else if (screenIndex == SCREEN_MINUTE_SETTER) {
      minuteDec();
    }
  }

  if (digitalRead(BTN3_PIN) == HIGH && millis() - btn3PrevMillis > 300) {
    Serial.println("BTN3 clicked");
    btn3PrevMillis = millis();
    screenPrevMillis = millis();

    if (screenIndex == SCREEN_INTENSITY) {
      intensityInc();
    } else if (screenIndex == SCREEN_HOUR_SETTER) {
      hourInc();
    } else if (screenIndex == SCREEN_MINUTE_SETTER) {
      minuteInc();
    }
  }

  if (screenIndex != SCREEN_CLOCK && millis() - screenPrevMillis > SCREEN_TIMEOUT) {
    _d1 = _d2 = _d3 = _d4 = -1;
    setScreenByIndex(SCREEN_CLOCK);
  } else {
    setScreenByIndex(screenIndex);
  }
}

void screenNext() {
  screenIndex++;

  if (screenIndex >= (sizeof(screenList) / sizeof(screenList[0]))) {
    screenIndex = 0;
  }

  _d1 = _d2 = _d3 = _d4 = -1;
  setScreenByIndex(screenIndex);
}

void setScreenByIndex(int index) {
  screenIndex = index;
  screenList[index]();
}


/* Screens {{{ */

void setScreenClock() {
  DateTime now = RTC.now();
  int h = now.hour();
  int m = now.minute();
  int d1, d2, d3, d4;

  splitInt(h, &d1, &d2);
  splitInt(m, &d3, &d4);
  fillScreen(d1, d2, d3, d4);

  if (millis() - dotsPrevMillis > 1000) {
    dotsToggle();
    dotsPrevMillis = millis();
  }
}

void setScreenTemp() {
  int d3, d4;
  int t = getDHTTemperature();

  splitInt(t, &d3, &d4);
  fillScreen(CHAR_TEMP, CHAR_PLUS, d3, d4);
}

void setScreenHumidity() {
  int d3, d4;
  int h = getDHTHumidity();

  splitInt(h, &d3, &d4);
  fillScreen(CHAR_HUMIDITY, CHAR_PERCENT, d3, d4);
}

void setScreenIntensity() {
  int d3, d4;
  splitInt(intensity, &d3, &d4);
  Serial.println(intensity);
  Serial.println(d3);
  Serial.println(d4);
  Serial.println();

  fillScreen(CHAR_INTENSITY, CHAR_EMPTY, d3, d4);
}

void setScreenHourSetter() {
  DateTime now = RTC.now();
  int h = now.hour();
  int d3, d4;

  splitInt(h, &d3, &d4);
  fillScreen(CHAR_H, CHAR_EMPTY, d3, d4);
}

void setScreenMinuteSetter() {
  DateTime now = RTC.now();
  int m = now.minute();
  int d3, d4;

  splitInt(m, &d3, &d4);
  fillScreen(CHAR_M, CHAR_EMPTY, d3, d4);
}

void setScreenQBBR() {
  fillScreen(CHAR_Q, CHAR_B, CHAR_B, CHAR_R);
}

/* }}} Screens */


void splitInt(int d, int *d1, int *d2) {
  if (d < 10) {
    *d1 = 0;
    *d2 = d;
  } else {
    String s = String(d);
    *d1 = s.substring(0, 1).toInt();
    *d2 = s.substring(1, 2).toInt();
  }
}

void intensityInc() {
  if (intensity < 15) {
    intensity++;
    for (int n = 0; n < MAX_DEVICES; n++) {
      matrix.setIntensity(n, intensity);
    }
  }
}

void intensityDec() {
  if (intensity > 0) {
    intensity--;
    for (int n = 0; n < MAX_DEVICES; n++) {
      matrix.setIntensity(n, intensity);
    }
  }
}

void hourInc() {
  DateTime now = RTC.now();
  DateTime now2 = now + TimeSpan(0, 1, 0, 0);
  RTC.adjust(now2);
}

void hourDec() {
  DateTime now = RTC.now();
  DateTime now2 = now - TimeSpan(0, 1, 0, 0);
  RTC.adjust(now2);
}

void minuteInc() {
  DateTime now = RTC.now();
  DateTime now2 = now + TimeSpan(0, 0, 1, 0);
  RTC.adjust(now2);
}

void minuteDec() {
  DateTime now = RTC.now();
  DateTime now2 = now - TimeSpan(0, 0, 1, 0);
  RTC.adjust(now2);
}

void dotsToggle() {
  dotsBlinkState = !dotsBlinkState;

  matrix.setLed(1, 2, 0, dotsBlinkState);
  matrix.setLed(1, 5, 0, dotsBlinkState);
  matrix.setLed(2, 2, 7, dotsBlinkState);
  matrix.setLed(2, 5, 7, dotsBlinkState);
}

void fillScreen(int d1, int d2, int d3, int d4) {
  if (_d1 != d1) {
    _d1 = d1;
    animateMatrix(3, d1);
  }

  if (_d2 != d2) {
    _d2 = d2;
    animateMatrix(2, d2);
  }

  if (_d3 != d3) {
    _d3 = d3;
    animateMatrix(1, d3);
  }

  if (_d4 != d4) {
    _d4 = d4;
    animateMatrix(0, d4);
  }
}

void animateMatrix(int addr, int fontLine) {
  for (int i = 0; i < 8; i++) {
    delay(ANIMATION_TIME);

    for (int j = 0; j < i + 1; j++) {
      byte dots = pgm_read_byte_near(&clockFont[fontLine][7 - i + j]);

      if (screenIndex == SCREEN_CLOCK) {
        if (addr == 1) {
          dots = dots >> 1;
        } else if (addr == 2) {
          dots = dots << 1;
        }
      }

      matrix.setRow(addr, j, dots);
    }
  }
}

//void fillMatrix(int addr, int fontLine) {
//  for (int row = 0; row < 8; row++) {
//    matrix.setRow(addr, row, pgm_read_byte_near(&clockFont[fontLine][row]));
//  }
//  delay(ANIMATION_TIME);
//}


/* DHT Sensor {{{ */

int getDHTTemperature() {
  float t = dht.readTemperature();

  if (isnan(t)) {
    printDHTError();
  }

  return (int) round(t);
}

int getDHTHumidity() {
  float h = dht.readHumidity();

  if (isnan(h)) {
    printDHTError();
  }

  return (int) round(h);
}

void printDHTError() {
  Serial.println("[E] something wrong with DHT sensor.");
}

/* }}} */
