#include <SoftwareSerial.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Adafruit_SSD1306.h>

// #define DEBUG

#ifdef DEBUG
 #define DEBUG_PRINT(...)  Serial.print(F(#__VA_ARGS__" = ")); Serial.print(__VA_ARGS__); Serial.print(F(" "))
#else
 #define DEBUG_PRINT(...)
#endif
#ifdef DEBUG
 #define DEBUG_PRINTLN(...)  DEBUG_PRINT(__VA_ARGS__); Serial.println()
#else
 #define DEBUG_PRINTLN(...)
#endif

#define VERSION F("v0.28")

LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD address is 0x27 for a 16 chars and 2 line display

SoftwareSerial mySerial(10, 11); // RX, TX

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declarations for an SSD1306 display connected to I2C (SDA, SCL pins)

// new constructor - provide height and width instead of editing Adafruit_SSD1306.h
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT);

void setup() {
  Serial.begin(9600);
  Serial.print(F("\nGPS LCD+OLED Clock "));
  Serial.println(VERSION);

  mySerial.begin(9600); // u-blox NEO-8M default baud rate
  
  lcd.init();                      // initialize the lcd 
  // Print startup message to the LCD
  lcd.backlight();
  lcd.print(F("GPS LCD+OLED Clk")); // F(str) saves SRAM space when str is a constant
  lcd.setCursor(6, 1);
  lcd.print(VERSION);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C); // OLED address 0x3C
  
  // Clear the buffer
  oled.clearDisplay();

  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println(F("  GPS LCD+OLED Clock"));
  oled.print(F("        "));
  oled.println(VERSION);
  oled.println(F("         by"));
  oled.println(F("   C Sylvain KB3CS"));
  
  oled.display();
  delay(1500); // pause 1500 msec

  lcd.clear();
  oled.clearDisplay(); // clears buffer
  oled.display(); // displays cleared screen
      
  lcd.home();
  lcd.print(F("GPS Time: "));
  lcd.setCursor(3, 1);
  lcd.print(F("--:--:--"));
  lcd.print(F(" UTC"));
  
  delay(500);
}

int oldsec = 1, newsec = 0, echo = 0;
String sHH, sMM, sSS;
String buf = ""; // $xxGGA,HHMMSS.00,...\0

void loop() {
  char c;
  
  if (mySerial.available()) {
    c = mySerial.read(); // sentence starts with '$' char
    
    if (c == '$') { // need to capture the next 5 to know which sentence. at most the next 16 chars. 
      // buf is empty (initialized), or contains a full $xxGGA sentence
      if (buf.length() > 16) {
        sHH = buf.substring(7,9);       
        sMM = buf.substring(9,11);
        sSS = buf.substring(11,13);
        newsec = sSS.toInt();
      }
      DEBUG_PRINT("buf len = ");
      DEBUG_PRINTLN(buf.length());
      buf = ""; // reset buffer
      buf += c;
      echo = 0;
    }
    else if (buf.length() < 6) {
      buf += c;
    }
    else if (buf.length() == 6) {
      DEBUG_PRINT(buf);
      DEBUG_PRINTLN(c);        
      if (buf.substring(3,6) == F("GGA")) {
        DEBUG_PRINT("Found GGA - ");

        Serial.print(buf); // catch the serial monitor up

        echo = 1; // and echo the rest of the $xxGGA sentence
      }
      else echo = 0;
    }
    
    if (echo) {
      Serial.write(c);
      buf += c; // gather the rest of the $xxGGA sentence for later
    }
  }

  if (sHH.length() > 0 && oldsec != newsec) {
    lcd.setCursor(3, 1);
    // wait for GPS startup to complete. until then, time field in sentence will be empty.
    if (sMM.indexOf(',') == -1 && sSS.indexOf(',') == -1)  lcd.print(sHH+F(":")+sMM+F(":")+sSS+F(" UTC"));
    oldsec = newsec;
    oledclock(-5); // my local timezone is presently UTC-5. no, i'm not adding DST on/off logic - it would require the $xxZDA sentence
  }
  // what can't be done here is: delay(1000); (to leave time for human reading the display)
  // think about it: there's a flood of serial data from the GPS module. must process more right away.
  // need a non-blocking way to update the LCD instead --> so wait until the SS changes in HHMMSS
}

