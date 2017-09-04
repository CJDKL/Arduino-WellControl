//CFA-634 LCD functions
// 0X00  Reserved
// 0X01  Cursor Home
// 0X02  Hide Display
// 0X03  Show Display
// 0X04  Hide Cursor
// 0X05  Show Underline Cursor
// 0X06  Show Blinking Block Cursor with Underscore
// 0X07  Reserved
// 0X08  Backspace (Destructive)
// 0X09  Module Configuration
// 0X10  Line Feed
// 0X11  Delete In Place
// 0X12  Form Feed (Clear Display)
// 0X13  Carriage Return
// 0X14  Backlight Control
// 0X15  Contrast Control
// 0X16  Reserved
// 0X17  Set Cursor Position (Column and Row)
// 0X18  Horizontal Bar Graph
// 0X19  Scroll ON
// 0X20  Scroll OFF
// 0X21  Reserved
// 0X22  Reserved
// 0X23  Wrap ON
// 0X24  Wrap OFF
// 0X25  Set Custom Character Bitmap
// 0X26  Reboot
// 0X27  Escape Sequence Prefix
// 0X28  Large Block Number
// 0X29  Reserved
// 0X30  Send Data Directly to LCD Controller
// 0X31  Show Information Screen

#include <Keypad.h>
#include <SoftwareSerial.h>

#define LCD_DISPLAY_TX_PIN 51
#define ledpin 13
#define M1PWM 7
#define VIN_PIN A14
#define WIN_PIN A6
#define AIN_PIN A5

/*
 * KEYBPAD VARIABLES
 */
const byte ROWS = 4; // Four rows
const byte COLS = 4; // Four columns


// Define the Keymap
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = { 22, 24, 26, 28 }; // Connect keypad ROW0, ROW1, ROW2 and ROW3 to these Arduino pins.
byte colPins[COLS] = { 30, 32, 34, 36 }; // Connect keypad COL0, COL1 and COL2 to these Arduino pins.
Keypad kpd = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

volatile boolean interrupted = false;
/*
 * LCD VARIABLES
 */
 
SoftwareSerial displayLCD(0, LCD_DISPLAY_TX_PIN);

/*
 * MEASURED VARIABLES
 */
 

String words = ""; //stores the input of the user until action

/*ADJUSTABLES
 */
 
double max_voltage = 24.300;
double last_max_voltage = 24.300;
double max_current = 1500.0;
double last_max_current = 1500.0;

double currentSlope = 13.079;
double currentIntercept = 54.115;
double voltageSlope = 0.1079;
double voltageIntercept = 0.0094;

double lastCurrentSlope = 13.079;
double lastCurrentIntercept = 54.115;
double lastVoltageSlope = 0.1079;
double lastVoltageIntercept = 0.0094;

int menuPage = 1;
long refreshMillis = 0;
long measureMillis = 0;
int screenrefreshrate = 0;
double target_voltage = 0;
double current_voltage = 0;
double target_current = 0;
double current_current = 0;
float target_pwm = 0;

int sampleInterval = 500;

boolean measurement = false;
boolean outputting = false;
boolean set = false;

double temp_mA = 0;
double temp_mV = 0;
double temp_mAinmV = 0;
float flow_v = 0;
float flowRate = 0;

String outputString = "";

boolean resistenceEstimated = false;
float actualVoltage = 0;
float actualCurrent = 0;
float estimatedResistence = 0;
float targetVoltage = 0;
int pwm_val = 0;

void output_pwm(int x){
  if(x == 21 && outputting){
    analogWrite(M1PWM, 255 * target_pwm / max_voltage);
  }else if(x == 22 && outputting){
    if(resistenceEstimated){
      stableCurrent(target_current);
    }else{
      calibrateResistence(target_current);
    }
  }else if(x == 23 && outputting){
    analogWrite(M1PWM, target_pwm * 255 / 100); //this is responsible for sending out PWM waves
  }else{
    analogWrite(M1PWM, 0);
  }
}


void calibrateResistence(int targetCurrent){
  analogWrite(M1PWM, 80); //send 8V to the well
  delay(100);
  actualVoltage = (analogRead(WIN_PIN)*5.0/1024.0+voltageIntercept)/voltageSlope;
  actualCurrent = (analogRead(AIN_PIN)*5.0/1024.0*1000.0-currentIntercept)/currentSlope/0.25;
  estimatedResistence = actualVoltage*1000/actualCurrent;
  targetVoltage = targetCurrent * estimatedResistence / 1000.0;
  resistenceEstimated = true;
  pwm_val = (targetVoltage + 0.0229)/0.0926;
}

void stableCurrent(int targetCurrent){
    analogWrite(M1PWM, pwm_val);
    delay(100);
    actualCurrent = (analogRead(AIN_PIN)*5.0/1024.0*1000.0-currentIntercept)/currentSlope/0.25;
    if(actualCurrent < 0){
      actualCurrent = 0;
    }
    if(actualVoltage < 0){
      actualVoltage = 0;
    }
    actualVoltage = (analogRead(WIN_PIN)*5.0/1024.0+voltageIntercept)/voltageSlope;
    if(actualCurrent < targetCurrent){
      if(abs(actualCurrent - targetCurrent)/40 > 1){
        pwm_val+= abs(actualCurrent - targetCurrent)/4;
      }else{
        pwm_val++;
      }
    }else{
      if(abs(actualCurrent - targetCurrent)/40 > 1){
        pwm_val-= abs(actualCurrent - targetCurrent)/4;
      }else{
        pwm_val--;
      }
    }
    if(pwm_val > 255){
      pwm_val = 255;
    }else if(pwm_val < 0){
      pwm_val = 0;
    }
}

