#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <MD_YX5300.h>
#include <Bounce2.h>
#include "UID.h"
#include "lvl.h"

bool DEBUG = true;
bool ADMIN = true;
bool OVERRIDE = false;

#define RST_PIN 21
#define CS_PIN_2 2

#define BUTTON_PIN 10
Bounce buttonDebouncer = Bounce(); // Create a Bounce object for the button

const int gatePins[] = {22, 20, 17, 27, 28, 26};
const int numGatePins = 6;

const int ledPins[] = {11, 12, 13, 14, 15}; // Array for LED pins
const int numLeds = 5;

int currentLevel = 0; // Current level of the system

MFRC522 rfid(CS_PIN_2, RST_PIN);
SPISettings mfrc522SPISettings(50000, MSBFIRST, SPI_MODE0);

bool isReaderInitialized = false;

byte presentCards[numGatePins]; // Array to store the present card for each gate
byte cardsCount[13];            // Array to store the count of cards for each category

unsigned long lastScanTime = 0;           // Variable to track the last scan time
const unsigned long scanInterval = 10000; // 5 seconds interval

#define MP3Stream Serial2
MD_YX5300 mp3(MP3Stream); // Create instance of MD_YX5300 class for MP3 player using MP3Stream

bool introduction01 = true;
bool introduction02 = false;

