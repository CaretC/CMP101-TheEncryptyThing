/******************************************************************************
 CMP101 Assessment - The Encrypty Thing
 Author: Joseph Lee 
 Student# 1903399

 This device was created for the CMP101 end of module assessment. It uses the 
 following components:
    * Wemos board 
    * OLED 
    * RTC 
    * Potentiometer
    * Button
    * LED
    * SD card module
    * LCD screen

 This device is called The Encrypty Thing. This device utilizes XOR encryption
 to encrypt a message from the SD using a key generated using user input 
 potentiometer value and a ranomly generated number. If the message is longer 
 then the key, the key is cycled. For more information on how to use this 
 device for code implementaion please consult README.
 *****************************************************************************/

// ========== Includes ====================
// General
// -------
// For C++ style syntax (e.g. << operators)
#include <Streaming.h>

// OLED
// ----
// Needed to drive the OLED screen
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// LCD
// ---
// Needed to drive the LCD screen
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// RTC
// ---
// Needed to use the Real Time Clock module
#include <DS3231.h>
#include <SPI.h>

// SD
// --
// Needed to use the SD card module
#include <SD.h>

// =========== Globals =====================
// General
// -------
bool MODE_CONFIRMED = false;
bool ENCRYPT_MODE = false;
bool FIRST_RUN = true;
bool TEST_MODE = false;

// OLED
// ----
#define OLED_RESET -1
#define OLED_SCREEN_I2C_ADDRESS 0x3C
Adafruit_SSD1306 display(OLED_RESET);

// LCD
// ---
LiquidCrystal_I2C lcd(0x27,20,4);

// Store a custom lock character used on LCD 
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

