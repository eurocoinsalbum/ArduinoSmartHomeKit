// If you do not have the extension kit then add // in front of the next line
#define EXTENSION_KIT

#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Input/Output Pins
#define MOVE_SENSOR_PIN       2
#define SOUND_PIN             3
#define BUTTON_1_PIN          4
#define LED_2_PIN             5
#define FAN_PIN_B_POWER       6
#define FAN_PIN_A_DIRECTION   7
#define BUTTON_2_PIN          8
#define SERVO_DOOR_PIN        9
#define SERVO_WINDOW_PIN     10
#define RELAY_PIN            12
#define LED_PIN              13
#define GAS_SENSOR_PIN       A0
#define LIGHT_SENSOR_PIN     A1
#define SOIL_SENSOR_PIN      A2
#define RAIN_SENSOR_PIN      A3

// Code fuer die Tuer
#define DOOR_CODE ".-."
// Brenndauer der Aussenleuchte
#define OUTER_LIGHT_ON_MILLIS 5000

// Servos
#define SERVO_DOOR_ANGLE_OPEN     100
#define SERVO_DOOR_ANGLE_CLOSED     0
#define SERVO_WINDOW_ANGLE_OPEN    90
#define SERVO_WINDOW_ANGLE_CLOSED   0
Servo servo_door;
Servo servo_window;
int servo_door_position;
int servo_window_position;

// LCD-Display
// I2C-Bus Adressee=0x27, 16 Zeichen breit, 2 Zeilen
LiquidCrystal_I2C lcd_display(0x27, 16, 2);

// aktuell eingetipptes Passwort
String password;

// Erweiterungen zum SmartHomeKit: DS1307
#ifdef EXTENSION_KIT
#include <RTClib.h>

char display_buffer[17];

// RTC Tiny RealTimeClock DS1307
RTC_DS1307 rtc;
#endif

void setup() {
  Serial.begin(115200);
  
  lcd_display.init();
  lcd_display.backlight();
  
  pinMode(BUTTON_1_PIN, INPUT);
  pinMode(BUTTON_2_PIN, INPUT);
  pinMode(MOVE_SENSOR_PIN, INPUT);
  pinMode(SOUND_PIN, OUTPUT);
  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  pinMode(SOUND_PIN, OUTPUT);
  pinMode(FAN_PIN_A_DIRECTION, OUTPUT);
  pinMode(FAN_PIN_B_POWER, OUTPUT);

  servo_door.attach(SERVO_DOOR_PIN);
  servo_window.attach(SERVO_WINDOW_PIN);

  lcd_display.setCursor(0, 0);
  lcd_display.print("Bitte warten...");
  
  // Propeller Richtung auf vorwaerts
  digitalWrite(FAN_PIN_A_DIRECTION, LOW);
  // Propeller aus
  turn_off_fan();

  // Tuer schliessen
  servo_door.write(SERVO_DOOR_ANGLE_CLOSED);
  servo_door_position = SERVO_DOOR_ANGLE_CLOSED;
  // Fenster öffnen
  servo_window.write(SERVO_WINDOW_ANGLE_OPEN);
  servo_window_position = SERVO_WINDOW_ANGLE_OPEN;

  #ifdef EXTENSION_KIT
  // Uhrzeit auf Kompilierungsdatum setzen, sofern diese noch nie initialisiert wurde
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  #endif
  
  // sound fuer alles OK
  beep();
  beep_long();
  beep();

  show_password_display();
}

void loop() {
  #ifdef EXTENSION_KIT
    update_door_display();
  #endif
  
  check_gas_sensor();
  check_soil_sensor();
  check_rain_sensor();
  check_move_sensor();
  check_light_sensor();
  check_door();
  check_command_mobile_app();
}

void check_gas_sensor() {
  // teste Gas auf Schwellenwert
  if (get_gas_sensor() > 700) {
    play_gas_alert();
    delay(1000);
  }
}

void check_soil_sensor() {
  // teste Bodenfeuchtigkeit auf Schwellenwert
  if (get_soil_sensor() > 50) {
    turn_on_relay();
    play_soil_alert();
    delay(1000);
  } else {
    turn_off_relay();
  }
}

void check_rain_sensor() {
  // teste Regen auf oberen Schwellenwert
  if (get_rain_sensor() > 800) {
    if (is_window_open()) {
      dimm_fan(70);
      close_window();
    }
  }
  
  // teste Regen auf unteren Schwellenwert
  if (get_rain_sensor() < 100) {
    if (!is_window_open()) {
      turn_off_fan();
      open_window();
    }
  }
}

void check_move_sensor() {
  // teste Bewegungsmelder
  if (get_move_sensor() == HIGH) {
    turn_on_outer_light();
  } else {
    turn_off_outer_light();
  }
}