void setup()
{
  Serial.begin(9600); // Initialize Serial Monitor
  SPI.begin();

  // Initialize MP3Stream
  MP3Stream.setRX(9); // Ensure these match your hardware setup
  MP3Stream.setTX(8);
  MP3Stream.begin(MD_YX5300::SERIAL_BPS);

  // Initialize MP3 player
  mp3.begin();
  delay(1000);      // Allow time for initialization
  mp3.device(0x02); // Select SD card as storage device
  delay(500);

  // Initialize all gate pins as outputs and set them LOW (closed gate)
  for (int i = 0; i < numGatePins; i++)
  {
    pinMode(gatePins[i], OUTPUT);
    digitalWrite(gatePins[i], LOW);
  }

  // Initialize all LED pins as outputs and set them LOW (off)
  for (int i = 0; i < numLeds; i++)
  {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  pinMode(CS_PIN_2, OUTPUT);
  pinMode(RST_PIN, OUTPUT);

  // Play the first file (001 in the main folder)
  Serial.println("Playing file 001 in the main folder...");
  mp3.playSpecific(1, 1); // File index 001 corresponds to 1
  introduction01 = true;

  // Initialize the button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  buttonDebouncer.attach(BUTTON_PIN);
  buttonDebouncer.interval(25); // Debounce interval in milliseconds
}

void initializeReader()
{
  digitalWrite(CS_PIN_2, LOW);
  delay(20);
  digitalWrite(RST_PIN, HIGH);
  delay(20); // Wait for the reader to reset
  digitalWrite(RST_PIN, LOW);
  delay(20); // Wait for the reader to stabilize
  SPI.beginTransaction(mfrc522SPISettings);
  rfid.PCD_Init();
  delay(20);
  SPI.endTransaction();
  delay(20);
  digitalWrite(CS_PIN_2, HIGH);

  if (DEBUG)
  {
    Serial.print("Firmware version: ");
    rfid.PCD_DumpVersionToSerial();
  }
  isReaderInitialized = true;
}

void flickerLED(int ledIndex, int times = 3, int duration = 500, bool leaveOn = true)
{
  for (int i = 0; i < times; i++)
  {
    digitalWrite(ledPins[ledIndex], HIGH);
    delay(duration);
    digitalWrite(ledPins[ledIndex], LOW);
    delay(duration);
  }

  if (leaveOn)
  {
    digitalWrite(ledPins[ledIndex], HIGH);
  }
}

void checkReader(int gateIndex)
{
  byte scannedUID[7] = {0}; // Array to store the scanned UID
  bool uidFound = false;    // Flag to indicate if a UID was found
  const int maxAttempts = 3;

  initializeReader(); // Ensure the reader is properly reset
  delay(50);          // Give time to stabilize

  for (int attempt = 0; attempt < maxAttempts; attempt++)
  {
    digitalWrite(CS_PIN_2, LOW);
    SPI.beginTransaction(mfrc522SPISettings);

    byte bufferATQA[2];
    byte bufferSize = sizeof(bufferATQA);

    if (rfid.PICC_WakeupA(bufferATQA, &bufferSize) == MFRC522::STATUS_OK)
    {
      if (rfid.PICC_Select(&rfid.uid) == MFRC522::STATUS_OK)
      {
        if (DEBUG)
        {
          Serial.print("Card UID at Gate ");
          Serial.print(gateIndex + 1);
          Serial.print(": ");
        }

        // Copy the UID into the scannedUID array
        for (byte i = 0; i < rfid.uid.size; i++)
        {
          scannedUID[i] = rfid.uid.uidByte[i];
        }

        // Print the UID to Serial
        if (DEBUG)
        {
          Serial.print("UID: ");
          for (byte i = 0; i < rfid.uid.size; i++)
          {
            Serial.print(scannedUID[i], HEX);
            if (i < rfid.uid.size - 1)
            {
              Serial.print(":");
            }
          }
          Serial.println();
        }

        uidFound = true;
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
        break; // UID found, break out of retry loop
      }
    }

    SPI.endTransaction();
    digitalWrite(CS_PIN_2, HIGH);
    delay(20); // Wait a bit before retrying
  }

  if (uidFound)
  {
    // Compare the scanned UID with the registered list
    for (int i = 0; i < registeredCount; i++)
    {
      bool match = true;
      for (int j = 0; j < 7; j++)
      {
        if (scannedUID[j] != registered[i].uid[j])
        {
          match = false;
          break;
        }
      }

      if (match)
      {
        presentCards[gateIndex] = registered[i].category; // Update the array with the category
        if (DEBUG)
        {
          Serial.print("Gate ");
          Serial.print(gateIndex + 1);
          Serial.print(": Category ");
          Serial.println(registered[i].category);
        }
        return; // Exit the function once a match is found
      }
    }

    // If no match is found
    presentCards[gateIndex] = 0; // Set to 0 to indicate no match
    if (DEBUG)
    {
      Serial.print("Gate ");
      Serial.print(gateIndex + 1);
      Serial.println(": No matching category found.");
    }
  }
  else
  {
    presentCards[gateIndex] = 0; // Set to 0 if no UID was found
    if (DEBUG)
    {
      Serial.print("Gate ");
      Serial.print(gateIndex + 1);
      Serial.println(": No card detected.");
    }
  }
}

void scanCards()
{
  // Close all gates
  for (int i = 0; i < numGatePins; i++)
  {
    digitalWrite(gatePins[i], LOW);
  }
  delay(20);

  if (DEBUG)
  {
    Serial.println("All gates are closed.");
  }

  // Iterate through each gate and check for cards
  for (int i = 0; i < numGatePins; i++)
  {
    digitalWrite(gatePins[i], HIGH); // Open the current gate
    delay(20);

    if (DEBUG)
    {
      Serial.print("Activating gate ");
      Serial.print(gatePins[i]);
      Serial.println(".");
    }

    checkReader(i);
    delay(20);                      // Check the reader for the current gate
    digitalWrite(gatePins[i], LOW); // Close the current gate
  }

  if (DEBUG)
  {
    Serial.println("Present cards (categories):");
    for (int i = 0; i < numGatePins; i++)
    {
      Serial.print("Gate ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(presentCards[i]);
    }
  }

  for (int i = 0; i < 13; i++)
  {
    cardsCount[i] = 0;
  }

  // Count the number of cards in each category
  for (int i = 0; i < numGatePins; i++)
  {
    int val = presentCards[i];
    if (val >= 1 && val <= 13)
    {
      cardsCount[val - 1]++;
    }
  }
}

bool hasIllegalComponents(int level)
{
  const int *allowed = allowedComponents[level];
  int count = allowedComponentsCount[level];

  for (int i = 0; i < numGatePins; i++)
  {
    int val = presentCards[i];
    if (val == 0)
      continue;

    bool isAllowed = false;
    for (int j = 0; j < count; j++)
    {
      if (val == allowed[j])
      {
        isAllowed = true;
        break;
      }
    }

    if (!isAllowed)
      return true;
  }

  return false;
}

bool matchConnectionMasks()
{
  for (int i = 0; i < connectionMasksCount; i++)
  {
    bool match = true;
    for (int j = 0; j < numGatePins; j++)
    {
      if (connectionMasks[i][j] == 0 && presentCards[j] != 0)
      {
        match = false;
        break;
      }
      else if (connectionMasks[i][j] == 1 && presentCards[j] == 0)
      {
        match = false;
        break;
      }
    }
    if (match)
    {
      return true;
    }
  }
  return false;
}

// LEVEL 1
bool lvl1_0()
{
  // 1 weerstand en 1 led
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalLEDs == 1 && totalResistors == 1);
}

bool lvl1_1()
{
  // geen led aanwezig
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];

  return (totalLEDs == 0);
}

