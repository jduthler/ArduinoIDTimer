#include "TickTockShield.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include <TimerOne.h>
#include "TM1636.h"


TM1636 tm1636(7,8);
void timerIsr();
/*global variables also used in timer interrupt serve routine */
uint8_t key_menu_status;
uint16_t count_10ms_pressed;
uint16_t count_10ms_release;
boolean flag_pressed_3s;
boolean flag_release_10s;
boolean alarm_on_off;

int8_t disp[4];
int8_t disp_buff[4];
byte g_hand;

//--------------------------------//
void TickTockShield::init()
{

  fUpdateNextLoop = false;
  fLEDOn = false;
  g_brightness = BRIGHT_TYPICAL;
  tm1636.set(g_brightness);
  tm1636.init();
  keyInit();
  ledInit();
  sensorInit();
	buzzerInit();
  Wire.begin();
	pre_pin_number = -1;
	Timer1.initialize(100000); // set a timer of length 100000 microseconds 
		//(or 0.1 sec - or 10Hz => the led will blink 5 times, 5 cycles of on-and-off, per second)
  Timer1.attachInterrupt( timerIsr ); // attach the service routine here
}

void TickTockShield::keyInit()
{
  //Set all three keys to be input and internal pull-up
  
  pinMode(KEY_MENU, INPUT);
  digitalWrite(KEY_MENU, HIGH);
  pinMode(KEY_UP, INPUT);
  digitalWrite(KEY_UP, HIGH);
  pinMode(KEY_DOWN, INPUT);
  digitalWrite(KEY_DOWN, HIGH);
}
void TickTockShield::ledInit()
{
  pinMode(LED_D1, OUTPUT);
  pinMode(LED_D2, OUTPUT);
  pinMode(LED_D3, OUTPUT);
  pinMode(LED_D4, OUTPUT);
  turnOffLED();
}
void TickTockShield::sensorInit()
{
  pinMode(TEMPERATURE_SENSOR, INPUT);
  pinMode(LIGHT_SENSOR, INPUT);
}
void TickTockShield::buzzerInit()
{
	pinMode(ALARM_BUZZER, OUTPUT);
  pinMode(ALARM_BUZZER, OUTPUT);
}
void TickTockShield::setLed(unsigned char led_status, int pinLED)
{
	digitalWrite(pinLED, led_status);
}
/********************************/
void TickTockShield::turnOffLED()
{
	setLed(LOW,LED_D1);
	setLed(LOW,LED_D2);
	setLed(LOW,LED_D3);
	setLed(LOW,LED_D4);
}
/********************************/
void TickTockShield::turnOnLED()
{
	setLed(HIGH,LED_D1);
	setLed(HIGH,LED_D2);
	setLed(HIGH,LED_D3);
	setLed(HIGH,LED_D4);
}

