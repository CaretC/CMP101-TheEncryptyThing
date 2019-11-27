/******************************************************************************
 CMP101 Assessment Joseph Lee 1903399

 First attempt at getting the manual SD card message encryption up and runnnig
 The concept behind this program is that a user enters a message to be via the 
 serial monitor. The user then selects an element of the key, this is then 
 combined with the value on the RTC when the message is encrypted with a random
 number and this is used to make up the full key.

 Once the key is generated the user's message will then be encrypted using XOR
 encryption, and cycling through the key as needed to encrypt the full message.

 If the user uses the small button on the breadboard and the 'decrypt LED' is 
 lit then the user will be prompted for a file name to decrypt and to enter the 
 key to decode the message. 

 If the key is correct the message will be decrypted, if it is incorrect then 
 the decoding will just produce gibberish. 
 *****************************************************************************/

// ========== Includes ====================
// General
// -------
#include <Streaming.h>

// OLED
// ----
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// LCD
// ---
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// RTC
// ---
#include <DS3231.h>
#include <SPI.h>

// SD
// --
#include <SD.h>

// =========== Globals =====================
// General
// -------
bool MODE_CONFIRMED = false;
bool BUTTON_LAST_STATE = false;
bool ENCRYPT_MODE = false;
bool FIRST_RUN = true;

// OLED
// ----
#define OLED_RESET -1
#define OLED_SCREEN_I2C_ADDRESS 0x3C
Adafruit_SSD1306 display(OLED_RESET);

// LCD
// ---
LiquidCrystal_I2C lcd(0x27,20,4);

byte LOCK_CHAR[] = {
  B01110,
  B10001,
  B10001,
  B10001,
  B11111,
  B11111,
  B11111,
  B11111
};

byte UNLOCK_CHAR[] = {
  B01110,
  B10001,
  B10000,
  B10000,
  B11111,
  B11111,
  B11111,
  B11111
};

// GPIO
// ----
#define DECODE_LED D4
#define DECODE_BUTTON D5

// Analog
// ------
#define POT_INPUT A0

// RTC
// ---
DS3231 rtc;

// ========= Setup Function ======================
void setup() {
  // Serial Setup
  // ------------
  Serial.begin(115200);
  while(!Serial){
    ; // wait for serial port to connect.
  }
  delay(1000);
  Serial << endl;  
  Serial << "Initializing Systems..." << endl;
  
  
  // OLED Setup
  // ----------
  Serial << "Initializing OLED ..." << endl;
  display.begin(SSD1306_SWITCHCAPVCC, OLED_SCREEN_I2C_ADDRESS);
  display.display(); // Print Adafruit Logo
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1); // - a line is 21 chars in this size
  display.setTextColor(WHITE);
  display.setCursor(1,3);
  display << endl;
  display << endl;
  display << endl;
  display << endl;
  display << " Initializing Screen" << endl;
  display.display();
  delay(2000);
  display.clearDisplay();
  display.display();
  Serial << "OLED Setup Complete..." << endl;

  // LCD Setup
  // ---------
  Serial << "Initializing LCD ..." << endl;
  lcd.init();
  lcd.backlight();
  lcd << "Initializing Screen";
  delay(2000);
  lcd.clear();
  lcd.createChar(0, LOCK_CHAR);
  lcd.createChar(1, UNLOCK_CHAR);
  Serial << "LCD Setup Complete..." << endl;

  // GPIO
  // ---
  Serial << "Setting up GPIO pins..." << endl;
  // ### LED
  pinMode(DECODE_LED, OUTPUT);
  LEDflashTest(DECODE_LED);

  // ### Button
  pinMode(DECODE_BUTTON, INPUT);
  Serial << "GPIO Pin Setup Complete..." << endl;

  // SD Card Setup
  // -------------
  Serial << "Initializing SD Card ..." << endl;
  if(SD.begin(0)){
    Serial << "SD Initialization Failed" << endl;
    while(1); // Hold Here Forever
  } else {
    Serial << "Testing SD Card ..." << endl;
    if(SDcardTest()){
      Serial << "SD Card Test Passed" << endl;
    } else {
      Serial << "SD Card Test Failed" << endl;
      while(1); // Hold Here Forever
    }    
  }

  // Setup Finished Messages
  Serial << "System Setup Finished..." << endl;
  lcd << "System Setup Done";
  delay(2000);

  // Welcome Message
  // ---------------
  LCDPrintWelcome();
  SerialPrintWelcome();
  delay(6000);
  LCDUserPrompt();
}

