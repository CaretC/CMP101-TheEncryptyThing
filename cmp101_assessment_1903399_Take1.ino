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
bool TEST_MODE = false;

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
#define DECODE_BUTTON D8

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
  int testButtonState = digitalRead(DECODE_BUTTON);
  Serial << "Test mode button: " << testButtonState << endl;
  if(testButtonState == 1){
    Serial << "TEST MODE ENABLED" << endl;
    TEST_MODE = true;
  }

  
  Serial << "GPIO Pin Setup Complete..." << endl;

  // SD Card Setup
  // -------------
  Serial << "Initializing SD Card ..." << endl;
  if(!SD.begin(0)){
    Serial << "SD Initialization Failed" << endl;
    while(1){
      LEDErrFlash(DECODE_LED);
      delay(1); // Hold Here Forever
    }
  } else {
    Serial << "Testing SD Card ..." << endl;
    if(SDcardTest()){
      Serial << "SD Card Test Passed..." << endl;
    } else {
      Serial << "SD Card Test Failed" << endl;
      while(1){
        LEDErrFlash(DECODE_LED);
        delay(1); // Hold Here Forever
      }
    }    
  }

  // RTC
  // ---
  Serial << "Testing RTC..." << endl;
  if(RTCTest()){
    Serial << "RTC Test Passed..." << endl;
  } else {
    Serial << "RTC Test Failed" << endl;
    while(1){
      LEDErrFlash(DECODE_LED);
      delay(1); // To keep watchdog happy
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

     serialRXFlush();
     if(TEST_MODE){
      Serial << endl;
      Serial << "Serial State: " << Serial.available() << endl;
      Serial << endl;
     }
     
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

  if(TEST_MODE){
    Serial << endl;
    Serial << "Serial State: " << Serial.available() << endl;
    Serial << endl;
  }  
  
  // Set LED
  if(ENCRYPT_MODE){
    digitalWrite(DECODE_LED, 0);
  } else {
    digitalWrite(DECODE_LED, 1);
  }
  
  if(ENCRYPT_MODE){
    LCDEncryptMode();
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
        LEDErrFlash(DECODE_LED);
        delay(1);
      }
    } 
    
    // Print Message to Serial
    Serial << "Message is " << inMessage.length() << " chars long." << endl;  
    Serial << endl;
    Serial << "************ IN MESSAGE START ************" << endl;  
    Serial << inMessage << endl;
    Serial << "************ IN MESSAGE END **************" << endl;  
    Serial << endl;
    
    // Read in pot value
    LCDSetPot();
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

    // Generate Enc Key
    int encKey = generateEncKey(potValue);

    if(TEST_MODE){
      // TEST display Pot value set
      Serial << endl;
      Serial << "Potentiometer Value Selected: " << potValue << endl;
      Serial << "Enc Key is: " << encKey << endl;
      Serial << endl;
    }

    // Display EncyKey on Small OLED
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
    String fullInMessage = fileHeader(ENCRYPT_MODE) + inMessage;
    
    // Encrypt
    String EncMessage = encryptXOR(fullInMessage, encKey);

    // serial print EncMessage
    Serial << endl;
    Serial << "************ ENCRYPTED MESSAGE START ************" << endl;  
    Serial << EncMessage << endl;
    Serial << "************ ENCRYPTED MESSAGE END **************" << endl; 
    Serial << endl;

    // Write encMessage to SD
    File encMessageFile;

    // For test remove encMessage if exists
    if(TEST_MODE){
      if(SD.exists("encMessage.txt")){
        Serial << "TEST MODE removing exisitng encMessage.txt" << endl;
        SD.remove("encMessage.txt");    
      }
    }
    
    if(SD.exists("encMessage.txt")){
      Serial << "encMessage.txt already exists program on hold..." << endl;
      while(1){
        LEDErrFlash(DECODE_LED);
        delay(1); // To keep watchdog happy
      }     
    } else {
        encMessageFile = SD.open("encMessage.txt", FILE_WRITE);
        
        for(int i=0; i < EncMessage.length(); i++){
          encMessageFile.print(EncMessage.charAt(i));
        }

        encMessageFile.close();

        if(SD.exists("encMessage.txt")){
          Serial << "Encrypted file encMessage.txt written successfully..." << endl;
        } else {
            Serial << "File write error. Program on hold." << endl;

            while(1){
              LEDErrFlash(DECODE_LED);
              delay(1); // To keep watchdog happy
            }
        }
    }

    // TEST MODE Write decKeyFile
    if(TEST_MODE){
      Serial << endl;
      Serial << "TEST MODE - Overwritting existing decKey.txt..." << endl;
      if(SD.exists("decKey.txt")){        
        SD.remove("decKey.txt");     
        Serial << "TEST MODE - Deleted existing decKey.txt..." << endl;   
      }

      File decKeyFile = SD.open("decKey.txt", FILE_WRITE);

      String encKeyStr = String(encKey);
      
      for(int i=0; i < encKeyStr.length(); i++){
          decKeyFile.print(encKeyStr.charAt(i));
      }

      decKeyFile.close();

      if(SD.exists("decKey.txt")){
        Serial << "TEST MODE decKey.txt CREATED SUCESSFULLY" << endl;
        Serial << endl;
      } else {
        Serial << "TEST MODE FAILED TO CREATE decKey.txt. Program on hold..." << endl;
        
        while(1){
          LEDErrFlash(DECODE_LED);
          delay(1); // To keep watchdog happy
        }
      }
        
      
    }

    // Wait until user is done then clear screen
    Serial << "Encryption DONE. Please take note of encryption key from small screen" << endl;
    Serial << "Press 'Y' when done." << endl;
    Serial << "NOTE: KEY IS LOST ONCE 'Y' IS PRESSED. SO BE CAREFUL!" << endl;
    bool encFinished = false;
    while(!encFinished){
      if(Serial.available() > 0){
        char inChar = Serial.read();

        if(inChar == 'Y' || inChar == 'y'){
          encFinished = true;
          EncMessage = "";
          encKey = 0;
          Serial << "encKey and EncMessage DELETED!" << endl;
          Serial << "Restarting Program..." << endl;
          Serial << endl;

          display.clearDisplay();
          display.display();
        }         
      }
    }
    
    
    
    
  } else {
    LCDDecryptMode();
    Serial << "DECRYPT MOODE SELECTED" << endl;

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
    if(SD.exists("encMessage.txt")){
      File encInFile = SD.open("encMessage.txt");

      while(encInFile.available()){
        inEncMessage += encInFile.readStringUntil('\n');
        inEncMessage += '\n';
      }      
    } else {
      Serial << "encMessage.txt not found. Program on hold.." << endl;
      while(1){
        LEDErrFlash(DECODE_LED);
        delay(1); // To keep watchdog happy
      }
    }

    // TEST
    Serial << endl;
    Serial << "************ IN ENCRYPTED MESSAGE START ************" << endl; 
    Serial << inEncMessage << endl;
    Serial << "************ IN ENCRYPTED MESSAGE END **************" << endl; 
    Serial << endl;
    
    // read in key from SD
    String keyReadIn = "";
    int decKey = 0;

    // Read in encrypted message from SD
    if(SD.exists("decKey.txt")){
      File encKeyFile = SD.open("decKey.txt");

      while(encKeyFile.available()){
        keyReadIn += encKeyFile.readStringUntil('\n');
      }   

      decKey = keyReadIn.toInt();
    } else {
      Serial << "decKey.txt not found. Program on hold.." << endl;
      while(1){
        LEDErrFlash(DECODE_LED);
        delay(1); // To keep watchdog happy
      }
    }

    // TEST
    if(TEST_MODE){
      Serial << "Dec Key is: " << decKey << endl;
    }
    
    // Display Dec Key used
    display.clearDisplay();
    display.setCursor(0,0);
    display << "  Decryption Key" << endl;
    display << "  Used" << endl;    
    display.setTextSize(2);
    display << endl;
    display << " " << decKey << endl;
    display.setTextSize(1);
    display.display();

    // Decrypt
    decMessage = encryptXOR(inEncMessage, decKey);

    // TEST
    Serial << endl;
    Serial << "************ DECRYPTED MESSAGE START ************" << endl; 
    Serial << decMessage << endl;
    Serial << "************ DECRYPTED MESSAGE END **************" << endl;
    Serial << endl;

    // TEST MODE Write decMessage
    if(TEST_MODE){
      Serial << endl;
      Serial << "TEST MODE - Preparing to write decMessage.txt..." << endl;
      if(SD.exists("decMessage.txt")){        
        SD.remove("decMessage.txt");     
        Serial << "TEST MODE - Deleted existing decMessage.txt..." << endl;   
      }
    }
    
    // Write decMessage to SD
    File decMessageFile;
    if(SD.exists("decMessage.txt")){
      Serial << "decMessage.txt already exists program on hold..." << endl;

      while(1){
        LEDErrFlash(DECODE_LED);
        delay(1); // To keep watchdog happy
      }     
    } else {
        decMessageFile = SD.open("decMessage.txt", FILE_WRITE);

        decMessageFile.print(fileHeader(ENCRYPT_MODE));
        
        for(int i=0; i < decMessage.length(); i++){
          decMessageFile.print(decMessage.charAt(i));
        }

        decMessageFile.close();

        if(SD.exists("decMessage.txt")){
          Serial << "Decrypted file decMessage.txt written successfully..." << endl;
        } else {
            Serial << "File write error. Program on hold." << endl;

            while(1){
              LEDErrFlash(DECODE_LED);
              delay(1); // To keep watchdog happy
            }
          }    
        }

    Serial << "Decryption DONE. Press 'Y' when done" << endl;
    bool decFinished = false;
    while(!decFinished){
      if(Serial.available() > 0){
        char inChar = Serial.read();

        if(inChar == 'Y' || inChar == 'y'){
          decFinished = true;
          decMessage = "";
          decKey = 0;
          Serial << "decKey and decMessage DELETED!" << endl;
          Serial << "Restarting Program..." << endl;

          display.clearDisplay();
          display.display();
        }         
      }
    }       
  }

   
  delay(5000); // Restart Delay  
}

