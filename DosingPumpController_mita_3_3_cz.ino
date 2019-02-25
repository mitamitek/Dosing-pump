/*
||
|| @file DosingPumpController
|| @version 3.3 CZ
|| @author Milan Mitek
|| @created by modifying the original program dosing pump from Gabriel F. Campolina
||
|| @description
|| | This library provides a simple doser pump controller. 
|| | To use all the implementation, I recommend use the following hardware:
|| | - Arduino UNO R3
|| | - Keypad 4x3
|| | - IC2 RTC 3231
|| | - IC2 LCD 20x4
|| | - 2x Trigger Switch Module FET MOS 
|| | - Relay module (only for safety level switch)
|| #
||
|| @license
|| | This library is free software; you can redistribute it and/or
|| | modify it under the terms of the GNU Lesser General Public
|| | License as published by the Free Software Foundation; version
|| | 2.1 of the License.
|| |
|| | This library is distributed in the hope that it will be useful,
|| | but WITHOUT ANY WARRANTY; without even the implied warranty of
|| | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
|| | Lesser General Public License for more details.
|| |
|| | You should have received a copy of the GNU Lesser General Public
|| | License along with this library; if not, write to the Free Software
|| | Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
|| #
*/
#include <Wire.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

const byte max_time_evapor                       = 40;              // max. evaporation pump run time in sec.

#define DIGITAL_PIN_Pump_1 5
#define DIGITAL_PIN_Pump_2 6
#define DIGITAL_PIN_Pump_3 7
#define DIGITAL_PIN_Pump_4 8
#define DIGITAL_PIN_Pump_5 9
#define DIGITAL_PIN_Pump_6 10
#define FLOAT_EVAPOR 11
#define ONE_WIRE_BUS 12
#define RELAY_EVAPOR 13

int Val_FLOAT_EVAPOR= 0;       

#define DS3231_I2C_ADDRESS 104

#define  MENU_FUNCTION  8
#define  MENU_ITEM_MAIN_MENU -1
#define  MENU_ITEM_PUMP_1  0
#define  MENU_ITEM_PUMP_2  1
#define  MENU_ITEM_PUMP_3  2
#define  MENU_ITEM_PUMP_4  3
#define  MENU_ITEM_PUMP_5  4
#define  MENU_ITEM_PUMP_6  5
#define  MENU_ITEM_TIME  6
#define  MENU_ITEM_CALIBRATION  7
#define  MENU_VALUE  4

//LCD Setup
LiquidCrystal_I2C lcd(0x27,20,4);

byte iconTerm[8] =
{
    B00100,
    B01010,
    B01010,
    B01110,
    B01110,
    B11111,
    B11111,
    B01110
};

byte iconDrop[8] =
{
    B00100,
    B00100,
    B01010,
    B01010,
    B10001,
    B10001,
    B10001,
    B01110,
};

//Keypad setup
const byte column = 3;
const byte line = 4;
char Teclas[line][column] =
    {{'1','2','3'},
     {'4','5','6'},
     {'7','8','9'},
     {'*','0','#'}};
byte line_numbers[line] = {A3,A2,A1,A0};
byte column_numbers[column] = {2,3,4};
Keypad keypad = Keypad(makeKeymap(Teclas), line_numbers, column_numbers, line, column );

//RTC setup
byte seconds, minutes, hours, day, date, month, year;
char weekDay[4];
byte tMSB, tLSB;
float my_temp;
char my_array[100];
bool turnOnPump1 = false;
bool turnOnPump2 = false;
bool turnOnPump3 = false;
bool turnOnPump4 = false;
bool turnOnPump5 = false;
bool turnOnPump6 = false;

//Menu setup
int countMenu = MENU_ITEM_MAIN_MENU;
int countKey = 0;
int pump1_menu = 0;
int pump2_menu = 0;
int pump3_menu = 0;
int pump4_menu = 0;
int pump5_menu = 0;
int pump6_menu = 0;


String menuItem[] = 
{
    "Setup 1 Vapnik",
    "Setup 2 Soda",
    "Setup 3 Sul + horcik",
    "Setup 4 DusicnanKNO3",
    "Setup 5 Ultra Min S",
    "Setup 6 Pump 6",
    "Setup - Time",
    "Calibrate/Exit *"
};