// Store a custom unlock character used on LCD
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
#define ENCRYPT_LED D4
#define TEST_MODE_BUTTON D8

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
//  while(!Serial){
//    ; // wait for serial port to connect.
//  }
  delay(1000);
  Serial << endl;  
  Serial << "Initializing Systems..." << endl;
  
  
  // OLED Setup
  // ----------
  Serial << "Initializing OLED ..." << endl;

  // Display initial message on OLED
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

  // Display initial message on LCD
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
  
  // LED blink to confirm connection
  pinMode(ENCRYPT_LED, OUTPUT);
  LEDflashTest(ENCRYPT_LED);


  // Read button to set TEST_MODE. (TEST_MODE enabled if HIGH)
  pinMode(TEST_MODE_BUTTON, INPUT);
  int testButtonState = digitalRead(TEST_MODE_BUTTON);
  Serial << "Test mode button: " << testButtonState << endl;
  if(testButtonState == 1){
    Serial << "TEST MODE ENABLED" << endl;
    TEST_MODE = true;
  }
  
  Serial << "GPIO Pin Setup Complete..." << endl;

  // SD Card Setup
  // -------------
  Serial << "Initializing SD Card ..." << endl;

  // Initialise and test SD card
  if(!SD.begin(0)){
    // Error
    Serial << "SD Initialization Failed" << endl;
    while(1){
      LEDErrFlash(ENCRYPT_LED);
      delay(1); // To keep watchdog happy
    }
  } else {
    Serial << "Testing SD Card ..." << endl;
    if(SDcardTest()){
      Serial << "SD Card Test Passed..." << endl;
    } else {
      // Error
      Serial << "SD Card Test Failed" << endl;
      while(1){
        LEDErrFlash(ENCRYPT_LED);
        delay(1); // To keep watchdog happy
      }
    }    
  }

  // RTC
  // ---
  Serial << "Testing RTC..." << endl;

  // Test to confirm RTC is reading data
  if(RTCTest()){
    Serial << "RTC Test Passed..." << endl;
  } else {
    Serial << "RTC Test Failed" << endl;
    while(1){
      // Error
      LEDErrFlash(ENCRYPT_LED);
      delay(1); // To keep watchdog happy
    }
  }
  
  // Setup Finished Messages
  Serial << "System Setup Finished..." << endl;
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
  // First run, confirm the connection
  // ---------------------------------
  if(!Serial.available() && FIRST_RUN){

    // Wait for user to confirm they have a connection
    while(!Serial.available()){
      Serial << "Please Confirm Connection By Pressing Any Key:" << endl;
      int inByte = Serial.read();
      delay(3000);
    }

     // Flush unsed data from serial RX buffer
     serialRXFlush(); 

     // TEST MODE - Print serial state to help debug
     if(TEST_MODE){
      Serial << endl;
      Serial << "TEST MODE - Serial state: " << Serial.available() << endl;
      Serial << endl;
     }
     
    // Connection Confirmed Message
    Serial << "Serial Connection Confirmed" << endl;
    LCDConnectionConfirmed();
    delay(1000);


    // Set FIRST_RUN to false so this section only runs on startup
    FIRST_RUN = false;
  }

  // Select Mode
  // -----------  
  // Ask user to select mode (encrypt or decrypt)
  LCDSelectMode();
  Serial << "Please select mode. Input 'E' for Encrypt and 'D' for decrypt:" << endl;
  bool isValidMode = false;

  // Take user input via serial and set ENCRYPT_MODE
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

  // TEST MODE - Print serial state to help debug
  if(TEST_MODE){
    Serial << endl;
    Serial << "TEST MODE - Serial state: " << Serial.available() << endl;
    Serial << endl;
  }  
  
  // Set ENCRYPT_LED - (ENCRYPT = ON, DECRYPT = OFF)
  // ---------------
  if(ENCRYPT_MODE){
    digitalWrite(ENCRYPT_LED, 0);
  } else {
    digitalWrite(ENCRYPT_LED, 1);
  }

  // Encrypt Mode Section
  // ====================
  if(ENCRYPT_MODE){

    // Print mode on LCD
    LCDEncryptMode();

    // Prompt user input to confirm
    Serial << "ENCRYPT MODE SELECTED" << endl;
    Serial << "Would you like to encrypt inMessage.txt? Press 'Y' to encrypt:" << endl;
    bool encCont = false;
    String inMessage = "";

    while(!encCont){
      if(Serial.available() > 0){
        char inChar = Serial.read();

        if(inChar == 'Y' || inChar == 'y'){
          encCont = true;
        }         
      }
    }

    // Read in Data From SD Card
    // --------------------------
    // Reads in the data from inMessage.txt
    if(SD.exists("inMessage.txt")){
      File messageFile = SD.open("inMessage.txt");
      while(messageFile.available()){
        inMessage += messageFile.readStringUntil('\n');
        inMessage += '\n';
      }
      messageFile.close();
    } else {
      Serial << "inMessage.txt not found. Please check and restart..." << endl;
      while(1){
        // Error
        LEDErrFlash(ENCRYPT_LED);
        delay(1); // To keep watchdog happy
      }
    } 
    
    // Print inMessage summary to Serial
    // ---------------------------------
    Serial << endl;
    Serial << "Message is " << inMessage.length() << " characters long." << endl;  
    Serial << endl;
    Serial << "************ IN MESSAGE START ************" << endl;  
    Serial << inMessage << endl;
    Serial << "************ IN MESSAGE END **************" << endl;  
    Serial << endl;
    
    // Read in potentiometer value
    // ---------------------------    
    // Print LCD message
    LCDSetPot();

    // Read in value when user confirms
    Serial << "Set potentiometer value, use small screen. Enter 'Y' when done:" << endl;
    bool potSet = false;
    int potValue = 0;

    while(!potSet){
      potValue = analogRead(POT_INPUT);
      display.clearDisplay();
      display.setCursor(0,0);
      display << "Potentiometer Value:" << endl;
      display.setTextSize(3);
      display << endl;
      display << potValue << endl;
      display.setTextSize(1);
      display.display();

      if(Serial.available() > 0){
        char inChar = Serial.read();

        if(inChar == 'Y' || inChar == 'y'){
          potSet = true;
        }
      }      
      delay(1); // To keep watchdog happy;
    }

    // Generate Encryption Key
    // -----------------------
    // Takes in the potentiometer value to use in key generation
    int encKey = generateEncKey(potValue);

    // TEST MODE - Display the potentiometer value and encryption key to help
    // debugging.
    if(TEST_MODE){
      // TEST display Pot value set
      Serial << endl;
      Serial << "Potentiometer Value Selected: " << potValue << endl;
      Serial << "Enc Key is: " << encKey << endl;
      Serial << endl;
    }

    // Display EncKey on Small OLED
    // -----------------------------
    // Displays the generated encryption key on OLED so user can make note of it
    display.clearDisplay();
    display.setCursor(0,0);
    display << "  Message Encryption" << endl;
    display << "  Key" << endl;    
    display.setTextSize(2);
    display << endl;
    display << " " << encKey << endl;
    display.setTextSize(1);
    display.display();

    // Add Encryption Header to inMessage
    // ----------------------------------
    String fullInMessage = fileHeader(ENCRYPT_MODE) + inMessage;
    
    // Encrypt Message
    // ---------------
    // Using encryption key and XOR encrytion to encrypt the message
    String EncMessage = encryptXOR(fullInMessage, encKey);

    // Print Encrypted Message Summary Serial
    // --------------------------------------
    Serial << endl;
    Serial << "************ ENCRYPTED MESSAGE START ************" << endl;  
    Serial << EncMessage << endl;
    Serial << "************ ENCRYPTED MESSAGE END **************" << endl; 
    Serial << endl;

    // Write Encrypted Message to SD Card
    // ----------------------------------    
    File encMessageFile;

    // TEST MODE - If there is existing encMessage.txt remove it
    if(TEST_MODE){
      if(SD.exists("encMessage.txt")){
        Serial << "TEST MODE - removing exisitng encMessage.txt" << endl;
        SD.remove("encMessage.txt");   
        Serial << endl; 
      }
    }

    // Confirm if the encMessage.txt file exists
    if(SD.exists("encMessage.txt")){
      Serial << "encMessage.txt already exists program on hold..." << endl;
      while(1){
        // Error
        LEDErrFlash(ENCRYPT_LED);
        delay(1); // To keep watchdog happy
      }     
    } else {
        // If there is no encMessage.txt file make a file and write data to it
        encMessageFile = SD.open("encMessage.txt", FILE_WRITE);
        
        for(int i=0; i < EncMessage.length(); i++){
          encMessageFile.print(EncMessage.charAt(i));
        }

        encMessageFile.close();

        // Confirm the file has actually been written to SD card
        if(SD.exists("encMessage.txt")){
          Serial << "Encrypted file encMessage.txt written successfully..." << endl;
        } else {
            // Error
            Serial << "File write error. Program on hold." << endl;
            while(1){
              LEDErrFlash(ENCRYPT_LED);
              delay(1); // To keep watchdog happy
            }
        }
    }

    // TEST MODE - Write encryption key to decKey.txt to speed up testing
    // -------------------------------------------------------------------
    if(TEST_MODE){
      Serial << endl;
      Serial << "TEST MODE - Overwritting existing decKey.txt..." << endl;

      // If there is already a decKey.txt file overwrite it
      if(SD.exists("decKey.txt")){        
        SD.remove("decKey.txt");     
        Serial << "TEST MODE - Deleted existing decKey.txt..." << endl;   
      }

      // Write encryption key to file
      File decKeyFile = SD.open("decKey.txt", FILE_WRITE);

      String encKeyStr = String(encKey);
      
      for(int i=0; i < encKeyStr.length(); i++){
          decKeyFile.print(encKeyStr.charAt(i));
      }

      decKeyFile.close();

      // Confirm the file was actually written to SD card
      if(SD.exists("decKey.txt")){
        Serial << "TEST MODE - decKey.txt created sucessfully..." << endl;
        Serial << endl;
      } else {
        // Error
        Serial << "TEST MODE - Failed to create decKey.txt. Program on hold..." << endl;        
        while(1){
          LEDErrFlash(ENCRYPT_LED);
          delay(1); // To keep watchdog happy
        }
      }      
    }

    // Wait until user is done then clear screen
    // -----------------------------------------
    Serial << "Encryption DONE. Please take note of encryption key from small screen" << endl;
    Serial << "Press 'Y' when done." << endl;
    Serial << "NOTE: KEY IS LOST ONCE 'Y' IS PRESSED. SO BE CAREFUL!" << endl;
    bool encFinished = false;
    while(!encFinished){
      if(Serial.available() > 0){
        char inChar = Serial.read();

        if(inChar == 'Y' || inChar == 'y'){
          
          // Clear all encryption key data
          encFinished = true;
          EncMessage = "";
          encKey = 0;
          Serial << "encKey and EncMessage DELETED!" << endl;
          Serial << "Restarting Program..." << endl;
          Serial << endl;

          // Clear OLED display
          display.clearDisplay();
          display.display();
        }         
      }
    }
    // End Of Encrypt Mode Section
    // ===========================   
  } else { 
    // Decrypt Mode Section
    // ====================

    // Print Mode to LCD & Serial     
    // --------------------------
    LCDDecryptMode();
    Serial << "DECRYPT MODE SELECTED" << endl;

    // Prompt User to Confirm
    // ----------------------
    Serial << "Would you like to decrypt encMessage.txt? Press 'Y' to decrypt:" << endl;
    bool decCont = false;
    String inEncMessage = "";
    String decMessage = "";

    while(!decCont){
      if(Serial.available() > 0){
        char inChar = Serial.read();

        if(inChar == 'Y' || inChar == 'y'){
          decCont = true;
        }         
      }
    }

    // Read in encrypted message from SD
    // ---------------------------------
    // Check if encMessage.txt exists
    if(SD.exists("encMessage.txt")){
      // Read in encMessage.txt data
      File encInFile = SD.open("encMessage.txt");

      while(encInFile.available()){
        inEncMessage += encInFile.readStringUntil('\n');
        inEncMessage += '\n';
      }      
    } else {
      // Error
      Serial << "encMessage.txt not found. Program on hold.." << endl;
      while(1){
        LEDErrFlash(ENCRYPT_LED);
        delay(1); // To keep watchdog happy
      }
    }

    // Print encMessage Summary to Serial
    // ----------------------------------
    Serial << endl;
    Serial << "************ IN ENCRYPTED MESSAGE START ************" << endl; 
    Serial << inEncMessage << endl;
    Serial << "************ IN ENCRYPTED MESSAGE END **************" << endl; 
    Serial << endl;
    
    // Read in Decrypt Key From SD Card
    // --------------------------------
    String keyReadIn = "";
    int decKey = 0;

    // Check if decKey.txt exists
    if(SD.exists("decKey.txt")){
      // Read in decKey.txt data
      File encKeyFile = SD.open("decKey.txt");

      while(encKeyFile.available()){
        keyReadIn += encKeyFile.readStringUntil('\n');
      }   

      decKey = keyReadIn.toInt();
    } else {
      // Error
      Serial << "decKey.txt not found. Program on hold.." << endl;
      while(1){
        LEDErrFlash(ENCRYPT_LED);
        delay(1); // To keep watchdog happy
      }
    }

    // TEST MODE - Print decrypt key used to serial to help debugging
    if(TEST_MODE){
      Serial << endl;
      Serial << "TEST MODE - Decrypt key used: " << decKey << endl;
      Serial << endl;
    }
    
    // Display Decrypt Key Used On OLED
    // --------------------------------
    display.clearDisplay();
    display.setCursor(0,0);
    display << "  Decryption Key" << endl;
    display << "  Used" << endl;    
    display.setTextSize(2);
    display << endl;
    display << " " << decKey << endl;
    display.setTextSize(1);
    display.display();

    // Decrypt Message
    // ----------------
    decMessage = encryptXOR(inEncMessage, decKey);

    // Print Decrypted Message Summary to Serial
    // -----------------------------------------
    Serial << endl;
    Serial << "************ DECRYPTED MESSAGE START ************" << endl; 
    Serial << decMessage << endl;
    Serial << "************ DECRYPTED MESSAGE END **************" << endl;
    Serial << endl;

    // TEST MODE - If there is an existing decMessage delete it
    if(TEST_MODE){
      Serial << endl;
      if(SD.exists("decMessage.txt")){        
        SD.remove("decMessage.txt");     
        Serial << "TEST MODE - Deleted existing decMessage.txt..." << endl;   
      }
      Serial << endl;
    }
    
    // Write Decrypted Message to SD Card
    // ----------------------------------
    File decMessageFile;

    // Check if decMessage already exists
    if(SD.exists("decMessage.txt")){
      // Error
      Serial << "decMessage.txt already exists program on hold..." << endl;
      while(1){
        LEDErrFlash(ENCRYPT_LED);
        delay(1); // To keep watchdog happy
      }     
    } else {
        // Write Decrypted Message to SD Card
        decMessageFile = SD.open("decMessage.txt", FILE_WRITE);

        // Add File Header
        decMessageFile.print(fileHeader(ENCRYPT_MODE));
        
        for(int i=0; i < decMessage.length(); i++){
          decMessageFile.print(decMessage.charAt(i));
        }

        decMessageFile.close();

        // Confirm that file has actually been written to SD card
        if(SD.exists("decMessage.txt")){
          Serial << "Decrypted file decMessage.txt written successfully..." << endl;
        } else {
          // Error
            Serial << "File write error. Program on hold." << endl;
            while(1){
              LEDErrFlash(ENCRYPT_LED);
              delay(1); // To keep watchdog happy
            }
          }    
        }

    // Wait for confirmation that user is done
    // ---------------------------------------
    Serial << "Decryption DONE. Press 'Y' when done" << endl;
    bool decFinished = false;
    while(!decFinished){
      if(Serial.available() > 0){
        char inChar = Serial.read();

        if(inChar == 'Y' || inChar == 'y'){
          // Clear all decrypt data
          decFinished = true;
          decMessage = "";
          decKey = 0;
          Serial << "decKey and decMessage DELETED!" << endl;
          Serial << "Restarting Program..." << endl;

          // Clear OLED
          display.clearDisplay();
          display.display();
        }         
      }
    } 
    
    // End Of Decrypt Mode Section
    // ===========================      
  }

   
  delay(5000); // Restart Delay  
}

