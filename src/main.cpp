#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include "secrets.h"

// --- CONFIGURACIÓN DE TU WI-FI ---
const char* ssid = "Manente";
const char* password = "6453man6455";

// --- CONFIGURACIÓN DE SUPABASE ---
// Tu URL debe terminar en "/rest/v1/mediciones" para apuntar directo a tu tabla
const char* supabase_url = "https://dszgiimsmtboczkndblg.supabase.co/rest/v1/mediciones";
const char* supabase_api_key = "sb_publishable_EiRKDJn1DV1E9tlBnSyXsA_MvAzPnx3";

// --- CONFIGURACIÓN DE PINES ---
#define DHTPIN 23
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

const int pinSuelo = 34;
// Calibración de tu sensor de suelo (ajustá estos valores si es necesario)
const int valorSeco = 3100;  
const int valorHumedo = 1400;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  dht.begin();
  pinMode(pinSuelo, INPUT);

  // Conexión al Wi-Fi
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡Wi-Fi Conectado con éxito!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    
    // 1. Leer los sensores
    float t = dht.readTemperature();
    float h_aire = dht.readHumidity();
    int lecturaSuelo = analogRead(pinSuelo);
    
    // Convertir la lectura del suelo a porcentaje
    int h_suelo = map(lecturaSuelo, valorSeco, valorHumedo, 0, 100);
    h_suelo = constrain(h_suelo, 0, 100); // Evita porcentajes raros fuera de 0-100

    // Validar que el DHT no tire error
    if (isnan(t) || isnan(h_aire)) {
      Serial.println("Error al leer el sensor DHT22");
      delay(2000);
      return;
    }

    Serial.println("\n--- Nueva Medición ---");
    Serial.printf("Temp: %.1f°C | Hum. Aire: %.1f%% | Hum. Suelo: %d%%\n", t, h_aire, h_suelo);

    // 2. Crear el objeto JSON con los nombres exactos de las columnas de tu Supabase
    StaticJsonDocument<200> doc;
    doc["humedad_suelo"] = h_suelo;
    doc["temperatura_aire"] = t;
    doc["humedad_aire"] = h_aire;

    String datosJson;
    serializeJson(doc, datosJson);

    // 3. Preparar la petición HTTP POST hacia Supabase
    HTTPClient http;
    http.begin(supabase_url);
    
    // Encabezados obligatorios que te pide Supabase para dejarte guardar datos
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabase_api_key);
    http.addHeader("Authorization", "Bearer " + String(supabase_api_key));

    // 4. Enviar los datos
    Serial.println("Enviando datos a Supabase...");
    int codigoRespuesta = http.POST(datosJson);

    // 5. Revisar si salió todo bien
    if (codigoRespuesta > 0) {
      Serial.printf("Respuesta del servidor: %d\n", codigoRespuesta);
      if (codigoRespuesta == 201) {
        Serial.println("¡Datos guardados con éxito en la nube!");
      }
    } else {
      Serial.printf("Error en el envío. Código: %s\n", http.errorToString(codigoRespuesta).c_str());
    }

    http.end(); // Cerrar la conexión
  } else {
    Serial.println("Error: Se perdió la conexión Wi-Fi");
  }

  // Esperar 10 segundos antes de la próxima medición
  delay(10000);
}