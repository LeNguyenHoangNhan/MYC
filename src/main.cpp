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

String STA_SSID, STA_PASS;
String AP_SSID = "WiFi_Config";
String AP_PASS = "12345678";
String FAILSAFE_PWD;
bool is_locked{false};
static float t{0}, h{0};

bool SPIFFS_status{false};
bool WiFi_status{false};
bool display_IP{true};
bool IS_LOCKED{false};
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

void lcd_err_pr(LiquidCrystal_I2C &lcd, const char *error_code) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERROR");
    lcd.setCursor(0, 1);
    lcd.print(error_code);
}
void lcd_clr_pr(LiquidCrystal_I2C &lcd, const String &first_row, const String &second_row) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(first_row);
    lcd.setCursor(0, 1);
    lcd.print(second_row);
}
void lcd_clr_pr(LiquidCrystal_I2C &lcd, const char *first_row, const char *second_row) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(first_row);
    lcd.setCursor(0, 1);
    lcd.print(second_row);
}
bool sendData() {
    http.begin("mongyencute.tk", 8086, "/write?db=mycdata");
    char writeBuf[512];
    sprintf(writeBuf, "meas temp=%f,humd=%f", t, h);
    int _latestResponse = http.POST(writeBuf);
    return _latestResponse == 204;
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
    lcd.setCursor(0, 0);
    lcd.print("Initializing");
    lcd.setCursor(0, 1);
    lcd.print("Please wait");

    if (!SPIFFS.begin(true)) {
        Serial.println("[FATAL] ERROR mounting the SPIFFS, the WEB interface will not work, please contact ADMIN");
        lcd_err_pr(lcd, "001");
        delay(1000);
    } else {
        SPIFFS_status = true;
    }
    if (SPIFFS_status) {
        File cfg_file = SPIFFS.open("/wfcf.json", "r");
        if (!cfg_file) {
            Serial.println("An error has occurred while open file");
            Serial.println("Using default configuration");
            STA_SSID = "CPH1605";
            STA_PASS = "06052004";
            lcd_err_pr(lcd, "002");
            delay(1000);
        } else {
            String cfgf_content = cfg_file.readString();
            Serial.printf("WiFi Config file: %s\n", cfgf_content.c_str());
            ArduinoJson::DynamicJsonDocument wifi_config(1024);
            ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(wifi_config, cfgf_content);
            if (error) {
                Serial.println(F("Failed to parse config file, using default configuration"));
                STA_SSID = "CPH1605";
                STA_PASS = "06052004";
            } else {
                ArduinoJson::JsonObject obj = wifi_config.as<ArduinoJson::JsonObject>();
                STA_SSID = obj["ssid"].as<String>();
                STA_PASS = obj["pass"].as<String>();
            }
        }
        cfg_file.close();
        File fss_file = SPIFFS.open("/fss.txt", "r");
        if (!fss_file) {
            Serial.println("An error has occurred while open status file");
            Serial.println("LOCKING UP THE DEVICE");
            is_locked = true;
            lcd_err_pr(lcd, "EEE");
        } else {
            String fss_status = fss_file.readString();
            if (fss_status == "unlock") {
                is_locked = false;
            } else {
                is_locked = true;
                Serial.println("This device has been locked");
                lcd_err_pr(lcd, "FFF");
            }
        }
        fss_file.close();
        File fspwd_file = SPIFFS.open("/fspw.txt", "r");
        if (!fspwd_file) {
            Serial.println("An error has occurred while open password file");
            Serial.println("LOCKING UP THE DEVICE");
            is_locked = true;
            lcd_err_pr(lcd, "DDD");
        } else {
            FAILSAFE_PWD = fspwd_file.readString();
            Serial.printf("Fail safe password: %s \n", FAILSAFE_PWD.c_str());
        }
        fspwd_file.close();
    }
    Serial.printf("Connecting to WiFi with SSID is %s, PASS is %s", STA_SSID.c_str(), STA_PASS.c_str());
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.begin(STA_SSID.c_str(), STA_PASS.c_str());
    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());

    lcd.createChar(0, oCSymbol);
    lcd.createChar(1, percentSymbol);

    pinMode(2, OUTPUT);
    long long timeout = millis();
    lcd_clr_pr(lcd, "Connecting", "to WiFi");
    while (WiFi.status() != WL_CONNECTED && !WiFi.isConnected()) {
        Serial.print(".");
        delay(1000);
        if (millis() - timeout > 15000) {
            lcd_err_pr(lcd, "004");
            Serial.println("Timed out connect to WiFi");
            delay(1000);
            break;
        }
    }
    if (SPIFFS_status) {
        server.on("/failsafe", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/failsafe.html", "text/html", false, [](const String &var) -> String {
                if (var == "STATUS") {
                    if (is_locked) {
                        return "LOCKED";
                    } else {
                        return "OPEN";
                    }
                } else {
                    return String();
                }
            });
        });
        server.on("/smfs", HTTP_GET, [](AsyncWebServerRequest *request) {
            String passwd_recv = request->getParam("pwd")->value();
            Serial.println("passwd_recv: " + passwd_recv);
            if (passwd_recv == FAILSAFE_PWD) {
                if (!is_locked) {
                    is_locked = true;
                    request->send_P(200, "text/plain", "", nullptr);
                } else if (is_locked) {
                    is_locked = false;
                    request->send_P(400, "text/plain", "", nullptr);
                }
                {
                    SPIFFS.remove("/fss.txt");
                    File fss_file = SPIFFS.open("/fss.txt", "w");
                    for (int i = 0; i < 2; i++) {
                        if (!fss_file) {
                            Serial.println("An error has occurred while open status file");
                            Serial.println("retrying!");
                        } else {
                            if (fss_file.print(is_locked ? "lock" : "unlock")) {
                                Serial.println("Write config file succesfully");
                            } else {
                                Serial.println("ERROR writing file");
                            }
                        }
                        fss_file.close();
                        ESP.restart();
                    }
                }
            } else {
                request->send_P(300, "text/plain", "", nullptr);
            }
        });
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
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/index.html", "text/html", false, [](const String &var) -> String {
                if (var == "TEMPERATURE") {
                    return String(t);
                } else if (var == "HUMIDITY") {
                    return String(h);
                }
                return String(); });
        });
        server.on("/update.js", HTTP_GET, [](AsyncWebServerRequest *request) {
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
        server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/favicon.ico", "image/x-icon", false, nullptr);
        });
        server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/style.css", "text/css", false, nullptr);
        });
        server.begin();
    }
    if (!bme.begin()) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        lcd_err_pr(lcd, "006");
        while (1) {
        }  // No point of continue!
    }
    WiFi.onEvent([](WiFiEvent_t event) {
        Serial.printf("[WiFi-event] event: %d\n", event);
        switch (event) {
        case SYSTEM_EVENT_STA_GOT_IP:
            WiFi_status = true;
            digitalWrite(2, HIGH);
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            WiFi_status = true;
            digitalWrite(2, HIGH);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            WiFi_status = false;
            digitalWrite(2, LOW);
            break;
        }
    });
}

int count{0};
void loop() {
    lcd.clear();
    if (is_locked) {
        lcd_err_pr(lcd, "LLL");
        Serial.println("Device is locked!");
        while (1) {
            delay(1000);
        }
        return;
    } else {
        Serial.println("Device is not locked!");
        if (display_IP == true) {
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
        Serial.printf("Loop time %d\n", count);
        if (WiFi.status() == WL_CONNECTED && WiFi.isConnected() && WiFi.localIP() != 0) {
            WiFi_status = true;
        } else {
            WiFi_status = false;
        }
        if (count == 10) {
            count = 0;

            Serial.printf("WiFi status: %s\n", WiFi_status ? "Ok" : "Not OK");
            if (WiFi_status) {
                if (sendData()) {
                    Serial.println("Database OK");
                } else {
                    Serial.println("Database failed");
                    lcd_err_pr(lcd, "007");
                    delay(1000);
                }
            }
        } else {
            count++;
        }
        delay(2000);
        h = bme.readHumidity();
        t = bme.readTemperature();
    }
}