// ========== Loop Function ======================
void loop() {
  // Check if the serial connection is active
  if(!Serial.available() && FIRST_RUN){

    // Wait for user to confirm they have a connection
    while(!Serial.available()){
      Serial << "Please Confirm Connection By Pressing Any Key:" << endl;
      int inByte = Serial.read();
      delay(3000);
    }

//    // Clear Buffer
//    while(Serial.available()){
//      int inByte = Serial.read();
//      Serial << "Clear Buffer Char" << endl;
//    }
     serialRXFlush();
     Serial << Serial.available() << endl;

    // Connection Confirmed
    Serial << "Serial Connection Confirmed" << endl;
    LCDConnectionConfirmed();
    delay(1000);


    // Set FIRST_RUN
    FIRST_RUN = false;
  }

  // Ask user to select mode (encrypt or decrypt)
  LCDSelectMode();
  Serial << "Please select mode. Input 'E' for Encrypt and 'D' for decrypt:" << endl;
  bool isValidMode = false;

  // Take user input via serial and set ENCRYPT MODE
  while(!isValidMode){
    // Only read when there is data  
    while(Serial.available()){
      char inChar = Serial.read();

      if(inChar == 'E' || inChar == 'e' || inChar == 'D' || inChar == 'd'){
        isValidMode = true;

        if(inChar == 'E' || inChar == 'e'){
          ENCRYPT_MODE = true;
          serialRXFlush();
        } else {
          ENCRYPT_MODE = false;
          serialRXFlush();
        }        
      } else {
        Serial << "Invalid mode entered, Input 'E' for Encrypt and 'D' for decrypt:" << endl;
        serialRXFlush();
      }
    }    
  }

  Serial << Serial.available() << endl;
  
  // Set LED
  if(ENCRYPT_MODE){
    digitalWrite(DECODE_LED, 0);
  } else {
    digitalWrite(DECODE_LED, 1);
  }

  // TODO: Fix this it curretly isn't working.
  while(digitalRead(DECODE_BUTTON) == HIGH){
    delay(1); // To satify watchdog
  }

  
  if(ENCRYPT_MODE){
    LCDEncryptMode();
    Serial << "ENCRYPT MODE SELECTED" << endl;

    // TODO: Get message to encrypt;
//    serialRXFlush();
//    Serial << "Please enter the message you would like to encrypt below:" << endl;
//    bool isValidMessage = false;
    String inMessage = "Hello World";

//    while(!isValidMessage){
//      if(Serial.available() > 0){
//        char inChar = Serial.read();
//
//        if(inChar != '\r' || inChar != '\n'){
//          inMessage += inChar;
//        } else {
//          Serial << "End" << endl; // Do nothing
//          isValidMessage = true;
//        }
//      }
//    } 

      Serial << "Message is " << inMessage.length() << " chars long." << endl;
      // TEST
      Serial << inMessage << endl;
    
    // TODO: Generate Key from pot, & random number
    LCDSetPot();
    Serial << "Set potentiometer value, use small screen. Enter 1 when done:" << endl;
    bool potSet = false;
    int potValue = 0;
    
    while(!potSet){
      potValue = analogRead(POT_INPUT);
      display.clearDisplay();
      display.setCursor(0,0);
      display << "Potentiometer Value:" << endl;
      display << potValue << endl;
      display.display();

      if(Serial.available() > 0){
        char inChar = Serial.read();

        if(inChar == '1'){
          potSet = true;
        }
      }      
      delay(1); // To keep watchdog happy;
    }

    Serial << "Pot Value Set to: " << potValue << endl;

    int encKey = generateEncKey(potValue);

    Serial << "Enc Key is: " << encKey << endl;
    
    // Encrypt
    String EncMessage = encryptXOR(inMessage, encKey);

    Serial << EncMessage << endl;
    
    
    
  } else {
    LCDDecryptMode();
    serialRXFlush();
    Serial << "DECRYPT MOODE SELECTED" << endl;

    String encMessage = "~T[XXgYC[P";
    int key = 6174720;

    String decMessage = encryptXOR(encMessage, key);

    Serial << decMessage << endl;
    // TODO: Add encryptXOR()
  }
   
  delay(7000); // TESTING USE ONLY
  // Encode or Decode serial? > LED to confirm > button to move on

  // Encode
  // ------
  // * User Inputs Message > Save in GLOBAL?
  // * Generate Key > int generateKey() > encKey > User uses pot to set? + random()
  // * Encrypt message > string encryptXOR(encKey) > encMessage
  // * Save encMessage to file > file name "encMessage<RTCtime><date>.txt"
  // * Confirm file has been created
  // * Display key on the small screen for 60sec? > LCD & Serial Prompt user to take note. (super nice to have QR code)
  // * Clear little screen and forget key
  // * Back to start of programme

  // Decode
  // ------
  // * User Inputs file name
  // * Check file exists
  // * IF file exists then get user to enter key
  // * XOR message with the decryptXOR(encKey) > decMessage
  // * Select safe decrypt mode or destructive. Safe dosn't delete message file, it creates a new one to decMessage
  // * Decode and write to a file > file name "decMessage<RTCtime><date>.txt"
  // * Confirm file has been created
  // * IF ! safe mode delete original file.
  // * back to start of program

  
}

// ========== Functions ==========================
// Tests
// -----
// Simple Test to Confirm SD Card is working
bool SDcardTest(){
  bool testResult = false;

  // Check for test file
  if(SD.exists("test.txt")){
    SD.remove("text.txt");
    testResult = true;
  } else {
  File testFile = SD.open("test.txt", FILE_WRITE);
  testFile.close();
  if(SD.exists("test.txt")){
    SD.remove("text.txt");
    testResult = true;
  } else {
    testResult = false;
  }    
  }  
}