void check_light_sensor() {
  int light_value = get_light_sensor();
  // teste Helligkeitssensor auf Schwellenwert
  if (light_value > 512) {
    turn_off_inner_light();
  } else if (light_value > 100) {
    dimm_inner_light((512 - light_value) / 4);
  } else {
    turn_on_inner_light();
  }
}

void check_door() {
  // teste, ob Button 1 gedrueckt wurde
  if (digitalRead(BUTTON_1_PIN) == 0) {
    #ifdef EXTENSION_KIT
      // das Display zeigt beim ersten Druecken evtl etwas anderes an. Passwort anzeigen.
      if (password.length() == 0) {
        show_password_display();
      }
    #endif
    
    // starte Zeitmessung
    unsigned long start_time = millis();
    
    // warte, solange Button 1 gedrueckt gehalten wird
    while (digitalRead(BUTTON_1_PIN) == 0) {}
  
    // Button 1 kurz oder lang gedrueckt (500ms)
    if (millis() - start_time < 500) {
      password += ".";
    } else {
      password += "-";
    }
    // Zeige aktuelles Passwort in Spalte 0, Zeile 1
    lcd_display.setCursor(0, 1);
    lcd_display.print(password);
  }

  // teste, ob Button 2 gedrueckt wurde
  if (digitalRead(BUTTON_2_PIN) == 0) {
    // teste, ob der richtige Code eingetippte wurde
    if (password == DOOR_CODE) {
      play_correct_code_sound();
      open_and_close_door();
    } else {
      lcd_display.setCursor(0, 0);
      lcd_display.print("Falscher Code!");
      play_wrong_code_sound();
      delay(3000);
    }
    password = "";
    show_password_display();
  }
}

int get_light_sensor() {
  return analogRead(LIGHT_SENSOR_PIN);
}

int get_move_sensor() {
  return digitalRead(MOVE_SENSOR_PIN);
}

int get_rain_sensor() {
  return analogRead(RAIN_SENSOR_PIN);
}

int get_soil_sensor() {
  return analogRead(SOIL_SENSOR_PIN);
}

int get_gas_sensor() {
  return analogRead(GAS_SENSOR_PIN);
}

bool is_window_open() {
  return servo_window_position == SERVO_WINDOW_ANGLE_OPEN;
}

bool is_window_closed() {
  return servo_window_position == SERVO_WINDOW_ANGLE_CLOSED;
}

void close_window() {
  move_servo(servo_window, servo_window_position, SERVO_WINDOW_ANGLE_CLOSED);
}

void open_window() {
  move_servo(servo_window, servo_window_position, SERVO_WINDOW_ANGLE_OPEN);
}

void turn_on_relay() {
  digitalWrite(RELAY_PIN, HIGH);
}

void turn_off_relay() {
  digitalWrite(RELAY_PIN, LOW);
}

void turn_on_outer_light() {
  digitalWrite(LED_PIN, HIGH);
}

void turn_off_outer_light() {
  digitalWrite(LED_PIN, LOW);
}

void turn_on_inner_light() {
  digitalWrite(LED_2_PIN, HIGH);
}

void turn_off_inner_light() {
  digitalWrite(LED_2_PIN, LOW);
}

void dimm_inner_light(int value) {
  // 0-255, 0=dunkel, 255=hell
  analogWrite(LED_2_PIN, value);
}

void turn_on_fan() {
  digitalWrite(FAN_PIN_B_POWER, HIGH);
}

void turn_off_fan() {
  digitalWrite(FAN_PIN_B_POWER, LOW);
}

void dimm_fan(int value) {
  // 0-255, 0=aus, 255=hoechste Stufe
  analogWrite(FAN_PIN_B_POWER, value);
}

void open_and_close_door() {
  lcd_display.clear();
  lcd_display.setCursor(0, 0);
  lcd_display.print("Offen!");
  open_door();
  delay(5000);
  lcd_display.clear();
  lcd_display.setCursor(0, 0);
  lcd_display.print("Schliessen");
  close_door();
  delay(1000);
}

void open_door() {
  move_servo(servo_door, servo_door_position, SERVO_DOOR_ANGLE_OPEN);
}

void close_door() {
  move_servo(servo_door, servo_door_position, SERVO_DOOR_ANGLE_CLOSED);
}

void move_servo(Servo& servo, int& from_rotate, int to_rotate) {
  while (from_rotate != to_rotate) {
    if (from_rotate > to_rotate) {
      from_rotate--;
    }
    if (from_rotate < to_rotate) {
      from_rotate++;
    }
    servo.write(from_rotate);
    delay(10);
  }
}