/*******************************/
void TickTockShield::runLED(byte speed, byte direction)
{
	if((speed > 0)&&(speed < 11))//If the value of speed is valid?
	{
		turnOffLED();
		uint8_t shifter = 0x01;
		if(LEFT_TO_RIGHT == direction)
		{
			for(uint8_t i = 0;i < 4;i ++)
			{
				if(shifter&0x01) setLed(LED_ON,  LED_D4);
				else						 setLed(LED_OFF, LED_D4);
				if(shifter&0x02) setLed(LED_ON,  LED_D3);
				else					   setLed(LED_OFF, LED_D3);
				if(shifter&0x04) setLed(LED_ON,  LED_D2);
				else						 setLed(LED_OFF, LED_D2);
				if(shifter&0x08) setLed(LED_ON,  LED_D1);
				else						 setLed(LED_OFF, LED_D1);
				shifter <<= 1;
				delay(500/speed);
			}
		}
		else
		{
			for(uint8_t i = 0;i < 4;i ++)
			{
				if(shifter&0x01) setLed(LED_ON,  LED_D1);
				else						 setLed(LED_OFF, LED_D1);
				if(shifter&0x02) setLed(LED_ON,  LED_D2);
				else					   setLed(LED_OFF, LED_D2);
				if(shifter&0x04) setLed(LED_ON,  LED_D3);
				else						 setLed(LED_OFF, LED_D3);
				if(shifter&0x08) setLed(LED_ON,  LED_D4);
				else						 setLed(LED_OFF, LED_D4);
				shifter <<= 1;
				delay(500/speed);
			}
		}
		turnOffLED();
	}
}
//--------------------------------//
//-Return:the pin number of the key pressed
//--------------------------------//
//
int16_t TickTockShield::scanKey()
{
	int16_t pin_number = 0;
	static boolean pre_key_menu_level = HIGH;
	boolean cur_key_menu_level;
	if(digitalRead(KEY_MENU) == LOW)
	{
		delay(30);
		if(digitalRead(KEY_MENU) == LOW)
		{
			cur_key_menu_level = LOW;
			pin_number = KEY_MENU;
		}
		else cur_key_menu_level = HIGH;
	}
	else
	{
		cur_key_menu_level = HIGH;
	}
	if(pre_key_menu_level > cur_key_menu_level)
	{

		key_menu_status = KEY_PRESSED;
		if(system_status < 7)system_status ++;
		count_10ms_pressed = 0;
		flag_pressed_3s = 0;
	#ifdef DEBUG
		Serial.print("system_status = ");
	  Serial.println(system_status);
		Serial.println("Key_Menu pressed");
	#endif
	}
	else if(pre_key_menu_level < cur_key_menu_level)
	{
		key_menu_status = KEY_RELEASE;
		count_10ms_release = 0;
		flag_release_10s = 0;
	#ifdef DEBUG
		Serial.println("Key_Menu release");
  #endif
	}
	pre_key_menu_level = cur_key_menu_level;
	
	if(digitalRead(KEY_UP) == LOW)
	{
		delay(20);
		if(digitalRead(KEY_UP) == LOW)
		{
			pin_number = KEY_UP;
			count_10ms_release = 0;
			flag_release_10s = 0;
		#ifdef DEBUG
			Serial.println("Key_UP pressed");
		#endif
		}
		
	}
	else if(digitalRead(KEY_DOWN) == LOW)
	{
		delay(20);
		if(digitalRead(KEY_DOWN) == LOW)
		{
			pin_number = KEY_DOWN;
			count_10ms_release = 0;
			flag_release_10s = 0;
		#ifdef DEBUG
			Serial.println("KEY_DOWN pressed");
		#endif
		}
	}
	if(pin_number == 0)pin_number = -1;
	key_pin_pressed = pin_number;
	return key_pin_pressed;
}
/*************************************************/
/*Function:*/
/*Return:   void*/
void TickTockShield::processKey()
{
	if((key_pin_pressed > 0)&&(pre_pin_number != key_pin_pressed))
	{
		ringOn();
		delay(100);
		ringOff();
	}
	pre_pin_number = key_pin_pressed;
	if(flag_pressed_3s)
	{
		flag_pressed_3s = 0;
		system_status = SS_CONFIRM_EXIT;
	#ifdef DEBUG
		Serial.print("system_status = ");
	  Serial.println(system_status);
		Serial.println("Press for 3s...");
	#endif
	}
	if(flag_release_10s)
	{
		flag_release_10s = 0;
		system_status = SS_QUIT;
	#ifdef DEBUG
		Serial.print("system_status = ");
	  Serial.println(system_status);
		Serial.println("Release for 5s");
	#endif
	}	
}
void TickTockShield::processSystemStatus()
{
	switch(system_status)
	{
		case SS_NO_INPUT:			
													break;										
		case SS_CLOCKH_ADJUST:adjustClock(HOUR);
													break;	
		case SS_CLOCKM_ADJUST:adjustClock(MINUTE);
													break;	
		case SS_ALARMH_ADJUST:adjustAlarm(HOUR);
													break;	
		case SS_ALARMM_ADJUST:adjustAlarm(MINUTE);
													break;	
		case SS_ALARM_ENABLE: flag_adjust_time = 0;
													modifyAlarmFlag();
													break;	
		case SS_BRIGHT_ADJUST:adjustBrightness();
													break;	
		case SS_CONFIRM_EXIT: saveChanges();
													system_status = SS_NO_INPUT;
													break;	
		case SS_QUIT:					quit();
													system_status = SS_NO_INPUT;
													break;
		default:break;	
	}
	
}
void TickTockShield::quit()
{
	flag_require_quit = 1;
	flag_adjust_time = 0;
	tm1636.set(g_brightness);
	turnOffLED();
	if(alarm.flag_enable)
		setLed(HIGH,LED_D2);
	else setLed(LOW,LED_D2);
	
}
void TickTockShield::saveChanges()
{
	g_hour   = g_hour_temp;
	g_minute = g_minute_temp;
	writeTime();
	
	alarm.hour        = alarm_temp.hour;
	alarm.minute      = alarm_temp.minute;
	alarm.flag_enable = alarm_temp.flag_enable;
	setAlarm(alarm);
	
	g_brightness 			= g_brightness_temp;
	tm1636.set(g_brightness);

	flag_alarm_over = 0;
	quit();
}
inline void TickTockShield::modifyAlarmFlag()
{
	turnOffLED();
	if(key_pin_pressed == KEY_UP)
	{
		alarm_temp.flag_enable = ~alarm_temp.flag_enable;
	}
	if(alarm_temp.flag_enable)
	{
		setLed(HIGH,LED_D2);
	}
	else
	{
		setLed(LOW,LED_D2);
	}
	disp[0] = alarm_temp.hour/10;
	disp[1] = alarm_temp.hour%10;
	disp[2] = alarm_temp.minute/10;
	disp[3] = alarm_temp.minute%10;
	tm1636.display(disp);
}
void TickTockShield::adjustClock(uint8_t hand)
{
	turnOffLED();
	setLed(HIGH,LED_D4);
	if(hand == HOUR)
	{
		g_hand = HOUR;
		if(key_pin_pressed == KEY_DOWN)
		{
			if(g_hour_temp> 0)
				g_hour_temp--;
		}
		else if(key_pin_pressed == KEY_UP)
		{
			if(g_hour_temp< 23)
				g_hour_temp++;
			else g_hour_temp = 0;
		}
	}
	else
	{
		g_hand = MINUTE;
		if(key_pin_pressed == KEY_DOWN)
		{
			if(g_minute_temp> 0)
				g_minute_temp--;
		}
		else if(key_pin_pressed == KEY_UP)
		{
			if(g_minute_temp< 59)
				g_minute_temp++;
			else g_minute_temp = 0;
		}
	}
	disp[0] = g_hour_temp/10;
	disp[1] = g_hour_temp%10;
	disp[2] = g_minute_temp/10;
	disp[3] = g_minute_temp%10;
//	tm1636.display(disp);
}
void TickTockShield::adjustAlarm(uint8_t hand)
{
	turnOffLED();
	setLed(HIGH,LED_D3);
	if(hand == HOUR)
	{
		g_hand = HOUR;
		if(key_pin_pressed == KEY_DOWN)
		{
			if(alarm_temp.hour> 0)
				alarm_temp.hour--;
		}
		else if(key_pin_pressed == KEY_UP)
		{
			if(alarm_temp.hour< 23)
				alarm_temp.hour++;
			else alarm_temp.hour = 0;
		}
	}
	else
	{
		g_hand = MINUTE;
		if(key_pin_pressed == KEY_DOWN)
		{
			if(alarm_temp.minute> 0)
				alarm_temp.minute--;
		}
		else if(key_pin_pressed == KEY_UP)
		{
			if(alarm_temp.minute< 59)
				alarm_temp.minute++;
			else alarm_temp.minute = 0;
		}
	}
	disp[0] = alarm_temp.hour/10;
	disp[1] = alarm_temp.hour%10;
	disp[2] = alarm_temp.minute/10;
	disp[3] = alarm_temp.minute%10;
//	tm1636.display(disp);
}
/***********************************/
void TickTockShield::adjustBrightness()
{
	turnOffLED();
	setLed(HIGH,LED_D1);
	if(key_pin_pressed == KEY_DOWN)
	{
		if(g_brightness_temp> 0)
			g_brightness_temp--;
	}
	else if(key_pin_pressed == KEY_UP)
	{
		if(g_brightness_temp< 7)
			g_brightness_temp++;
	}
	tm1636.set(g_brightness_temp);
	disp[0] = alarm_temp.hour/10;
	disp[1] = alarm_temp.hour%10;
	disp[2] = alarm_temp.minute/10;
	disp[3] = alarm_temp.minute%10;
	tm1636.display(disp);
}
void TickTockShield::adjustBrightness(uint8_t grayscale)
{
	g_brightness = grayscale;
	tm1636.set(g_brightness);
}
/****************************************************************/
/*Return:int8_t,Temperature that range from -40 to 125 degrees.					*/
int8_t TickTockShield::getTemperature()
{
	float temperature,resistance;
	int a;
	int B = 3975;
	a = analogRead(TEMPERATURE_SENSOR);
	resistance   = (float)(1023-a)*RESISTOR_CONNECT_THERMISTOR/a; 
	temperature  = 1/(log(resistance/RESISTOR_CONNECT_THERMISTOR)/B+1/298.15)-273.15;
	return (int8_t)temperature;
}
/**********************************************************************/
/*Function:   Display the temperature on the 4-digit display										*/
/*Parameter:-int8_t temperature,Temperature that range from -40 to 125 degrees. */
/*Return:	  void																												*/
void TickTockShield::displayTemperature(int8_t temperature)
{
  int8_t temp[4];
  if(temperature < 0)
	{
		temp[0] = INDEX_NEGATIVE_SIGN;
		temperature = abs(temperature);
	}
	else if(temperature < 100)temp[0] = INDEX_BLANK;
	else temp[0] = temperature/100;
	temperature %= 100;
	temp[1] = temperature / 10;
	temp[2] = temperature % 10;
	temp[3] = 12;	          //index of 'C' for celsius degree symbol.
	tm1636.display(temp);
}

