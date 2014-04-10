#include <EEPROM.h>
#include <SoftwareSerial.h>
const int TxPin = 7;
SoftwareSerial lcd = SoftwareSerial(255, TxPin);
SoftwareSerial xbee(35, 2); // RX, TX

#include "DHT.h"
#define DHTPIN 8
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

const int numReadings = 10;
float readings[numReadings];      // the readings from the analog input
int index = 0;                  // the index of the current reading
float total = 0;                  // the running total
float average = 0;                // the average

int inputPin = A2;

const int buttonPin1 = A0;
const int buttonPin2 = A1;
const int buttonPin3 = A4;
const int buttonPin4 = A5;
const int buttonPin5 = A3;

const int heatPin = 3;
const int coolPin = 4;
const int fanPin = 5;

const int COOL = 2;
const int OFF = 0;
const int HEAT = 1;

int desiredTemp; // variable to store setpoint
int readTemp; // variable to store setpoint input set by user
int hvacStatus; // variable to store current status, eg.. heat-on, cool-on, off
int thermostatMode; // variable to store mode set by user, eg.. heat, cool, off
int fanStatus = 2; // Fan status set to off at start


// Temp Down Button variables
int val; // variable for reading the pin status
int val2;  // variable for reading the delayed/debounced status
int temp_up;
int buttonState1; // variable to hold the button state

// Temp Up Button variables
int val3; // variable for reading the pin status
int val4; // variable for reading the delayed/debounced status
int temp_down;
int buttonState2; // variable to hold the button state

// button 3 debounce (Menu)
int buttonState3 = HIGH;
int previous = HIGH;
int counter = 2;
int setStatus;
int old_setStatus;
long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 50;    // the debounce time; increase if the output flickers

// Button variables for Select Button
int buttonState4 = HIGH;
int buttonVal = 0; // value read from button
int buttonLast = 0; // buffered value of the button's previous state
long btnDnTime; // time the button was pressed down
long btnUpTime; // time the button was released
boolean ignoreUp = false; // whether to ignore the button release because the click+hold was triggered
#define debounce 0 // ms debounce period to prevent flickering when pressing or releasing the button
#define holdTime 2000 // ms hold period: how long to wait for press+hold event


unsigned long nextLogTime; // variable to log currenttemp, and check if heat or cool should do something
unsigned long lastStatusChangeRequest; // variable to store last change

// Furnace Off 
unsigned long hvacLastRun = 0; // what time did we turn the output on?
const unsigned long hvacDelay = 480000; //900000; // Once furnace has finshed a cycle it wont start again for 16 minutes - 3-CPH

// Furnace On
unsigned long hvacLastStart = 0; // what time did we turn the output on?
const unsigned long hvacDelayOffAc = 240000; //300000; // Once started cool will run for 4 minutes Reguardless
const unsigned long hvacDelayOffHeat = 240000; //300000; // Once started heat will run for 4 minutes Reguardless

unsigned long hvacWaitHeat = 0; // what time did we turn the output on? Wait to turn on cool
unsigned long hvacWaitCool = 0; // what time did we turn the output on? Wait to turn on Heat


#define LCD_LIGHT_ON_TIME 60000 // How long (in milliseconds) should lcd light stay on?
unsigned int currentLcdLightOnTime = 0; // For calculating the lcd light on time.
unsigned long lcdLightOn_StartMillis;
boolean isLcdLightOn;
boolean clearLcd = false;
int clearLcdOnStart = 0;

int tempState;         
int lastTempState;

int modeState;         
int lastModeState;

float tempCheck;         
float lastTempCheck;



//************************************************BEGIN-SETUP*********************************************************//

