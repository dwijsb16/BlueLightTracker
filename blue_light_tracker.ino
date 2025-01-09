#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Adafruit_NeoPixel.h>
#define PIN 8

// More accurate detection system
//  connect LED to digital 4 or GROUND for ambient light sensing
//  connect SCL to analog 5
//  connect SDA to analog 4
//  connect Vin to 3.3-5V DC
//  connect GROUND to common ground
//  some magic numbers for this device from the DN40 application note
#define TCS34725_R_Coef 0.136
#define TCS34725_G_Coef 1.000
#define TCS34725_B_Coef -0.444
#define TCS34725_GA 1.0
#define TCS34725_DF 310.0
#define TCS34725_CT_Coef 3810.0
#define TCS34725_CT_Offset 1391.0
// Autorange class for TCS34725
class tcs34725
{
private:
    struct tcs_agc
    {
        tcs34725Gain_t ag;
        tcs34725IntegrationTime_t at;
        uint16_t mincnt;
        uint16_t maxcnt;
    };
    static const tcs_agc agc_lst[];
    uint16_t agc_cur;
    void setGainTime(void);
    Adafruit_TCS34725 tcs;

public:
    tcs34725(void);
    boolean begin(void);
    uint16_t getData(void);
    void setAmbientLight(void);
    boolean isAvailable, isSaturated;
    uint16_t againx, atime, atime_ms;
    uint16_t r, g, b, c;
    uint16_t ir;
    uint16_t r_comp, g_comp, b_comp, c_comp;
    uint16_t saturation, saturation75;
    float cratio, cpl, ct, lux, maxlux;
};
//
// Gain/time combinations to use and the min/max limits for hysteresis
// that avoid saturation. They should be in order from dim to bright.
//
// Also set the first min count and the last max count to 0 to indicate
// the start and end of the list.
//
const tcs34725::tcs_agc tcs34725::agc_lst[] = {
    {TCS34725_GAIN_60X, TCS34725_INTEGRATIONTIME_700MS, 0, 20000},
    {TCS34725_GAIN_60X, TCS34725_INTEGRATIONTIME_154MS, 4990, 63000},
    {TCS34725_GAIN_16X, TCS34725_INTEGRATIONTIME_154MS, 16790, 63000},
    {TCS34725_GAIN_4X, TCS34725_INTEGRATIONTIME_154MS, 15740, 63000},
    {TCS34725_GAIN_1X, TCS34725_INTEGRATIONTIME_154MS, 15740, 0}};
