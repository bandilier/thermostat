
#include <SoftwareSerial.h>
const int TxPin = 7;
SoftwareSerial lcd = SoftwareSerial(255, TxPin);
SoftwareSerial xbee(35, 2); // RX, TX

const int buttonPin1 = A0;
const int buttonPin2 = A1;
const int buttonPin3 = A4;
const int buttonPin4 = A5;

const int heatPin = 3;
const int coolPin = 4;
const int fanPin = 5;
const int alarmPin = 6;

const int FANOFF = 3;
const int FANON = 3;
const int COOL = -1;
const int OFF = 0;
const int HEAT = 1;

const float temperature_correction = .40; // THIS ADJUSTS READING FROM THERMISTOR
const int defaultTemp = 68; // DEFAULT SETPOINT AT START-UP
int desiredTemp; // variable to store setpoint
int readTemp; // variable to store setpoint input set by user
int hvacStatus; // variable to store current status, eg.. heat-on, cool-on, off
int thermostatMode; // variable to store mode set by user, eg.. heat, cool, off
int temp_print; // changes float_temp F into two digits (for lcd)
int fanStatus = 2; // Fan status set to off at start

#define debounce 0 // ms debounce period to prevent flickering when pressing or releasing the button
#define holdTime 2000 // ms hold period: how long to wait for press+hold event

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

unsigned long nextLogTime; // variable to log currenttemp, and check if heat or cool should do something
unsigned long nextTempLog; // variable to log currenttemp for web page
unsigned long lastStatusChangeRequest; // variable to store last change

// Furnace Off 
unsigned long hvacLastRun = 0; // what time did we turn the output on?
const unsigned long hvacDelay = 960000; // Once furnace has finshed a cycle it wont start again for 16 minutes - 3-CPH

// Furnace On
unsigned long hvacLastStart = 0; // what time did we turn the output on?
const unsigned long hvacDelayOffAc = 240000; // Once started cool will run for 4 minutes Reguardless
const unsigned long hvacDelayOffHeat = 240000; // Once started heat will run for 4 minutes Reguardless

unsigned long hvacWaitHeat = 0; // what time did we turn the output on? Wait to turn on cool
unsigned long hvacWaitCool = 0; // what time did we turn the output on? Wait to turn on Heat


#define THERMISTORPIN A2 // which analog pin to connect
#define THERMISTORNOMINAL 10000 // resistance at 25 degrees C
#define TEMPERATURENOMINAL 25 // temp. for nominal resistance (almost always 25 C)
#define NUMSAMPLES 1 // how many samples to take and average, more = better but slower changes
#define BCOEFFICIENT 3950 // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR 10000 // the value of the resistor, 10K preferred
int samples[NUMSAMPLES]; // variable to store samples from thermistor


#define LCD_LIGHT_ON_TIME 60000 // How long (in milliseconds) should lcd light stay on?
unsigned int currentLcdLightOnTime = 0; // For calculating the lcd light on time.
unsigned long lcdLightOn_StartMillis;
boolean isLcdLightOn;
boolean clearLcd = false;
int clearLcdOnStart = 0;


//************************************************BEGIN-SETUP*********************************************************//

void setup(void) {

  analogReference(EXTERNAL); //reference for thermistor

  digitalWrite(heatPin, HIGH); //keeps pin from toggle at start-up
  digitalWrite(coolPin, HIGH); //keeps pin from toggle at start-up
  digitalWrite(fanPin, HIGH); //keeps pin from toggle at start-up
  digitalWrite(alarmPin, HIGH); //keeps pin from toggle at start-up

  pinMode(heatPin, OUTPUT); //Heatpin
  pinMode(coolPin, OUTPUT); //Coolpin
  pinMode(fanPin, OUTPUT); //Fanpin
  pinMode(alarmPin, OUTPUT); //alarmpin

  pinMode(buttonPin1, INPUT_PULLUP); 
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(buttonPin3, INPUT_PULLUP);
  pinMode(buttonPin4, INPUT_PULLUP);

  Serial.begin(9600);
  xbee.begin(9600);
  thermostatMode = HEAT;       //sets mode to heat at start-up
  hvacStatus = OFF;            // initial status
  nextLogTime += 0;            // initial logtime
  nextTempLog += 15000;        // initial Templogtime
  lastStatusChangeRequest = 0; // initial change request
  desiredTemp = defaultTemp;   // intial temp
  hvacLastRun = - 900000;

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

}

//************************************************BEGIN-LOOP*********************************************************//

