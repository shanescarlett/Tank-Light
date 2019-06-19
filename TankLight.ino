#include <math.h>
#include <Wire.h>
#include "StringSplitter.h"
#include "DS3231.h"

const int FAN_PIN = 12;
const int ADJ_PIN = A0;
const int OVR_PIN = 13;
const float MIN_BRIGHTNESS = 0.0;
const float GAMMA = 2.2;
const float T_SUNRISE = 10.5;
const float T_SUNSET = 19.5;
const float FADE_TIME = 4.0;
String serialBuffer = "";
volatile float setMaxBrightness = 0;
volatile int overrideSwitch = 0;

struct ts timeStruct;
float sunriseFadeStartTime;
float sunriseFadeEndTime;
float sunsetFadeStartTime;
float sunsetFadeEndTime;

void setup()
{
  Serial.begin(9600);
  Serial.println("Initialising ...");
  initI2C();
  Serial.println("I2C initialised.");
  initPWM();
  Serial.println("PWM initialised.");
  initFan();
  Serial.println("Fan initialised.");
  initAdjust();
  DS3231_init(DS3231_CONTROL_INTCN);
  Serial.println("DS3231 initialised.");
  initValues();
  Serial.println("Calculations done.");
}

void loop()
{
  serialLoop();
  timeLoop();
  lightingLoop();
}

void initI2C()
{
  pinMode(SDA, INPUT);
  pinMode(SCL, INPUT);
  Wire.begin();
}

void initPWM()
{
  DDRB  |= _BV(PB1) | _BV(PB2);       /* set pins as outputs */
  TCCR1A = _BV(COM1A1) | _BV(COM1B1)  /* non-inverting PWM */
        | _BV(WGM11);                 /* mode 14: fast PWM, TOP=ICR1 */
  TCCR1B = _BV(WGM13) | _BV(WGM12)
        | _BV(CS10);                  /* prescaler 1 */
  ICR1 = 4096;                         /* TOP counter value (freeing OCR1A*/
}

void initFan()
{
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);
}

void initAdjust()
{
  pinMode(ADJ_PIN, INPUT);
  pinMode(OVR_PIN, INPUT);
  digitalWrite(OVR_PIN, LOW);
}

void initValues()
{
  sunriseFadeStartTime = T_SUNRISE - FADE_TIME/2;
  sunriseFadeEndTime = T_SUNRISE + FADE_TIME/2;
  sunsetFadeStartTime = T_SUNSET - FADE_TIME/2;
  sunsetFadeEndTime = T_SUNSET + FADE_TIME/2;
}

void setPWM(float value)
{
  float range = setMaxBrightness - MIN_BRIGHTNESS;
  float boundedValue = MIN_BRIGHTNESS + (range * value);
  float correctedValue = pow(boundedValue, GAMMA);
  uint16_t intValue = round(correctedValue * 4095);
  OCR1A = intValue;
}

void setFan(float value)
{
  if(value > 0)
  {
    digitalWrite(FAN_PIN, HIGH);
  }
  else
  {
    digitalWrite(FAN_PIN, LOW);
  }
}

void readAdjustment()
{
  int rawValue = analogRead(ADJ_PIN);
  setMaxBrightness = ((float)rawValue)/((float)1023);
  overrideSwitch = digitalRead(OVR_PIN);
}

void serialLoop()
{
  while (Serial.available() > 0)
    {
        char recieved = Serial.read();
        serialBuffer += recieved; 

        if(serialBuffer.length() > 64)
        {
          serialBuffer.remove(0,1);
        }
        if (recieved == '\n')
        {
            processSerialInput(serialBuffer);
            serialBuffer = "";
        }
    }
}

void timeLoop()
{
  DS3231_get(&timeStruct);
}

void lightingLoop()
{
  float currentHour = timeToHourFraction(timeStruct);
  float lightingValue = calculateLighting(currentHour);
  readAdjustment();
  setPWM(lightingValue);
  setFan(lightingValue);
}

float calculateLighting(float currentHour)
{
  if(overrideSwitch == 0)
  {
    if(currentHour < sunriseFadeStartTime)
    {
      return 0.0;
    }
    else if(currentHour >= sunriseFadeStartTime && currentHour < sunriseFadeEndTime)
    {
      float progress = (currentHour - sunriseFadeStartTime)/FADE_TIME;
      return (progress);
    }
    else if(currentHour >= sunriseFadeEndTime && currentHour < sunsetFadeStartTime)
    {
      return 1.0;
    }
    else if(currentHour >= sunsetFadeStartTime && currentHour < sunsetFadeEndTime)
    {
      float progress = (currentHour - sunsetFadeStartTime)/FADE_TIME;
      return (1.0 - progress);
    }
    else
    {
      return 0.0;
    }
  }
  else
  {
    return 1.0;
  }
}

void processSerialInput(String input)
{
  //Set time with serial input of the format
  //Timeset:YYYY:MM:DD:HH:mm:SS
  if(input.startsWith("Timeset:"))
  {
    struct ts timeSet;
//    Serial.println(input);
//    StringSplitter *splitter = new StringSplitter(input, ':', 7);
//    timeSet.year = (splitter->getItemAtIndex(1)).toInt();
//    timeSet.mon = (splitter->getItemAtIndex(2)).toInt();
//    timeSet.mday = (splitter->getItemAtIndex(3)).toInt();
//    timeSet.hour = (splitter->getItemAtIndex(4)).toInt();
//    timeSet.min = (splitter->getItemAtIndex(5)).toInt();
//    timeSet.sec = (splitter->getItemAtIndex(6)).toInt();

    timeSet.year = 2019;
    timeSet.mon = 6;
    timeSet.mday = 17;
    timeSet.hour = 12;
    timeSet.min = 39;
    timeSet.sec = 00;
    
    DS3231_set(timeSet);
    Serial.println("Time set complete.");
  }
  else if(input.startsWith("Timeget"))
  {
    Serial.print(timeStruct.year);
    Serial.print("-");
    Serial.print(timeStruct.mon);
    Serial.print("-");
    Serial.print(timeStruct.mday);
    Serial.print(" ");
    Serial.print(timeStruct.hour);
    Serial.print(":");
    Serial.print(timeStruct.min);
    Serial.print(":");
    Serial.println(timeStruct.sec);
  }
  else if(input.startsWith("Light"))
  {
    float currentHour = timeToHourFraction(timeStruct);  
    Serial.print("Current lighting: ");
    Serial.println(calculateLighting(currentHour));
  }
  else
  {
    Serial.println("Unrecognised command");
  }
}

//Helper functions
float timeToHourFraction(struct ts t)
{
  float hourPercentage = ((float)t.min)/60.0 + ((float)t.sec)/3600.0 ;
  return ((float)t.hour) + hourPercentage;
}