// ========== Functions ==========================
// Tests
// -----
// Simple Test to Confirm SD Card is working
bool SDcardTest(){
  bool testResult = false;
   SD.begin(0);
  // Check for test file
  if(SD.exists("test.txt")){
    SD.remove("test.txt");
    testResult = true;
  } else {
  File testFile = SD.open("test.txt", FILE_WRITE);
  testFile.close();
  if(SD.exists("test.txt")){
    SD.remove("test.txt");
    testResult = true;
  } else {
    testResult = false;
  }    
  }  
}

// Simple Test That Flashes LED
void LEDflashTest(int LEDpin){
  int flashDelay = 900;
  
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

void LEDErrFlash(int LEDpin){
  int flashDelay = 100;
  
  digitalWrite(DECODE_LED, 0);
  delay(flashDelay);
  digitalWrite(DECODE_LED, 1);
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
    if(TEST_MODE){
      Serial << "Clear Buffer Char" << endl;
    }    
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

// Print Encyption Header in file
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

  String TestUser = "Joseph Lee";
  String userString = "";
  if(isEncryptMode){
    userString = "            Encrypted by " + TestUser;
  } else {
    userString = "            Decrypted by " + TestUser;
  }
  String DateString = "            At " + String(timeHour) + ":" + String(timeMin) + ":" +
    String(timeSec) + " on " + String(dateDay) + "/" + String(dateMonth) + "/" + String(dateYear);
  String starLine = "****************************************************\n";
  String headMessage = "";

  if(isEncryptMode){
    headMessage = "This File Was Encrypted Using The Encrypty Thing\n";
  } else {
    headMessage = "This File Was Decrypted Using The Encrypty Thing\n";
  }
  

  String fullHeader = starLine + headMessage + userString + "\n" + DateString + "\n" + starLine +"\n\n";
    
  return fullHeader;         
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

// RTC
// ---
bool RTCTest(){
  bool testPassed = false;
  int rtcData = rtc.getMinute();

  if(TEST_MODE){
    Serial << "TEST MODE - Test RTC Data: " << rtcData << endl;
  }
  
  if(rtcData >= 0 && rtcData <=60){
    testPassed = true;
  }  
  return testPassed;
}
