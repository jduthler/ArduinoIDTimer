/*******************************************************************************/
//  HAM LED ID Timer 
//
//
//
//
//  Based on a demo app by:
//  Author:Frankie.Chu
//  Date:23 September, 2012
//
/*******************************************************************************/
#include <Wire.h>
#include <TimerOne.h>
#include <MsTimer2.h>
#include <EEPROM.h>
#include <TM1636.h>
#include <TickTockShield.h>
#include <SoftwareSerial.h>
#include <TinyGPS.h>

#include "HAMLEDIDTIMER.h"  


/****************************************************************
Function:Setup
  Inputs:None
 Returns:None
    Desc:

*****************************************************************/
void setup() 
{
#ifdef DEBUG
	Serial.begin(115200);
#endif
	ticktockshield.init();

        pinMode(RXPIN, INPUT);
        pinMode(TXPIN, OUTPUT);
        
        nss.begin(9600);                   // The GPS Shield I developed with sends at 9600
	                                     
	ticktockshield.runLED();           // Run the 4 LEDs from left to right

	ticktockshield.setAlarm(0,0,0);    // No disable the alarm
       
	delay(1000);
	MsTimer2::set(500, Timer2ISR);    // Timer2 generates an interrupt every 500ms
	MsTimer2::start();                // Timer2 starts counting

#ifdef DEBUG
        Serial.println("Setup complete");
#endif
}
/****************************************************************
Function:Main Loop
    Desc:

*****************************************************************/
void loop() 
{
  clockStart();
}
/****************************************************************
Function:clockStart
  Inputs:None
 Returns:None
    Desc:



*****************************************************************/
void clockStart()
{
    
    if(feedgps())							// Poll the serial service
       gpsdump(gps);							// If a string is ready to go parse it. The GPS outputs a new string every second once it is up and aquired lock the stinrg will be received 
									// on each new second.
  
    if(flag_500ms)							// if update flag is not zero?				
    { 
      flag_500ms = false;
           
      ticktockshield.getTime();						//
              
      if(flag_1s)							// Has a second elapsed?
      {
        flag_1s = false;
        
        ticktockshield.displayTime();					// Update the display
	
	flag_clockpoint = (~flag_clockpoint) & 0x01;			// Flip Flop the points between the digits
        if(flag_clockpoint)
          tm1636.point(POINT_ON);
        else 
          tm1636.point(POINT_OFF);
#ifdef DEBUG        
        if(ticktockshield.isIDMinute())				        //
            Serial.println("******************* ID Minute == TRUE *******************");
#else
        ticktockshield.isIDMinute();

#endif
      }

      if(flag_2s)
        flag_2s = 0;
      
      if(flag_10s)
      {
        flag_10s = 0;
#ifdef DEBUG        
        Serial.println("Sats HDOP Latitude Longitude Fix  Date       Time       Date Alt     Course Speed Card  Chars Sentences Checksum");
        Serial.println("          (deg)    (deg)     Age                        Age  (m)     --- from GPS ----  RX    RX        Fail");
        Serial.println("----------------------------------------------------------------------------------------------------------------");
#endif        
      }
    }
}
/****************************************************************
Function:It is timer 2 interrupt service routine.Timer2 generates an interrupt
  Inputs:
 Returns:
    Desc: every 500ms.And every time it interrupts,this function will be executed.*



*****************************************************************/	
void Timer2ISR()
{
static int halfsecond = 0;   //each time the timer2 interrupts,halfsecond plus one.
int  nCurrentSecond;

  if(++halfsecond >= 2)      // Every second
  {
    halfsecond = 0;
    nCurrentSecond = ticktockshield.nGetSecond();
    
    if(!(nCurrentSecond % 1))
      flag_1s = TRUE;
    
    if(!(nCurrentSecond % 2))
      flag_2s = TRUE;
    
    if(!(nCurrentSecond % 20))
      flag_10s = TRUE;
   
  }
    
  flag_500ms = true;
  
 }