void setup(void) {

  analogReference(EXTERNAL); //reference for thermistor

  digitalWrite(heatPin, HIGH); //keeps pin from toggle at start-up
  digitalWrite(coolPin, HIGH); //keeps pin from toggle at start-up
  digitalWrite(fanPin, HIGH); //keeps pin from toggle at start-up

  pinMode(heatPin, OUTPUT); //Heatpin
  pinMode(coolPin, OUTPUT); //Coolpin
  pinMode(fanPin, OUTPUT); //Fanpin

  pinMode(buttonPin1, INPUT_PULLUP); 
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(buttonPin3, INPUT_PULLUP);
  pinMode(buttonPin4, INPUT_PULLUP);
  pinMode(buttonPin5, INPUT_PULLUP);

  Serial.begin(9600);
  xbee.begin(9600);
  dht.begin();
  thermostatMode = EEPROM.read(1);       //sets mode to heat at start-up
  hvacStatus = OFF;            // initial status
  nextLogTime += 15000;            // initial logtime
  lastStatusChangeRequest = 0; // initial change request
  desiredTemp = EEPROM.read(2);   // intial temp
  hvacLastRun = - 360000;//- 480000; //- 900000;

  pinMode(TxPin, OUTPUT);
  digitalWrite(TxPin, HIGH);
  lcd.begin(19200);
  lcd.write(12); 

  isLcdLightOn = true;

  Serial.println(" ");
  Serial.print("set: ");
  Serial.print(desiredTemp);
  Serial.println(" ");
  Serial.print("Fan: ");
  Serial.print(fanStatus);
  Serial.println(" ");

  clearLcdOnStart = 1;

  for (int thisReading = 0; thisReading < numReadings; thisReading++)
    readings[thisReading] = 0;   

}

//************************************************BEGIN-LOOP*********************************************************//

void loop(void) {

  lcd.write(22);  //start screen no cursor

  if(isLcdLightOn){
    currentLcdLightOnTime = millis() - lcdLightOn_StartMillis;
    if(currentLcdLightOnTime > LCD_LIGHT_ON_TIME){
      isLcdLightOn = false;
      lcd.write(18); //Light off
      clearLcdOnStart = 1;

      if (thermostatMode == COOL){
        setStatus = 2;
        counter = 3;
      }
      else
        if (thermostatMode == HEAT){
          setStatus = 1;
          counter = 2;
        }
        else
        {
          setStatus = 0;
          counter = 1;
        }
    }  
  }


  float temp_f = currentTemp();
  desiredTemp = readDesiredTemp();


  if (thermostatMode == HEAT) {
    checkHeat(temp_f);
    lcd.write (149);
    lcd.print("Heat");
  }
  if (thermostatMode == COOL) {
    checkCool(temp_f);
    lcd.write (149);
    lcd.print("Cool");
  }
  else if (thermostatMode == OFF) {
    lcd.write (149);
    lcd.print("Off ");
  }
  if (nextLogTime <= millis()) {
    logToSerial(temp_f);
    nextLogTime += 15000;
  } 
  if (fanStatus == 2 ) {     // fan relay is off
    lcd.write (155);         // Fan off
    lcd.print("   "); 
  }
  if (fanStatus == 3 ) {     // fan relay is on
    lcd.write (155);         // Fan on
    lcd.print("Fan");  
  }
  if (clearLcdOnStart == 1) {
    lcd.write (159);
    lcd.print("    ");
    clearLcdOnStart = 0;
  }
  tempState = desiredTemp;
  if (tempState != lastTempState) {
    EEPROM.write(2,desiredTemp); ////////////////////////////////////////////////
    lastTempState = tempState;

  }
  modeState = thermostatMode;
  if (modeState != lastModeState) {
    EEPROM.write(1,thermostatMode); ////////////////////////////////////////////////
    lastModeState = modeState;

  } 

}

//************************************************BEGIN-USER-CONTROL*********************************************************//

