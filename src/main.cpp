#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h> 
#include <Wire.h>

unsigned char low_data[8] = {0};
unsigned char high_data[12] = {0};

#define NO_TOUCH       0xFE
#define THRESHOLD      100
#define ATTINY1_HIGH_ADDR   0x78
#define ATTINY2_LOW_ADDR   0x77
#define HUMIDITY_PIN 34
#define PUMP_PIN 12
#define LED_PIN 25

// Web server running on port 80
WebServer server(80);

StaticJsonDocument<1024> jsonDocument;

char buffer[1024];


void createJson(char *name, float value) {  
  jsonDocument.clear();
  jsonDocument[name] = value;
  serializeJson(jsonDocument, buffer);  
}
 
void addJsonObject(char *name, float value) {
  JsonObject obj = jsonDocument.createNestedObject();
  obj[name] = value;
}

void getHigh12SectionValue(void)
{
  memset(high_data, 0, sizeof(high_data));
  Wire.requestFrom(ATTINY1_HIGH_ADDR, 12);
  while (12 != Wire.available());

  for (int i = 0; i < 12; i++) {
    high_data[i] = Wire.read();
  }
  delay(10);
}

void getLow8SectionValue(void)
{
  memset(low_data, 0, sizeof(low_data));
  Wire.requestFrom(ATTINY2_LOW_ADDR, 8);
  while (8 != Wire.available());

  for (int i = 0; i < 8 ; i++) {
    low_data[i] = Wire.read(); // receive a byte as character
  }
  delay(10);
}

int getWaterLevelPercentage()
{
  int maxCycles = 10;
  int currentCycle = 0;

  int averagePercentage = 0;

  int sensorvalue_min = 250;
  int sensorvalue_max = 255;
  int low_count = 0;
  int high_count = 0;
  while (currentCycle < maxCycles)
  {
    uint32_t touch_val = 0;
    uint8_t trig_section = 0;
    low_count = 0;
    high_count = 0;
    getLow8SectionValue();
    getHigh12SectionValue();

    Serial.println("low 8 sections value = ");
    for (int i = 0; i < 8; i++)
    {
      Serial.print(low_data[i]);
      Serial.print(".");
      if (low_data[i] >= sensorvalue_min && low_data[i] <= sensorvalue_max)
      {
        low_count++;
      }
      if (low_count == 8)
      {
        Serial.print("      ");
        Serial.print("PASS");
      }
    }
    Serial.println("  ");
    Serial.println("  ");
    Serial.println("high 12 sections value = ");
    for (int i = 0; i < 12; i++)
    {
      Serial.print(high_data[i]);
      Serial.print(".");

      if (high_data[i] >= sensorvalue_min && high_data[i] <= sensorvalue_max)
      {
        high_count++;
      }
      if (high_count == 12)
      {
        Serial.print("      ");
        Serial.print("PASS");
      }
    }

    Serial.println("  ");
    Serial.println("  ");

    for (int i = 0 ; i < 8; i++) {
      if (low_data[i] > THRESHOLD) {
        touch_val |= 1 << i;

      }
    }
    for (int i = 0 ; i < 12; i++) {
      if (high_data[i] > THRESHOLD) {
        touch_val |= (uint32_t)1 << (8 + i);
      }
    }

    while (touch_val & 0x01)
    {
      trig_section++;
      touch_val >>= 1;
    }
    Serial.print("water level = ");
    Serial.print(trig_section * 5);
    Serial.println("% ");
    Serial.println(" ");
    Serial.println("*********************************************************");
    averagePercentage += trig_section * 5;
    currentCycle++;
    delay(500);
  }
  averagePercentage = (int)(averagePercentage / maxCycles);
  return averagePercentage;
}

void getWaterLevel() {
  Serial.println("Getting Water Level...");

  int waterLevel = getWaterLevelPercentage();
  Serial.print("Returning Average Water Level: ");
  Serial.println(waterLevel);

  jsonDocument.clear(); // Clear json buffer
  addJsonObject("waterLevel", waterLevel);
  serializeJson(jsonDocument, buffer);

  server.send(200, "application/json", buffer);
}

void getHumidity() {
  Serial.println("Getting Humidity...");
  int maxCycles = 10;
  int currentCycle = 0;

  int averageHumidity = 0;

  while(currentCycle < maxCycles) {
    int humidity = map(analogRead(HUMIDITY_PIN), 0, 4095, 0, 100);
    Serial.print("Humidity: ");
    Serial.println(humidity);
    averageHumidity += humidity;
    currentCycle++;
    delay(500);
  }
  averageHumidity = averageHumidity / maxCycles;
  Serial.print("Returning Average Humidity: ");
  Serial.println(averageHumidity);

  jsonDocument.clear(); // Clear json buffer
  addJsonObject("humidity", averageHumidity);
  serializeJson(jsonDocument, buffer);

  server.send(200, "application/json", buffer);
}

void activatePump(int waterAmount) {
  Serial.println("Activating Pump...");
  digitalWrite(PUMP_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(1000 + waterAmount);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}

void warnLED(void *pvParameters){
  int *duration = (int*)pvParameters;
  const int maxBlinkCount = *duration;
  int blinkCount = 0;
  while(blinkCount < maxBlinkCount){  
    digitalWrite(LED_PIN, HIGH);
    delay(500); 
    digitalWrite(LED_PIN, LOW);
    delay(500);
    blinkCount++;
  }
  vTaskDelete(NULL);
}

void postActivatePump() {
  Serial.println("Activating Pump...");
  if (server.hasArg("plain") == false) {
    //handle error here
    return;
  }

  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);

  int waterAmount = jsonDocument["waterAmount"];
  activatePump(waterAmount);

  // Respond to the client
  server.send(200, "application/json", "{}");
}

void postIdenfity() {
  Serial.println("Identifying...");

  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);

  int duration = jsonDocument["duration"];

  xTaskCreate(
        warnLED,      // Function name of the task
        "Blink LED",   // Name of the task (e.g. for debugging)
        2048,        // Stack size (bytes)
        (void*)duration,        // Parameter to pass
        1,           // Task priority
        NULL   // Assign task handle
      );
  server.send(200, "application/json", "{\"name\": \"Smart Irrigation System\"}");
}

void getPing() {
  Serial.println("Pinging...");
  server.send(200, "application/json", "{\"message\": \"Pong\"}");
}

void setupApi() {
  server.on("/water-level", getWaterLevel);
  server.on("/humidity", getHumidity);
  server.on("/activate-pump", postActivatePump);
  server.on("/identify", postIdenfity);
  server.on("/ping", getPing);

  // start server
  server.begin();
}

void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found
  delay(1500); 


  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // it is a good practice to make sure your code sets wifi mode how you want it.

  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  // wm.resetSettings();

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

  if(!res) {
      Serial.println("Failed to connect");
      ESP.restart();
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("Connected...yeey :)");
  }

  setupApi();
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
}