void start_fan() {
  digitalWrite(FAN_PIN_B_POWER, HIGH);
}

void stop_fan() {
  digitalWrite(FAN_PIN_B_POWER, LOW);
}

void show_password_display() {
  lcd_display.setCursor(0, 0);
  lcd_display.print("Passwort:       ");
  lcd_display.setCursor(0, 1);
  lcd_display.print("                ");
}

void beep() {
   tone(SOUND_PIN, 440);
   delay(200);
   noTone(SOUND_PIN);
   delay(300);
}
void beep_long() {
   tone(SOUND_PIN, 440);
   delay(700);
   noTone(SOUND_PIN);
   delay(300);
}

void play_soil_alert() {
  beep();
  beep();
  beep_long();
}

void play_gas_alert() {
  beep();
  beep_long();
  beep_long();
}

void play_correct_code_sound() {
  beep();
  beep();
  beep();
}

void play_wrong_code_sound() {
  beep_long();
  beep_long();
  beep_long();
}

void check_command_mobile_app() {
  if (Serial.available() > 0) {
    switch (Serial.read()) {
      case 't': {
        int servo_1_angle = Serial.readStringUntil('#').toInt();
        // Sicherheits-Check Servo
        if (servo_1_angle > SERVO_DOOR_ANGLE_OPEN) {
          servo_1_angle = SERVO_DOOR_ANGLE_OPEN;
        }
        move_servo(servo_door, servo_door_position, servo_1_angle);
        break;
      }
      case 'u': {
        int servo_2_angle = Serial.readStringUntil('#').toInt();
        // Sicherheits-Check Servo
        if (servo_2_angle > SERVO_WINDOW_ANGLE_OPEN) {
          servo_2_angle = SERVO_WINDOW_ANGLE_OPEN;
        }
        move_servo(servo_window, servo_window_position, servo_2_angle);
        break;
      }
      case 'v': {
        int value_led_2 = Serial.readStringUntil('#').toInt();
        dimm_inner_light(value_led_2);
        break;
      }
      case 'w': {
        int value_fan = Serial.readStringUntil('#').toInt();
        analogWrite(FAN_PIN_B_POWER, value_fan); // the larger the value, the faster the fan
        break;
      }
      case 'a': {
        digitalWrite(LED_PIN, HIGH); // LED on
        break;
      }
      case 'b': {
        digitalWrite(LED_PIN, LOW); // LED off
        break;
      }
      case 'c': {
        digitalWrite(RELAY_PIN, HIGH); // Relay on
        break;
      }
      case 'd': {
        digitalWrite(RELAY_PIN, LOW); // Relay off
        break;
      }
      case 'e': {
        // do something
        break;
      }
      case 'f': {
        // do something
        break;
      }
      case 'g':{
        noTone(SOUND_PIN);
        break;
      }
      case 'h':{
        Serial.println(get_light_sensor());
        break;
      }
      case 'i':{
        Serial.println(get_gas_sensor());
        break;
      }
      case 'j': {
        Serial.println(get_soil_sensor());
        break;
      }
      case 'k': {
        Serial.println(get_rain_sensor());
        break;
      }
      case 'l': {
        open_door();
        break;
      }
      case 'm': {
        close_door();
        break;
      }
      case 'n': {
        open_window();
        break;
      }
      case 'o': {
        close_window();
        break;
      }
      case 'p': {
        digitalWrite(LED_2_PIN, HIGH);
        break;
      }
      case 'q': {
        digitalWrite(LED_2_PIN, LOW);
        break;
      }
      case 'r': {
        start_fan();
        break;
      }
      case 's': {
        stop_fan();
        break;
      }
    }
  }
}

#ifdef EXTENSION_KIT
// Diese Funktion zeigt verschiedene Daten auf dem Tuerdisplay an
void update_door_display() {
  // sofern gerade eine Passworteingabe stattfindet, das Display nicht aendern
  if (password.length() > 0) {
    return;
  }
  
  switch ((millis() / 1000) % 10) {
    // Passworteingabe
    case 0: {
      show_password_display();
      break;
    }

    // Uhrzeit
    case 5: {
      show_date_time_display();
      break;
    }
  }
}

void show_date_time_display() {
  DateTime now = rtc.now();
  // Datum
  sprintf(display_buffer, "Datum: %02d.%02d.%04d", now.day(),now.month(),now.year());
  lcd_display.setCursor(0, 0);
  lcd_display.print(display_buffer);

  // Uhrzeit
  sprintf(display_buffer, "Uhrzeit:%02d:%02d:%02d", now.hour(),now.minute(),now.second());
  lcd_display.setCursor(0, 1);
  lcd_display.print(display_buffer);
}
#endif