int readDesiredTemp() {
  readTemp = desiredTemp;
  lcd.write (139);
  lcd.print("SP=");
  lcd.print(desiredTemp);

  val = digitalRead(buttonPin1);     
  val2 = digitalRead(buttonPin1);     
  if (val == val2) {                
    if (val != buttonState1) {          
      if (val == LOW && isLcdLightOn == false) {
        lcdLightOn_StartMillis = millis(); // Lcd Light Timer
        currentLcdLightOnTime = 0;
        isLcdLightOn = true;
        lcd.write(17);
        goto BAILOUT1; 
      }
      else
        if (val == LOW && desiredTemp < 85) {                
          desiredTemp = desiredTemp + 1;
          lcdLightOn_StartMillis = millis(); // Lcd Light Timer
          currentLcdLightOnTime = 0;
          isLcdLightOn = true;
          lcd.write(17); 
          Serial.print("set: ");
          Serial.print(desiredTemp);
          Serial.println(" ");      
        }
        else { 
        }
    } 
BAILOUT1:
    buttonState1 = val;            // save the new state in our variable
  }

  val3 = digitalRead(buttonPin2);     
  val4 = digitalRead(buttonPin2);     
  if (val3 == val4) {                
    if (val3 != buttonState2) { 

      if (val3 == LOW && isLcdLightOn == false) {
        lcdLightOn_StartMillis = millis(); // Lcd Light Timer
        currentLcdLightOnTime = 0;
        isLcdLightOn = true;
        lcd.write(17);
        goto BAILOUT2; 
      }
      else
        if (val3 == LOW && desiredTemp > 62) {                
          desiredTemp = desiredTemp - 1;
          lcdLightOn_StartMillis = millis();  // Lcd Light Timer
          currentLcdLightOnTime = 0;
          isLcdLightOn = true;
          lcd.write(17);
          Serial.print("set: ");
          Serial.print(desiredTemp);
          Serial.println(" ");
        } 
        else {   
        }
    }
BAILOUT2: 
    buttonState2 = val3;                // save the new state in our variable
  }

  buttonState3 = digitalRead(buttonPin3);
  if(buttonState3 == LOW){
    if((millis() - lastDebounceTime) > debounceDelay){
      lcdLightOn_StartMillis = millis(); //...........................................
      currentLcdLightOnTime = 0;
      isLcdLightOn = true;
      lcd.write(17); 
      counter ++;

      //Reset count if over max mode number

      if(counter == 4)
      {
        counter = 1;
      }
    }
    lastDebounceTime = millis(); //set the current time
  }
  else

      //Change mode
    switch (counter) {
    case 1:
      setStatus = 0;

      break;
    case 2:
      setStatus = 1;

      break;
    case 3:
      setStatus = 2;

      break;
    } 

  if (setStatus != old_setStatus){
    if (setStatus == 0 && clearLcd == false){
      lcd.write (159);
      lcd.print("Off ");

    }
    if (setStatus == 1 && clearLcd == false){
      lcd.write (159);
      lcd.print("Heat");

    } 
    if (setStatus == 2 && clearLcd == false){
      lcd.write (159);
      lcd.print("Cool");

    }
    old_setStatus = setStatus;
  }
  if (clearLcd == true){
    lcd.write (159);
    lcd.print("    ");
    clearLcd = false;
  }  
  buttonState4 = digitalRead(buttonPin4);

  if (buttonState4 == LOW && setStatus == 0  && thermostatMode == OFF) {
    lcd.write (159);
    lcd.print("    ");
  }   

  if (buttonState4 == LOW && setStatus == 1  && thermostatMode == HEAT) {
    lcd.write (159);
    lcd.print("    ");
  }
  if (buttonState4 == LOW && setStatus == 2  && thermostatMode == COOL) {
    lcd.write (159);
    lcd.print("    ");
  }
  if (buttonState4 == LOW && setStatus == 0  && thermostatMode == HEAT) {
    setHvacStatus(OFF);
    thermostatMode = setStatus;
    lcd.write (159);
    lcd.print("    ");
  }
  if (buttonState4 == LOW && setStatus == 0  && thermostatMode == COOL) {
    setHvacStatus(OFF);
    thermostatMode = setStatus;
    lcd.write (159);
    lcd.print("    ");
  }
  if (buttonState4 == LOW && setStatus == 2  && thermostatMode == HEAT) {
    setHvacStatus(OFF);
    thermostatMode = setStatus;
    lcd.write (159);
    lcd.print("    ");
  }
  if (buttonState4 == LOW && setStatus == 2  && thermostatMode == OFF) {
    setHvacStatus(OFF);
    thermostatMode = setStatus;
    lcd.write (159);
    lcd.print("    ");
  }
  if (buttonState4 == LOW && setStatus == 1  && thermostatMode == COOL) {
    setHvacStatus(OFF);
    thermostatMode = setStatus;
    lcd.write (159);
    lcd.print("    ");
  }
  if (buttonState4 == LOW && setStatus == 1  && thermostatMode == OFF) {
    setHvacStatus(OFF);
    thermostatMode = setStatus;
    lcd.write (159);
    lcd.print("    ");
  }
  // Read the state of the button
  buttonVal = digitalRead(buttonPin4);

  // Test for button pressed and store the down time
  if (buttonVal == LOW && buttonLast == HIGH && (millis() - btnUpTime) > long(debounce))
  {
    btnDnTime = millis();
  }

  // Test for button release and store the up time
  if (buttonVal == HIGH && buttonLast == LOW && (millis() - btnDnTime) > long(debounce))
  {
    if (ignoreUp == false) event1();
    else ignoreUp = false;
    btnUpTime = millis();
  }

  // Test for button held down for longer than the hold time
  if (buttonVal == LOW && (millis() - btnDnTime) > long(holdTime))
  {
    event2();
    ignoreUp = true;
    btnDnTime = millis();
  }

  buttonLast = buttonVal;



  if (Serial.available() > 0) {
    readTemp = Serial.read();

    if (readTemp == 100) {
      setHvacStatus(OFF);
      (thermostatMode = COOL);
      counter = 3;
      clearLcd = true;
      Serial.print("Mode: ");
      Serial.print(thermostatMode);
      Serial.println(" ");      
    }
    if (readTemp == 101) {
      setHvacStatus(OFF);
      (thermostatMode = HEAT);
      counter = 2;
      clearLcd = true;
      Serial.print("Mode: ");
      Serial.print(thermostatMode);
      Serial.println(" ");
    }
    if (readTemp == 102) {
      setHvacStatus(OFF);
      (thermostatMode = OFF);
      counter = 1;
      clearLcd = true;
      Serial.print("Mode: ");
      Serial.print(thermostatMode);
      Serial.println(" ");
    }
    if (readTemp == 103) {
      fanStatus = 3;
      digitalWrite(fanPin, LOW);
      Serial.print("Fan: ");
      Serial.print(fanStatus);
      Serial.println(" ");
    }
    if (readTemp == 104) {
      fanStatus = 2;
      digitalWrite(fanPin, HIGH);
      Serial.print("Fan: ");
      Serial.print(fanStatus);
      Serial.println(" ");
    }
    if (readTemp < 62) {
      goto BAILOUT;
    }
    if (readTemp > 85) {
      goto BAILOUT;
    }
    Serial.flush();
    return readTemp;
  } 
  else {
BAILOUT:
    return desiredTemp;
    Serial.println(desiredTemp);
  }
}

