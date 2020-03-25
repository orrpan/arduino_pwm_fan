/***
 *  orrpan github
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
int SWITCH_CURRENT[SWITCHES] = {1, 1};

bool debugResponse = false;
bool sendStartMessage = true;

void (*resetFunc)(void) = 0;

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
  digitalWrite(PIN_SWITCH[0], SWITCH_CURRENT[0]);
  digitalWrite(PIN_SWITCH[1], SWITCH_CURRENT[1]);

  Serial.begin(115200);
  Serial.println("Online");
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
  if (msg.length() > 0 || sendStartMessage)
  {
    JsonObject &inputJSON = jsonBuffer.parseObject(msg);
    const char *cmdExists = inputJSON["cmd"];

    if (cmdExists)
    {
      if (inputJSON["cmd"] == "restart")
      {
        Serial.println("restart");
        delay(2);
        resetFunc();
      }
      else if (inputJSON["cmd"] == "debug")
      {
        debugResponse = !debugResponse;
        delay(2);
      }
    }

    const char *fanAllExists = inputJSON["all"];
    const char *fanExists[FANS] = {inputJSON["0"], inputJSON["1"], inputJSON["2"]};
    int fanValue[FANS] = {inputJSON["0"], inputJSON["1"], inputJSON["2"]};
    if (fanAllExists)
    {
      fanValue[0] = inputJSON["all"];
      fanValue[1] = inputJSON["all"];
      fanValue[2] = inputJSON["all"];
    }

    if (fanExists[0] || fanExists[1] || fanExists[2] || fanAllExists || sendStartMessage)
    {
      JsonObject &outputJSON = jsonBuffer.createObject();
      JsonArray &fansJSON = outputJSON.createNestedArray("fans");

      for (auto fanID = 0; fanID < FANS; fanID++)
      {
        if (fanExists[fanID] || fanAllExists)
        {
          if (!setFanValue(fanID, fanValue[fanID]))
          {
            outputJSON["error"] = String("Unable to set value, id:" + String(fanID) + ", value: " + String(fanValue[fanID]));
          }
        }

        JsonObject &fan = jsonBuffer.createObject();
        if (debugResponse)
        {
          fan["raw"] = PWM_CURRENT[fanID];
          if (validateSwitchID(fanID))
          {
            fan["switch"] = SWITCH_CURRENT[fanID];
          }
        }
        fan["id"] = fanID;
        fan["value"] = getFanValue(fanID);
        fansJSON.add(fan);
      }
      outputJSON.printTo(Serial);
      Serial.print("\n");
    }
    jsonBuffer.clear();
  }
  sendStartMessage = false;
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

bool validateSwitchID(int id)
{
  if (id >= 0 && id < SWITCHES)
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
      if (validateSwitchID(id))
      {
        if (val <= 0)
        {
          // LOW
          SWITCH_CURRENT[id] = 0;
          digitalWrite(PIN_SWITCH[id], 0);
        }
        else
        {
          // HIGH
          SWITCH_CURRENT[id] = 1;
          digitalWrite(PIN_SWITCH[id], 1);
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

int getSwitchValue(int id)
{
  if (validateSwitchID(id))
  {
    return SWITCH_CURRENT[id];
  }
  return -1;
}
