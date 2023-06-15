#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <Time.h>
#include <TimeLib.h>
#include <String.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <DatabaseOnSD.h>
MyTable testTable("test.csv");

// Data wire is plugged into port 8
#define ONE_WIRE_BUS 0

//const char* ssid = "HUAWEI-B310-E360";
//const char* password = "CASACOSS21";

const char* ssid = "iPhone de coss ";
const char* password = "123456789";

//const char* ssid = "LGHH";
//const char* password = "zebr@123";

const char* sensorId = "18111";
const char* serverName = "http://200.94.111.227:8530/API/sensor";
//const char* serverName = "http://192.168.0.4:8530/API/sensor";  //produccion
unsigned long lastTime = 0;
unsigned long timerDelay = 6000;  //30000 30 seg  Cada cuanto se va a leer el sensor 300000 5 min


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
TinyGPSPlus gps;
SoftwareSerial ss(16, 15);


char tempReadings[20];
char hourReadings[20];
char dateReadings[20];
char latReadings[20];
char lngReadings[20];
int cantidadLecturas = 0;

void setup() {
  Serial.begin(115200);
  ss.begin(38400);

  sensors.begin();
  Serial.println("iniciando");
  WiFi.begin(ssid, password);
  Serial.print("Conectando a la red Wi-Fi...");
  setTime(12, 55, 00, 21, 5, 2022);
  testTable.printSDstatus();  //[optional] print the initialization status of SD card
  testTable.emptyTable();     //[optional] empty table content (make sure to call begin(rowN, colN) after emptying a table) // you could always add more rows.
  testTable.begin(1, 5);
  Serial.println("writing to table...");
  testTable.writeCell(0, 0, "DATE");
  testTable.writeCell(0, 1, "HOUR");
  testTable.writeCell(0, 2, "TEMP");
  testTable.writeCell(0, 3, "LAT");
  testTable.writeCell(0, 4, "LNG");
  Serial.println("finished writing!");
}

void loop() {
  time_t t = now();
  String fecha = String(year(t)) + "-" + String(month(t)) + "-" + String(day(t));
  String hora = String(hour(t)) + ":" + String(minute(t)) + ":" + String(second(t));


  while (ss.available() > 0) {

    if (gps.encode(ss.read())) {
      Serial.print(F("Location: "));
      if (gps.location.isValid()) {
        Serial.print("LAT=");
        Serial.println(gps.location.lat(), 6);
        Serial.print("LONG=");
        Serial.println(gps.location.lng(), 6);
        dtostrf(gps.location.lng(), 10, 6, lngReadings);
        dtostrf(gps.location.lat(), 10, 6, latReadings);
        Serial.println(lngReadings);
        Serial.println(latReadings);

      } else {
        Serial.print(F("INVALID"));
      }
    }
  }
  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println(F("No GPS detected: check wiring."));
    while (true)
      ;
  }

  if ((millis() - lastTime) >= timerDelay) {
    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures();  // Send the command to get temperatures
    Serial.println("DONE");
    String(sensors.getTempCByIndex(0), 2).toCharArray(tempReadings, 5);  //guarda la temperatura en un char array
    hora.toCharArray(hourReadings, 9);                                   //guarda la hora en un char array
    fecha.toCharArray(dateReadings, 9);


    if (cantidadLecturas + 1 >= testTable.countRows()) {
      testTable.appendEmptyRow();
    }

    testTable.writeCell(cantidadLecturas + 1, 0, dateReadings);
    testTable.writeCell(cantidadLecturas + 1, 1, hourReadings);
    testTable.writeCell(cantidadLecturas + 1, 2, tempReadings);
    testTable.writeCell(cantidadLecturas + 1, 3, latReadings);
    testTable.writeCell(cantidadLecturas + 1, 4, lngReadings);
    Serial.println("-------------read all table content at once ----------------------");
    testTable.printTable();
    //guarda la fecha en un char array
    cantidadLecturas = cantidadLecturas + 1;
    Serial.println("Temperatura guardada");
    Serial.println(tempReadings);
    Serial.println(hourReadings);
    Serial.println(dateReadings);
    Serial.println("Cantidad de Lecturas");
    Serial.println(cantidadLecturas);
    lastTime = millis();
  }
  if (WiFi.status() == WL_CONNECTED) {

    WiFiClient client;
    HTTPClient http;
    int lecturasAEnviar = cantidadLecturas;
    for (int i = lecturasAEnviar; i > 0; i--) {
      Serial.println("WiFi Connected");
      http.begin(client, serverName);
      http.addHeader("Content-Type", "application/json");
      DynamicJsonDocument json(200);
      Serial.println(i);
      json["sensorId"] = sensorId;
      json["Fecha"] = testTable.readCell(i, 0);  //
      JsonArray updates = json.createNestedArray("updates");
      JsonObject update = updates.createNestedObject();
      update["TimeStamp"] = testTable.readCell(i, 1);
      update["Temp"] = testTable.readCell(i, 2);
      update["Lat"] = testTable.readCell(i, 3);
      update["Lng"] = testTable.readCell(i, 4);
      String requestBody;
      serializeJson(json, requestBody);
      Serial.println(requestBody);
      int httpResponseCode = http.POST(requestBody);
      if (httpResponseCode == 200) {
        Serial.println("Datos enviados correctamente.");
        Serial.println(tempReadings);
        http.end();
        cantidadLecturas--;
      } else {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        i++;
        if (WiFi.status() != WL_CONNECTED) {
          break;
        }
      }
    }
  } else {
    Serial.print(".");
  }
}