bool lvl1_2()
{
  // geen weerstand aanwezig
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalResistors == 0);
}

// LEVEL 2
bool lvl2_0()
{
  // één SW, één led, en één weerstand
  int totalSW = cardsCount[6] + cardsCount[7]; // Normale switches
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalSW == 1 && totalLEDs == 1 && totalResistors == 1);
}

bool lvl2_1()
{
  // Nieuwe conditie: geen weerstand
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalResistors == 0);
}

bool lvl2_2()
{
  // (was lvl2_1) één LED, één weerstand, en geen SW
  int totalSW = cardsCount[6] + cardsCount[7]; // Normale switches
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalSW == 0 && totalLEDs == 1 && totalResistors == 1);
}

bool lvl2_3()
{
  // (was lvl2_2) één SW, en geen LED
  int totalSW = cardsCount[6] + cardsCount[7]; // Normale switches
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];

  return (totalSW == 1 && totalLEDs == 0);
}

// LEVEL 3
bool lvl3_0()
{
  // één PushSW, één led, en één weerstand
  int totalPushSW = cardsCount[8] + cardsCount[9]; // Push switches
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalPushSW == 1 && totalLEDs == 1 && totalResistors == 1);
}

bool lvl3_1()
{
  // Nieuwe conditie: geen weerstand
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalResistors == 0);
}

bool lvl3_2()
{
  // (was lvl3_1) één SW, één led, en één weerstand
  int totalSW = cardsCount[6] + cardsCount[7]; // Normale switches
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalSW == 1 && totalLEDs == 1 && totalResistors == 1);
}

bool lvl3_3()
{
  // (was lvl3_2) geen PushSW
  int totalPushSW = cardsCount[8] + cardsCount[9]; // Push switches

  return (totalPushSW == 0);
}

// LEVEL 4
bool lvl4_0()
{
  // 2 weerstanden en 2 LED
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalLEDs == 2 && totalResistors == 2);
}

bool lvl4_1()
{
  // Nieuwe conditie: geen weerstand
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalResistors == 0);
}

bool lvl4_2()
{
  // (was lvl4_1) 1 weerstand en 2 LED
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalLEDs == 2 && totalResistors == 1);
}

bool lvl4_3()
{
  // (was lvl4_2) 1 LED
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];

  return (totalLEDs == 1);
}

// LEVEL 5
bool lvl5_0()
{
  // 1 fotodiode, 1 weerstand, een PushSW, een LED en een T-junction
  int totalPhotodiodes = cardsCount[12]; // Fotodiode is op index 12
  int totalResistors = cardsCount[10] + cardsCount[11];
  int totalPushSW = cardsCount[8] + cardsCount[9]; // Push switches
  int totalLEDs = cardsCount[3] + cardsCount[4] + cardsCount[5];
  int totalTJunctions = cardsCount[2]; // T-junctions op index 2

  return (totalPhotodiodes == 1 && totalResistors == 1 && totalPushSW == 1 && totalLEDs == 1 && totalTJunctions == 1);
}

bool lvl5_1()
{
  // Nieuwe conditie: geen weerstand
  int totalResistors = cardsCount[10] + cardsCount[11];

  return (totalResistors == 0);
}

