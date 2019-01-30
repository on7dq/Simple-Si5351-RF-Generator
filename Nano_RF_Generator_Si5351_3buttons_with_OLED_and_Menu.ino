//Simple RF generator with only 3 buttons and small OLED display

// materials
// Arduino Nano
// Si5351 breakout module
// 3 pushbuttons
// OLED display

// Connections
// Si5351 : 5V and GND, SDA to A4 , SCL to A5
// LCD    : 5V and GND, SDA to A4 , SCL to A5
// buttons from D2, D7 and D12 to GND (any digital pin is ok)

//libraries
#include <Wire.h>               // for I2C control
#include <LiquidCrystal_I2C.h>  // for LCD via I2C control

#include "SSD1306Ascii.h"       // text only on OLED , saves a lot of memory
#include "SSD1306AsciiWire.h"
SSD1306AsciiWire oled;          // create an oled object

// buttons
int stepPin = 2;        // step / menu / exit
int downPin = 7;        // down / band down
int upPin = 12;         // up   / band up
int count = 0;          // counter for press time
int longTime = 500;     // limit for short/long press

const int debounceDelay = 50; // milliseconds to wait until stable

//some variables

unsigned long f = 100000000;         // the Si5351 main frequency in Hz
unsigned long stp = 10;             // initial step in Hz
unsigned long offset = 10700000;    // IF frequency of receiver, if needed
int power = 4;                      // 4 steps of power

// pre-programmed steps, adapt as needed
unsigned long steps[] = { 10, 100, 1000,  5000, 100000, 1000000, 10000000};
String stepLabels[] = {"10Hz",  "100Hz" , "1kHz" , "5kHz" , "100kHz" , "1MHz", "10MHz"};
int stepIndex = 0, maxStep = 7;

boolean menu = false; // true when in menu mode

void setup() {
  //I2C
  Wire.begin();
  //Serial.begin(9600); // for debugging
  //LCD
  oled.begin(&Adafruit128x64, 0x3C);
  oled.setFont(Arial_bold_14);
  oled.clear();
  oled.println("RF Generator");
  oled.println();
  oled.println("** (c) ON7DQ **");
  oled.println("** 15/1/2019 **");
  // configure buttons
  pinMode(stepPin, INPUT);
  digitalWrite(stepPin, HIGH); // turn on internal pull-up on the inputPin
  pinMode(downPin, INPUT);
  digitalWrite(downPin, HIGH); // turn on internal pull-up
  pinMode(upPin, INPUT);
  digitalWrite(upPin, HIGH); // turn on internal pull-up

  //Si5351
  SetFrequency (f);           // Set start frequency
  // or this
  // SetFrequency (f + offset);           // Set start frequency for IF offset

  SetParkMode ();
  TX_ON();                            // Switches transmitter on ...
  SetPower(power);                    // with maximum power

  //Serial.print (" PLL started on ");
  //Serial.println (f);



  delay(2000);
  display(); // clear the text and display data
}

// the main loop
void loop() {
  if (!menu) {



    count = 0;
    while (digitalRead(stepPin) == LOW) {
      count++;
      delay(1);
    }
    if (count > 50 && count < longTime) {
      // change tuning step, see predefined steps above
      stepIndex++;
      if (stepIndex == maxStep) stepIndex = 0;
      stp = steps[stepIndex];

      //Serial.println("New step ");
      //Serial.println(f);

      display();
    }
    if (count > longTime) {
      menu = !menu; // long press toggles MENU mode
      oled.clear();
      oled.println("SET PWR UP/DOWN");
    }



    // ToDo change these into own debounce code too
    if (debounce (downPin)) {
      f = f - stp;
      TX_OFF();
      SetFrequency(f);
      TX_ON();
      //Serial.println("New freq ");
      //Serial.println(f);
      display();
    }

    if (debounce (upPin)) {
      f = f + stp;
      TX_OFF();
      SetFrequency(f);
      TX_ON();
      //Serial.println("New freq ");

      //Serial.println(f);
      display();
    }
  }
  else { // MENU items
    if (debounce (downPin)) {
      power--;
      if (power == -1) power = 4;
      oled.print("PWR = ");
      oled.println(power);
    }

    if (debounce (upPin)) {
      power++;
      if (power == 5) power = 0;
      oled.print("PWR = ");
      oled.println(power);
    }

    count = 0;
    while (digitalRead(stepPin) == LOW) {
      count++;
      delay(1);
    }
    if (count > longTime) {
      menu = !menu; // long press toggles MENU mode
      display();
    }
  }

  delay (100);
}

void display() {
  // send to display
  oled.clear();
  oled.setCursor(0, 0);
  oled.println("Frequency = ");
  oled.print(f / 1000.0);
  oled.println(" kHz");
  oled.print("Step = ");
  oled.println(stepLabels[stepIndex]);
  oled.print("PWR = ");
  oled.println(power);
}