float TickTockShield::getLightIntensity()
{
  int sensorValue = analogRead(LIGHT_SENSOR);
  float rsensor;
  rsensor=(float)(1023-sensorValue)*10/sensorValue;
	return rsensor;
}

uint8_t TickTockShield::getLightLevel()
{
	uint16_t resistance;
	uint8_t light_level;
	resistance = (uint16_t)getLightIntensity();
	if(resistance < 10) light_level = 0;
	else if(resistance < 50)light_level = 1;
	else if(resistance < 80)light_level = 2;
	else if(resistance < 110)light_level = 3;
	else if(resistance < 140)light_level = 4;
	else if(resistance < 170)light_level = 5;
	else if(resistance < 200)light_level = 6;
	else light_level = 7;
	return light_level;
}

void TickTockShield::ringOn()
{
	digitalWrite(ALARM_BUZZER, HIGH);
}
void TickTockShield::ringOff()
{
	digitalWrite(ALARM_BUZZER, LOW);
}
void TickTockShield::alarming()
{
	if(alarm_on_off)ringOn();
	else ringOff();
}
void TickTockShield::turnOffAlarm()
{
	ringOff();
	flag_alarm_over = 1;
}
boolean TickTockShield::isAlarmEnable()
{
	if(compareWithAlarm())
	{
		if(!flag_alarm_over)


			return true;
	}
	else flag_alarm_over = 0;
	return false;
}
/*******************************/
boolean TickTockShield::compareWithAlarm()
{
	if(alarm.flag_enable > 0)
	{
		if((alarm.hour == g_hour)&&(alarm.minute == g_minute))
		{
			return true;
		}
	}
	return false;
}