// ========== Functions ==========================
// SD Card Functions
// -----------------
// Conducts a simple test of SD card by writting a 
// test.txt to the card and removing. Function returns
// test pass bool.
bool SDcardTest(){
  bool testResult = false;
   SD.begin(0);
  // Check for test.txt file
  if(SD.exists("test.txt")){
    SD.remove("test.txt");
    testResult = true;
    // If it can be read, remove it. TEST PASSED
  } else {
  File testFile = SD.open("test.txt", FILE_WRITE);
  testFile.close();
  if(SD.exists("test.txt")){
    SD.remove("test.txt");
    testResult = true;
    // If not make a new test.txt and remove. TEST PASSED
  } else {
    // TEST FAILED
    testResult = false;
  }    
  }  
}

// LED
// ---
// Slow Flash of LED to confirm it is wired correctly
void LEDflashTest(int LEDpin){
  int flashDelay = 900;
  
  digitalWrite(ENCRYPT_LED, 0);
  delay(flashDelay);
  digitalWrite(ENCRYPT_LED, 1);
  delay(flashDelay);
  digitalWrite(ENCRYPT_LED, 0);
  delay(flashDelay);
  digitalWrite(ENCRYPT_LED, 1); 
  delay(flashDelay); 
  digitalWrite(ENCRYPT_LED, 0);
  delay(flashDelay);
  digitalWrite(ENCRYPT_LED, 1);  
}