char valueMatrix[MENU_FUNCTION][MENU_VALUE] = 
{
  {'_','_','_','_'},
  {'_','_','_','_'},
  {'_','_','_','_'},
  {'_','_','_','_'},
  {'_','_','_','_'},
  {'_','_','_','_'},
  {'_','_','_','_'},
  {'_','_','_','_'}
};

//18B20 setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float Celcius=0;

//EEPROM setup
int startingAddress = 30;

void setup()
{
  lcd.init();
  lcd.createChar(1,iconTerm);
  lcd.createChar(2,iconDrop);
  Wire.begin();
  startRelayModule();
  readMatrixInEEPROM();
  checkvaluepumps();
  Serial.begin(9600);
  sensors.begin();
  readTemp();
  
}

void loop()
{
  if(seconds == 55)
    {
        readTemp();
    }
    
  char keyPress = keypad.getKey();

  if(keyPress != NO_KEY)
  {
    showMenu(keyPress);
  }

  if(countMenu == MENU_ITEM_MAIN_MENU)
  {
    showMainMenu();
    checkRelayStatus();
  }
}

void readTemp(void)
{ 
  sensors.requestTemperatures(); 
  Celcius=sensors.getTempCByIndex(0);
//  delay(500);
}

void verifyRelay(int calibrationValue, int pumpMatrixIndex, int relayIndex, int parts_per_day)
{ 
      int pumpValue = getSetupValue(pumpMatrixIndex);
      if(pumpValue > 0)
      {
        float timeInSeconds = ((float)pumpValue*60)/(float)calibrationValue;
        if(timeInSeconds > 0)
        {
          turnOnRelay(relayIndex);
          delay(timeInSeconds * parts_per_day);    //1000 pro 1x denne, 42 pro 24x denne
          turnOffRelay(relayIndex);
        }
      }
}

void evaporation(byte max_time_evapor, byte countEvapor)
{
      while ((countEvapor < max_time_evapor) && Val_FLOAT_EVAPOR == false)   
      { turnOnRelay(RELAY_EVAPOR);      
      Val_FLOAT_EVAPOR = digitalRead(FLOAT_EVAPOR);     // zjištění stavu plováku
      delay (1000);
      countEvapor++;                   // zvětšení proměnné o 1 
      }
          turnOffRelay(RELAY_EVAPOR);
}

void checkRelayStatus()
{
  int calibrationValue = getSetupValue(MENU_ITEM_CALIBRATION);
  if(calibrationValue > 0)
  {
    //Pump 1
    if(minutes == 0 && seconds == 0)                   //dosing Ca
    {
      lcd.setCursor(0,3);  
      lcd.print("Davkuji vapnik      ");
      int parts_per_day = 42;
      verifyRelay(calibrationValue, MENU_ITEM_PUMP_1, DIGITAL_PIN_Pump_1, parts_per_day);    
    }
    //Pump 2
    if(minutes == 15 && seconds == 0)                   //dosing Kh
    {
      lcd.setCursor(0,3);  
      lcd.print("Davkuji sodu        ");
      int parts_per_day = 42;
      verifyRelay(calibrationValue, MENU_ITEM_PUMP_2, DIGITAL_PIN_Pump_2, parts_per_day);    
    }
    //Pump 3
    if(minutes == 30 && seconds == 0)                  //dosing soli + Mg 
    {
      lcd.setCursor(0,3);  
      lcd.print("Davkuji sul + Mg    ");
      int parts_per_day = 42;
      verifyRelay(calibrationValue, MENU_ITEM_PUMP_3, DIGITAL_PIN_Pump_3, parts_per_day);    
    }
    //Pump 4
    if(hours == 15 && minutes == 45 && seconds == 0)   //dosing pump 4
    {
      lcd.setCursor(0,3);  
      lcd.print("Davkuji KNO3        ");
      int parts_per_day = 1000;
      verifyRelay(calibrationValue, MENU_ITEM_PUMP_4, DIGITAL_PIN_Pump_4, parts_per_day);    
    }
    //Pump 5
    if(hours == 18 && minutes == 45 && seconds == 0)   //dosing pumpa 5
    {
      lcd.setCursor(0,3);  
      lcd.print("Davkuji Ultra Min S ");
      int parts_per_day = 1000;
      verifyRelay(calibrationValue, MENU_ITEM_PUMP_5, DIGITAL_PIN_Pump_5, parts_per_day);    
    }
    //Pump 6
    if(hours == 22 && minutes == 45 && seconds == 0)   //dosing pumpa 6
    {
      lcd.setCursor(0,3);  
      lcd.print("Davkuji pumpa 6     ");
      int parts_per_day = 1000;
      verifyRelay(calibrationValue, MENU_ITEM_PUMP_6, DIGITAL_PIN_Pump_6, parts_per_day);    
    }
     //Evapor   
    if(minutes == 58 && seconds == 10)
    { 
      lcd.setCursor(0,3);  
      lcd.print("Doplnuji odpar      ");
      byte countEvapor=0;
      Val_FLOAT_EVAPOR = digitalRead(FLOAT_EVAPOR);     // zjištění stavu plováku
      evaporation(max_time_evapor, countEvapor);  
    }
  }
}

