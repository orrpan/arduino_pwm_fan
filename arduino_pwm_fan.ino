#include <PWM.h> //include PWM library http://forum.arduino.cc/index.php?topic=117425.0
#include <ArduinoJson.h> // using ArduinoJson 5

/***
 *  orrpan github
 */

StaticJsonBuffer<256> jsonBuffer;

const int FANS = 3;
const int FAN_INIT = 51;
const int FAN_INIT_FREQ = 25000; // 51=20% duty cycle, 255=100% duty cycle

const int SWITCHES = 3;
const int SWITCH_INIT = 1;

const int PIN_PWM[FANS] = {9, 10, 3};
const int PIN_SWITCH[SWITCHES] = {5, 6, 7};

String msg;
String val[FANS];
char c;

int pwmCurrent[FANS] = {FAN_INIT, FAN_INIT, FAN_INIT};
int switchCurrent[SWITCHES] = {SWITCH_INIT, SWITCH_INIT, SWITCH_INIT};

bool debugResponse = false;
bool sendStartMessage = true;

void (*resetFunc)(void) = 0;

void setup()
{
  InitTimersSafe(); //not sure what this is for, but I think i need it for PWM control?
  bool success =
      SetPinFrequencySafe(PIN_PWM[0], FAN_INIT_FREQ) &&
      SetPinFrequencySafe(PIN_PWM[1], FAN_INIT_FREQ) &&
      SetPinFrequencySafe(PIN_PWM[2], FAN_INIT_FREQ); //set frequency to 25kHz
  pwmWrite(PIN_PWM[0], pwmCurrent[0]);               // 51=20% duty cycle, 255=100% duty cycle
  pwmWrite(PIN_PWM[1], pwmCurrent[1]);               // 51=20% duty cycle, 255=100% duty cycle
  pwmWrite(PIN_PWM[2], pwmCurrent[2]);               // 51=20% duty cycle, 255=100% duty cycle
  pinMode(PIN_SWITCH[0], OUTPUT);
  pinMode(PIN_SWITCH[1], OUTPUT);
  pinMode(PIN_SWITCH[2], OUTPUT);
  digitalWrite(PIN_SWITCH[0], switchCurrent[0]);
  digitalWrite(PIN_SWITCH[1], switchCurrent[1]);
  digitalWrite(PIN_SWITCH[2], switchCurrent[2]);

  Serial.begin(115200);
  JsonObject &outputJSON = jsonBuffer.createObject();
  if (success)
  {
    outputJSON["status"] = "online";
  }
  else
  {
    outputJSON["status"] = "error";
  }
  outputJSON.printTo(Serial);
  Serial.println();
  jsonBuffer.clear();
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
    const char *cmdExists = inputJSON["cmd"];
    const char *idExists = inputJSON["id"];

    if (cmdExists || idExists)
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
        delay(5);
        Serial.println();
        resetFunc();
      }
      else
      {
        JsonArray &fanJSON = outputJSON.createNestedArray("fan");
        int fromID = 0;
        int toID = FANS;
        if (inputJSON["id"] != "all")
        {
          fromID = int(inputJSON["id"]) + 0;
          toID = int(inputJSON["id"]) + 1;
        }

        for (auto id = fromID; id < toID; id++)
        {
          int val = inputJSON["value"];
          if (!setFanValue(id, val))
          {
            outputJSON["error"] = String("Unable to set value, id:" + String(id) + ", value: " + String(val));
          }

          JsonObject &fan = jsonBuffer.createObject();
          fan["id"] = id;
          fan["value"] = getFanValue(id);
          fan["switch"] = getSwitchValue(id);
          if (debugResponse)
          {
            fan["raw"] = pwmCurrent[id];
          }
          fanJSON.add(fan);
        }
      }

      outputJSON.printTo(Serial);
      Serial.println();
      jsonBuffer.clear();
    }
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
  if (validateFanID(id) && validateFanValue(val))
  {
    if (validateSwitchID(id))
    {
      if (val <= 0)
      {
        // LOW
        switchCurrent[id] = 0;
        digitalWrite(PIN_SWITCH[id], 0);
      }
      else
      {
        // HIGH
        switchCurrent[id] = 1;
        digitalWrite(PIN_SWITCH[id], 1);
      }
    }
    pwmCurrent[id] = toRawValue(val);
    pwmWrite(PIN_PWM[id], pwmCurrent[id]);
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

int getSwitchValue(int id)
{
  if (validateSwitchID(id))
  {
    return switchCurrent[id];
  }
  return -1;
}