bool lvl5_2()
{
  // (was lvl5_1) geen fotodiode en/of geen PushSW
  int totalPhotodiodes = cardsCount[12];           // Fotodiode is op index 12
  int totalPushSW = cardsCount[8] + cardsCount[9]; // Push switches

  return (totalPhotodiodes == 0 || totalPushSW == 0);
}

bool lvl5_3()
{
  // (was lvl5_2) geen weerstand en/of geen t-junction
  int totalResistors = cardsCount[10] + cardsCount[11];
  int totalTJunctions = cardsCount[2]; // T-junctions op index 2

  return (totalResistors == 0 || totalTJunctions == 0);
}

void handleAdminCommands()
{
  if (!ADMIN)
    return; // Only execute if ADMIN is active

  OVERRIDE = false; // Reset OVERRIDE to false
  for (int i = 0; i < numGatePins; i++)
  {
    switch (presentCards[i])
    {
    case ADMIN_KEY_A:
      // Full reset
      currentLevel = 0;
      for (int i = 0; i < numLeds; i++)
      {
        digitalWrite(ledPins[i], LOW);
      }
      mp3.playSpecific(1, 1); // File index 001 corresponds to 1
      introduction01 = true;
      if (DEBUG)
        Serial.println("Admin: Full reset performed.");
      OVERRIDE = true;
      break;

    case ADMIN_KEY_B:
      // Go to the previous level
      if (currentLevel > 0)
        currentLevel--;
      if (DEBUG)
        Serial.println("Admin: Moved to previous level.");
      OVERRIDE = true;
      break;

    case ADMIN_KEY_C:
      // Level approved
      if (DEBUG)
        Serial.println("Admin: Level approved.");
      if (currentLevel == 0)
      {
        currentLevel = 1;
        mp3.playSpecific(1, 4);
        delay(5000);
        for (int i = 0; i < 5; i++)
        {
          digitalWrite(ledPins[i], HIGH);
          delay(500);
        }
        for (int i = 0; i < 3; i++)
        {
          for (int j = 0; j < numLeds; j++)
          {
            digitalWrite(ledPins[j], HIGH);
          }
          delay(500);
          for (int j = 0; j < numLeds; j++)
          {
            digitalWrite(ledPins[j], LOW);
          }
          delay(500);
        }
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
        delay(1000); // Wait for 1 second before proceeding
        mp3.playSpecific(1, 6);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
      }
      else if (currentLevel == 1)
      {
        currentLevel = 2;
        mp3.playSpecific(1, 7);
        flickerLED(0, 3, 500, true);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
        delay(1000); // Wait for 1 second before proceeding
        mp3.playSpecific(1, 10);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
      }
      else if (currentLevel == 2)
      {
        currentLevel = 3;
        mp3.playSpecific(1, 11);
        flickerLED(1, 3, 500, true);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
        delay(1000); // Wait for 1 second before proceeding
        mp3.playSpecific(1, 14);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
      }
      else if (currentLevel == 3)
      {
        currentLevel = 4;
        mp3.playSpecific(1, 15);
        flickerLED(2, 3, 500, true);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
        delay(1000); // Wait for 1 second before proceeding
        mp3.playSpecific(1, 18);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
      }
      else if (currentLevel == 4)
      {
        currentLevel = 5;
        mp3.playSpecific(1, 19);
        flickerLED(3, 3, 500, true);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
        delay(1000); // Wait for 1 second before proceeding
        mp3.playSpecific(1, 22);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
      }
      else if (currentLevel == 5)
      {
        currentLevel = 10;
        mp3.playSpecific(1, 23);
        flickerLED(4, 3, 500, true);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
        mp3.playSpecific(1, 26);
        while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
        {
        }
      }
      OVERRIDE = true;
      break;

    case ADMIN_KEY_D:
      // Error 1
      if (currentLevel == 0)
        mp3.playSpecific(1, 5);
      if (currentLevel == 1)
        mp3.playSpecific(1, 8);
      if (currentLevel == 2)
        mp3.playSpecific(1, 9);
      if (currentLevel == 3)
        mp3.playSpecific(1, 9);
      if (currentLevel == 4)
        mp3.playSpecific(1, 9);
      if (currentLevel == 5)
        mp3.playSpecific(1, 9);

      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      if (DEBUG)
        Serial.println("Admin: Error 1 executed.");
      OVERRIDE = true;
      break;

    case ADMIN_KEY_E:
      // Error 2
      if (currentLevel == 1)
        mp3.playSpecific(1, 9);
      if (currentLevel == 2)
        mp3.playSpecific(1, 12);
      if (currentLevel == 3)
        mp3.playSpecific(1, 16);
      if (currentLevel == 4)
        mp3.playSpecific(1, 20);
      if (currentLevel == 5)
        mp3.playSpecific(1, 24);

      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      if (DEBUG)
        Serial.println("Admin: Error 2 executed.");
      OVERRIDE = true;
      break;

    case ADMIN_KEY_F:
      // Error 3
      if (currentLevel == 2)
        mp3.playSpecific(1, 13);
      if (currentLevel == 3)
        mp3.playSpecific(1, 17);
      if (currentLevel == 4)
        mp3.playSpecific(1, 21);
      if (currentLevel == 5)
        mp3.playSpecific(1, 25);

      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      if (DEBUG)
        Serial.println("Admin: Error 3 executed.");
      OVERRIDE = true;
      break;

    case ADMIN_KEY_G:
      // Fallback
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      if (DEBUG)
        Serial.println("Admin: Fallback executed.");
      OVERRIDE = true;
      break;

    case ADMIN_KEY_H:
      // Restart current level
      if (currentLevel == 0)
        mp3.playSpecific(1, 3);
      if (currentLevel == 1)
        mp3.playSpecific(1, 6);
      if (currentLevel == 2)
        mp3.playSpecific(1, 10);
      if (currentLevel == 3)
        mp3.playSpecific(1, 14);
      if (currentLevel == 4)
        mp3.playSpecific(1, 18);
      if (currentLevel == 5)
        mp3.playSpecific(1, 22);

      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      if (DEBUG)
        Serial.println("Admin: Restarted current level.");
      OVERRIDE = true;
      break;

    case ADMIN_KEY_J:
      // Toggle debug
      DEBUG = !DEBUG;
      Serial.print("Admin: Debug mode is now ");
      Serial.println(DEBUG ? "on" : "off");
      OVERRIDE = true;
      break;

    default:
      // No admin key found
      break;
    }
  }
}

