#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include "DHT.h"
#include "secrets.h"

// --- CREDENCIALES DE SUPABASE (desde secrets.h) ---
const char* supabase_url = SUPABASE_URL;
const char* supabase_api_key = SUPABASE_KEY;

// --- CONFIGURACIÓN DE PINES SEGÚN TU PLANO ---
#define DHTPIN 23          // DHT22 en GPIO 23
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

const int pinSuelo = 36;   // Sensor de suelo en GPIO 36 (VP)
const int pinRele  = 25;   // Relé en GPIO 25

// Calibración del sensor de suelo
const int valorSeco = 2600;
const int valorHumedo = 1000;

// --- CONTROL DE TIEMPO (NO BLOQUEANTE) ---
unsigned long tiempoPrevio = 0;
const unsigned long intervaloLectura = 60000; // 1 minuto (en milisegundos)

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configuración de pines
  pinMode(pinSuelo, INPUT);
  pinMode(pinRele, OUTPUT);
  digitalWrite(pinRele, HIGH); // Apagado inicial (Active LOW: HIGH = relé abierto/apagado)

  dht.begin();

  // --- CONFIGURACIÓN DE WIFIMANAGER ---
  WiFiManager wm;

  // Timeout para cerrar el portal si nadie se conecta (3 minutos)
  wm.setConfigPortalTimeout(180);

  Serial.println("\nIniciando conexión Wi-Fi...");
  bool conectado = wm.autoConnect("ESP32-Planta-Config");

  if (!conectado) {
    Serial.println("No se pudo conectar y expiró el tiempo del portal. Reiniciando...");
    ESP.restart();
  }

  Serial.println("\n¡Wi-Fi Conectado con éxito!");
  Serial.print("IP asignada: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long tiempoActual = millis();

  // Se ejecuta solo cuando transcurre el intervalo configurado
  if (tiempoActual - tiempoPrevio >= intervaloLectura) {
    tiempoPrevio = tiempoActual;

    if (WiFi.status() == WL_CONNECTED) {

      // 1. Lectura de sensores
      float t = dht.readTemperature();
      float h_aire = dht.readHumidity();
      int lecturaSuelo = analogRead(pinSuelo);

      Serial.printf("\n[CALIBRACIÓN] -> Valor crudo analogRead(pinSuelo): %d\n", lecturaSuelo);

      if (isnan(t) || isnan(h_aire)) {
        Serial.println("Error al leer el sensor DHT22");
        return;
      }

      int h_suelo = map(lecturaSuelo, valorSeco, valorHumedo, 0, 100);
      h_suelo = constrain(h_suelo, 0, 100);

      Serial.printf("Temp: %.1f°C | Hum. Aire: %.1f%% | Hum. Suelo: %d%%\n", t, h_aire, h_suelo);

      // 2. Pulso de prueba del Relé (2 segundos activo)
      Serial.println(">> Accionando relé (ON)...");
      digitalWrite(pinRele, LOW);  // Disparo (Active LOW)
      delay(2000);
      digitalWrite(pinRele, HIGH); // Corte
      Serial.println(">> Relé apagado (OFF)");

      // 3. Creación del payload JSON
      StaticJsonDocument<200> doc;
      doc["humedad_suelo"] = h_suelo;
      doc["temperatura_aire"] = t;
      doc["humedad_aire"] = h_aire;

      String datosJson;
      serializeJson(doc, datosJson);

      // 4. Envío a Supabase vía POST
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
        Serial.printf("Error en el envío: %s\n", http.errorToString(codigoRespuesta).c_str());
      }

      http.end();
    } else {
      Serial.println("Aviso: Conexión Wi-Fi perdida.");
    }
  }
}