void checkvaluepumps()
  {
  pump1_menu=getSetupValue(MENU_ITEM_PUMP_1);
  pump2_menu=getSetupValue(MENU_ITEM_PUMP_2);
  pump3_menu=getSetupValue(MENU_ITEM_PUMP_3);
  pump4_menu=getSetupValue(MENU_ITEM_PUMP_4);
  pump5_menu=getSetupValue(MENU_ITEM_PUMP_5);
  pump6_menu=getSetupValue(MENU_ITEM_PUMP_6);
}
void showMainMenu()
{
  updateRTCVariables();   
  
  lcd.backlight();
  
  lcd.setCursor(0,0);
  sprintf(my_array, "%s %2d:%02d:%02d  ", weekDay, hours, minutes, seconds);
  lcd.print(my_array);
  lcd.setCursor(14,0);
  drawTerm();
  lcd.print(Celcius);

  lcd.setCursor(0,1);
  sprintf(my_array, "Ca%4d Kh%4d Mg%4d", pump1_menu, pump2_menu, pump3_menu);
  lcd.print(my_array);
 
  lcd.setCursor(0,2);
  sprintf(my_array, "N3%4d US%4d 6.%4d", pump4_menu, pump5_menu, pump6_menu);
  lcd.print(my_array);
  
  lcd.setCursor(0,3);  
  lcd.print("Menu -> Type #      ");
}


int getSetupValue(int indexValue)
{
  String setupValue = String(valueMatrix[indexValue][0]) 
    + String(valueMatrix[indexValue][1]) 
    + String(valueMatrix[indexValue][2]) 
    + String(valueMatrix[indexValue][3]);        
  setupValue.replace('_', '0');
  return setupValue.toInt();
}

void showMenu(char keyPress)
{
   handlerKeyPress(keyPress);
   printMenuItem(countMenu);
}