/****************************************************************
Function:
  Inputs:
 Returns:
    Desc:



*****************************************************************/
static void gpsdump(TinyGPS &gps)
{
  float flat, flon;
  unsigned long age, date, time, chars = 0;
  unsigned short sentences = 0, failed = 0;
  
  
  gps.f_get_position(&flat, &flon, &age);
  
#ifdef DEBUG
  
  print_int(gps.satellites(), TinyGPS::GPS_INVALID_SATELLITES, 5);
  print_int(gps.hdop(), TinyGPS::GPS_INVALID_HDOP, 5);
  print_float(flat, TinyGPS::GPS_INVALID_F_ANGLE, 9, 5);
  print_float(flon, TinyGPS::GPS_INVALID_F_ANGLE, 10, 5);
  print_int(age, TinyGPS::GPS_INVALID_AGE, 5);
  
  print_date(gps);

  print_float(gps.f_altitude(), TinyGPS::GPS_INVALID_F_ALTITUDE, 8, 2);
  print_float(gps.f_course(), TinyGPS::GPS_INVALID_F_ANGLE, 7, 2);
  print_float(gps.f_speed_kmph(), TinyGPS::GPS_INVALID_F_SPEED, 6, 2);
  print_str(gps.f_course() == TinyGPS::GPS_INVALID_F_ANGLE ? "*** " : TinyGPS::cardinal(gps.f_course()), 6);

  gps.stats(&chars, &sentences, &failed);
  print_int(chars, 0xFFFFFFFF, 6);
  print_int(sentences, 0xFFFFFFFF, 10);
  print_int(failed, 0xFFFFFFFF, 9);
  Serial.println();
  
#endif

  
  ticktockshield.GPSLock(age, gps.satellites());
  updateRTC(gps); 
  
}
/****************************************************************
Function:
  Inputs:
 Returns:
    Desc:



*****************************************************************/
static void print_int(unsigned long val, unsigned long invalid, int len)
{
  char sz[32];
  if (val == invalid)
    strcpy(sz, "*******");
  else
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  feedgps();
}
/****************************************************************
Function:
  Inputs:
 Returns:
    Desc:



*****************************************************************/
static void print_float(float val, float invalid, int len, int prec)
{
  char sz[32];
  if (val == invalid)
  {
    strcpy(sz, "*******");
    sz[len] = 0;
        if (len > 0) 
          sz[len-1] = ' ';
    for (int i=7; i<len; ++i)
        sz[i] = ' ';
    Serial.print(sz);
  }
  else
  {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1);
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      Serial.print(" ");
  }
  feedgps();
}
/****************************************************************
Function:
  Inputs:
 Returns:
    Desc:



*****************************************************************/
static void print_date(TinyGPS &gps)
{
  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned long age;
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  if (age == TinyGPS::GPS_INVALID_AGE)
    Serial.print("*******    *******    ");
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d %02d:%02d:%02d   ",
        month, day, year, hour, minute, second);
    Serial.print(sz);
  }
  print_int(age, TinyGPS::GPS_INVALID_AGE, 5);
  feedgps();
}
/****************************************************************
Function:
  Inputs:
 Returns:
    Desc:



*****************************************************************/
static void updateRTC(TinyGPS &gps)
{
  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned long age;
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  if (age != TinyGPS::GPS_INVALID_AGE)
    ticktockshield.writeTime(hour,minute,second,month,day,year);
    
  feedgps();
}
/****************************************************************
Function:
  Inputs:
 Returns:
    Desc:



*****************************************************************/
static void print_str(const char *str, int len)
{
  int slen = strlen(str);
  for (int i=0; i<len; ++i)
    Serial.print(i<slen ? str[i] : ' ');
  feedgps();
}
/****************************************************************
Function:
  Inputs:
 Returns:
    Desc:



*****************************************************************/
static bool feedgps()
{
  while (nss.available())
  {
    if (gps.encode(nss.read()))
      return true;
  }
  return false;
}


/****************************************************************
Function:
  Inputs:
 Returns:
    Desc:



*****************************************************************/