// constants to define clockface element geometry

#define CF_R  23  // clockface radius
#define CF_R3 20  // CF_R-3
#define CF_R6 17  // CF_R-6
#define CF_R8 15
#define CF_R14 9
#define CF_CO  7  // clockface center offset

// oledclock() blows up if i call oled.width() and oled.height() {uses too much stack? suspect so: lcd display goes crazy}
// have screen size manifest constants already for Adafruit_SSD1306 constructor, so skip the function calls to reduce stack use

#define IDHW  (SCREEN_WIDTH/2)  // integer display half-width
#define IDHH  ((SCREEN_HEIGHT/2)+CF_CO)  // integer display half-height plus center offset

#define DEGPERRAD  57.29577951 // 360 / tau = 180 / pi

void oledclock (int tz) {
  int x2, y2, x3, y3;

  float angle;

  int iHH = sHH.toInt() + tz;
  
  if (iHH < 0)  iHH += 24;
  if (iHH > 23) iHH -= 24;
  
  oled.clearDisplay();
  // refactored analog clockface code originally taken from 
  // https://github.com/rydepier/Arduino-OLED-Clock
  //
  // i have a yellow-blue 128x64 OLED, so i needed to shift the clockface center down so it's all blue with no yellow
  // i also made the face diameter larger to use more of the display area, but left the clock ticks alone for legibility
  //

  // display local time in digital format in yellow px 
  oled.setCursor(15,2); 
  if (iHH < 10)  oled.print('0');
  oled.print(iHH, DEC);
  oled.print(':'+sMM+':'+sSS);
  oled.print(F("  UTC"));
  if (tz > 0)  oled.print('+');
  if (tz != 0)  oled.print(tz, DEC);

  oled.drawCircle(IDHW, IDHH, CF_R, WHITE); // draw the clock face outline
  oled.drawCircle(IDHW, IDHH,    2, WHITE); //   and "center pin"
  
  // draw hour ticks
  for (int z = 0; z < 360; z += 30) { // deg per hr tick = 360 / 12
    // Begin at 0° and stop at 360°
    angle = z ;
    angle /= DEGPERRAD; // Convert degrees to radians
    x2 = IDHW + (sin(angle)*CF_R3);
    y2 = IDHH - (cos(angle)*CF_R3);
    x3 = IDHW + (sin(angle)*CF_R8);
    y3 = IDHH - (cos(angle)*CF_R8);
    oled.drawLine(x2, y2, x3, y3, WHITE);
  }
  
  // display second hand
  angle = sSS.toInt() * 6 ; // deg per sec tick = 360 / 60
  angle /= DEGPERRAD; // Convert degrees to radians  
  x3 = IDHW + (sin(angle)*CF_R3);
  y3 = IDHH - (cos(angle)*CF_R3);
  oled.drawLine(IDHW, IDHH, x3, y3, WHITE);

  // display minute hand
  angle = sMM.toInt() * 6 ;
  angle /= DEGPERRAD; // Convert degrees to radians  
  x3 = IDHW + (sin(angle)*CF_R6);
  y3 = IDHH - (cos(angle)*CF_R6);
  oled.drawLine(IDHW, IDHH, x3, y3, WHITE);

  // display hour hand
  angle = (iHH * 30) + int((sMM.toInt() / 12) * 6);
  angle /= DEGPERRAD; // Convert degrees to radians  
  x3 = IDHW + (sin(angle)*CF_R14);
  y3 = IDHH - (cos(angle)*CF_R14);
  oled.drawLine(IDHW, IDHH, x3, y3, WHITE);

  oled.display();
}
