/***
 *  For arduino nano timer1 pin 9 and 10, timer 2 pin 3 
 *  Usage 
 *  {'0':100,'1':-1,'3':25}
 *  -1 is don't change.  
 */

#include <PWM.h> //include PWM library http://forum.arduino.cc/index.php?topic=117425.0

#include <ArduinoJson.h> // using ArduinoJson 5
StaticJsonBuffer<256> jsonBuffer;

const int FANS = 3;
const int SWITCHES = 2;

String msg;
String val[FANS];
char c;

int PIN_PWM[FANS] = {9, 10, 3};
int PWM_CURRENT[FANS] = {80, 80, 80}; // 51=20% duty cycle, 255=100% duty cycle
int PIN_SWITCH[SWITCHES] = {5, 6};

void setup()
{
  InitTimersSafe(); //not sure what this is for, but I think i need it for PWM control?
  bool success =
      SetPinFrequencySafe(PIN_PWM[0], 25000) &&
      SetPinFrequencySafe(PIN_PWM[1], 25000) &&
      SetPinFrequencySafe(PIN_PWM[2], 25000); //set frequency to 25kHz
  pwmWrite(PIN_PWM[0], PWM_CURRENT[0]);       // 51=20% duty cycle, 255=100% duty cycle
  pwmWrite(PIN_PWM[1], PWM_CURRENT[1]);       // 51=20% duty cycle, 255=100% duty cycle
  pwmWrite(PIN_PWM[2], PWM_CURRENT[2]);       // 51=20% duty cycle, 255=100% duty cycle
  pinMode(PIN_SWITCH[0], OUTPUT);
  pinMode(PIN_SWITCH[1], OUTPUT);
  digitalWrite(PIN_SWITCH[0], HIGH);
  digitalWrite(PIN_SWITCH[1], HIGH);

  Serial.begin(115200);
}

void loop()
{
  while (Serial.available() > 0)
  {
    delay(5);          // small delay because it can only deliver the bytes so quickly
    c = Serial.read(); // you have to read one byte (one character) at a time
    msg += c;          // append the character to our "command" string
  }
  Serial.flush();
  if (msg.length() > 0)
  {
    JsonObject &inputJSON = jsonBuffer.parseObject(msg);
    JsonObject &outputJSON = jsonBuffer.createObject();
    JsonArray &fansJSON = outputJSON.createNestedArray("fans");

    int val[FANS] = {inputJSON["0"], inputJSON["1"], inputJSON["2"]};
    int debug = inputJSON["debug"];

    for (auto fanID = 0; fanID < FANS; fanID++)
    {
      if (val[fanID] != -1)
      {
        if (!setFanValue(fanID, val[fanID]))
        {
          outputJSON["error"] = String("Unable to set value, id:" + String(fanID) + ", value: " + String(val[fanID]));
        }
      }
      JsonObject &fan = jsonBuffer.createObject();
      if (debug)
      {
        fan["id"] = fanID;
        fan["raw"] = PWM_CURRENT[fanID];
      }
      fan["value"] = getFanValue(fanID);
      fansJSON.add(fan);
    }
    outputJSON.printTo(Serial);
    Serial.print("\n");
    jsonBuffer.clear();
  }
  msg = "";
}

bool validateFanValue(int val)
{
  if (val >= 0 && val <= 100)
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

int toRawValue(int val)
{
  return map(val, 0, 100, 20, 255);
}

int fromRawValue(int val)
{
  return map(val, 20, 255, 0, 100);
}

bool setFanValue(int id, int val)
{
  if (validateFanValue(id))
  {
    if (validateFanID(id))
    {
      if (id >= 0 & id < SWITCHES)
      {
        if (val <= 0)
        {
          digitalWrite(PIN_SWITCH[id], LOW);
        }
        else
        {
          digitalWrite(PIN_SWITCH[id], HIGH);
        }
      }
      PWM_CURRENT[id] = toRawValue(val);
      pwmWrite(PIN_PWM[id], PWM_CURRENT[id]);
      return true;
    }
    return false;
  }
  return false;
}

int getFanValue(int id)
{
  if (validateFanID(id))
  {
    return fromRawValue(PWM_CURRENT[id]);
  }
  return -1;
}