void buttonPressed()
{

  if (DEBUG)
  {
    Serial.println("Button pressed.");
    Serial.print("Current level: ");
    Serial.println(currentLevel);
  }
  // Calculate the timestamp in hh:mm:ss format
  unsigned long currentMillis = millis();
  unsigned long seconds = currentMillis / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;

  seconds = seconds % 60; // Remaining seconds
  minutes = minutes % 60; // Remaining minutes

  // Print the timestamp to Serial
  Serial.print("Button pressed at: ");
  if (hours < 10)
    Serial.print("0");
  Serial.print(hours);
  Serial.print(":");
  if (minutes < 10)
    Serial.print("0");
  Serial.print(minutes);
  Serial.print(":");
  if (seconds < 10)
    Serial.print("0");
  Serial.println(seconds);

  scanCards(); // Scan cards when the button is pressed
  if (DEBUG)
  {
    Serial.println("Button pressed, scanning cards.");
  }

  // print the scanned cards to Serial
  Serial.print("Scanned cards: {");
  for (int i = 0; i < numGatePins; i++)
  {
    Serial.print(presentCards[i]);
    if (i < numGatePins - 1)
    {
      Serial.print(", ");
    }
  }
  Serial.println("}");

  handleAdminCommands(); // Handle admin commands

  if (OVERRIDE)
    return; // Skip if OVERRIDE is active

  if (!matchConnectionMasks())
  {
    mp3.playSpecific(1, 5);
    while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
    {
    }
    return;
  }

  if (currentLevel == 0)
  {
    if (hasIllegalComponents(0))
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else
    {
      currentLevel = 1;
      mp3.playSpecific(1, 4);
      delay(5000);
      for (int i = 0; i < 5; i++)
      {
        digitalWrite(ledPins[i], HIGH);
        delay(500);
      }
      for (int i = 0; i < 3; i++)
      {
        for (int j = 0; j < numLeds; j++)
        {
          digitalWrite(ledPins[j], HIGH);
        }
        delay(500);
        for (int j = 0; j < numLeds; j++)
        {
          digitalWrite(ledPins[j], LOW);
        }
        delay(500);
      }
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      delay(1000); // Wait for 1 second before proceeding
      mp3.playSpecific(1, 6);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
  }
  else if (currentLevel == 1)
  {
    if (hasIllegalComponents(1))
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl1_0())
    {
      currentLevel = 2;
      mp3.playSpecific(1, 7);
      flickerLED(0, 3, 500, true);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      delay(1000); // Wait for 1 second before proceeding
      mp3.playSpecific(1, 10);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl1_1())
    {
      mp3.playSpecific(1, 8);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl1_2())
    {
      mp3.playSpecific(1, 9);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
  }
  else if (currentLevel == 2)
  {
    if (hasIllegalComponents(2))
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl2_0())
    {
      currentLevel = 3;
      mp3.playSpecific(1, 11);
      flickerLED(1, 3, 500, true);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      delay(1000); // Wait for 1 second before proceeding
      mp3.playSpecific(1, 14);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl2_1())
    {
      mp3.playSpecific(1, 9);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl2_2())
    {
      mp3.playSpecific(1, 12);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl2_3())
    {
      mp3.playSpecific(1, 13);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
  }
  else if (currentLevel == 3)
  {
    if (hasIllegalComponents(3))
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl3_0())
    {
      currentLevel = 4;
      mp3.playSpecific(1, 15);
      flickerLED(2, 3, 500, true);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      delay(1000); // Wait for 1 second before proceeding
      mp3.playSpecific(1, 18);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl3_1())
    {
      mp3.playSpecific(1, 9);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl3_2())
    {
      mp3.playSpecific(1, 16);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl3_3())
    {
      mp3.playSpecific(1, 17);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
  }
  else if (currentLevel == 4)
  {
    if (hasIllegalComponents(4))
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl4_0())
    {
      currentLevel = 5;
      mp3.playSpecific(1, 19);
      flickerLED(3, 3, 500, true);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      delay(1000); // Wait for 1 second before proceeding
      mp3.playSpecific(1, 22);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl4_1())
    {
      mp3.playSpecific(1, 9);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl4_2())
    {
      mp3.playSpecific(1, 20);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl4_3())
    {
      mp3.playSpecific(1, 21);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
  }
  else if (currentLevel == 5)
  {
    if (hasIllegalComponents(5))
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl5_0())
    {
      currentLevel = 10;
      mp3.playSpecific(1, 23);
      flickerLED(4, 3, 500, true);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      mp3.playSpecific(1, 26);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl5_1())
    {
      mp3.playSpecific(1, 9);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl5_2())
    {
      mp3.playSpecific(1, 24);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else if (lvl5_3())
    {
      mp3.playSpecific(1, 25);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
    else
    {
      mp3.playSpecific(1, 2);
      while (!mp3.check() || mp3.getStatus()->code != MD_YX5300::STS_FILE_END)
      {
      }
      return;
    }
  }
  else
  {
    currentLevel = 10;
    if (DEBUG)
    {
      Serial.println("Invalid level, moving to level 10.");
    }
  }
}

void loop()
{
  // Check if the MP3 player has finished playing the current track
  if (mp3.check())
  {
    const MD_YX5300::cbData *status = mp3.getStatus();

    if (status->code == MD_YX5300::STS_FILE_END)
    {
      if (introduction01)
      {
        if (DEBUG)
        {
          Serial.println("Finished the introduction, moving towards challenge 0");
        }
        delay(500);
        mp3.playSpecific(1, 3);
        introduction01 = false;
        introduction02 = true;
      }
      else if (introduction02)
      {
        introduction02 = false;
      }
    }
  }

  buttonDebouncer.update(); // Update the button state

  // Check if the button was pressed
  if (buttonDebouncer.fell())
  {
    buttonPressed();
  }
}
