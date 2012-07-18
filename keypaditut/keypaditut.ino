#include "mpr121.h"
#include <Wire.h>
#include <Keypad.h>

#define HELSINKI_LAYOUT 1
// 0 1 2
// 3 4 5
// 6 7 8
// 9 10 11

// Event types
#define UP 0
#define DOWN 1
#define DRAG 2

// Pin positions
#define OPTIONS 14
#define MENU 15
#define BACK 16

// Draggin directions
#define DRAG_LEFT 0
#define DRAG_UP 1
#define DRAG_RIGHT 2
#define DRAG_DOWN 3

int irqpin = 13; // was 17 // Digital 2
boolean touchStates[12]; // to keep track of the previous touch states

int touchHistory[12];
unsigned long upHistory[12];

boolean dragState = UP;
int downPos = -1;
int dragPos = -1;
unsigned long downTime = 0;
unsigned long upTime = 0;

// Time of the most recent DRAG event that got sent to Serial
unsigned long lastDragTime = 0;
int lastDragDirection;
int previousDragButton; // Avoid sensor unreliability


// Keypad related begin

char previousPressedKey;
boolean hasReleasedKey = false;

const byte ROWS = 5; //five rows
 const byte COLS = 3; //three columns
 char keys[ROWS][COLS] = {
   {'M','C','B'},
   {'1','2','3'},
   {'4','5','6'},
   {'7','8','9'},
   {'#','0','*'}
 };
 byte rowPins[ROWS] = {2, 6, 5, 4, 3}; //connect to the row pinouts of the keypad
 byte colPins[COLS] = {12, 11, 10}; //connect to the column pinouts of the keypad
 
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );


// Keypad related ends

void setup(){
  pinMode(irqpin, INPUT);
  digitalWrite(irqpin, HIGH); //enable pullup resistor
  for (int i=14; i <17; i++){
    pinMode(i, INPUT);
    digitalWrite(i, LOW); 
  }
  Serial.begin(9600);
  Wire.begin();

  for (int i=0; i < 12; i++) {
    touchHistory[i] = 0;
    upHistory[i] = 0;
  }
  mpr121_setup();
  
  delay(1000);
}

void loop(){
  readTouchInputs();
  //readTactileInputs();

  keypadLoop();
}  

void keypadLoop() {
    // Keypad related
  char key = keypad.getKey();
  KeyState state = keypad.getState();
  if (state == PRESSED && key != NO_KEY) {
    previousPressedKey = key;
    hasReleasedKey = false;
    Serial.print("pressed_");
    Serial.println(key);
  }
  else if (state == RELEASED && !hasReleasedKey) {
    // Multiple RELEASED events occur when there had not been HOLD
    Serial.print("released_");
    Serial.println(previousPressedKey);
    hasReleasedKey = true;
  }
  /*
  else if (state == HOLD) {
    Serial.print("hold_");
    Serial.println(previousPressedKey);
  }
  */
}



void printTouchHistory() {
  for (int i = 0; i < 12; i++) {
    Serial.print( touchHistory[i] );
    Serial.print(", ");
  }
  Serial.println("");
}

void printUpHistory() {
  for (int i = 0; i < 12; i++) {
    Serial.print( upHistory[i] );
    Serial.print(", ");
  }
  Serial.println("");
}

void readTouchInputs(){
  if(!checkInterrupt()){

    //read the touch state from the MPR121
    Wire.requestFrom(0x5A,2); 

    byte LSB = Wire.read();
    byte MSB = Wire.read();

    uint16_t touched = ((MSB << 8) | LSB); //16bits that make up the touch states
    
    for (int i=0; i < 12; i++){  // Check what electrodes were pressed
      int key = convertKey(i);
      if(touched & (1<<i)){
        
        if (touchStates[i] == UP) {
          changeState(key, DOWN);
        }          
        
        touchStates[i] = DOWN;
        
//        if (millis() - upHistory[0] > 100) {
//          // reset command (hack)
//          Serial.println("5, 5,");
//        }
      }
      else {
        if (touchStates[i] == DOWN) {
          //pin i is no longer being touched
          if (addToHistory(key)) {
            checkHistory();
          }
          changeState(key, UP);
        }
        touchStates[i] = UP;
      }
    }
//    printTouchHistory();
//    printUpHistory();
  }
}

int convertKey(int k) {
  // remap key to LONDON_LAYOUT
  //  0 4 8
  //  1 5 9
  //  2 6 10
  //  3 7 11
  
  int rtn = k;
  if (HELSINKI_LAYOUT) {
    return rtn;
  }
  
  // Translate LONDON_LAYOUT to key index, starting from 0
  switch (k) {
  case 1:
    rtn = 3;
  case 2:
    rtn = 6;
  case 3:
    rtn = 9;
  case 4:
    rtn = 1;
  case 5:
    rtn = 4;
  case 6:
    rtn = 7;
    break;
  case 7:
    rtn = 10;
    break;
  case 8:
    rtn = 2;
    break;
  case 9:
    rtn = 5;
    break;
  case 10:
    rtn = 8;
    break;
  case 11:
    rtn = 11;
    break;
  }
  return rtn;
}