boolean debounce(int pin)
{
  boolean state;
  boolean previousState;
  previousState = digitalRead(pin); // store switch state
  for (int counter = 0; counter < debounceDelay; counter++)
  {
    delay(1); // wait for 1 millisecond
    state = digitalRead(pin); // read the pin
    if ( state != previousState)
    {
      counter = 0; // reset the counter if the state changes
      previousState = state; // and save the current state
    }
  }
  // here when the switch state has been stable longer than the debounce period
  if (state == LOW) // LOW means pressed (because pull-ups are used)
    return true;
  else
    return false;
}


// *************************************************************************************************
// functions from OE1CWG

void TX_ON () {                        // Enables output on CLK0 and disables Park Mode on CLK1
  Si5351a_Write_Reg (17, 128);         // Disable output CLK1
  Si5351a_Write_Reg (16, 79);          // Enable output CLK0, set crystal as source and Integer Mode on PLLA
}

void TX_OFF () {                       // Disables output on CLK0 and enters Park Mode on CLK1
  Si5351a_Write_Reg (16, 128);         // Disable output CLK0
  Si5351a_Write_Reg (17, 111);         // Enable output CLK1, set crystal as source and Integer Mode on PLLB
}

void SetFrequency (uint32_t frequency) { // Frequency in Hz; must be within [7810 Hz to 200 Mhz]
#define F_XTAL 25000000;             // Frequency of Quartz-Oszillator
#define c 1048574;                   // "c" part of Feedback-Multiplier from XTAL to PLL
  uint32_t fvco;                  // VCO frequency (600-900 MHz) of PLL
  uint32_t outdivider;            // Output divider in range [4,6,8-900], even numbers preferred
  byte R = 1;                          // Additional Output Divider in range [1,2,4,...128]
  byte a;                              // "a" part of Feedback-Multiplier from XTAL to PLL in range [15,90]
  uint32_t b;                     // "b" part of Feedback-Multiplier from XTAL to PLL
  float f;                             // floating variable, needed in calculation
  uint32_t MS0_P1;                // Si5351a Output Divider register MS0_P1, P2 and P3 are hardcoded below
  uint32_t MSNA_P1;               // Si5351a Feedback Multisynth register MSNA_P1
  uint32_t MSNA_P2;               // Si5351a Feedback Multisynth register MSNA_P2
  uint32_t MSNA_P3;               // Si5351a Feedback Multisynth register MSNA_P3

  outdivider = 900000000 / frequency;  // With 900 MHz beeing the maximum internal PLL-Frequency

  while (outdivider > 900) {           // If output divider out of range (>900) use additional Output divider
    R = R * 2;
    outdivider = outdivider / 2;
  }
  if (outdivider % 2) outdivider--;    // finds the even divider which delivers the intended Frequency

  fvco = outdivider * R * frequency;   // Calculate the PLL-Frequency (given the even divider)

  switch (R) {                         // Convert the Output Divider to the bit-setting required in register 44
    case 1: R = 0; break;              // Bits [6:4] = 000
    case 2: R = 16; break;             // Bits [6:4] = 001
    case 4: R = 32; break;             // Bits [6:4] = 010
    case 8: R = 48; break;             // Bits [6:4] = 011
    case 16: R = 64; break;            // Bits [6:4] = 100
    case 32: R = 80; break;            // Bits [6:4] = 101
    case 64: R = 96; break;            // Bits [6:4] = 110
    case 128: R = 112; break;          // Bits [6:4] = 111
  }

  a = fvco / F_XTAL;                   // Multiplier to get from Quartz-Oscillator Freq. to PLL-Freq.
  f = fvco - a * F_XTAL;               // Multiplier = a+b/c
  f = f * c;                           // this is just "int" and "float" mathematics
  f = f / F_XTAL;
  b = f;

  MS0_P1 = 128 * outdivider - 512;     // Calculation of Output Divider registers MS0_P1 to MS0_P3
  // MS0_P2 = 0 and MS0_P3 = 1; these values are hardcoded, see below

  f = 128 * b / c;                     // Calculation of Feedback Multisynth registers MSNA_P1 to MSNA_P3
  MSNA_P1 = 128 * a + f - 512;
  MSNA_P2 = f;
  MSNA_P2 = 128 * b - MSNA_P2 * c;
  MSNA_P3 = c;

  Si5351a_Write_Reg (16, 128);                      // Disable output during the following register settings
  Si5351a_Write_Reg (26, (MSNA_P3 & 65280) >> 8);   // Bits [15:8] of MSNA_P3 in register 26
  Si5351a_Write_Reg (27, MSNA_P3 & 255);            // Bits [7:0]  of MSNA_P3 in register 27
  Si5351a_Write_Reg (28, (MSNA_P1 & 196608) >> 16); // Bits [17:16] of MSNA_P1 in bits [1:0] of register 28
  Si5351a_Write_Reg (29, (MSNA_P1 & 65280) >> 8);   // Bits [15:8]  of MSNA_P1 in register 29
  Si5351a_Write_Reg (30, MSNA_P1 & 255);            // Bits [7:0]  of MSNA_P1 in register 30
  Si5351a_Write_Reg (31, ((MSNA_P3 & 983040) >> 12) | ((MSNA_P2 & 983040) >> 16)); // Parts of MSNA_P3 und MSNA_P1
  Si5351a_Write_Reg (32, (MSNA_P2 & 65280) >> 8);   // Bits [15:8]  of MSNA_P2 in register 32
  Si5351a_Write_Reg (33, MSNA_P2 & 255);            // Bits [7:0]  of MSNA_P2 in register 33
  Si5351a_Write_Reg (42, 0);                        // Bits [15:8] of MS0_P3 (always 0) in register 42
  Si5351a_Write_Reg (43, 1);                        // Bits [7:0]  of MS0_P3 (always 1) in register 43
  Si5351a_Write_Reg (44, ((MS0_P1 & 196608) >> 16) | R);  // Bits [17:16] of MS0_P1 in bits [1:0] and R in [7:4]
  Si5351a_Write_Reg (45, (MS0_P1 & 65280) >> 8);    // Bits [15:8]  of MS0_P1 in register 45
  Si5351a_Write_Reg (46, MS0_P1 & 255);             // Bits [7:0]  of MS0_P1 in register 46
  Si5351a_Write_Reg (47, 0);                        // Bits [19:16] of MS0_P2 and MS0_P3 are always 0
  Si5351a_Write_Reg (48, 0);                        // Bits [15:8]  of MS0_P2 are always 0
  Si5351a_Write_Reg (49, 0);                        // Bits [7:0]   of MS0_P2 are always 0
  if (outdivider == 4) {
    Si5351a_Write_Reg (44, 12 | R);                 // Special settings for R = 4 (see datasheet)
    Si5351a_Write_Reg (45, 0);                      // Bits [15:8]  of MS0_P1 must be 0
    Si5351a_Write_Reg (46, 0);                      // Bits [7:0]  of MS0_P1 must be 0
  }
  Si5351a_Write_Reg (177, 32);                      // This resets PLL A
}