tcs34725::tcs34725() : agc_cur(0), isAvailable(0), isSaturated(0)
{
}
// initialize the sensor
boolean tcs34725::begin(void)
{
    tcs = Adafruit_TCS34725(agc_lst[agc_cur].at, agc_lst[agc_cur].ag);
    if ((isAvailable = tcs.begin()))
        setGainTime();
    return (isAvailable);
}
// set the sensor for ambient light
void tcs34725::setAmbientLight(void)
{
    tcs.setInterrupt(true); // turn off LED
    // Serial.println("LED OFF- BEGINNING TEST");//
}
// Set the gain and integration time
void tcs34725::setGainTime(void)
{
    tcs.setGain(agc_lst[agc_cur].ag);
    tcs.setIntegrationTime(agc_lst[agc_cur].at);
    atime = int(agc_lst[agc_cur].at);
    atime_ms = ((256 - atime) * 2.4);
    switch (agc_lst[agc_cur].ag)
    {
    case TCS34725_GAIN_1X:
        againx = 1;
        break;
    case TCS34725_GAIN_4X:
        againx = 4;
        break;
    case TCS34725_GAIN_16X:
        againx = 16;
        break;
    case TCS34725_GAIN_60X:
        againx = 60;
        break;
    }
}
// Retrieve data from the sensor and do the calculations
uint16_t tcs34725::getData(void)
{
    uint16_t timeSpentHere = 0; // in ms
    // read the sensor and autorange if necessary
    tcs.getRawData(&r, &g, &b, &c); // Everytime getRaw data takes the amount of tim
    e in a_time
        timeSpentHere = timeSpentHere + atime_ms;
    while (1)
    {
        if (agc_lst[agc_cur].maxcnt && c > agc_lst[agc_cur].maxcnt)
            agc_cur++;
        else if (agc_lst[agc_cur].mincnt && c < agc_lst[agc_cur].mincnt)
            agc_cur--;
        else
            break;
        setGainTime();
        uint16_t delayTime = ((256 - atime) * 2.4 * 2);
        // Serial.print("delayTime "); Serial.println(delayTime, DEC);
        delay(delayTime);               // shock absorber
        tcs.getRawData(&r, &g, &b, &c); // Everytime getRaw data takes the amount of ti
        me in a_time
            timeSpentHere = timeSpentHere + atime_ms;
        timeSpentHere = (timeSpentHere + (delayTime)); // in milliseconds
        break;
    }
    // DN40 calculations
    ir = (r + g + b > c) ? (r + g + b - c) / 2 : 0;
    r_comp = r - ir;
    g_comp = g - ir;
    b_comp = b - ir;
    c_comp = c - ir;
    cratio = float(ir) / float(c);
    saturation = ((256 - atime) > 63) ? 65535 : 1024 * (256 - atime);
    saturation75 = (atime_ms < 150) ? (saturation - saturation / 4) : saturation;
    isSaturated = (atime_ms < 150 && c > saturation75) ? 1 : 0;
    cpl = (atime_ms * againx) / (TCS34725_GA * TCS34725_DF);
    maxlux = 65535 / (cpl * 3);
    lux = (TCS34725_R_Coef * float(r_comp) + TCS34725_G_Coef * float(g_comp) + TCS3 4725_B_Coef * float(b_comp)) / cpl;
    ct = TCS34725_CT_Coef * float(b_comp) / float(r_comp) + TCS34725_CT_Offset;
    // Serial.print("timeSpentHere "); Serial.println(timeSpentHere, DEC);
    return (timeSpentHere);
}
tcs34725 rgb_sensor;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, PIN, NEO_GRB + NEO_KHZ800);
// Global Variables
int CurrentLux = 0;
uint32_t TimeElapsed = 0;
float TotalRadiance = 0.0;
float CurrentRadiance = 0.0;
int BlueLed = 8;
float EmissionsLimit = 0.0;
bool printSwitch30min = true;
bool printSwitch1hr = true;
bool printSwitch1hr30min = true;
bool printSwitch2hr = true;
bool printSwitch2hr30min = true;
bool printSwitch3hr = true;
int loopCount = 0;
int loopBoundary = 0;
void setup(void)
{
    bool success = false;
    Serial.begin(9600);
    strip.begin();
    strip.setBrightness(50);
    strip.show(); // Initialize all pixels to 'off'
    while (!(success = rgb_sensor.begin()))
    {
        Serial.println("problem with sensor!");
        delay(1000);
    }
    rgb_sensor.setAmbientLight();
    colorWipe(strip.Color(0, 255, 0), 500); // Green
    Serial.println("This Test has begun");
}
void loop(void)
{
    rgb_sensor.setAmbientLight();
    uint16_t CurrentLux;
    uint16_t delayGettingData = rgb_sensor.getData();
    // Serial.print("rgb_sensor.atime_ms ");Serial.println(rgb_sensor.atime_ms,DEC);
    TimeElapsed = (TimeElapsed + delayGettingData);
    // Serial.print("TimeElapsed(ms)");Serial.println(TimeElapsed,DEC);
    CurrentLux = rgb_sensor.lux;
    CurrentRadiance = (CurrentLux * 0.001464);
    TotalRadiance = TotalRadiance + CurrentRadiance;
    if (TimeElapsed >= 10000000)
    {
        EmissionsLimit = 100;
    }
    else
    {
        int inSeconds = (TimeElapsed / 1000);
        if (inSeconds == 0)
        {
            inSeconds = 1;
        } // To avoid divide by 0
        EmissionsLimit = 1000000 / inSeconds;
    }
    if (TotalRadiance >= EmissionsLimit)
    {
        Serial.print("Radiance: ");
        Serial.println(TotalRadiance, DEC);
        Serial.print("Emissions Limit: ");
        Serial.println(EmissionsLimit, DEC);
        Serial.print("TimeElapsed(ms)");
        Serial.println(TimeElapsed, DEC);
        colorWipe(strip.Color(255, 0, 0), 500); // Red
        colorWipe(strip.Color(0, 255, 0), 500); // Green
        colorWipe(strip.Color(0, 0, 255), 500); // Blue
        rainbowCycle(100);
    }
    if ((TimeElapsed >= 1800000) && (TimeElapsed < 3600000))
    {
        if (printSwitch30min)
        {
            Serial.println("After 30 Minutes:");
            Serial.print(TotalRadiance, DEC);
            printSwitch30min = false;
        }
    }
    if ((TimeElapsed >= 3600000) && (TimeElapsed < 5400000))
    {
        if (printSwitch1hr)
        {
            Serial.println("After 1 hour:");
            Serial.print(TotalRadiance, DEC);
            printSwitch1hr = false;
        }
    }
    if ((TimeElapsed >= 5400000) && (TimeElapsed < 7200000))
    {
        if (printSwitch1hr30min)
        {
            Serial.println("After 1 hour and 30 minutes:");
            Serial.print(TotalRadiance, DEC);
            printSwitch1hr30min = false;
        }
    }
    if ((TimeElapsed >= 7200000) && (TimeElapsed < 9000000))
    {
        if (printSwitch2hr)
        {
            Serial.println("After 2 hour:");
            Serial.print(TotalRadiance, DEC);
            printSwitch2hr = false;
        }
    }
    if ((TimeElapsed >= 9000000) && (TimeElapsed < 10800000))
    {
        if (printSwitch2hr30min)
        {
            Serial.println("After 2 hours 30 minutes:");
            Serial.print(TotalRadiance, DEC);
            printSwitch2hr30min = false;
        }
    }
    if (TimeElapsed >= 10800000)
    {
        if (printSwitch3hr)
        {
            Serial.println("After 3 hours:");
            Serial.print(TotalRadiance, DEC);
            printSwitch3hr = false;
        }
    }
    delay(1000);
    TimeElapsed = TimeElapsed + 1000;
    // int loopBoundary = (loopCount % 60);
    if (loopBoundary == 0)
    {
        Serial.print("Time Elapsed(millisec): ");
        Serial.println(TimeElapsed, DEC);
        Serial.print("Radiance thus far: ");
        Serial.println(TotalRadiance, DEC);
        loopBoundary = 1;
    }
    loopCount += 1;
}
// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait)
{
    for (uint16_t i = 0; i < strip.numPixels(); i++)
    {
        strip.setPixelColor(i, c);
        strip.show();
        delay(wait);
    }
}
// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait)
{
    uint16_t i, j;
    for (j = 0; j < 256 * 5; j++)
    { // 5 cycles of all colors on wheel
        for (i = 0; i < strip.numPixels(); i++)
        {
            strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
        }
        strip.show();
        delay(wait);
    }
}
// Input a value 0 to 255 to get a color value.
//  The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos)
{
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85)
    {
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    else if (WheelPos < 170)
    {
        WheelPos -= 85;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    else
    {
        WheelPos -= 170;
        return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    }
}