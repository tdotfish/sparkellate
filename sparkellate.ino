//******************************************************
//
//                   "Sparkellate"
//     Two-stage Code for HackerBox Tessellation PCB
//
// by T.Fish 
//
// Github:  (https://github.com/tdotfish/)
// Twitter: @tdotfish
//
// Incorporates code from the FastLED Demo Reel by Mark Kreigsman
// https://github.com/FastLED/FastLED/blob/master/examples/DemoReel100/DemoReel100.ino
// With code provided by HackerBoxes (https://hackerboxes.com)
// https://www.instructables.com/HackerBox-0059-Tessellate/
//
// Enables the polyhedron to display a pattern when sitting stationary
// while switching to the default tilt color change effect when moved around
//
//******************************************************


//////////////////////////////////////////////////
//
//              Tilt Color Demo for
//      Tessellation PCB from HackerBox 0059
//
//                    uuuuuuu
//                uu$$$$$$$$$$$uu
//             uu$$$$$$$$$$$$$$$$$uu
//            u$$$$$$$$$$$$$$$$$$$$$u
//           u< WWW.HACKERBOXES.COM >u
//          u$$$$$$$$$$$$$$$$$$$$$$$$$u
//          u$$$$$$$$$$$$$$$$$$$$$$$$$u
//          u$$$$$$"   "$$$"   "$$$$$$u
//          "$$$$"      u$u       $$$$"
//           $$$u       u$u       u$$$
//           $$$u      u$$$u      u$$$
//            "$$$$uu$$$   $$$uu$$$$"
//             "$$$$$$$"   "$$$$$$$"
//               u$$$$$$$u$$$$$$$u
//                u$"$"$"$"$"$"$u
//     uuu        $$u$ $ $ $ $u$$       uuu
//    u$$$$        $$$$$u$u$u$$$       u$$$$
//     $$$$$uu      "$$$$$$$$$"     uu$$$$$$
//   u$$$$$$$$$$$uu    """""    uuuu$$$$$$$$$$
//   $$$$"""$$$$$$$$$$uuu   uu$$$$$$$$$"""$$$"
//    """      ""$$$$$$$$$$$uu ""$"""
//              uuuu ""$$$$$$$$$$uuu
//     u$$$uuu$$$$$$$$$uu ""$$$$$$$$$$$uuu$$$
//     $$$$$$$$$$""""           ""$$$$$$$$$$$"
//      "$$$$$"                      ""$$$$""
//        $$$"                         $$$$"
//
//
// ADXL345 I2C code modified from milesburton.com
//               which was modified from SparkFun
// LED control uses FastLED Library
//////////////////////////////////////////////////

#include <Wire.h>
#include <FastLED.h>

FASTLED_USING_NAMESPACE

#define NUM_LEDS 120
#define DATA_PIN 25
#define FRAMES_PER_SECOND  120
CRGB leds[NUM_LEDS];

// ADXL345 I2C address is 0x53(83)
#define Addr 0x53

// ADXL345 I2C Pins
const uint8_t scl = 16;
const uint8_t sda = 17; 

float xAccl, yAccl, zAccl;
float prev_xAccl = 0, prev_yAccl = 0, prev_zAccl = 0;
float diff_x = 0, diff_y = 0, diff_z = 0;
unsigned long start_time;

uint8_t moved = 0;
uint8_t firstcycle = 0;

//uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void ReadADXL()
{
  unsigned int data[6];

  // Start I2C Transmission
  Wire.beginTransmission(Addr);
  // Select bandwidth rate register
  Wire.write(0x2C);
  // Normal mode, Output data rate = 100 Hz
  Wire.write(0x0A);
  // Stop I2C transmission
  Wire.endTransmission();

  // Start I2C Transmission
  Wire.beginTransmission(Addr);
  // Select power control register
  Wire.write(0x2D);
  // Auto-sleep disable
  Wire.write(0x08);
  // Stop I2C transmission
  Wire.endTransmission();

  // Start I2C Transmission
  Wire.beginTransmission(Addr);
  // Select data format register
  Wire.write(0x31);
  // Self test disabled, 4-wire interface, Full resolution, Range = +/-2g
  Wire.write(0x08);
  // Stop I2C transmission
  Wire.endTransmission();
  delay(100);

  for (int i = 0; i < 6; i++)
  {
    // Start I2C Transmission
    Wire.beginTransmission(Addr);
    // Select data register
    Wire.write((50 + i));
    // Stop I2C transmission
    Wire.endTransmission();

    // Request 1 byte of data
    Wire.requestFrom(Addr, 1);

    // Read 6 bytes of data
    // xAccl lsb, xAccl msb, yAccl lsb, yAccl msb, zAccl lsb, zAccl msb
    if (Wire.available() == 1)
    {
      data[i] = Wire.read();
    }
  }

  prev_xAccl = xAccl;
  prev_yAccl = yAccl;
  prev_zAccl = zAccl;
  
  // Convert the data to 10-bits
  xAccl = (((data[1] & 0x03) * 256) + data[0]);
  if (xAccl > 511)
  {
    xAccl -= 1024;
  }
  yAccl = (((data[3] & 0x03) * 256) + data[2]);
  if (yAccl > 511)
  {
    yAccl -= 1024;
  }
  zAccl = (((data[5] & 0x03) * 256) + data[4]);
  if (zAccl > 511)
  {
    zAccl -= 1024;
  }
  
  diff_x = xAccl - prev_xAccl;
  diff_y = yAccl - prev_yAccl;
  diff_z = zAccl - prev_zAccl;
}

void setup()
{
  delay(3000); // 3 second delay for recovery
  // Initialize LEDs 
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

  FastLED.setBrightness(96);
  
  // Initialize I2C communication for ADXL345
  Wire.begin(sda, scl);
  
  // Initialize serial communication, set baud rate to 9600
  Serial.begin(9600);
}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { confetti, motionSense };

void loop()
{
  gPatterns[moved]();
  FastLED.show();
  FastLED.delay(1000/FRAMES_PER_SECOND); 
  if( moved == 0 ){
    
    // Reading the ADXL every cycle causes the confetti effect to perform
    // slowly, so read it less often when the object is stationary.
    // The trade off is a delay in changing modes when you move the object.
    // Decreasing this number will make the mode change more responsive
    // at the expense of a slower sparkle effect.
    EVERY_N_MILLISECONDS(1000) { ReadADXL(); }
    
  } else {
    ReadADXL();
  }
  if( diff_x > 10 || diff_y > 10 || diff_z > 10 ){
    moved = 1;
  } else { 
    if (moved == 1){
      if(!firstcycle){
        firstcycle = 1;
        start_time = millis();
      } else {
        if(millis() - start_time > 5000){
          firstcycle = 0;
          moved = 0;
        }
      }
    }
  }

  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow

  // Output data to serial monitor
 /* 
  Serial.print("Acceleration in X-Axis : ");
  Serial.println(xAccl);
  Serial.print("Acceleration in Y-Axis : ");
  Serial.println(yAccl);
  Serial.print("Acceleration in Z-Axis : ");
  Serial.println(zAccl);
  delay(20);
  */
}

void motionSense()
{
  int r,g,b;

  //changing these mappings from Accl to RGB defines
  //how the LED colors respond to tilt of the PCB
  
  r=(int)xAccl;
  r=abs(r);
  r=r*0.2;
  r=min(50,r);

  g=(int)yAccl;
  g=abs(g);
  g=g*0.2;
  g=min(50,g);
  
  b=(int)zAccl;
  b=abs(b);
  b=b*0.2;
  b=min(50,b);
   
  for (int i=0;i<NUM_LEDS;i++)
  {
    leds[i] = CRGB(r,g,b);
  }
  FastLED.show();
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}
