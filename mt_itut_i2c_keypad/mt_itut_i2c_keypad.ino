#include <Keypad.h>
#include "SoftI2CMaster.h"
#include "mpr121.h"

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

#define SDA_PIN 19 // The software SDA pin number.
#define SCL_PIN 18 // The software SCL pin number.

#define IRQ_PIN 20 // interrupts at this pin

boolean touchStates[12]; // to keep track of the previous touch states

// button is the index, milliseconds is the value
// History of events sent
unsigned long historyTouched[12]; 
unsigned long historyLifted[12];

// History of actual button states
unsigned long stateTouched[12]; 
unsigned long stateLifted[12];

// Keep the history of the previous drag position
int dragPos = -1;

// Four last drag buttons
int dragBtnHistory[4];

boolean dragState = UP;

unsigned long downTime = 0;
unsigned long upTime = 0;

// Time of the most recent DRAG event that got sent to Serial
unsigned long lastDragTime = 0;
int lastDragDirection;
int previousDragButton; // Avoid sensor unreliability



// -- Keypad related begin

char previousPressedKey;
boolean hasReleasedKey = false;

const byte ROWS = 5; //five rows
const byte COLS = 4; //three columns
char keys[ROWS][COLS] = {
   {'M','C','B'},
   {'1','2','3'},
   {'4','5','6'},
   {'7','8','9'},
   {'#','0','*'}
};
byte rowPins[ROWS] = {4, 0, 1, 2, 3}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {5, 6, 7, 8}; //connect to the column pinouts of the keypad
 
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
// -- Keypad related ends



SoftI2CMaster i2c = SoftI2CMaster();


void setup(){
  
  pinMode(IRQ_PIN, INPUT);
  
  Serial.begin(9600);
  
  i2c.setPins( SCL_PIN,SDA_PIN, 0 );

  delay(100);
  
  
  for (int i=0; i < 12; i++) {
    historyTouched[i] = 0;
    historyLifted[i] = 0;
    stateTouched[i] = 0;
    stateLifted[i] = 0;
  }
  
  for (int j = 0; j < 4; ++j) {
    dragBtnHistory[j] = -1;
  }
  
  mpr121_setup();
  
  delay(1000);
  
}

void loop(){
  readTouchInputs();
  delay(45);
  
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
  else if (state == HOLD) {
    Serial.print("hold_");
    Serial.println(previousPressedKey);
  }
}




void printHistoryTouched() {
  for (int i = 0; i < 12; i++) {
    Serial.print( historyTouched[i] );
    Serial.print(", ");
  }
  Serial.println("");
}

void printHistoryLifted() {
  for (int i = 0; i < 12; i++) {
    Serial.print( historyLifted[i] );
    Serial.print(", ");
  }
  Serial.println("");
}


boolean checkInterrupt(void){
  return digitalRead(IRQ_PIN);
}

// Problem seems to be that when smae finger is touching two buttons
// those buttons will trigger touched/lifted events in every loop
// But while using different fingers to touch two buttons, that works as expected.
void readTouchInputs(){
  if (!checkInterrupt()) {
    //read the touch state from the MPR121
    i2c.beginTransmission(0x5A);
    i2c.requestFrom(0x5A); 
    byte LSB = i2c.receive();
    byte MSB = i2c.receiveLast();
    i2c.endTransmission();
    
    

    uint16_t touched = ((MSB << 8) | LSB); //16bits that make up the touch states
    
    // There should be no recurring event within 120 ms
    unsigned long currentTime = millis();
    int timeLimit = 120;
    
    for (int i=0; i < 12; i++) {  // Check what electrodes were pressed
    
    
      // Touch state is updated every time in the list, but event passed only if it changed
      if (touched & (1<<i)) {
        
        // Previously UP, thus change to DOWN
        if (touchStates[i] == UP) {
          if (currentTime - stateTouched[i] > timeLimit) {
            changeState(i, DOWN);
          }
          stateTouched[i] = currentTime;
        }          
        
        touchStates[i] = DOWN;
        
      }
      else {
        // Previously DOWN, thus change to UP
        if (touchStates[i] == DOWN) {
          // pin i is no longer being touched
          if (currentTime - stateLifted[i] > timeLimit) {
            changeState(i, UP);
          }
          stateLifted[i] = currentTime;
          
        }
        
        touchStates[i] = UP;
      }
    }
    //printHistoryTouched();
    //printHistoryLifted();
  }
}

