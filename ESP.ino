// Library
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Adafruit_MLX90614.h>
#include <LiquidCrystal_I2C.h>

// RFID Variables
#define resetPin D3
#define ssPin D8
MFRC522 Reader;
String lastCard;

// Others
bool onDelay = false; // to prevent card from being readed while nodemcu doing something else

// RFID functions
//{
String ReadTag()
{
    byte *buffer = Reader.uid.uidByte;
    byte bufferSize = Reader.uid.size;
    String output;
    for (byte i = 0; i < bufferSize; i++)
    {
        output += buffer[i] < 0x10 ? F("0") : F("");
        output += String(buffer[i]);
    }
    return output;
}
// }

// Other
// {
void space(int len = 10)
{
    for (int i = 0; i < len; i++)
        Serial.println();
}
// }

void setup()
{
    Serial.begin(57600);
    space();

    // RFID initialization
    SPI.begin();
    Reader.PCD_Init(ssPin, resetPin);

    // check if the pin connected properly
    int antennaStrength = Reader.PCD_GetAntennaGain();
    if (antennaStrength < 64)
        Serial.println(F("Reader is not conected properly"));

    // Dump some information
    Serial.print(F(" initilised on pin ") +
                 String(ssPin) + F(". Antenna strength: ") +
                 String(Reader.PCD_GetAntennaGain()) +
                 F(". Version ."));
    Reader.PCD_DumpVersionToSerial();
    delay(100);

    Serial.println("==== End Setup ====");
}

void loop()
{
    // RFID cycle
    {
        Reader.PCD_Init();
        bool cardDetected = Reader.PICC_IsNewCardPresent() && Reader.PICC_ReadCardSerial();
        if (!onDelay && cardDetected)
        {
            String currentCard = ReadTag();
            if (lastCard == currentCard)
                return;
            Serial.println(currentCard);
            lastCard = currentCard;
            space(5);
        }
    }
}