void stableCurrentTimed(int numTests, int targetCurrent1, int ms1, int targetCurrent2, int ms2, int targetCurrent3, int ms3){
  analogWrite(M1PWM, 80); //send 31.4% pwm to the well
  delay(100);
  float actualVoltage = (analogRead(WIN_PIN)*5.0/1024.0+voltageIntercept)/voltageSlope;
  float actualCurrent = (analogRead(AIN_PIN)*5.0/1024.0*1000.0-currentIntercept)/currentSlope/0.25;
  float estimatedResistence = actualVoltage*1000/actualCurrent;
  float targetVoltage = targetCurrent1 * estimatedResistence / 1000.0;
  flow_v = analogRead(VIN_PIN) * 5.0 / 1024.0;
  flowRate = (flow_v + 0.01 - 0.937)/ 0.00719 + 4;
  double pwm_val = (targetVoltage + 0.0229)/0.0926;
  if(actualVoltage < 0){
    actualVoltage = 0;
  }
  if(actualCurrent < 0){
    actualCurrent = 0;
  }
  if(flowRate < 0){
    flowRate = 0;
  }
  if(pwm_val > 255){
    pwm_val = 255;
  }
  outputString = "";
  outputString += 31.4;
  outputString += "%, ";
  outputString += targetCurrent1;
  outputString += ", ";
  outputString += actualCurrent;
  outputString += ", ";
  outputString += flowRate;
  Serial.println(outputString);
  
  int runTest = numTests;
  int savedMs = ms1;
  int savedTargetCurrent = targetCurrent1;
  
  unsigned long startMillis = millis();
  unsigned long outputMillis = startMillis;
  while(runTest > 0){
    unsigned long currentMillis = millis();
    if(currentMillis - outputMillis > 100){
      outputMillis = currentMillis;
      analogWrite(M1PWM, pwm_val);
      actualCurrent = (analogRead(AIN_PIN)*5.0/1024.0*1000.0-currentIntercept)/currentSlope/0.25;
      if(actualCurrent < 0){
        actualCurrent = 0;
      }
      actualVoltage = (analogRead(WIN_PIN)*5.0/1024.0+voltageIntercept)/voltageSlope;
      if(actualVoltage < 0){
        actualVoltage = 0;
      }
      flow_v = analogRead(VIN_PIN) * 5.0 / 1024.0;
      flowRate = (flow_v + 0.01 - 0.937)/ 0.00719 + 4;
      if(flowRate < 0){
        flowRate = 0;
      }
      clearScreen();
      displayLCD.println("Proportional");
      displayLCD.println("Test Running...");
      displayLCD.print("Voltage: ");
      displayLCD.println(String(actualVoltage, 2));
      displayLCD.print("Current: ");
      displayLCD.print(String(actualCurrent, 2));
      if(actualCurrent < savedTargetCurrent){
        if(abs(actualCurrent - savedTargetCurrent)/20 > 1){
          pwm_val+= abs(actualCurrent - savedTargetCurrent)/4;
        }else{
          pwm_val++;
        }
      }else{
        if(abs(actualCurrent - savedTargetCurrent)/20 > 1){
          pwm_val-= abs(actualCurrent - savedTargetCurrent)/4;
        }else{
          pwm_val--;
        }
      }
      if(savedTargetCurrent == 0){
        pwm_val = 0;
      }
      if(pwm_val > 255){
        pwm_val = 255;
      }else if(pwm_val < 0){
        pwm_val = 0;
      }  
    }
    if(currentMillis - measureMillis > 500){
      measureMillis = currentMillis;
      outputString = "";
      outputString += String(pwm_val / 255.0 * 100, 1);
      outputString += "%, ";
      outputString += savedTargetCurrent;
      outputString += ", ";
      outputString += actualCurrent;
      outputString += ", ";
      outputString += flowRate;
      Serial.println(outputString);
    }
    if(currentMillis - startMillis > savedMs){
      startMillis = currentMillis;
      switch(runTest){
        case 3:
          runTest--;
          savedMs = ms2;
          savedTargetCurrent = targetCurrent2;
          break;
        case 2:
          runTest--;
          savedMs = ms3;
          savedTargetCurrent = targetCurrent3;
          break;
        case 1:
          runTest--;
      }
    }
  }
}

void EMO(){
  clearScreen();
  clearScreen();
  displayLCD.println("EMO!");
  displayLCD.println("    EMO!");
  displayLCD.println("        EMO!");
  displayLCD.print(  "            EMO!");
  analogWrite(M1PWM,0);
  //interrupted = true;
  
  while(1)
  {
  }
}
void setup(){
  displayLCD.begin(9600);
  Serial.begin(9600);
  pinMode(M1PWM, OUTPUT);
  pinMode(18,INPUT);
  attachInterrupt(digitalPinToInterrupt(18),EMO,CHANGE);
  TCCR4B = (TCCR2B & 0xF8) | 0x01;
  clearScreen();
  clearScreen();
  displayLCD.println("Select a mode:");
  displayLCD.println("A: Manual B: On/Off");
  displayLCD.println("C: Proportional");
  displayLCD.print("D: Configuration");
  screenrefreshrate = 10;
  Serial.println("End of File");
}