// Change the state of the given button. Only triggered when it does change.
void changeState(int btn, int state) {
  unsigned long currentTime = millis();
  
  // Send touching state, can be touched_ or lifted_
  sendState(btn, state);
  
  if (btn >= 12) return;
  /*
  Serial.print("  dragState:");
  Serial.print(dragState);
  Serial.print("  dragPos:");
  Serial.println(dragPos);
  */
  
  if (state == DOWN) {
    downTime = currentTime;

    
    if (dragState != UP) {
      dragState = DRAG;
      sendDrag(dragPos, btn);
    } 
    else {
      if (btn != dragPos && (currentTime - upTime) < 300) {
        dragState = DRAG;
        sendDrag(dragPos, btn);
      } 
      else {
        dragState = DOWN;
      }
    }
    dragPos = btn;
    
    
  } 
  else if (state == UP) {
    upTime = currentTime;
    
    dragState = UP;
  }
   
}

// Sensor too sensitive, time limit for same keys
void sendState(int btn, int state) {
  unsigned long currentTime = millis();
  
  if (state == UP) {
    historyLifted[ btn ] = currentTime;
    Serial.print("lifted_");
  } else {
    historyTouched[ btn ] = currentTime;
    Serial.print("touched_");
  }
    
  switch (btn) {
    /*
    // No hardware yet...
    case 14:
      Serial.println("options");
      break;
    case 15:
      Serial.println("menu");
      break;
    case 16:
      Serial.println("back");
      break;
    */
      
    case 9:
      Serial.println('*');
      break;
    case 10:
      Serial.println('0');
      break;
    case 11:
      Serial.println('#');
      break;
      
    default:
      Serial.println(btn + 1); // numbers
  }

}


// Scrolling should take place only at one direction at a time.
// Should also check that within certin time limit, the direction cannot change 180 degrees.
// Allows turning the direction after 300 ms
void sendDrag(int prev, int curr) {
  
  // Now using two keys for checking direction. Should start using three, in order to catch the false initial direction
  // when moving sideways...
  dragBtnHistory[0] = dragBtnHistory[1];
  dragBtnHistory[1] = dragBtnHistory[2];
  dragBtnHistory[2] = dragBtnHistory[3];
  dragBtnHistory[3] = curr;
  
   
  int histCol = (dragBtnHistory[0] / 3);
  int prevCol = (dragBtnHistory[1] / 3);
  int startCol = (prev / 3);
  int endCol = (curr / 3);
  int histRow = (dragBtnHistory[0] % 3);
  int prevRow = (dragBtnHistory[1] % 3);
  int startRow = (prev % 3);
  int endRow = (curr % 3);
  
  /*
  Serial.print("  cols: ");
  Serial.print(histCol);
  Serial.print(" ");
  Serial.print(prevCol);
  Serial.print(" ");
  Serial.print(startCol);
  Serial.print(" ");
  Serial.print(endCol);
  Serial.print(" -  rows: ");
  Serial.print(histRow);
  Serial.print(" ");
  Serial.print(prevRow);
  Serial.print(" ");
  Serial.print(startRow);
  Serial.print(" ");
  Serial.print(endRow);
  Serial.println();
  */
  
  // As there are only three columns but four rows, columns win when choosing between
  // vertical and horizontal scroll
  
  // Debugging 
  int isUp = 0;
  int isDown = 0;
  int isLeft = 0;
  int isRight = 0;
  
  
  int currentDirection = -1;
  
  // When all colums are different between each other, then it is horizontal
  // if the two values in the middle are the same, then check the fourth...
  if ((prevCol > startCol || (prevCol == startCol && histCol > prevCol)) && startCol > endCol && prevCol > endCol) {
    isUp = 1;
    currentDirection = DRAG_UP;
  }
  if ((prevCol < startCol || (prevCol == startCol && histCol < prevCol)) && startCol < endCol && prevCol < endCol) {
    isDown = 1;
    currentDirection = DRAG_DOWN;
  }
  if ((prevRow > startRow || (prevRow == startRow && histRow > prevRow)) && startRow > endRow && prevRow > endRow) {
    isLeft = 1;
    currentDirection = DRAG_LEFT;
  }
  if ((prevRow < startRow || (prevRow == startRow && histRow < prevRow)) && startRow < endRow && prevRow < endRow) {
    isRight = 1;
    currentDirection = DRAG_RIGHT;
  }
  /*
  Serial.print("  isUp: ");
  Serial.print(isUp);
  Serial.print("  isDown: ");
  Serial.print(isDown);
  Serial.print("  isLeft: ");
  Serial.print(isLeft);
  Serial.print("  isRight: ");
  Serial.print(isRight);
  Serial.println();
  */
  
  // Which direction would it be then?
  if (currentDirection != -1) {
    
      lastDragDirection = currentDirection;
      lastDragTime = millis();
      Serial.print("scrolled_");
      
      switch (currentDirection) {
        case DRAG_RIGHT: Serial.println("right"); break;
        case DRAG_LEFT: Serial.println("left"); break;
        case DRAG_DOWN: Serial.println("down"); break;
        case DRAG_UP: Serial.println("up"); break;
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

}

void set_register(int address, unsigned char r, unsigned char v){
  i2c.beginTransmission(address);
  i2c.send(r);
  i2c.send(v);
  i2c.endTransmission();
}