inline void TickTockShield::alarmDisable()
{}
void TickTockShield::setAlarm(struct AlarmStruct alarm_)
{
	EEPROM.write(5,alarm_.hour);
	EEPROM.write(6,alarm_.minute);
	EEPROM.write(7,alarm_.flag_enable);
	
}
void TickTockShield::setAlarm(uint8_t hour,uint8_t minute,uint8_t flag_enable)
{
	EEPROM.write(5,hour);
	EEPROM.write(6,minute);
	EEPROM.write(7,flag_enable);
}

/************************************************************/
/*Function:Read the alarm clock information from the built-in EEPROM  */
/*Return:   void																							   */
void TickTockShield::getAlarm()
{
	alarm.hour        = EEPROM.read(5);
	alarm.minute      = EEPROM.read(6);
	alarm.flag_enable = EEPROM.read(7);
#ifdef DEBUG
	Serial.print("alarm = ");
  Serial.print(alarm.hour);
	Serial.print(" : ");
	Serial.println(alarm.minute);
#endif
	if(alarm.flag_enable)
		setLed(HIGH,LED_D2);
	else setLed(LOW,LED_D2);
}
/***********************************************************/
/*Function:write data to the global variables in Ajust Area 					  */
/*Return:   void																							   */
void TickTockShield::writeToAdjustArea()
{
	g_hour_temp       = g_hour;
	g_minute_temp     = g_minute;
	g_brightness_temp = g_brightness;
	alarm_temp.hour        = alarm.hour;
	alarm_temp.minute      = alarm.minute;
	alarm_temp.flag_enable = alarm.flag_enable;
	flag_require_quit = 0;
	flag_adjust_time = 1;
	pre_pin_number = -1;
	tm1636.point(POINT_ON);
	turnOffLED();
}

/***********************************************************/
/*Function:write data to the global variables in Normal Area    			  */
/*Return:   void																							   */
void TickTockShield::writeToNormalArea()
{
	g_hour       = g_hour_temp;
	g_minute     = g_minute_temp;
	g_brightness = g_brightness_temp;
	alarm.hour        = alarm_temp.hour;
	alarm.minute      = alarm_temp.minute;
	alarm.flag_enable = alarm_temp.flag_enable;
}