void handlerKeyPress(int keyPress)
{
  if(countMenu == MENU_ITEM_MAIN_MENU)
  {
      readMatrixInEEPROM();
  }
  if(keyPress == '#')
  {
      writeMatrixInEEPROM();

       if(turnOnPump1){
         turnOnPump1 = false;
         turnOffRelay(DIGITAL_PIN_Pump_1);
      }
      if(turnOnPump2){
         turnOnPump2 = false;
         turnOffRelay(DIGITAL_PIN_Pump_2);
      }
      if(turnOnPump3){
         turnOnPump3 = false;
         turnOffRelay(DIGITAL_PIN_Pump_3);
      }
      if(turnOnPump4){
         turnOnPump4 = false;
         turnOffRelay(DIGITAL_PIN_Pump_4);
      }
      if(turnOnPump5){
         turnOnPump5 = false;
         turnOffRelay(DIGITAL_PIN_Pump_5);
      }
      if(turnOnPump6){
         turnOnPump6 = false;
         turnOffRelay(DIGITAL_PIN_Pump_6);
      }
      
      
      if(countMenu == (MENU_FUNCTION-1))
      {
        countMenu = 0;
      }else
      {
        countMenu++;
      }
      countKey = 0;
    }else if(keyPress == '*')
    {
      //When time change, set this value in RTC
      if(countMenu == MENU_ITEM_TIME)
      {        
          updateTimeInRTC();
      }      
      else if(countMenu == MENU_ITEM_PUMP_1)
      {
         if(!turnOnPump1){
            turnOnPump1 = true;        
            turnOnRelay(DIGITAL_PIN_Pump_1);
         }
      }
      else if(countMenu == MENU_ITEM_PUMP_2)
      {
         if(!turnOnPump2){
            turnOnPump2 = true;        
            turnOnRelay(DIGITAL_PIN_Pump_2);
         }
      }
      else if(countMenu == MENU_ITEM_PUMP_3)
      {
         if(!turnOnPump3){
            turnOnPump3 = true;        
            turnOnRelay(DIGITAL_PIN_Pump_3);
         }
      }
      else if(countMenu == MENU_ITEM_PUMP_4)
      {
         if(!turnOnPump4){
            turnOnPump4 = true;        
            turnOnRelay(DIGITAL_PIN_Pump_4);
         }
      }
         else if(countMenu == MENU_ITEM_PUMP_5)
      {
         if(!turnOnPump5){
            turnOnPump5 = true;        
            turnOnRelay(DIGITAL_PIN_Pump_5);
         }
      }   
      else if(countMenu == MENU_ITEM_PUMP_6)
      {
         if(!turnOnPump6){
            turnOnPump6 = true;        
            turnOnRelay(DIGITAL_PIN_Pump_6);
         }
      }else{
        //Come back to The main menu
        countMenu = MENU_ITEM_MAIN_MENU;
        countKey = 0;
      }
    }else
    {
      valueMatrix[countMenu][countKey]  = keyPress;

      if(countKey == 3)
      {
        countKey = 0;
      }else
      {
        countKey++;
      }
    }
}

void printMenuItem(int countMenu)
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(menuItem[countMenu]);
  
  lcd.setCursor(0,1);
  if(countMenu == MENU_ITEM_TIME)
  {
    lcd.print(String(valueMatrix[countMenu][0]) + String(valueMatrix[countMenu][1]) + String(":") + String(valueMatrix[countMenu][2])+String(valueMatrix[countMenu][3]));  
  }else if(countMenu == MENU_ITEM_CALIBRATION)
  {
    lcd.print(String(valueMatrix[countMenu][0]) + String(valueMatrix[countMenu][1]) + String(valueMatrix[countMenu][2])+String(valueMatrix[countMenu][3]) + String(" ml/minute"));
  }else
  {
    lcd.print(String(valueMatrix[countMenu][0]) + String(valueMatrix[countMenu][1]) + String(valueMatrix[countMenu][2])+String(valueMatrix[countMenu][3]) + String(" ml"));
  }
}

void updateTimeInRTC()
{
  String hourTemp = String(valueMatrix[MENU_ITEM_TIME][0]) + String(valueMatrix[MENU_ITEM_TIME][1]);
  String minuteTemp = String(valueMatrix[MENU_ITEM_TIME][2]) + String(valueMatrix[MENU_ITEM_TIME][3]);
  
  hourTemp.replace('_', '0');
  minuteTemp.replace('_', '0');
   
  set3231Date((byte)hourTemp.toInt(), (byte)minuteTemp.toInt());
  
  //Clear time
  valueMatrix[MENU_ITEM_TIME][0] = '_';
  valueMatrix[MENU_ITEM_TIME][1] = '_';
  valueMatrix[MENU_ITEM_TIME][2] = '_';
  valueMatrix[MENU_ITEM_TIME][3] = '_';        
}

int convertStringToInt(String value)
{
  return value.toInt();
}

void drawTerm()
{
  lcd.write(1);
}
void drawDrop()
{
  lcd.write(1);
}

void drawDegree()
{
 lcd.print((char)223);
}