//Main loop that executes the measurement and displays sensor
void loop(){
  output_pwm(menuPage); //this function outputs the pwm waveform based on the menu page it is in
  unsigned long currentMillis = millis();
  /*
   * This section is for outputting the results onto a file if measuring
   */
  if(currentMillis - measureMillis > sampleInterval && measurement){ //this if statement outputs the measurements
    measureMillis = currentMillis;
    actualCurrent = (analogRead(AIN_PIN)*5.0/1024.0*1000.0-currentIntercept)/currentSlope/0.25;
    //actualVoltage = (analogRead(WIN_PIN)*5.0/1024.0+voltageIntercept)/voltageSlope;
    temp_mV = (max_voltage / 255.0) * 255 * 1000;
    flow_v = analogRead(VIN_PIN) * 5.0 / 1024.0;
    flowRate = (flow_v + 0.01 - 0.937)/ 0.00719 + 4;
    if(actualCurrent < 0){
      actualCurrent = 0;
    }
    if(flowRate < 0){
      flowRate = 0;
    }    
    outputString = "";
    switch(menuPage){
      case 21:
        outputString += target_pwm * 100 / max_voltage;
        outputString += "%, ";
        outputString += actualCurrent;
        outputString += ", ";
        outputString += flowRate;
        break;
      case 22:
        outputString += String(pwm_val *100.0 / 255.0,1);
        outputString += "%, ";
        outputString += actualCurrent;
        outputString += ", ";
        outputString += flowRate;
        break;
      case 23:
        outputString += target_pwm;
        outputString += "%, ";
        outputString += actualCurrent;
        outputString += ", ";
        outputString += flowRate;
    }
    Serial.println(outputString);
  }
  
  /*
   * This section is for programming the user interface display
   */
  if(currentMillis - refreshMillis > screenrefreshrate){ 
    refreshMillis = currentMillis;
    clearScreen();
    switch(menuPage){
      case 1:
        displayLCD.println("Select a mode:");
        displayLCD.println("A: Manual B: On/Off");
        displayLCD.println("C: Proportional");
        displayLCD.print("D: Configuration");
        screenrefreshrate = 10000;
        break;
      case 2:
        displayLCD.println("Manual:");
        displayLCD.println("A: Voltage");
        displayLCD.println("B: Current");
        displayLCD.print("C: PWM   D: Back");
        screenrefreshrate = 10000;
        break;
      case 21:
        displayLCD.println("Manual (Voltage):");
        displayLCD.print("Voltage: ");
        displayLCD.print(words);
        if(set){
          displayLCD.println("!");
        }else{
          displayLCD.println(" ");
        }
        displayLCD.println("A: Set    B: Clear");
        if(outputting){
          displayLCD.print("C: Stop   D: Back");
        }else{
          displayLCD.print("C: Start  D: Back");
        }
        screenrefreshrate = 10000;
        break;
      case 22:
        if(outputting){
          displayLCD.print("Current: ");
          displayLCD.println(String(actualCurrent,2));
        }else{
          displayLCD.println("Manual (Current):");
        }
        displayLCD.print("Set Current: ");
        displayLCD.print(words);
        if(set){
          displayLCD.println("!");
        }else{
          displayLCD.println("");
        }
        displayLCD.println("A: Set    B: Clear");
        if(outputting){
          displayLCD.print("C: Stop   D: Back");
        }else{
          displayLCD.print("C: Start  D: Back");
        }
        if(outputting){
          screenrefreshrate = 300;
        }else{
          screenrefreshrate = 10000;
        }
        break;
      case 23:
        displayLCD.println("Manual (PWM):");
        displayLCD.print("PWM: ");
        displayLCD.print(words);
        if(set){
          displayLCD.println("!");
        }else{
          displayLCD.println("");
        }
        displayLCD.println("A: Set    B: Clear");
        if(outputting){
          displayLCD.print("C: Stop   D: Back");
        }else{
          displayLCD.print("C: Start  D: Back");
        }
        screenrefreshrate = 10000;
        break;
      case 3:
        displayLCD.println("On/Off:");
        if(outputting){
          displayLCD.println("Test running...");
          displayLCD.println("");
          displayLCD.print(" ");
          lowHigh(); //this is put here because screen needs to be updated first before program runs for a minute
        }else{
          displayLCD.println("A: Start Test");   
          displayLCD.println(" ");    
          displayLCD.print("D: Back  ");
        }
        screenrefreshrate = 10000;
        break;
      case 4:
        displayLCD.println("Proportional:");
        if(outputting){
          displayLCD.println("Test running...");
          displayLCD.println("");
          displayLCD.print(" ");
          Serial.println("Proportional");
          Serial.println("Start of File");
          stableCurrentTimed(3, 0, 20000, 60, 20000, 300, 20000);
          Serial.println("End of File");
          outputting = false;
        }else{
          displayLCD.println("A: Run");
          displayLCD.println("");
          displayLCD.print("D: Back");
        }
        screenrefreshrate = 10000;
        break;
      case 5:
        displayLCD.println("A: Max Voltage");
        displayLCD.println("B: Max Current ");
        displayLCD.println("C: Calibration");
        displayLCD.print("D: Back");
        screenrefreshrate = 10000;
        break;
      case 51:
        displayLCD.println("Input Voltage:");
        displayLCD.print("V= ");
        if(words.equals("")){
          if(max_voltage == 0){
            displayLCD.println(String(last_max_voltage, 3));
          }else{
            displayLCD.println(String(max_voltage, 3));
          }
        }else{
          displayLCD.println(words);
        }
        displayLCD.println("A: Save    B: Clear");
        displayLCD.print("D: Back");
        screenrefreshrate = 10000;
        break;
      case 52:
        displayLCD.println("Input Current (mA):");
        displayLCD.print("mA= ");
        if(words.equals("")){
          if(max_current == 0){
            displayLCD.println(String(last_max_current, 0));
          }else{
            displayLCD.println(String(max_current, 0));
          }
        }else{
          displayLCD.println(words);
        }
        displayLCD.println("A: Save    B: Clear");
        displayLCD.print("D: Back");
        screenrefreshrate = 10000;
        break;
      case 53:
        displayLCD.println("A: Vslope B: Vx");
        displayLCD.println("C: Islope D: Ix");
        displayLCD.println(" ");
        displayLCD.print("#: Back");
        screenrefreshrate = 10000;
        break;
      case 531:
        displayLCD.println("Input V Slope:");
        displayLCD.print("V(s)= ");
        if(words.equals("")){
          displayLCD.println(String(lastVoltageSlope, 4));
        }else{
          displayLCD.println(words);
        }
        displayLCD.println("A: Save    B: Clear");
        displayLCD.print("D: Back");
        screenrefreshrate = 10000;
        break;
      case 532:
        displayLCD.println("Input V Intercept:");
        displayLCD.print("V(i)= ");
        if(words.equals("")){
          displayLCD.println(String(lastVoltageIntercept, 4));
        }else{
          displayLCD.println(words);
        }
        displayLCD.println("A: Save    B: Clear");
        displayLCD.print("D: Back");
        screenrefreshrate = 10000;
        break;
      case 533:
        displayLCD.println("Input I Slope:");
        displayLCD.print("I(s)= ");
        if(words.equals("")){
          displayLCD.println(String(lastCurrentSlope, 4));
        }else{
          displayLCD.println(words);
        }
        displayLCD.println("A: Save    B: Clear");
        displayLCD.print("D: Back");
        screenrefreshrate = 10000;
        break;
      case 534:
        displayLCD.println("Input I Intercept:");
        displayLCD.print("I(i)= ");
        if(words.equals("")){
          displayLCD.println(String(lastCurrentIntercept, 4));
        }else{
          displayLCD.println(words);
        }
        displayLCD.println("A: Save    B: Clear");
        displayLCD.print("D: Back");
        screenrefreshrate = 10000;
        break;
    }
  }
  /*
   * This section is for programming the user-input behavior
   */
  switch(menuPage){
    case 1:
      switch(keypadInput()){
        case 'A':
          screenrefreshrate = 0;
          menuPage = 2; break;
        case 'B':
          screenrefreshrate = 0;
          menuPage = 3; break;
        case 'C':
          screenrefreshrate = 0;
          menuPage = 4; break;
        case 'D':
          screenrefreshrate = 0;
          menuPage = 5; break;
        default:
          break;
      } 
      break;
    case 2:
      switch(keypadInput()){
        case 'A':
          screenrefreshrate = 0;
          set = false;
          menuPage = 21; break;
        case 'B':
          screenrefreshrate = 0;
          set = false;
          menuPage = 22; break;
        case 'C':
          screenrefreshrate = 0;
          menuPage = 23; break;
        case 'D':
          screenrefreshrate = 0;
          menuPage = 1; break;
      }
      break;
    case 21:
      switch(keypadInput()){
        case '0': (words.length() < 5) && !outputting && !set ? words = words + '0' : words = words; goto update; break;
        case '1': (words.length() < 5) && !outputting && !set ? words = words + '1' : words = words; goto update; break;
        case '2': (words.length() < 5) && !outputting && !set ? words = words + '2' : words = words; goto update; break;
        case '3': (words.length() < 5) && !outputting && !set ? words = words + '3' : words = words; goto update; break;
        case '4': (words.length() < 5) && !outputting && !set ? words = words + '4' : words = words; goto update; break;
        case '5': (words.length() < 5) && !outputting && !set ? words = words + '5' : words = words; goto update; break;
        case '6': (words.length() < 5) && !outputting && !set ? words = words + '6' : words = words; goto update; break;
        case '7': (words.length() < 5) && !outputting && !set ? words = words + '7' : words = words; goto update; break;
        case '8': (words.length() < 5) && !outputting && !set ? words = words + '8' : words = words; goto update; break;
        case '9': (words.length() < 5) && !outputting && !set ? words = words + '9' : words = words; goto update; break;
        case '*': (words.length() < 5) && !outputting && !set ? words = words + '.' : words = words; goto update; break;
        update:
        case 'U':
          if(!outputting && !set)
            screenrefreshrate = 10;
          break;
        case 'A':
          if(!outputting && !set){
            set = true;
            target_pwm = words.toFloat();
            if(target_pwm - 0.001 > max_voltage){
              target_pwm = 0;
              words = "";
              set = false;
            }else{
              words = String(target_pwm, 2);
            }
            screenrefreshrate = 10;
          }
          break;
        case 'B':
          if(!outputting){
            set = false;
            target_pwm = 0;
            words = "";
            screenrefreshrate = 10;
          }
          break;
        case 'C':
          if(set){
            if(!outputting){
              Serial.println("Manual V");
              Serial.println("Start of File");
              measurement = true;
            }else{
              Serial.println("End of File");
              measurement = false;
            }
            outputting = !outputting;
            
            screenrefreshrate = 10;
          }
          break;
        case 'D':
          if(!outputting){
            set = false;
            target_pwm = 0;
            words = "";
            screenrefreshrate = 10;
            menuPage = 2;
          }
          break;
      }
      break;
    case 22:
      switch(keypadInput()){
        case '0': (words.length() < 7) && !outputting && !set ? words = words + '0' : words = words; goto update1; break;
        case '1': (words.length() < 7) && !outputting && !set ? words = words + '1' : words = words; goto update1; break;
        case '2': (words.length() < 7) && !outputting && !set ? words = words + '2' : words = words; goto update1; break;
        case '3': (words.length() < 7) && !outputting && !set ? words = words + '3' : words = words; goto update1; break;
        case '4': (words.length() < 7) && !outputting && !set ? words = words + '4' : words = words; goto update1; break;
        case '5': (words.length() < 7) && !outputting && !set ? words = words + '5' : words = words; goto update1; break;
        case '6': (words.length() < 7) && !outputting && !set ? words = words + '6' : words = words; goto update1; break;
        case '7': (words.length() < 7) && !outputting && !set ? words = words + '7' : words = words; goto update1; break;
        case '8': (words.length() < 7) && !outputting && !set ? words = words + '8' : words = words; goto update1; break;
        case '9': (words.length() < 7) && !outputting && !set ? words = words + '9' : words = words; goto update1; break;
        update1:
        case 'U':
          if(!outputting && !set)
            screenrefreshrate = 10;
          break;
        case 'A':
          if(!outputting && !set){
            set = true;
            target_current = words.toFloat();
            if(target_current > max_current){
              target_current = 0;
              words = "";
              set = false;
            }else{
              words = String(target_current,0);
            }
            screenrefreshrate = 10;
          }
          break;
        case 'B':
          if(!outputting){
            set = false;
            target_current = 0;
            words = "";
            screenrefreshrate = 10;
          }
          break;
        case 'C':
          if(set){
            outputting = !outputting;
            if(outputting){
              Serial.println("Manual A");
              Serial.println("Start of File");
              measurement = true;
            }else{
              Serial.println("End of File");
              measurement = false;
            }
            screenrefreshrate = 0;
          }
          break;
        case 'D':
          if(!outputting){
            set = false;
            words = "";
            target_current = 0;
            menuPage = 2;
            screenrefreshrate = 10;
            resistenceEstimated = false;
          }
          break;
      }
      break;
    case 23:
      switch(keypadInput()){
        case '0': (words.length() < 3) && !outputting && !set ? words = words + '0' : words = words; goto update2; break;
        case '1': (words.length() < 3) && !outputting && !set ? words = words + '1' : words = words; goto update2; break;
        case '2': (words.length() < 3) && !outputting && !set ? words = words + '2' : words = words; goto update2; break;
        case '3': (words.length() < 3) && !outputting && !set ? words = words + '3' : words = words; goto update2; break;
        case '4': (words.length() < 3) && !outputting && !set ? words = words + '4' : words = words; goto update2; break;
        case '5': (words.length() < 3) && !outputting && !set ? words = words + '5' : words = words; goto update2; break;
        case '6': (words.length() < 3) && !outputting && !set ? words = words + '6' : words = words; goto update2; break;
        case '7': (words.length() < 3) && !outputting && !set ? words = words + '7' : words = words; goto update2; break;
        case '8': (words.length() < 3) && !outputting && !set ? words = words + '8' : words = words; goto update2; break;
        case '9': (words.length() < 3) && !outputting && !set ? words = words + '9' : words = words; goto update2; break;
        update2:
        case 'U':
          if(!outputting && !set)
            screenrefreshrate = 10;
          break;
        case 'A':
          if(!outputting && !set){
            set = true;
            target_pwm = words.toFloat();
            if(target_pwm > 100){
              target_pwm = 0;
              words = "";
              set = false;
            }else{
              words = String(target_pwm, 0);
            }
            screenrefreshrate = 10;
          }
          break;
        case 'B':
          if(!outputting){
            set = false;
            target_pwm = 0;
            words = "";
            screenrefreshrate = 10;
          }
          break;
        case 'C':
          if(set){
            if(!outputting){
              Serial.println("Manual PWM");
              Serial.println("Start of File");
              measurement = true;
            }else{
              Serial.println("End of File");
              measurement = false;
            }
            outputting = !outputting;
            screenrefreshrate = 10;
          }
          break;
        case 'D':
          if(!outputting){
            set = false;
            target_pwm = 0;
            words = "";
            screenrefreshrate = 10;
            menuPage = 2;
          }
          break;
      }
      break;
    case 3:
      switch(keypadInput()){
        case 'A':
          if(!outputting){
            outputting = true;
            screenrefreshrate = 10;
          }
          break;
        case 'D':
          if(!outputting){
            screenrefreshrate = 10;
            menuPage = 1; 
          }break;
      }
      break;
    case 4:
      switch(keypadInput()){
        case 'A':
          screenrefreshrate = 0;
          if(!outputting){
            outputting = true;              
          }
          break;
        case 'D':
          screenrefreshrate = 0;
          menuPage = 1; break;
      }
      break;
    case 5:
      switch(keypadInput()){
        case 'A':
          screenrefreshrate = 0;
          menuPage = 51; break;
        case 'B':
          screenrefreshrate = 0;
          menuPage = 52; break;
        case 'C':
          screenrefreshrate = 0;
          menuPage = 53; break;
        case 'D':
          screenrefreshrate = 0;
          menuPage = 1; break;
      }
      break;
    case 51:
      switch(keypadInput()){
        case '0': (words.length() < 5) ? words = words + '0' : words = words; goto update3; break;
        case '1': (words.length() < 5) ? words = words + '1' : words = words; goto update3; break;
        case '2': (words.length() < 5) ? words = words + '2' : words = words; goto update3; break;
        case '3': (words.length() < 5) ? words = words + '3' : words = words; goto update3; break;
        case '4': (words.length() < 5) ? words = words + '4' : words = words; goto update3; break;
        case '5': (words.length() < 5) ? words = words + '5' : words = words; goto update3; break;
        case '6': (words.length() < 5) ? words = words + '6' : words = words; goto update3; break;
        case '7': (words.length() < 5) ? words = words + '7' : words = words; goto update3; break;
        case '8': (words.length() < 5) ? words = words + '8' : words = words; goto update3; break;
        case '9': (words.length() < 5) ? words = words + '9' : words = words; goto update3; break;
        case '*': (words.length() < 5) ? words = words + '.' : words = words; goto update3; break;
        update3:
        case 'U':
          if(!outputting && !set)
            screenrefreshrate = 10;
          break;
        case 'A':
          if(!set){
            max_voltage = words.toFloat();
            if(max_voltage == 0){
              max_voltage = last_max_voltage;
            }
            screenrefreshrate = 10;
            words = "";
            words += String(max_voltage,3);
            words += "!";
            set = true;
          }
          break;
        case 'B':
          max_voltage = last_max_voltage;
          words = "";
          screenrefreshrate = 10;
          set = false;
          break;
        case 'D':
          if(max_voltage >= 0){
            last_max_voltage = max_voltage;
            menuPage = 5;
            words = "";
            set = false;
          }
          screenrefreshrate = 10;
          break;
        }
        break;
    case 52:
      switch(keypadInput()){
        case '0': (words.length() < 5) && !set ? words = words + '0' : words = words; goto update4; break;
        case '1': (words.length() < 5) && !set ? words = words + '1' : words = words; goto update4; break;
        case '2': (words.length() < 5) && !set ? words = words + '2' : words = words; goto update4; break;
        case '3': (words.length() < 5) && !set ? words = words + '3' : words = words; goto update4; break;
        case '4': (words.length() < 5) && !set ? words = words + '4' : words = words; goto update4; break;
        case '5': (words.length() < 5) && !set ? words = words + '5' : words = words; goto update4; break;
        case '6': (words.length() < 5) && !set ? words = words + '6' : words = words; goto update4; break;
        case '7': (words.length() < 5) && !set ? words = words + '7' : words = words; goto update4; break;
        case '8': (words.length() < 5) && !set ? words = words + '8' : words = words; goto update4; break;
        case '9': (words.length() < 5) && !set ? words = words + '9' : words = words; goto update4; break;
        update4:
        case 'U':
          if(!outputting && !set)
            screenrefreshrate = 10;
          break;
        case 'A':
          if(!set){
            max_current = words.toFloat();
            if(max_current == 0){
              max_current = last_max_current;
            }
          screenrefreshrate = 10;
            words = "";
            words += String(max_current,0);
            words += "!";
            set = true;
          }
          break;
        case 'B':
          max_current = last_max_current;
          words = "";
          screenrefreshrate = 10;
          set = false;
          break;
        case 'D':
          if(max_current >= 0){
            last_max_current = max_current;
            menuPage = 5;
            words = "";
            set = false;
          }
          screenrefreshrate = 10;
          break;
      }
      break;  
    case 53:
      switch(keypadInput()){
      case 'A':
        screenrefreshrate = 0;
        menuPage = 531; break;
      case 'B':
        screenrefreshrate = 0;
        menuPage = 532; break;
      case 'C':
        screenrefreshrate = 0;
        menuPage = 533; break;
      case 'D':
        screenrefreshrate = 0;
        menuPage = 534; break;
      case '#':
        screenrefreshrate = 0;
        menuPage = 5; break;
      }
      break;
    case 531:
      switch(keypadInput()){
        case '0': (words.length() < 7) && !set ? words = words + '0' : words = words; goto update5; break;
        case '1': (words.length() < 7) && !set ? words = words + '1' : words = words; goto update5; break;
        case '2': (words.length() < 7) && !set ? words = words + '2' : words = words; goto update5; break;
        case '3': (words.length() < 7) && !set ? words = words + '3' : words = words; goto update5; break;
        case '4': (words.length() < 7) && !set ? words = words + '4' : words = words; goto update5; break;
        case '5': (words.length() < 7) && !set ? words = words + '5' : words = words; goto update5; break;
        case '6': (words.length() < 7) && !set ? words = words + '6' : words = words; goto update5; break;
        case '7': (words.length() < 7) && !set ? words = words + '7' : words = words; goto update5; break;
        case '8': (words.length() < 7) && !set ? words = words + '8' : words = words; goto update5; break;
        case '9': (words.length() < 7) && !set ? words = words + '9' : words = words; goto update5; break;
        case '*': (words.length() < 7) && !set ? words = words + '.' : words = words; goto update5; break;
        case '#': (words.length() < 7) && !set ? words = words + '-' : words = words; goto update5; break;
        update5:
        case 'U':
          if(!outputting && !set)
            screenrefreshrate = 10;
          break;
        case 'A':
          if(!set){
            voltageSlope = words.toFloat();
            screenrefreshrate = 10;
            words = "";
            words += String(voltageSlope,4);
            words += "!";
            set = true;
          }
          break;
        case 'B':
          voltageSlope = lastVoltageSlope;
          words = "";
          screenrefreshrate = 10;
          set = false;
          break;
        case 'D':
          lastVoltageSlope = voltageSlope;
          menuPage = 53;
          words = "";
          set = false;
          screenrefreshrate = 10;
          break;
      }
      break;
    case 532:
      switch(keypadInput()){
        case '0': (words.length() < 7) && !set ? words = words + '0' : words = words; goto update6; break;
        case '1': (words.length() < 7) && !set ? words = words + '1' : words = words; goto update6; break;
        case '2': (words.length() < 7) && !set ? words = words + '2' : words = words; goto update6; break;
        case '3': (words.length() < 7) && !set ? words = words + '3' : words = words; goto update6; break;
        case '4': (words.length() < 7) && !set ? words = words + '4' : words = words; goto update6; break;
        case '5': (words.length() < 7) && !set ? words = words + '5' : words = words; goto update6; break;
        case '6': (words.length() < 7) && !set ? words = words + '6' : words = words; goto update6; break;
        case '7': (words.length() < 7) && !set ? words = words + '7' : words = words; goto update6; break;
        case '8': (words.length() < 7) && !set ? words = words + '8' : words = words; goto update6; break;
        case '9': (words.length() < 7) && !set ? words = words + '9' : words = words; goto update6; break;
        case '*': (words.length() < 7) && !set ? words = words + '.' : words = words; goto update6; break;
        case '#': (words.length() < 7) && !set ? words = words + '-' : words = words; goto update6; break;
        update6:
        case 'U':
          if(!outputting && !set)
            screenrefreshrate = 10;
          break;
        case 'A':
          if(!set){
            voltageIntercept = words.toFloat();
            screenrefreshrate = 10;
            words = "";
            words += String(voltageIntercept,4);
            words += "!";
            set = true;
          }
          break;
        case 'B':
          voltageIntercept = lastVoltageIntercept;
          words = "";
          screenrefreshrate = 10;
          set = false;
          break;
        case 'D':
          lastVoltageIntercept = voltageIntercept;
          menuPage = 53;
          words = "";
          set = false;
          screenrefreshrate = 10;
          break;
      }
      break;
    case 533:
      switch(keypadInput()){
        case '0': (words.length() < 7) && !set ? words = words + '0' : words = words; goto update7; break;
        case '1': (words.length() < 7) && !set ? words = words + '1' : words = words; goto update7; break;
        case '2': (words.length() < 7) && !set ? words = words + '2' : words = words; goto update7; break;
        case '3': (words.length() < 7) && !set ? words = words + '3' : words = words; goto update7; break;
        case '4': (words.length() < 7) && !set ? words = words + '4' : words = words; goto update7; break;
        case '5': (words.length() < 7) && !set ? words = words + '5' : words = words; goto update7; break;
        case '6': (words.length() < 7) && !set ? words = words + '6' : words = words; goto update7; break;
        case '7': (words.length() < 7) && !set ? words = words + '7' : words = words; goto update7; break;
        case '8': (words.length() < 7) && !set ? words = words + '8' : words = words; goto update7; break;
        case '9': (words.length() < 7) && !set ? words = words + '9' : words = words; goto update7; break;
        case '*': (words.length() < 7) && !set ? words = words + '.' : words = words; goto update7; break;
        case '#': (words.length() < 7) && !set ? words = words + '-' : words = words; goto update7; break;
        update7:
        case 'U':
          if(!outputting && !set)
            screenrefreshrate = 10;
          break;
        case 'A':
          if(!set){
            currentSlope = words.toFloat();
            screenrefreshrate = 10;
            words = "";
            words += String(currentSlope,4);
            words += "!";
            set = true;
          }
          break;
        case 'B':
          currentSlope = lastCurrentSlope;
          words = "";
          screenrefreshrate = 10;
          set = false;
          break;
        case 'D':
          lastCurrentSlope = currentSlope;
          menuPage = 53;
          words = "";
          set = false;
          screenrefreshrate = 10;
          break;
      }
      break;
    case 534:
      switch(keypadInput()){
        case '0': (words.length() < 7) && !set ? words = words + '0' : words = words; goto update8; break;
        case '1': (words.length() < 7) && !set ? words = words + '1' : words = words; goto update8; break;
        case '2': (words.length() < 7) && !set ? words = words + '2' : words = words; goto update8; break;
        case '3': (words.length() < 7) && !set ? words = words + '3' : words = words; goto update8; break;
        case '4': (words.length() < 7) && !set ? words = words + '4' : words = words; goto update8; break;
        case '5': (words.length() < 7) && !set ? words = words + '5' : words = words; goto update8; break;
        case '6': (words.length() < 7) && !set ? words = words + '6' : words = words; goto update8; break;
        case '7': (words.length() < 7) && !set ? words = words + '7' : words = words; goto update8; break;
        case '8': (words.length() < 7) && !set ? words = words + '8' : words = words; goto update8; break;
        case '9': (words.length() < 7) && !set ? words = words + '9' : words = words; goto update8; break;
        case '*': (words.length() < 7) && !set ? words = words + '.' : words = words; goto update8; break;
        case '#': (words.length() < 7) && !set ? words = words + '-' : words = words; goto update8; break;
        update8:
        case 'U':
          if(!outputting && !set)
            screenrefreshrate = 10;
          break;
        case 'A':
          if(!set){
            currentIntercept = words.toFloat();
            screenrefreshrate = 10;
            words = "";
            words += String(currentIntercept,4);
            words += "!";
            set = true;
          }
          break;
        case 'B':
          currentIntercept = lastCurrentIntercept;
          words = "";
          screenrefreshrate = 10;
          set = false;
          break;
        case 'D':
          lastCurrentIntercept = currentIntercept;
          menuPage = 53;
          words = "";
          set = false;
          screenrefreshrate = 10;
          break;
      }
      break;
  } 
}
char keypadInput(){
  char key = kpd.getKey();
  if(key){
    screenrefreshrate = 100;
    return key;
  }else{
  }
}
void askKeypad(){
  char key = kpd.getKey();
  if(key){
    switch (key){
      case '#':
        words = "";
        break;
      case '*':
        words = words + '.';
        break;
      case 'A':
        break;
      case 'B':
        break;
      case 'C':
        break;
      case 'D':
        break;
      default:
        words = words + key;
    }
    screenrefreshrate = 100;
  }
}
void clearScreen(){
  displayLCD.println("");
  displayLCD.println("");
  displayLCD.println("");
  displayLCD.print("");
  displayLCD.write((byte)012); // form feed (clears the display)
  displayLCD.write((byte)012); // form feed (clears the display)
  displayLCD.write((byte)012); // form feed (clears the display)
  displayLCD.write((byte)012); // form feed (clears the display)
  displayLCD.write((byte)001); // move cursor to top-left (home position) 
  displayLCD.write((byte)004); // hide blinking cursor
}