/************************************************************/
/*Frunction: Write the time that includes the date to the RTC chip			*/
/*Return:	  void																							*/
void TickTockShield::writeTime()
{
	Wire.beginTransmission(DS1307_I2C_ADDRESS);
	Wire.write((byte)0x00);
	Wire.write(decToBcd(g_second)); 	 // 0 to bit 7 starts the clock
	Wire.write(decToBcd(g_minute));
	Wire.write(decToBcd(g_hour)); 	 // If you want 12 hour am/pm you need to set
									// bit 6 (also need to change readDateDs1307)
	Wire.write(decToBcd(g_dayOfWeek));
	Wire.write(decToBcd(g_dayOfMonth));
	Wire.write(decToBcd(g_month));
	Wire.write(decToBcd(g_year));
	Wire.endTransmission();
}
/************************************************/
/*Frunction: Read time from RTC											*/
/*Parameter:A 2-byte array that tells the hour and			*/ 
//			minute at the end of the function.

void TickTockShield::getTime()
{
    // Reset the register pointer
	Wire.beginTransmission(DS1307_I2C_ADDRESS);
	Wire.write((byte)0x00);
	Wire.endTransmission();  
	Wire.requestFrom(DS1307_I2C_ADDRESS, 7);
	// A few of these need masks because certain bits are control bits
	g_second	   = bcdToDec(Wire.read() & 0x7f);
	g_minute	   = bcdToDec(Wire.read());
	g_hour	     = bcdToDec(Wire.read() & 0x3f);// Need to change this if 12 hour am/pm
	g_dayOfWeek  = bcdToDec(Wire.read());
	g_dayOfMonth = bcdToDec(Wire.read());
	g_month      = bcdToDec(Wire.read());
	g_year	     = bcdToDec(Wire.read());
}
void TickTockShield::displayTime()
{
  unsigned char time[4];
  time[0] = g_hour/10;
  time[1] = g_hour%10;
  time[2] = g_minute/10;
  time[3] = g_minute%10;
  tm1636.display((int8_t*)time);
}
void TickTockShield::display(int8_t DispData [])
{
	tm1636.display(DispData);
}

void TickTockShield::clearTime(uint8_t hand)
{
	unsigned char time[4];
	if(hand == HOUR)
	{
		time[0] = INDEX_BLANK;
  	time[1] = INDEX_BLANK;
  	time[2] = g_minute/10;
  	time[3] = g_minute%10;
	}
	else 
	{
		time[0] = g_hour/10;
	  time[1] = g_hour%10;
	  time[2] = INDEX_BLANK;
	  time[3] = INDEX_BLANK;
	}
	tm1636.display((int8_t*)time);
}
// Convert normal decimal numbers to binary coded decimal
byte TickTockShield::decToBcd(byte val)
{
	return ( (val/10*16) + (val%10) );
}

// Convert binary coded decimal to normal decimal numbers
byte TickTockShield::bcdToDec(byte val)
{
	return ( (val/16*10) + (val%16) );
}

boolean TickTockShield::isAdjustingTime()
{
	if(flag_adjust_time)return true;
	else return false;
}
/// --------------------------
/// Custom ISR Timer Routine
/// --------------------------
void timerIsr()
{
	alarm_on_off = (~alarm_on_off) & 0x01;
	if(key_menu_status == KEY_PRESSED)
 	{
 		count_10ms_pressed ++;
		if(count_10ms_pressed == 30)
		{
			flag_pressed_3s = 1;
			key_menu_status = 0;
		}
 	}
	else if(key_menu_status == KEY_RELEASE)
	{
		count_10ms_release ++;
		if(count_10ms_release == 100)
		{
			flag_release_10s = 1;
			key_menu_status = 0;
		}
	}
}

boolean TickTockShield::isIDMinute()
{

    if(!(g_minute % 10))   // 2 for every other minute, 10 for 10,20,30,40,50,00 
    {
	AlertIDTime();
	return(1);
    }
    else
    {
	AlertIDTime(TRUE);
	return(0);
    }

}
void TickTockShield::AlertIDTime(boolean fReset)
{

    if(fLEDOn && fReset )
    {
      fLEDOn = FALSE;
#ifdef DEBUG
      Serial.println("Dim ID LED");
#endif
      setLed(LOW,LED_D4);
    }
    
    
    if(!fReset)
    {
	if(!(g_second % 2))
	{
#ifdef DEBUG
	    Serial.println("Dim display");
#endif
	    adjustBrightness(BRIGHT_DARKEST);
            setLed(LOW,LED_D4);
	}
	else
	{
#ifdef DEBUG
	    Serial.println("Bright display");
#endif
	    fLEDOn = TRUE;
	    adjustBrightness(BRIGHTEST);
	    setLed(HIGH,LED_D4);
	}
    }
}
  