// Simple Test That Flashes LED
void LEDflashTest(int LEDpin){
  int flashDelay = 500;
  
  digitalWrite(DECODE_LED, 0);
  delay(flashDelay);
  digitalWrite(DECODE_LED, 1);
  delay(flashDelay);
  digitalWrite(DECODE_LED, 0);
  delay(flashDelay);
  digitalWrite(DECODE_LED, 1); 
  delay(flashDelay); 
  digitalWrite(DECODE_LED, 0);
  delay(flashDelay);
  digitalWrite(DECODE_LED, 1);  
}

// LCD
// ---
// Print Welcome Message on LCD
void LCDPrintWelcome(){
  lcd.clear();
  lcd << "********************";
  lcd.setCursor(0,1);
  lcd << "*  Welcome to the  *";
  lcd.setCursor(0,2);
  lcd << "*  Encrypty Thing  *";
  lcd.setCursor(0,3);
  lcd << "********************";
}

// Prints User Prompt
void LCDUserPrompt(){
  lcd.clear();
  lcd << "~~~~~~~~~~~~~~~~~~~~";
  lcd.setCursor(0,1);
  lcd << " Please Open Serial";
  lcd.setCursor(0,2);
  lcd << "Monitor To Continue";
  lcd.setCursor(0,3);
  lcd << "~~~~~~~~~~~~~~~~~~~~";
}

// Print COnnection Confirmed
void LCDConnectionConfirmed(){
  lcd.clear();
  lcd << "~~~~~~~~~~~~~~~~~~~~";
  lcd.setCursor(0,1);
  lcd << "  Serial Connection ";
  lcd.setCursor(0,2);
  lcd << "      Confirmed     ";
  lcd.setCursor(0,3);
  lcd << "~~~~~~~~~~~~~~~~~~~~";
}

// Print Select Mode
void LCDSelectMode(){
  lcd.clear();
  lcd << "~~~~~~~~~~~~~~~~~~~~";
  lcd.setCursor(0,1);
  lcd << "    Select Mode     ";
  lcd.setCursor(0,2);
  lcd << " Encrypt or Decrypt ";
  lcd.setCursor(0,3);
  lcd << "~~~~~~~~~~~~~~~~~~~~";
}

// Print Set Pot
void LCDSetPot(){
  lcd.clear();
  lcd << "~~~~~~~~~~~~~~~~~~~~";
  lcd.setCursor(0,1);
  lcd <<"  Set Potentiometer  ";
  lcd.setCursor(0,2);
  lcd << "  Use Small Screen  ";
  lcd.setCursor(0,3);
  lcd << "~~~~~~~~~~~~~~~~~~~~";
}

// Print Encrypt Mode
void LCDEncryptMode(){
  lcd.clear();
  lcd << "********************";
  lcd.setCursor(0,1);
  lcd << "*   Encrypt Mode   *";
  lcd.setCursor(0,2);
  lcd << "* ";

  // Print Line of LOCK_CHAR
  for(int i=0; i < 16; i++){
    lcd.write(0);
  }

  lcd << " *";
  lcd.setCursor(0,3);
  lcd << "********************";
}

// Print Decrypt Mode
void LCDDecryptMode(){
  lcd.clear();
  lcd << "********************";
  lcd.setCursor(0,1);
  lcd << "*   Decrypt Mode   *";
  lcd.setCursor(0,2);
  lcd << "* ";

  // Print Line of UNLOCK_CHAR
  for(int i=0; i < 16; i++){
    lcd.write(1);
  }

  lcd << " *";
  lcd.setCursor(0,3);
  lcd << "********************";
}

// Serial
//-------
// Print Wlecome Message Serial
void SerialPrintWelcome(){
  Serial << "********************" << endl;
  Serial << "*  Welcome to the  *" << endl;
  Serial << "*  Encrypty Thing  *" << endl;
  Serial << "********************" << endl;  
}

// Clears the RX Serial Buffer
void serialRXFlush(){
  while(Serial.available() > 0){
    int inByte = Serial.read();
    Serial << "Clear Buffer Char" << endl;
  }
}  

// Recieve String
String serialRecieveString(){
  String rxString = "";

  if(Serial.available() > 0){
    char inChar = Serial.read();

    if(inChar != '\r' || inChar != '\n'){
      rxString += inChar;
    } else {
      Serial << "End" << endl; // Do nothing
    }
  }

  serialRXFlush();
  
  return rxString;
}

// Encryption
// ----------
// Generate a key
int generateEncKey(int value){
  int key = random(0, 10000);

  key *= value;
  
  return key;
}

// Get Key index to loop
int keyIndex(int key, int index){
  int keyLen = String(key).length();
      
  return index % keyLen;
}

// Encrypt
String encryptXOR(String message, int key){
  int keyLength = String(key).length();
  char keySplit[keyLength];
  sprintf(keySplit, "%i", key); 
  String encMessage = "";

  for(int i=0; i < message.length(); i++){
    char encChar = message.charAt(i) ^ keySplit[i % keyLength];

    encMessage += encChar;
  }

  return encMessage;
}
