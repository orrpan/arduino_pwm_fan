#include <ArduinoJson.h> // using ArduinoJson 5

/***
 *  orrpan github
 */

StaticJsonBuffer<256> jsonBuffer;

const int FANS = 4;
const int FAN_INIT = 10;

const int RELAYS = 4;
const int RELAY_INIT = 1;

const int PIN_PWM[FANS] = {3, 5, 9, 10};
const int PIN_RELAY[RELAYS] = {2, 4, 7, 12};

const int FAN_MIN = 0;
const int FAN_MAX = 100;
const int RAW_MIN = 2;
const int RAW_MAX = 158;

String msg;
String val[FANS];
char c;

int pwmCurrent[FANS] = {};
int relayCurrent[RELAYS] = {};

bool debugResponse = false;
bool sendStartMessage = true;

void (*resetFunc)(void) = 0;

void setup()
{

  // PWM init
  // https://arduino.stackexchange.com/questions/25609/set-pwm-frequency-to-25-khz
  // Set the main system clock to 8 MHz.
  noInterrupts();
  CLKPR = _BV(CLKPCE); // enable change of the clock prescaler
  CLKPR = _BV(CLKPS0); // divide frequency by 2
  interrupts();

  // Configure Timer 0 for phase correct PWM @ 25 kHz.
  TCCR0A = 0;            // undo the configuration done by...
  TCCR0B = 0;            // ...the Arduino core library
  TCNT0 = 0;             // reset timer
  TCCR0A = _BV(COM0B1)   // non-inverted PWM on ch. B
           | _BV(WGM00); // mode 5: ph. correct PWM, TOP = OCR0A
  TCCR0B = _BV(WGM02)    // ditto
           | _BV(CS00);  // prescaler = 1
  OCR0A = 160;           // TOP = 160

  // Same for Timer 1.
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  TCCR1A = _BV(COM1A1)   // non-inverted PWM on ch. A
           | _BV(COM1B1) // same on ch. B
           | _BV(WGM11); // mode 10: ph. correct PWM, TOP = ICR1
  TCCR1B = _BV(WGM13)    // ditto
           | _BV(CS10);  // prescaler = 1
  ICR1 = 160;

  // Same for Timer 2.
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;
  TCCR2A = _BV(COM2B1)   // non-inverted PWM on ch. B
           | _BV(WGM20); // mode 5: ph. correct PWM, TOP = OCR2A
  TCCR2B = _BV(WGM22)    // ditto
           | _BV(CS20);  // prescaler = 1
  OCR2A = 160;
  for (auto pin = 0; pin < FANS; pin++)
  {
    analogWrite(PIN_PWM[pin], FAN_INIT);
  }

  // Relay init
  for (auto pin = 0; pin < RELAYS; pin++)
  {
    pinMode(PIN_RELAY[pin], OUTPUT);
    digitalWrite(PIN_RELAY[pin], RELAY_INIT);
  }

  // serial is half of below due to chaning chip speed
  Serial.begin(115200);
  JsonObject &outputJSON = jsonBuffer.createObject();
  outputJSON["status"] = "online";
  outputJSON.printTo(Serial);
  Serial.println();
  jsonBuffer.clear();
}

void loop()
{
  while (Serial.available() > 0)
  {
    delay(10);         // small delay because it can only deliver the bytes so quickly
    c = Serial.read(); // you have to read one byte (one character) at a time
    msg += c;          // append the character to our "command" string
  }
  Serial.flush();
  if (msg.length() > 0 || sendStartMessage)
  {
    JsonObject &inputJSON = jsonBuffer.parseObject(msg);
    JsonObject &outputJSON = jsonBuffer.createObject();
    const char *cmdExists = inputJSON["cmd"];
    const char *idExists = inputJSON["id"];

    if (cmdExists || idExists || sendStartMessage)
    {
      if (inputJSON["cmd"] == "debug")
      {
        debugResponse = !debugResponse;
        outputJSON["debug"] = debugResponse;
      }
      else if (inputJSON["cmd"] == "restart")
      {
        outputJSON["status"] = "offline";
        outputJSON.printTo(Serial);
        delay(10);
        Serial.println();
        resetFunc();
      }
      else
      {
        JsonArray &fansJSON = outputJSON.createNestedArray("fans");
        for (auto id = 0; id < FANS; id++)
        {
          if (inputJSON["id"] == "all" || int(inputJSON["id"]) == id)
          {
            int val = int(inputJSON["value"]);
            if (!setFanValue(id, val))
            {
              outputJSON.remove("fan");
              outputJSON["error"] = String("Unable to set value, id: " + String(id) + ", value: " + String(val));
            }
          }
          JsonObject &fan = jsonBuffer.createObject();
          fan["id"] = id;
          fan["value"] = getFanValue(id);
          fan["relay"] = getRelayValue(id);
          if (debugResponse)
          {
            fan["raw"] = pwmCurrent[id];
          }
          fansJSON.add(fan);
        }
      }

      outputJSON.printTo(Serial);
      Serial.println();
    }
  }
  jsonBuffer.clear();
  sendStartMessage = false;
  msg = "";
}

bool validateFanValue(int val)
{
  if (val >= FAN_MIN && val <= FAN_MAX)
  {
    return true;
  }
  return false;
}

bool validateFanID(int id)
{
  if (id >= 0 && id < FANS)
  {
    return true;
  }
  return false;
}

bool validateRelayID(int id)
{
  if (id >= 0 && id < RELAYS)
  {
    return true;
  }
  return false;
}

int toRawValue(int val)
{
  return map(val, FAN_MIN, FAN_MAX, RAW_MIN, RAW_MAX);
}

int fromRawValue(int val)
{
  return map(val, RAW_MIN, RAW_MAX, FAN_MIN, FAN_MAX);
}

bool setFanValue(int id, int val)
{
  if (validateFanID(id) && validateFanValue(val))
  {
    if (validateRelayID(id))
    {
      if (val <= 0)
      {
        // LOW
        relayCurrent[id] = 0;
        digitalWrite(PIN_RELAY[id], 0);
      }
      else
      {
        // HIGH
        relayCurrent[id] = 1;
        digitalWrite(PIN_RELAY[id], 1);
      }
    }
    pwmCurrent[id] = toRawValue(val);
    analogWrite(PIN_PWM[id], pwmCurrent[id]);
    return true;
  }
  return false;
}

int getFanValue(int id)
{
  if (validateFanID(id))
  {
    return fromRawValue(pwmCurrent[id]);
  }
  return -1;
}

int getRelayValue(int id)
{
  if (validateRelayID(id))
  {
    return relayCurrent[id];
  }
  return -1;
}