//************************************************ press+hold *********************************************************//
// Events to trigger by click and press+hold

void event1()
{
  lcdLightOn_StartMillis = millis(); //...........................................
  currentLcdLightOnTime = 0;
  isLcdLightOn = true;
  lcd.write(17);
  Serial.print("Mode: ");
  Serial.print(thermostatMode);
  Serial.println(" "); 

}
void event2()
{
  if (fanStatus == 2) { // fan relay is off
    digitalWrite(fanPin, LOW);
    fanStatus = 3;
    Serial.print("Fan: ");
    Serial.print(fanStatus);
    Serial.println(" ");
  }
  else 
    if (fanStatus == 3) { // fan relay is on
    digitalWrite(fanPin, HIGH);
    fanStatus = 2;
    Serial.print("Fan: ");
    Serial.print(fanStatus);
    Serial.println(" ");
  }
}
//************************************************BEGIN-CHECK-COOL*********************************************************//
void checkCool(float temp_f) {
  if (hvacStatus == COOL && shouldTurnOffAc(temp_f) && (millis() - hvacLastStart) > hvacDelayOffAc) {
    setHvacStatus(OFF);
    hvacLastRun = millis();
    Serial.println("off-Cool");

  } 
  else if (hvacStatus == OFF && shouldTurnOnAc(temp_f) && (millis() - hvacLastRun) > hvacDelay) {
    setHvacStatus(COOL);
    hvacLastStart = millis();
    Serial.println("on-Cool");
  }
}
//************************************************BEGIN-CHECK-HEAT*********************************************************//
void checkHeat(float temp_f) {
  if (hvacStatus == HEAT && shouldTurnOffHeat(temp_f) && (millis() - hvacLastStart) > hvacDelayOffHeat) {
    setHvacStatus(OFF);
    hvacLastRun = millis();
    Serial.println("off-Heat");

  } 
  else if (hvacStatus == OFF && shouldTurnOnHeat(temp_f) && (millis() - hvacLastRun) > hvacDelay) {
    setHvacStatus(HEAT);
    hvacLastStart = millis();
    Serial.println("on-Heat");
  }
}

