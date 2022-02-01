#include "RTClib.h"

#define SOUND_PIN 3

RTC_DS1307 rtc;

void setup () {
  pinMode(SOUND_PIN, OUTPUT);
  
  if (!rtc.isrunning()) {
    // Uhrzeit auf Kompilierungsdatum setzen
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop () {
    DateTime now = rtc.now();
    // alle 5 Sekunden für 1 Sekunde piepen
    if (now.second() % 5 == 0) {
         tone(SOUND_PIN, 440);
         delay(1000);
         noTone(SOUND_PIN);
    }

    delay(200);
}