void loop(void) { 
  lcd.write(22);  //start screen no cursor

  if(isLcdLightOn){
    currentLcdLightOnTime = millis() - lcdLightOn_StartMillis;
    if(currentLcdLightOnTime > LCD_LIGHT_ON_TIME){
      isLcdLightOn = false;
      lcd.write(18); //Light off
    }
  }

  float temp_f = currentTemp();
  temp_print = (float (temp_f) + .65); // This offset keeps the degree shown from bouncing back and forth
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
    nextLogTime += 10000;
  } 
  if (nextTempLog <= millis()) {
    xbee.print(temp_print);
    nextTempLog += 60000;
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
      setStatus = -1;

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
    if (setStatus == -1 && clearLcd == false){
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
  if (buttonState4 == LOW && setStatus == -1  && thermostatMode == COOL) {
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
  if (buttonState4 == LOW && setStatus == -1  && thermostatMode == HEAT) {
    setHvacStatus(OFF);
    thermostatMode = setStatus;
    lcd.write (159);
    lcd.print("    ");
  }
  if (buttonState4 == LOW && setStatus == -1  && thermostatMode == OFF) {
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
    if (readTemp == 106) {
      if (desiredTemp > 62) { 
        desiredTemp = desiredTemp - 1;
        Serial.print("set: ");
        Serial.print(desiredTemp);
        Serial.println(" ");
      }
    }
    if (readTemp == 107) {
      if (desiredTemp < 85) { 
        desiredTemp = desiredTemp + 1;
        Serial.print("set: ");
        Serial.print(desiredTemp);
        Serial.println(" ");
      }
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

  } 
  else if (hvacStatus == OFF && shouldTurnOnAc(temp_f) && (millis() - hvacLastRun) > hvacDelay) {
    setHvacStatus(COOL);
    hvacLastStart = millis();
  }
}
//************************************************BEGIN-CHECK-HEAT*********************************************************//
void checkHeat(float temp_f) {
  if (hvacStatus == HEAT && shouldTurnOffHeat(temp_f) && (millis() - hvacLastStart) > hvacDelayOffHeat) {
    setHvacStatus(OFF);
    hvacLastRun = millis();

  } 
  else if (hvacStatus == OFF && shouldTurnOnHeat(temp_f) && (millis() - hvacLastRun) > hvacDelay) {
    setHvacStatus(HEAT);
    hvacLastStart = millis();
  }
}

//************************************************BEGIN-THERMISTOR-READ*********************************************************//

float currentTemp() {

  uint8_t i;
  float average;
  for (i=0; i< NUMSAMPLES; i++) {
    samples[i] = analogRead(THERMISTORPIN);
    delay(10);
  }
  average = 0;
  for (i=0; i< NUMSAMPLES; i++) {
    average += samples[i];
  }
  average /= NUMSAMPLES;
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;
  float steinhart;
  steinhart = average / THERMISTORNOMINAL;
  steinhart = log(steinhart);
  steinhart /= BCOEFFICIENT;
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;
  float temp_c = steinhart;

  if (temp_c < 15){ // 59 degrees F
    //digitalWrite(alarmPin, LOW);
    return (9.0 / 5) * 21.111 + 32;

  }
  if (temp_c > 30){  // 86 degrees F
    //digitalWrite(alarmPin, LOW); 
    return (9.0 / 5) * 21.111 + 32;

  }
  else{
    //digitalWrite(alarmPin, HIGH);
    return (9.0 / 5) * temp_c + 32 + temperature_correction;

  }
}
//************************************************BEGIN-LOG-SERIAL*********************************************************//

void logToSerial(float temp_f) {
  lcd.write(129);            // Current Mode
  lcd.print(temp_print);
  lcd.print("  ");
  //Serial.println (float (temp_f));
  //xbee.print(temp_print);

  //************************************************BEGIN-SET-RELAYS*********************************************************//

  if (hvacStatus == HEAT && (millis() - hvacWaitHeat) > 70000) {
    digitalWrite(coolPin, HIGH);//cool off
    digitalWrite(heatPin, LOW);
    //Serial.println("heat-on");
    hvacWaitCool = millis();

  }
  else if (hvacStatus == COOL && (millis() - hvacWaitCool) > 70000) {
    digitalWrite(heatPin, HIGH);//heat off
    digitalWrite(coolPin, LOW);
    //Serial.println("ac-on");
    hvacWaitHeat = millis();

  }
  else {
    digitalWrite(heatPin, HIGH);
    digitalWrite(coolPin, HIGH);
    //Serial.println("off");

  } 
}

//************************************************BEGIN-SET-HVAC*********************************************************//

void setHvacStatus(int status) {
  Serial.print("setting hvac status: ");
  Serial.print(status);
  Serial.println(" ");

  hvacStatus = status;
}
bool shouldTurnOffAc(float temp) {
  //if (temp < (desiredTemp - 1)) {
  if (temp <= desiredTemp) {
    if (changeRequestTimelyEnough(lastStatusChangeRequest)) {
      return true;
    }
    lastStatusChangeRequest = millis();
  }

  return false;
}

bool shouldTurnOnAc(float temp) {
  if (temp > (desiredTemp + 1)) {
    if (changeRequestTimelyEnough(lastStatusChangeRequest)) {
      return true;
    }
    lastStatusChangeRequest = millis();
  }
  return false;
}

bool shouldTurnOffHeat(float temp) {
  //if (temp > (desiredTemp + 1)) {
  if (temp >= desiredTemp) {
    if (changeRequestTimelyEnough(lastStatusChangeRequest)) {
      return true;
    }
    lastStatusChangeRequest = millis();
  }

  return false;
}

bool shouldTurnOnHeat(float temp) {
  if (temp < (desiredTemp - 1)) {
    if (changeRequestTimelyEnough(lastStatusChangeRequest)) {
      return true;
    }
    lastStatusChangeRequest = millis();
  }

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