void sendState(int btn, int state) {

  if (state == UP) {
    Serial.print("lifted_");
  } else {
    Serial.print("touched_");
  }
    
  switch (btn) {
    case 14:
      Serial.println("options");
      break;
    case 15:
      Serial.println("menu");
      break;
    case 16:
      Serial.println("back");
      break;
      
    case 0:
      Serial.println("itut_one");
      break;
    case 1:
      Serial.println("itut_two");
      break;
    case 2:
      Serial.println("itut_three");
      break;
    case 3:
      Serial.println("itut_four");
      break;
    case 4:
      Serial.println("itut_five");
      break;
    case 5:
      Serial.println("itut_six");
      break;
    case 6:
      Serial.println("itut_seven");
      break;
    case 7:
      Serial.println("itut_eight");
      break;
    case 8:
      Serial.println("itut_nine");
      break;
    case 9:
      Serial.println("itut_star");
      break;
    case 10:
      Serial.println("itut_zero");
      break;
    case 11:
      Serial.println("itut_hash");
      break;

    default:
      Serial.println(btn);
  }

}


void changeState(int btn, int state) {
  // Send touching state, can be touched_ or lifted_
  sendState(btn, state);
  
  if (btn >= 12) return;
  
  unsigned long currentTime = millis();
    
  if (state == DOWN) {
    downTime = currentTime;

    if (dragState != UP) {
      dragState = DRAG;
      sendDrag(dragPos, btn);
    } else {
      if (btn != dragPos && (currentTime - upTime) < 300) {
        dragState = DRAG;
        sendDrag(dragPos, btn);
      } else {
        downPos = btn;
        dragState = DOWN;
      }
    }
    dragPos = btn;
    
  } else if (state == UP) {
    upTime = currentTime;
    dragState = UP;
  }
   
}

// Scrolling should take place only at one direction at a time.
// Should also check that within certin time limit, the direction cannot change 180 degrees.
void sendDrag(int dragPos, int btn) {
    
  int startCol = (dragPos / 3);
  int endCol = (btn / 3);
  int startRow = (dragPos % 3);
  int endRow = (btn % 3);
  
  int deltaY = (endCol - startCol);
  int deltaX = (endRow - startRow);
  
  if ((deltaX != 0 || deltaY != 0) && previousDragButton != btn) {
    unsigned long currentTime = millis();
    int currentDirection;
    
    previousDragButton = btn;
    
    // Movement between column items
    if (deltaX > 0) {
      currentDirection = DRAG_RIGHT;
    } 
    else if (deltaX < 0) {
      currentDirection = DRAG_LEFT;
    }
  
    // Movement between row items
    if (deltaY > 0) {
      currentDirection = DRAG_DOWN;
    } 
    else if (deltaY < 0) {
      currentDirection = DRAG_UP;
    }
    
    boolean turned = (lastDragDirection != currentDirection);
    boolean turned90 = (lastDragDirection + 1 == currentDirection || lastDragDirection - 1 == currentDirection);
    boolean turned180 = (lastDragDirection + 2 == currentDirection || lastDragDirection - 2 == currentDirection);
    unsigned long timeDiff = currentTime - lastDragTime;
    
    /*
    Serial.print("  turned:");
    Serial.print(turned);
    Serial.print("  turned90:");
    Serial.print(turned90);
    Serial.print("  turned180:");
    Serial.print(turned180);
    Serial.print("  timeDiff:");
    Serial.print(timeDiff);
    Serial.println("");
    */
    
    // check time and direction
    if (!turned || timeDiff > 120) {
      lastDragDirection = currentDirection;
      lastDragTime = currentTime;
      Serial.print("scrolled_");
      
      switch (currentDirection) {
        case DRAG_RIGHT: Serial.println("right"); break;
        case DRAG_LEFT: Serial.println("left"); break;
        case DRAG_DOWN: Serial.println("down"); break;
        case DRAG_UP: Serial.println("up"); break;
      }
      
    }
  }

}


boolean addToHistory(int k) {
  
  if (k == touchHistory[0]) return false; // ignore duplicates

  for (int idx = 11; idx >= 1; idx--) {
    touchHistory[idx] = touchHistory[idx-1];
    upHistory[idx] = upHistory[idx-1];
  }
  touchHistory[0] = k;
  upHistory[0] = millis();
  return true;
}

void checkHistory() {

  int startCol = (touchHistory[1] / 3);
  int endCol = (touchHistory[0] / 3);
  int startRow = (touchHistory[1] % 3);
  int endRow = (touchHistory[0] % 3);
  
  unsigned long time = (upHistory[0] - upHistory[1]);
  
//  if (time > 300) {
//    // reset command (hack)
//    Serial.println("15, 15,");
//    return;
//  }
  
//  Serial.println(upHistory[0]);
//  Serial.println(upHistory[1]);
//  Serial.println(time);
  
  int tx = (endCol - startCol); // * (200/time);
  int ty = (endRow - startRow); // * (200/time);
  
//  int tx = 0;
//  int ty = 0;
//  int diff = touchHistory[0] - touchHistory[1];
//  if (abs(diff) == 1) {
//    if (diff > 0) {
//      ty = 1;
//    } else {
//      ty = -1;
//    }
//  } else if (abs(diff) == 4) {
//    if (diff > 0) {
//      tx = 1;
//    } else {
//      tx = -1;
//    }
//  }
  
//  Serial.print(tx);
//  Serial.print(",");
//  Serial.print(ty);
//  Serial.println(",");

//  Serial.print(" (");
//  Serial.print(time);
//  Serial.println(")");
  
}