// Fast LED flash to indicate a fault in the system
void LEDErrFlash(int LEDpin){
  int flashDelay = 100;
  
  digitalWrite(ENCRYPT_LED, 0);
  delay(flashDelay);
  digitalWrite(ENCRYPT_LED, 1);
  delay(flashDelay);
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

// Prints User Prompt on LCD
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

// Print Connection Confirmed on LCD
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

// Print Select Mode on LCD
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

// Print Set Pot on LCD
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

// Print Encrypt Mode on LCD
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

// Print Decrypt Mode on LCD
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
// Print Welcome Message to Serial
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
    if(TEST_MODE){
      Serial << "TEST MODE - Clear serial RX buffer char" << endl;
    }    
  }
}  

// File Header
// -----------
// Returns File Header message String to be added to encrypted
// and decrypted messages. It takes in ENCRYPT_MODE to ensure 
// message matches operation.
String fileHeader(bool isEncryptMode){
  // Get the date and time from RTC
  bool Flag24Hr = true;
  bool FlagAmPm;
  bool Century;
  int timeHour = rtc.getHour(Flag24Hr, FlagAmPm);
  int timeMin = rtc.getMinute();
  int timeSec = rtc.getSecond();
  int dateDay = rtc.getDate();
  int dateMonth = rtc.getMonth(Century);
  int dateYear = rtc.getYear();

  String TestUser = "Joseph Lee"; // Hard coded user for each device
  String userString = "";

  // Set depending on ENCRYPT_MODE
  if(isEncryptMode){
    userString = "            Encrypted by " + TestUser;
  } else {
    userString = "            Decrypted by " + TestUser;
  }

  // Concatenate DateString
  String DateString = "            At " + String(timeHour) + ":" + String(timeMin) + ":" +
    String(timeSec) + " on " + String(dateDay) + "/" + String(dateMonth) + "/" + String(dateYear);
  String starLine = "****************************************************\n";
  String headMessage = "";

  // Set depending on ENCRYPT_MODE
  if(isEncryptMode){
    headMessage = "This File Was Encrypted Using The Encrypty Thing\n";
  } else {
    headMessage = "This File Was Decrypted Using The Encrypty Thing\n";
  }

  // Concatenate Full Header String
  String fullHeader = starLine + headMessage + userString + "\n" + DateString + "\n" + starLine +"\n\n";
    
  return fullHeader;         
}