void SetParkMode () {                               // Sets CLK1 to the Park Mode frequency of 150 MHz to keep the Si5351a warm during key-up
  Si5351a_Write_Reg (17, 128);                      // Disable output during the following register settings
  Si5351a_Write_Reg (34, 255);                      // Bits [15:8] of MSNB_P3
  Si5351a_Write_Reg (35, 254);                      // Bits [7:0]  of MSNB_P3
  Si5351a_Write_Reg (36, 0);                        // Bits [17:16] of MSNB_P1 in bits [1:0]
  Si5351a_Write_Reg (37, 14);                       // Bits [15:8]  of MSNB_P1
  Si5351a_Write_Reg (38, 169);                      // Bits [7:0]  of MSNB_P1
  Si5351a_Write_Reg (39, 252);                      // Parts of MSNB_P3 und MSNB_P1
  Si5351a_Write_Reg (40, 130);                      // Bits [15:8]  of MSNB_P2
  Si5351a_Write_Reg (41, 82);                       // Bits [7:0]  of MSNB_P2
  Si5351a_Write_Reg (50, 0);                        // Bits [15:8] of MS1_P3
  Si5351a_Write_Reg (51, 1);                        // Bits [7:0]  of MS1_P3
  Si5351a_Write_Reg (52, 0);                        // Bits [17:16] of MS1_P1 in bits [1:0] and R in [7:4]
  Si5351a_Write_Reg (53, 1);                        // Bits [15:8]  of MS1_P1
  Si5351a_Write_Reg (54, 0);                        // Bits [7:0]  of MS1_P1
  Si5351a_Write_Reg (55, 0);                        // Bits [19:16] of MS1_P2 and MS1_P3
  Si5351a_Write_Reg (56, 0);                        // Bits [15:8]  of MS1_P2
  Si5351a_Write_Reg (57, 0);                        // Bits [7:0]   of MS1_P2
  Si5351a_Write_Reg (177, 128);                     // This resets PLL B
}

void SetPower (byte power) {                        // Sets the output power level
  if (power == 0 || power > 4) {
    power = 0; // valid power values are 0 (25%), 1 (50%), 2 (75%) or 3 (100%)
  }
  switch (power) {
    case 1:
      Si5351a_Write_Reg (16, 76);                   // CLK0 drive strength = 2mA; power level ~ -8dB
      break;
    case 2:
      Si5351a_Write_Reg (16, 77);                   // CLK0 drive strength = 4mA; power level ~ -3dB
      break;
    case 3:
      Si5351a_Write_Reg (16, 78);                   // CLK0 drive strength = 6mA; power level ~ -1dB
      break;
    case 4:
      Si5351a_Write_Reg (16, 79);                   // CLK0 drive strength = 8mA; power level := 0dB
      break;
  }
}

void Si5351a_Write_Reg (byte regist, byte value) {  // Writes "byte" into "regist" of Si5351a via I2C
  Wire.beginTransmission(96);                       // Starts transmission as master to slave 96, which is the
  // I2C address of the Si5351a (see Si5351a datasheet)
  Wire.write(regist);                               // Writes a byte containing the number of the register
  Wire.write(value);                                // Writes a byte containing the value to be written in the register
  Wire.endTransmission();                           // Sends the data and ends the transmission
}

