

#define ON 1
#define OFF 0
#define TRUE ON
#define FALSE OFF

#define DEBUG  1

#define RXPIN  0
#define TXPIN  1


TinyGPS gps;
SoftwareSerial nss(RXPIN,TXPIN);

static void gpsdump(TinyGPS &gps);
static bool feedgps();
static void print_float(float val, float invalid, int len, int prec);
static void print_int(unsigned long val, unsigned long invalid, int len);
static void print_date(TinyGPS &gps);
static void print_str(const char *str, int len);

//definitions of global variables
boolean flag_500ms;//1 = update the time on the 4-digit display

boolean flag_clockpoint = 1;//change between 0 and 1 every 500ms. If it is 1,
						//the clockpoint is on.Else the clockpoint is off.
boolean flag_1s;
boolean flag_2s;//2 seconds flag.1 = to the end of 2 seconds so as to set 
				//flag_display_time
boolean flag_10s;//10 seconds flag.1 = to the end of 10 seconds so as to 
				//display temperature
boolean flag_display_time = 1;//1 = can display time that get from the RTC

uint8_t counter_times = 20;//20*halfsecond = 10seconds.

//--Declare a TickTockShield Class object--//
TickTockShield ticktockshield;

extern int8_t disp[4];//external global array defined in TickTockShield.cpp