void startRelayModule()
{
   pinMode(FLOAT_EVAPOR, INPUT_PULLUP);
   
   pinMode(DIGITAL_PIN_Pump_1, OUTPUT);
   pinMode(DIGITAL_PIN_Pump_2, OUTPUT);
   pinMode(DIGITAL_PIN_Pump_3, OUTPUT);
   pinMode(DIGITAL_PIN_Pump_4, OUTPUT);
   pinMode(DIGITAL_PIN_Pump_5, OUTPUT);
   pinMode(DIGITAL_PIN_Pump_6, OUTPUT);
  
   pinMode(RELAY_EVAPOR, OUTPUT);           
   pinMode(ONE_WIRE_BUS,INPUT_PULLUP);
   
   turnOffRelay(DIGITAL_PIN_Pump_1);
   turnOffRelay(DIGITAL_PIN_Pump_2);
   turnOffRelay(DIGITAL_PIN_Pump_3);
   turnOffRelay(DIGITAL_PIN_Pump_4);
   turnOffRelay(DIGITAL_PIN_Pump_5);
   turnOffRelay(DIGITAL_PIN_Pump_6);
}

void turnOnRelay(int relayNu)
{
  digitalWrite(relayNu, HIGH);
}

void turnOffRelay(int relayNumber)
{
  digitalWrite(relayNumber, LOW);
}

//EEPROM functions
void  readMatrixInEEPROM()
{
  int count = startingAddress;
  for(int i = 0; i < MENU_FUNCTION; i++)
  {
    for(int j = 0; j < MENU_VALUE; j++)
    {
      valueMatrix[i][j] = EEPROM.read(count++);
    }
  }
 // checkvaluepumps();
}

void  writeMatrixInEEPROM(){
  int count = startingAddress;
  for(int i = 0; i < MENU_FUNCTION; i++)
  {
    for(int j = 0; j < MENU_VALUE; j++)
    {
      EEPROM.write(count++, valueMatrix[i][j]);
    }
  }
  checkvaluepumps();
}
//RTC functions
void updateRTCVariables()
{
  // send request to receive data starting at register 0
  Wire.beginTransmission(DS3231_I2C_ADDRESS); // 104 is DS3231 device address
  Wire.write(0x00); // start at register 0
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7); // request seven bytes

  if(Wire.available()) 
  {
    seconds = Wire.read(); // get seconds
    minutes = Wire.read(); // get minutes
    hours   = Wire.read();   // get hours
    day     = Wire.read();
    date    = Wire.read();
    month   = Wire.read(); //temp month
    year    = Wire.read();
       
    seconds = (((seconds & B11110000)>>4)*10 + (seconds & B00001111)); // convert BCD to decimal
    minutes = (((minutes & B11110000)>>4)*10 + (minutes & B00001111)); // convert BCD to decimal
    hours   = (((hours & B00110000)>>4)*10 + (hours & B00001111)); // convert BCD to decimal (assume 24 hour mode)
    day     = (day & B00000111); // 1-7
    date    = (((date & B00110000)>>4)*10 + (date & B00001111)); // 1-31
    month   = (((month & B00010000)>>4)*10 + (month & B00001111)); //msb7 is century overflow
    year    = (((year & B11110000)>>4)*10 + (year & B00001111));
  }
 
  switch (day) {
    case 1:
      strcpy(weekDay, "Ned");
      break;
    case 2:
      strcpy(weekDay, "Pon");
      break;
    case 3:
      strcpy(weekDay, "Ute");
      break;
    case 4:
      strcpy(weekDay, "Str");
      break;
    case 5:
      strcpy(weekDay, "Ctv");
      break;
    case 6:
      strcpy(weekDay, "Pat");
      break;
    case 7:
      strcpy(weekDay, "Sob");
      break;
  }
}

// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return ( (val/10*16) + (val%10) );
}

void set3231Date(byte _hours, byte _minutes)
{
//T(sec)(min)(hour)(dayOfWeek)(dayOfMonth)(month)(year)
//T(00-59)(00-59)(00-23)(1-7)(01-31)(01-12)(00-99)
//Example: 02-Feb-09 @ 19:57:11 for the 3rd day of the week -> T1157193020209
// T1124154091014
  seconds = 0; // Use of (byte) type casting and ascii math to achieve result.  
  minutes = _minutes ;
  hours   = _hours;
  day     = 1;
  date    = 1;
  month   = 1;
  year    = 00;
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0x00);
  Wire.write(decToBcd(seconds));
  Wire.write(decToBcd(minutes));
  Wire.write(decToBcd(hours));
  Wire.write(decToBcd(day));
  Wire.write(decToBcd(date));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));
  Wire.endTransmission();
}