// Encryption
// ----------
// Generate an encryption key, taking in a value. In this device this will be the potentiometer
int generateEncKey(int value){

  // Generate a random number 0 - 9999
  int key = random(0, 10000);

  // Multiply by the value, in this device potentiometer value
  key *= value;
  
  return key;
}

// XOR encrypt message using the key, if the message is longer than the key
// loop the key until entire message is encrypted
String encryptXOR(String message, int key){

  // Get key length
  int keyLength = String(key).length();

  // Make Char array for key digits
  char keySplit[keyLength];

  // Split key into digits and store in keySplit
  sprintf(keySplit, "%i", key); 

  // Blank encMessage variable to store message
  String encMessage = "";

  // XOR each char in string with a digit in the key
  // Key split index will loop through the key index 0 - keyLength
  for(int i=0; i < message.length(); i++){
    char encChar = message.charAt(i) ^ keySplit[i % keyLength];
    
    // Append encrypted char to encMessage
    encMessage += encChar;
  }

  return encMessage;
}

// RTC
// ---
// Test RTC is reading valid data. Read the time minute value and confirm it is 
// a realistic value. It should only be between 0 and 60. If the RTC is not 
// powered or connected this value will be 165. This fuction returns test 
// passed bool
bool RTCTest(){
  bool testPassed = false; // Default is TEST FAILURE
  int rtcData = rtc.getMinute();

  // TEST MODE - Print RTC minute value to serial to help with debugging
  if(TEST_MODE){
    Serial << "TEST MODE - Test RTC Data: " << rtcData << endl;
  }
  
  if(rtcData >= 0 && rtcData <=60){
    testPassed = true;
    // Is a realistic value. TEST PASSED
  }  
  return testPassed;
}
