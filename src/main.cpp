#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include "secrets.h"

// --- VINCULACIÓN CON SECRETS.H ---
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* supabase_url = SUPABASE_URL;
const char* supabase_api_key = SUPABASE_KEY;

// --- CONFIGURACIÓN DE PINES ---
#define DHTPIN 23
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

const int pinSuelo = 36; // Usamos el pin VP (GPIO 36)

// Calibración de tu sensor de suelo
const int valorSeco = 2600;  // Lectura promedio al aire
const int valorHumedo = 1000; // Lectura promedio en agua

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
    
    // IMPRIMIR VALOR CRUDO EN CONSOLA PARA CALIBRACIÓN
    Serial.printf("\n[CALIBRACIÓN] -> Valor crudo analogRead(pinSuelo): %d\n", lecturaSuelo);

    // Convertir la lectura del suelo a porcentaje
    int h_suelo = map(lecturaSuelo, valorSeco, valorHumedo, 0, 100);
    h_suelo = constrain(h_suelo, 0, 100); 

    // Validar que el DHT no tire error
    if (isnan(t) || isnan(h_aire)) {
      Serial.println("Error al leer el sensor DHT22");
      delay(2000);
      return;
    }

    Serial.printf("Temp: %.1f°C | Hum. Aire: %.1f%% | Hum. Suelo: %d%%\n", t, h_aire, h_suelo);

    // 2. Crear el objeto JSON
    StaticJsonDocument<200> doc;
    doc["humedad_suelo"] = h_suelo;
    doc["temperatura_aire"] = t;
    doc["humedad_aire"] = h_aire;

    String datosJson;
    serializeJson(doc, datosJson);

    // 3. Enviar a Supabase
    HTTPClient http;
    http.begin(supabase_url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabase_api_key);
    http.addHeader("Authorization", "Bearer " + String(supabase_api_key));

    Serial.println("Enviando datos a Supabase...");
    int codigoRespuesta = http.POST(datosJson);

    if (codigoRespuesta > 0) {
      Serial.printf("Respuesta del servidor: %d\n", codigoRespuesta);
      if (codigoRespuesta == 201) {
        Serial.println("¡Datos guardados con éxito en la nube!");
      }
    } else {
      Serial.printf("Error en el envío. Código: %s\n", http.errorToString(codigoRespuesta).c_str());
    }

    http.end(); 
  } else {
    Serial.println("Error: Se perdió la conexión Wi-Fi");
  }

  // --- ESPERA ENTRE LECTURAS (TEMPORAL PARA DEMO: 1 MINUTO) ---
  // 1 minuto = 60 * 1000 ms = 60.000 ms
  Serial.println("\nEsperando 1 minuto para la próxima lectura...");
  delay(60 * 1000);
}