/*
 * TEST FUNCTIONS
 */

//used by case 3 with the on off function,
void lowHigh(){
  Serial.println("Low High");
  Serial.println("Start of File");
  unsigned long lowHighMillis_c = millis();
  long lowHighMillis_p = lowHighMillis_c;
  int decaSeconds = 0;
  int targetPWM = 0;
  while(1){
    lowHighMillis_c = millis();
    if(decaSeconds > 39 && decaSeconds < 80){
      analogWrite(M1PWM, 255);
      targetPWM = 100;
    }else if(decaSeconds < 120){
      analogWrite(M1PWM, 0);
      targetPWM = 0;
    }else{
      break;
    }
    if(lowHighMillis_c - lowHighMillis_p > 500){
      lowHighMillis_p = lowHighMillis_c;
      temp_mAinmV = analogRead(AIN_PIN) * 5.0 / 1024.0 * 1000.0;
      temp_mA = (temp_mAinmV - 30.875) / 12.938 * 4.0;
      temp_mV = (max_voltage / 255.0) * 255 * 1000;
      flow_v = analogRead(VIN_PIN) * 5.0 / 1024.0;
      flowRate = (flow_v + 0.01 - 0.937)/ 0.00719 + 4;
      actualVoltage = (analogRead(WIN_PIN)*5.0/1024.0+voltageIntercept)/voltageSlope;
      actualCurrent = (analogRead(AIN_PIN)*5.0/1024.0*1000.0-currentIntercept)/currentSlope/0.25;
      if(actualVoltage < 0){
        actualVoltage = 0;
      }
      if(actualCurrent < 0){
        actualCurrent = 0;
      }
      if(flowRate < 0){
        flowRate = 0;
      }
      outputString = "";
      outputString += targetPWM;
      outputString += "%, ";
      outputString += actualCurrent;
      outputString += ", ";
      outputString += flowRate;
      Serial.println(outputString);
      decaSeconds++;
    }
  }
  outputting = false;
  Serial.println("End of File");
  screenrefreshrate = 10;
}
