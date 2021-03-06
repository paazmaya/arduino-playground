#include <Keypad.h>


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

void setup(){
  Serial.begin(9600);
  delay(1000);
}

void loop(){
  delay(75);
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