int  TickTockShield::nGetSecond()
{
    return((int)g_second);

}
/************************************************************/
/*Frunction: Write the time that includes the date to the RTC chip			*/
/*Return:	  void																							*/
void TickTockShield::writeTime(uint8_t uHour, uint8_t uMinute, uint8_t uSecond, uint8_t uMonth, uint8_t uDay, int nYear)
{
byte second,minute,hour,dayOfWeek,dayOfMonth,month;
int year;
static int nLEDDelay = 10;


// The RTC continues to run while reading and the internal countdown timer is reset when the write is commited.
// So when the RTC needs to be adjusted push the next second with the clock halted and the next time in 


	if (uMinute == 59 || uSecond == 59)	// Near a roll-over just bail it can wait until the next second
	    return;


	nYear -= 1970;
    	  
	// Reset the register pointer
	Wire.beginTransmission(DS1307_I2C_ADDRESS);
	Wire.write((byte)0x00);
	Wire.endTransmission();



	// A few of these need masks because certain bits are control bits
	Wire.requestFrom(DS1307_I2C_ADDRESS, 7);
	second	    = bcdToDec(Wire.read() & 0x7f);
	minute	    = bcdToDec(Wire.read());
	hour	    = bcdToDec(Wire.read() & 0x3f);	// Need to change this if 12 hour am/pm
	dayOfWeek   = bcdToDec(Wire.read());
	dayOfMonth  = bcdToDec(Wire.read());
	month	    = bcdToDec(Wire.read());
	year	    = bcdToDec(Wire.read());
	

	if( second != uSecond || 
	    minute != uMinute ||
	    hour   != uHour   ||
	    dayOfMonth != uDay ||
	    month  != uMonth  ||
	    year   != nYear )					// Need to update the RTC
	{
	    
	    uUpdateSecond = uSecond + 1;

	    Wire.beginTransmission(DS1307_I2C_ADDRESS);
	    Wire.write((byte)0x00);
	    Wire.write(decToBcd(uSecond + 1 )); 
	    Wire.write(decToBcd(uMinute));
	    Wire.write(decToBcd(uHour)); 			    // If you want 12 hour am/pm you need to set
	           						    // bit 6 (also need to change readDateDs1307)
	    Wire.write(decToBcd(1));				    // Day of the week, does not matter
	    Wire.write(decToBcd(uDay));
	    Wire.write(decToBcd(uMonth));
	    Wire.write(decToBcd(nYear));
	    Wire.endTransmission();

	    nLEDDelay = 10;
	    setLed(HIGH,LED_D3);


#ifdef DEBUG
	    Serial.print("Setting RTC current time ");
	    Serial.print(hour);
	    Serial.print(":");
	    Serial.print(minute);
	    Serial.print(":");
	    Serial.print(second);
	    Serial.print("  ");

	    Serial.print(month);
	    Serial.print("/");
	    Serial.print(dayOfMonth);
	    Serial.print("/");
	    Serial.print(year);


	    Serial.print(" passed time ");
	    Serial.print(uHour);
	    Serial.print(":");
	    Serial.print(uMinute);
	    Serial.print(":");
	    Serial.print(uSecond);
	    Serial.print("  ");


	    Serial.print(uMonth);
	    Serial.print("/");
	    Serial.print(uDay);
	    Serial.print("/");
	    Serial.println(nYear);

#endif
	}


	if (nLEDDelay == 0 )
	{
          setLed(LOW,LED_D3);
          nLEDDelay = -1;
	}
	else
           if (nLEDDelay != -1)
             --nLEDDelay;
}

void TickTockShield::GPSLock(int nLockage ,int nSats)
{

    if (nSats)
    {
	if (nSats < 4)
	{
	    setLed(HIGH,LED_D2);
	    setLed(LOW,LED_D1);
	}
	else
	{
	    setLed(HIGH,LED_D2);	  // 4 or more sats
	    setLed(HIGH,LED_D1);
	}

    }
    else
    {
	setLed(LOW,LED_D2);		  // No lock
	setLed(LOW,LED_D1);
    }

}