void readTactileInputs(){
  static boolean backPressed, menuPressed, optionPressed;
  if(digitalRead(BACK)==HIGH){
    if(backPressed == UP) {
      changeState(BACK, DOWN);
      backPressed = DOWN; 
      delay(50);
    }
  } else {
    if(backPressed == DOWN){
      changeState(BACK, UP);
      backPressed = UP;
      delay(50);
    } 
  }

  if(digitalRead(MENU)==HIGH){
    if(menuPressed == UP){
      changeState(MENU, DOWN);
      menuPressed = DOWN; 
      delay(50);
    }
  }
  else{
    if(menuPressed == DOWN){
      changeState(MENU, UP);
      menuPressed = UP;
      delay(50);
    } 
  }

  if(digitalRead(OPTIONS)==HIGH){
    if(optionPressed == UP){
      changeState(OPTIONS, DOWN);
      optionPressed = DOWN; 
      delay(50);
    }
  } else {
    if(optionPressed == DOWN){
      changeState(OPTIONS, UP);
      optionPressed = UP;
      delay(50);
    } 
  }
}



void mpr121_setup(void){

  set_register(0x5A, ELE_CFG, 0x00); 

  // Section A - Controls filtering when data is > baseline.
  set_register(0x5A, MHD_R, 0x01);
  set_register(0x5A, NHD_R, 0x01);
  set_register(0x5A, NCL_R, 0x00);
  set_register(0x5A, FDL_R, 0x00);

  // Section B - Controls filtering when data is < baseline.
  set_register(0x5A, MHD_F, 0x01);
  set_register(0x5A, NHD_F, 0x01);
  set_register(0x5A, NCL_F, 0xFF);
  set_register(0x5A, FDL_F, 0x02);

  // Section C - Sets touch and release thresholds for each electrode
  set_register(0x5A, ELE0_T, TOU_THRESH);
  set_register(0x5A, ELE0_R, REL_THRESH);

  set_register(0x5A, ELE1_T, TOU_THRESH);
  set_register(0x5A, ELE1_R, REL_THRESH);

  set_register(0x5A, ELE2_T, TOU_THRESH);
  set_register(0x5A, ELE2_R, REL_THRESH);

  set_register(0x5A, ELE3_T, TOU_THRESH);
  set_register(0x5A, ELE3_R, REL_THRESH);

  set_register(0x5A, ELE4_T, TOU_THRESH);
  set_register(0x5A, ELE4_R, REL_THRESH);

  set_register(0x5A, ELE5_T, TOU_THRESH);
  set_register(0x5A, ELE5_R, REL_THRESH);

  set_register(0x5A, ELE6_T, TOU_THRESH);
  set_register(0x5A, ELE6_R, REL_THRESH);

  set_register(0x5A, ELE7_T, TOU_THRESH);
  set_register(0x5A, ELE7_R, REL_THRESH);

  set_register(0x5A, ELE8_T, TOU_THRESH);
  set_register(0x5A, ELE8_R, REL_THRESH);

  set_register(0x5A, ELE9_T, TOU_THRESH);
  set_register(0x5A, ELE9_R, REL_THRESH);

  set_register(0x5A, ELE10_T, TOU_THRESH);
  set_register(0x5A, ELE10_R, REL_THRESH);

  set_register(0x5A, ELE11_T, TOU_THRESH);
  set_register(0x5A, ELE11_R, REL_THRESH);

  // Section D
  // Set the Filter Configuration
  // Set ESI2
  set_register(0x5A, FIL_CFG, 0x04);

  // Section E
  // Electrode Configuration
  // Set ELE_CFG to 0x00 to return to standby mode
  set_register(0x5A, ELE_CFG, 0x0C);  // Enables all 12 Electrodes


  // Section F
  // Enable Auto Config and auto Reconfig
  /*set_register(0x5A, ATO_CFG0, 0x0B);
   set_register(0x5A, ATO_CFGU, 0xC9);  // USL = (Vdd-0.7)/vdd*256 = 0xC9 @3.3V   set_register(0x5A, ATO_CFGL, 0x82);  // LSL = 0.65*USL = 0x82 @3.3V
   set_register(0x5A, ATO_CFGT, 0xB5);*/  // Target = 0.9*USL = 0xB5 @3.3V

  set_register(0x5A, ELE_CFG, 0x0C);

}


boolean checkInterrupt(void){
  return digitalRead(irqpin);
}


void set_register(int address, unsigned char r, unsigned char v){
  Wire.beginTransmission(address);
  Wire.write(r);
  Wire.write(v);
  Wire.endTransmission();
}



