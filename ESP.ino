// Library
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <type_traits>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_MLX90614.h>
#include <LiquidCrystal_I2C.h>

// RFID Variables
#define resetPin D3
#define ssPin D8
MFRC522 Reader;
String lastCard;

// Others
#define buzzer D4
int outputs[] = {buzzer};

#define log Serial.println
bool onDelay = false; // to prevent card from being readed while nodemcu doing something else
bool cardDetectedBefore = false;

// // termogun
// Adafruit_MLX90614 mlx();

// LCD
#define LCD_ROW 2
#define LCD_COLMN 16
LiquidCrystal_I2C lcd_(0x23, LCD_COLMN, LCD_ROW);

// Http verb
#define GET_ 1
#define POST_ 2
#define PUT_ 3
#define PATCH_ 5

// http response code
bool code(int in, int expected)
{
    return in >= expected && in < expected + 100;
}

// Server
#define SERVER_ "https://iot-by-shaladdin.shaladddin.repl.co/api/"
String server;

// Wifi
// #define WIFI_SSID_ "Mas MHD"
// #define WIFI_PASSWORD_ "Fahri_Ali-17082016"
#define WIFI_SSID_ "Lab Komputer 2"
#define WIFI_PASSWORD_ "mtsn4bisa"

String wifiSSID;
String wifiPassword; // Password of your wifi network
#define wifi WiFi

// Server functions
template <typename T>
String query(String key, T value) { return key + "=" + String(value); }

class url
{
public:
    String server;
    String path;
    String querys = "";
    void addQuery(String query) { querys += "&" + query; }
    void clearQuery() { querys = ""; }
    String get()
    {
        return server + path + (querys == "" ? "" : ("?" + querys));
    }
    void init(String server, String path = "", String querys = "")
    {
        this->server = server;
        this->path = path;
        this->querys = querys;
    }
};

// used url:
url hulloWorld;
url absen;

// fetch http request
int responseCode;
String fetch(int verb, String path, String *Headers = NULL, String body = "")
{
    while (wifi.status() != WL_CONNECTED)
        log(F("No wifi access"));

    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();

    log("sending request to " + path);

    http.begin(client, path.c_str());

    switch (verb)
    {
    case GET_:
        responseCode = http.GET();
        break;
    case POST_:
        responseCode = http.POST(body);
        break;
    case PUT_:
        responseCode = http.PUT(body);
        break;
    case PATCH_:
        responseCode = http.PATCH(body);
        break;
    }

    String payload = "{}";
    if (code(responseCode, 200))
        log("accepted with code " + responseCode);
    else
        log("rejected with code " + responseCode);
    if (!code(responseCode, 500))
        payload = http.getString();
    log("response code: " + responseCode);

    http.end();

    return payload;
}
DynamicJsonDocument fetch(int capacity, int verb, String path, String *Headers = NULL, String body = "")
{
    int payloadSize;
    char *payload;
    fetch(verb, path, Headers, body).toCharArray(payload, payloadSize);
    DynamicJsonDocument res(capacity);
    DeserializationError err = deserializeJson(res, payload, payloadSize);

    if (err)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(err.f_str());
        return res;
    }
    return res;
}

// RFID functions
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

// lcd function
struct
{
    String *currentText[2];
    String *nextText[2];
    unsigned long int nextUpdate;
    bool notExecuted = false;
    void print(String top, String bottom)
    {
        currentText[0] = &top;
        currentText[1] = &bottom;
        if (top == *currentText[0] && bottom == *currentText[1])
            return;
        for (int i = 0; i < LCD_ROW; i++)
        {
            lcd_.setCursor(0, i);
            lcd_.print(*currentText[i]);
        }
        // update the text
        lcd_.clear();
    }
    void printLater(int delay_, String top = "", String bottom = "")
    {
        String *text[2] = {&top, &bottom};
        nextUpdate = millis() + delay_;
        nextText[0] = text[0];
        nextText[1] = text[1];
    }
    void print(String text, int row = 0)
    {
        lcd_.clear();
        lcd_.setCursor(0, 0);
        lcd_.print(text);
    }
} lcd;

// Other
void space(int len = 1)
{
    for (int i = 0; i < len; i++)
        Serial.println();
}
void beep(int freq, int duration, int blynk)
{
    int delayTime = duration / (blynk * 2);
    bool on = true;
    while (duration > 0)
    {
        if (on)
            tone(buzzer, freq);
        else
            noTone(buzzer);
        delay(delayTime);
        duration -= delayTime;
        on = !on;
        if (duration - delayTime < 0)
            on = false;
    }
}

void setup()
{
    server = F(SERVER_);
    wifiSSID = F(WIFI_SSID_);
    wifiPassword = F(WIFI_PASSWORD_);
    // initialize urls
    hulloWorld.init(server, "hullo");
    absen.init(server, "absen");

    for (int i = 0; i < sizeof(outputs) / sizeof(outputs[0]); i++)
        pinMode(outputs[i], OUTPUT);

    beep(1000, 500, 2);

    Serial.begin(9600);

    //   if (!mlx.begin()) {
    //     Serial.println("Error connecting to MLX sensor. Check wiring.");
    //     while (1);
    //   };

    delay(500);

    WiFi.begin(wifiSSID, wifiPassword);
    log("\n\n");
    Serial.println("connecting to " + wifiSSID);

#define tikDelay 500
    int tik = millis() + tikDelay, manyDots = 3;
    while (WiFi.status() != WL_CONNECTED)
    {
        if (manyDots == 3)
            lcd.print(F("connecting to  ") + wifiSSID);
        if (tik < millis())
        {
            manyDots++;
            lcd_.print(F("."));
        }
        if (wifi.status() == WL_WRONG_PASSWORD)
        {
            lcd.print(F("wrong pasword lol"));
            log(F("wrong pasword lol"));
        }
    }
    space();
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());

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
    delay(1000);

    log("server: " + server);
    log("fetching from " + hulloWorld.get());
    log("response: " + fetch(GET_, hulloWorld.get()));

    log("status code: " + String(responseCode));
    Serial.println("==== End Setup ====");
}

void loop()
{
    // RFID cycle
    {
        Reader.PCD_Init();
        bool cardDetected = Reader.PICC_IsNewCardPresent() && Reader.PICC_ReadCardSerial();
        bool newCard = cardDetected && !cardDetectedBefore;
        cardDetectedBefore = cardDetected;
        if (!onDelay && newCard)
        {
            String currentCard = ReadTag();
            Serial.println(currentCard);
            beep(1000, 500, 2);

            absen.addQuery(query("tag", currentCard));
            fetch(POST_, absen.get());
            // DynamicJsonDocument doc = fetch()
            // absen.clearQuery();

            switch (responseCode)
            {
            case HTTP_CODE_ACCEPTED:
                log("absen accepted");
                beep(1500, 1000, 3);
                return;
                break;
            case HTTP_CODE_CONFLICT:
                log("already absen");
                break;
            case HTTP_CODE_UNAUTHORIZED:
                log("card is not authorized");
                break;
            }
            beep(500, 3000, 3);
        }
    }
}