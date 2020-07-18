#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <Wire.h>
#define BME_CS 5
#define LCD_ADDR 0x27
#define SEALEVELPRESSURE_HPA (1013.25)

AsyncWebServer server(80);
HTTPClient http;
Adafruit_BME280 bme(BME_CS);  // use hardware SPI (ESP32 VSPI)
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

String AP_SSID, AP_PASS;
static float currentTemp{0}, currentHumidity{0};

static byte oCSymbol[] = {
    B00000,
    B11000,
    B11000,
    B00011,
    B00100,
    B00100,
    B00011,
    B00000,
};
static byte percentSymbol[] = {
    B00000,
    B11000,
    B11001,
    B00010,
    B00100,
    B01000,
    B10011,
    B00011,
};

String templateProcessor(const String &var) {
    if (var == "TEMPERATURE") {
        return String(currentTemp);
    } else if (var == "HUMIDITY") {
        return String(currentHumidity);
    }
    return String();
}

void setup() {
    Serial.begin(9600);
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
    }
    File file = SPIFFS.open("/wfcf.json", FILE_READ);
    if (!file) {
        Serial.println("An error has occurred while open file");
        return;
    }
    String file_content = file.readString();
    file.close();
    Serial.printf("WiFi Config file: %s", file_content.c_str());
    ArduinoJson::DynamicJsonDocument wifi_config(1024);
    ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(wifi_config, file_content);
    if (error) {
        Serial.println(F("Failed to read file, using default configuration"));
        AP_SSID = "CPH1605";
        AP_PASS = "06052004";
    } else {
        ArduinoJson::JsonObject obj = wifi_config.as<ArduinoJson::JsonObject>();
        AP_SSID = obj["ssid"].as<String>();
        AP_PASS = obj["pass"].as<String>();
    }
    Serial.printf("Connecting to WiFi with SSID is %s, PASS is %s", AP_SSID.c_str(), AP_PASS.c_str());
    lcd.begin();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Initializing");
    lcd.setCursor(0, 1);
    lcd.print("Please wait...");
    lcd.createChar(0, oCSymbol);
    lcd.createChar(1, percentSymbol);
    WiFi.begin(AP_SSID.c_str(), AP_PASS.c_str());
    pinMode(2, OUTPUT);
    long long timeout = millis();
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
        if (millis() - timeout >= 15000) {
            Serial.println();
            Serial.println("Cannot connect to WiFi, Please config WiFi");
            WiFi.softAP("WiFi_Config", "12345678");
            delay(5000);
            server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->send(SPIFFS, "/wificf.html", "text/html", false, [](const String &var) -> String {
                    if (var == "SSID") {
                        return AP_SSID;
                    } else if (var == "PASS") {
                        return AP_PASS;
                    } else {
                        return String();
                    }
                });
            });
            server.on(
                "/postcf", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
              String sdata = "";
              for (size_t i = 0; i < len; i++) {
                sdata += (char) data[i];
              }
              Serial.println(sdata);
              SPIFFS.remove("/wfcf.json");
              File file = SPIFFS.open("/wfcf.json", FILE_WRITE);
              if (!file) {
                Serial.println("ERROR creating config file");
              } else {
                if (file.print(sdata)) {
                  Serial.println("Write config file succesfully");
                } else {
                  Serial.println("ERROR writing file");
                }
              }
              file.close();
              ESP.restart(); });
            break;
        }
    }
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html", false, templateProcessor);
    });
    server.on("/update.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/update.js", "application/javascript", false, nullptr);
    });
    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/plain", String(currentTemp).c_str());
    });
    server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/plain", String(currentHumidity).c_str());
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
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/favicon.ico", "image/x-icon", false, nullptr);
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css", false, nullptr);
    });
    if (!bme.begin()) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1)
            ;
    }
    server.begin();
}

bool sendData() {
    http.begin("mongyencute.tk", 8086, "/write?db=mycdata");
    char writeBuf[512];
    sprintf(writeBuf, "meas temp=%f,humd=%f", currentTemp, currentHumidity);
    int _latestResponse = http.POST(writeBuf);
    return _latestResponse == 204;
}
int count = 0;
bool display_IP{true};
void loop() {
    lcd.clear();
    if (display_IP == true) {
        lcd.setCursor(0, 0);
        lcd.print("IP");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
        display_IP = false;
    } else {
        lcd.setCursor(0, 0);
        lcd.printf("Temp: %2.2f", currentTemp);
        lcd.setCursor(11, 0);
        lcd.write(0);
        lcd.setCursor(11, 1);
        lcd.write(1);
        lcd.setCursor(0, 1);
        lcd.printf("Humd: %2.2f", currentHumidity);
        display_IP = true;
    }
    if (WiFi.status() == WL_CONNECTED && WiFi.isConnected() == true) {
        digitalWrite(2, HIGH);
        if (count == 10) {
            if (sendData()) {
                Serial.println("Database OK");
            } else {
                Serial.println("Database failed");
            }
            count = 0;
        } else {
            count++;
        }
    } else {
        digitalWrite(2, LOW);
    }
    delay(2000);
    currentHumidity = bme.readHumidity();
    currentTemp = bme.readTemperature();
}
