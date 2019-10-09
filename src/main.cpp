#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#define BME_CS 5
#define LCD_ADDR 0x27 
#define SEALEVELPRESSURE_HPA (1013.25)

AsyncWebServer server(80);
HTTPClient http;
Adafruit_BME280 bme(BME_CS); // use hardware SPI (ESP32 VSPI)
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

static const char* const AP_SSID = "hNiP";
static const char* const AP_PASS = "LunarQueen12273";

static float t{0}, h{0};

static byte oC[] = {
    B00000,
    B11000,
    B11000,
    B00011,
    B00100,
    B00100,
    B00011,
    B00000,
};
static byte percent[] = {
    B00000,
    B11000,
    B11001,
    B00010,
    B00100,
    B01000,
    B10011,
    B00011,
};


String processor(const String &var) {
    if (var == "TEMPERATURE") {
        return String(t);
    } else if (var == "HUMIDITY") {
        return String(h);
    }
    return String();
}

void setup() {
    Serial.begin(9600);
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
    }
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Initializing");
    lcd.setCursor(0, 1);
    lcd.print("Please wait...");
    lcd.createChar(0, oC);
    lcd.createChar(1, percent);
    WiFi.begin(AP_SSID, AP_PASS);
    delay(10000);
    if (WiFi.status() == WL_CONNECTED) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Got IP:");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
    }
    delay(10000);
    server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });
    server.on("/update.js", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/update.js", "application/javascript", false, nullptr);
    });
    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/plain", String(t).c_str());
    });
    server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/plain", String(h).c_str());
    });
    server.on("/all.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/all.css", "text/css", false, nullptr);
    });
    server.on("/webfonts/fa-solid-900.woff", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/fa-solid-900.woff", "application/font-woff2", false, nullptr);
    });
    server.on("/webfonts/fa-solid-900.woff2", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/fa-solid-900.woff2", "application/font-woff2", false, nullptr);
    });
    server.on("/webfonts/fa-solid-900.tff", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/fa-solid-900.tff", "application/font-woff2", false, nullptr);
    });
    server.on("/favicon.ico", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/favicon.ico", "image/x-icon", false, nullptr);
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css", false, nullptr);
    });
    if (! bme.begin(&Wire)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1);
    }
    server.begin();
}

bool sendData() {
    http.begin("mongyencute.tk", 8086, "/write?db=mycdata");
    char writeBuf[512];
    sprintf(writeBuf, "meas temp=%f,humd=%f", t, h);
    int _latestResponse = http.POST(writeBuf);
    return _latestResponse == 204;
}
int count = 0;
bool display_IP{true};
void loop() {
    lcd.clear();
    if (display_IP == true){
        lcd.setCursor(0, 0);
        lcd.print("IP");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
        display_IP = false;
    } else {
        lcd.setCursor(0, 0);
        lcd.printf("Temp: %2.2f", t);
        lcd.setCursor(11, 0);
        lcd.write(0);
        lcd.setCursor(11, 1);
        lcd.write(1);
        lcd.setCursor(0, 1);
        lcd.printf("Humd: %2.2f", h);
        display_IP = true;
    }
    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(2, HIGH);
        if (count == 10) {
            if (sendData()) {
                Serial.println("Database OK");
            } else {
                Serial.println("Database failed");
            }
            count = 0;
        } else {count++;}
    } else {
        digitalWrite(2, LOW);
    }
    delay(2000);
    h = bme.readHumidity();
    t = bme.readTemperature();
}

