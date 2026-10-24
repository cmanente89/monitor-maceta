#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include "DHT.h"
#include "secrets.h"

// --- CREDENCIALES DE SUPABASE (desde secrets.h) ---
const char* supabase_url = SUPABASE_URL; // URL del endpoint de telemetría
const char* supabase_api_key = SUPABASE_KEY;

// Extraemos el dominio base para construir el endpoint de control
// Asume formato: "https://xxxx.supabase.co/rest/v1/telemetry"
String getSupabaseBaseUrl() {
  String url = String(supabase_url);
  int index = url.indexOf("/rest/v1/");
  if (index != -1) {
    return url.substring(0, index);
  }
  return url;
}

// --- CONFIGURACIÓN DE PINES ---
#define DHTPIN 23          // DHT22 en GPIO 23
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

const int pinSuelo = 36;   // Sensor de suelo en GPIO 36 (VP)
const int pinRele  = 25;   // Relé en GPIO 25 (Active LOW: LOW = ON, HIGH = OFF)

// Calibración del sensor de suelo
const int valorSeco = 2600;
const int valorHumedo = 1000;

// --- CONTROL DE TIEMPO (NO BLOQUEANTE) ---
unsigned long tiempoPrevioLectura = 0;
const unsigned long intervaloLectura = 60000; // Telemetría cada 1 minuto

unsigned long tiempoPrevioPolling = 0;
const unsigned long intervaloPolling = 4000;  // Heartbeat y lectura de comandos cada 4s

// Variables de estado del actuador
bool bombaActiva = false;
unsigned long tiempoInicioBomba = 0;
unsigned long duracionBombaMs = 0;

// Variables de estado leídas de Supabase
bool modoPruebaActivo = false;
unsigned long ultimoCicloPrueba = 0;

// Declaración de funciones
void enviarTelemetria();
void consultarComandosSupabase();
void actualizarLastSeen();
void resetearManualTrigger();

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configuración de pines
  pinMode(pinSuelo, INPUT);
  pinMode(pinRele, OUTPUT);
  digitalWrite(pinRele, HIGH); // Apagado inicial (Active LOW)

  dht.begin();

  // --- CONFIGURACIÓN DE WIFIMANAGER ---
  WiFiManager wm;
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

  // 1. TEMPORIZADOR NO BLOQUEANTE DE LA BOMBA
  if (bombaActiva) {
    if (tiempoActual - tiempoInicioBomba >= duracionBombaMs) {
      digitalWrite(pinRele, HIGH); // Apagar relé
      bombaActiva = false;
      Serial.println(">> Bomba APAGADA (Tiempo cumplido)");
      resetearManualTrigger(); // Notifica a Supabase que finalizó el riego
    }
  }

  // 2. POLLING DE COMANDOS Y HEARTBEAT (Cada 4 segundos)
  if (tiempoActual - tiempoPrevioPolling >= intervaloPolling) {
    tiempoPrevioPolling = tiempoActual;
    if (WiFi.status() == WL_CONNECTED) {
      consultarComandosSupabase();
    }
  }

  // 3. LECTURA DE SENSORES Y ENVÍO DE TELEMETRÍA (Cada 1 minuto)
  if (tiempoActual - tiempoPrevioLectura >= intervaloLectura) {
    tiempoPrevioLectura = tiempoActual;
    if (WiFi.status() == WL_CONNECTED) {
      enviarTelemetria();
    } else {
      Serial.println("Aviso: Conexión Wi-Fi perdida.");
    }
  }
}

// --- CONSULTA TABLA 'irrigation_control' Y EJECUTA ACCIONES ---
void consultarComandosSupabase() {
  HTTPClient http;
  String baseUrl = getSupabaseBaseUrl();
  String url = baseUrl + "/rest/v1/irrigation_control?id=eq.1&select=manual_trigger,duration_seconds,test_mode";

  http.begin(url);
  http.addHeader("apikey", supabase_api_key);
  http.addHeader("Authorization", "Bearer " + String(supabase_api_key));

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc.size() > 0) {
      bool manualTrigger = doc[0]["manual_trigger"] | false;
      int durationSeconds = doc[0]["duration_seconds"] | 20;
      modoPruebaActivo = doc[0]["test_mode"] | false;

      // Envía latido para mantener viva la BD y reportar estado Online a la UI
      actualizarLastSeen();

      // PRIORIDAD 1: Orden manual de riego desde la App
      if (manualTrigger && !bombaActiva) {
        Serial.printf(">> Orden de riego manual recibida: %d segundos\n", durationSeconds);
        digitalWrite(pinRele, LOW); // Enciende bomba
        bombaActiva = true;
        tiempoInicioBomba = millis();
        duracionBombaMs = durationSeconds * 1000UL;
      }
      // PRIORIDAD 2: Modo Prueba Cíclico (1 pulso breve de 2s cada 1 min)
      else if (modoPruebaActivo && !bombaActiva) {
        if (millis() - ultimoCicloPrueba >= 60000) {
          ultimoCicloPrueba = millis();
          Serial.println(">> Ejecutando pulso de Modo Prueba");
          digitalWrite(pinRele, LOW);
          bombaActiva = true;
          tiempoInicioBomba = millis();
          duracionBombaMs = 2000; // 2 segundos
        }
      }
    }
  }
  http.end();
}

// --- ACTUALIZA TIMESTAMP 'last_seen' ---
void actualizarLastSeen() {
  HTTPClient http;
  String baseUrl = getSupabaseBaseUrl();
  String url = baseUrl + "/rest/v1/irrigation_control?id=eq.1";

  http.begin(url);
  http.addHeader("apikey", supabase_api_key);
  http.addHeader("Authorization", "Bearer " + String(supabase_api_key));
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"last_seen\":\"now()\"}";
  http.PATCH(payload);
  http.end();
}

// --- RESETEA EL TRIGGER EN SUPABASE ---
void resetearManualTrigger() {
  HTTPClient http;
  String baseUrl = getSupabaseBaseUrl();
  String url = baseUrl + "/rest/v1/irrigation_control?id=eq.1";

  http.begin(url);
  http.addHeader("apikey", supabase_api_key);
  http.addHeader("Authorization", "Bearer " + String(supabase_api_key));
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"manual_trigger\":false}";
  http.PATCH(payload);
  http.end();
}

// --- ENVÍO DE SENSORES (TELEMETRÍA) ---
void enviarTelemetria() {
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

  StaticJsonDocument<200> doc;
  doc["humedad_suelo"] = h_suelo;
  doc["temperatura_aire"] = t;
  doc["humedad_aire"] = h_aire;

  String datosJson;
  serializeJson(doc, datosJson);

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
}