//************************************************BEGIN-THERMISTOR-READ*********************************************************//

float currentTemp() {       

  float reading = analogRead(inputPin);
  reading = 1023 / reading - 1;
  reading = 10000 / reading;
  float steinhart;
  steinhart = reading / 10000;
  steinhart = log(steinhart);
  steinhart /= 3950;
  steinhart += 1.0 / (25 + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;
  float temp_c = steinhart;

  if (temp_c < 15){ // 59 degrees F
    return (desiredTemp + .00);
    //return (9.0 / 5) * 21.111 + 32;

  }
  if (temp_c > 30){  // 86 degrees F 
    return (desiredTemp + .00);
    //return (9.0 / 5) * 21.111 + 32;

  }
  else{
    //Serial.println(temp_c * 1.8 + 32.23);
    total= total - readings[index];         
    readings[index] = temp_c;
    total= total + readings[index];        
    index = index + 1;                    
    if (index >= numReadings)              
      index = 0;                          
    average = total / numReadings;
    average = 1.8 * average + 33.25;
    return average;
  }
}

//************************************************BEGIN-LOG-SERIAL*********************************************************//

void logToSerial(float temp_f) {

  tempCheck = (temp_f);
  if (tempCheck != lastTempCheck) {
    int humdState = dht.readHumidity();
    lcd.write(129);
    lcd.print(temp_f);
    lcd.print(" ");
    xbee.print('<');
    xbee.print(temp_f);
    xbee.print('p');
    xbee.print(humdState);
    xbee.print('>');
    lcd.write(135);
    lcd.print(humdState);
    lcd.print("%");
    lcd.print(" ");
    lastTempCheck = tempCheck;
    xbee.flush();
  }     

  //************************************************BEGIN-SET-RELAYS*********************************************************//

  if (hvacStatus == HEAT && (millis() - hvacWaitHeat) > 70000) {
    digitalWrite(coolPin, HIGH); //cool off
    digitalWrite(heatPin, LOW);
    hvacWaitCool = millis();

  }
  else if (hvacStatus == COOL && (millis() - hvacWaitCool) > 70000) {
    digitalWrite(heatPin, HIGH); //heat off
    digitalWrite(coolPin, LOW);
    hvacWaitHeat = millis();

  }
  else {
    digitalWrite(heatPin, HIGH);
    digitalWrite(coolPin, HIGH);
  } 
}

//************************************************BEGIN-SET-HVAC*********************************************************//

void setHvacStatus(int status) {
  hvacStatus = status;
}
bool shouldTurnOffAc(float temp) {
  if (temp <= desiredTemp) {
    //if (temp < (desiredTemp - .10)) {
    if (changeRequestTimelyEnough(lastStatusChangeRequest)) {
      return true;
    }
    lastStatusChangeRequest = millis();
  }

  return false;
}

bool shouldTurnOnAc(float temp) {
  if (temp > (lastTempCheck + .15)) {
    hvacLastRun = millis();
    Serial.println("Temp-Swing");
    goto BAILOUT4;
  }
  if (temp > (desiredTemp + .25)) {
    if (changeRequestTimelyEnough(lastStatusChangeRequest)) {
      return true;
    }
    lastStatusChangeRequest = millis();
  }
BAILOUT4:
  return false;
}

bool shouldTurnOffHeat(float temp) {
  if (temp >= desiredTemp) {
    //if (temp > (desiredTemp + .10)) {
    if (changeRequestTimelyEnough(lastStatusChangeRequest)) {
      return true;
    }
    lastStatusChangeRequest = millis();
  }

  return false;
}

bool shouldTurnOnHeat(float temp) {
  if (temp < (lastTempCheck - .15)) {
    hvacLastRun = millis();
    Serial.println("Temp-Swing");
    goto BAILOUT5;
  }
  if (temp < (desiredTemp - .25)) {
    if (changeRequestTimelyEnough(lastStatusChangeRequest)) {
      return true;
    }
    lastStatusChangeRequest = millis();
  }
BAILOUT5:
  return false;
}

bool changeRequestTimelyEnough(unsigned long previousRequest) {
  if (previousRequest > (millis() - 3000)) {
    return true;
  }
  else{
    return false;
  }
}


















