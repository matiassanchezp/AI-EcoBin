#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

// ---------- CONFIGURA ESTOS DATOS ----------
const char* ssid = "TU_WIFI";
const char* password = "TU_PASSWORD";
const char* serverUrl = "http://TU_IP_AQUI:5000/classify";
const char* fillStatusUrl = "http://TU_IP_AQUI:5000/fill_status";
// ---------------------------------------------

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Sensor 1: detecta objeto entrando
#define TRIG_ENTRADA 13
#define ECHO_ENTRADA 12
const int DISTANCIA_UMBRAL_ENTRADA_CM = 10;

// Sensor 2: nivel de llenado (mirando hacia abajo, dentro de la caneca)
#define TRIG_NIVEL 14
#define ECHO_NIVEL 15
const int DISTANCIA_UMBRAL_LLENO_CM = 8; // ajusta segun la profundidad de tu caneca

// Servo
#define SERVO_PIN 2
const int ANGULO_ORGANICO = 0;
const int ANGULO_INORGANICO = 180;
const int ANGULO_NEUTRO = 90;

Servo compuerta;

unsigned long ultimaMedicionNivel = 0;
const unsigned long INTERVALO_NIVEL_MS = 5000; // revisa nivel cada 5 segundos

long medirDistanciaCM(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duracion = pulseIn(echoPin, HIGH, 30000);
  if (duracion == 0) return -1;
  return duracion * 0.034 / 2;
}

void configurarCamara() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error iniciando la camara: 0x%x\n", err);
  }
}

void conectarWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado! IP: " + WiFi.localIP().toString());
}

void moverCompuerta(String categoria) {
  if (categoria == "organico") {
    compuerta.write(ANGULO_ORGANICO);
  } else if (categoria == "inorganico") {
    compuerta.write(ANGULO_INORGANICO);
  } else {
    return; // desconocido: no mover
  }
  delay(1500); // tiempo para que caiga el objeto
  compuerta.write(ANGULO_NEUTRO);
}

void capturarYEnviar() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Error al capturar imagen");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "image/jpeg");

  int httpResponseCode = http.POST(fb->buf, fb->len);

  if (httpResponseCode > 0) {
    String respuesta = http.getString();
    Serial.println("Respuesta del servidor: " + respuesta);

    if (respuesta.indexOf("\"organico\"") >= 0) {
      moverCompuerta("organico");
    } else if (respuesta.indexOf("\"inorganico\"") >= 0) {
      moverCompuerta("inorganico");
    }
  } else {
    Serial.println("Error enviando imagen: " + String(httpResponseCode));
  }

  http.end();
  esp_camera_fb_return(fb);
}

void enviarNivelLlenado() {
  long distancia = medirDistanciaCM(TRIG_NIVEL, ECHO_NIVEL);
  if (distancia <= 0) return;

  bool lleno = distancia <= DISTANCIA_UMBRAL_LLENO_CM;

  HTTPClient http;
  http.begin(fillStatusUrl);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"distancia_cm\":" + String(distancia) + ",\"lleno\":" + (lleno ? "true" : "false") + "}";
  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    Serial.println("Nivel enviado: " + payload);
  } else {
    Serial.println("Error enviando nivel: " + String(httpResponseCode));
  }

  http.end();
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_ENTRADA, OUTPUT);
  pinMode(ECHO_ENTRADA, INPUT);
  pinMode(TRIG_NIVEL, OUTPUT);
  pinMode(ECHO_NIVEL, INPUT);

  compuerta.attach(SERVO_PIN);
  compuerta.write(ANGULO_NEUTRO);

  configurarCamara();
  conectarWiFi();
}

void loop() {
  long distanciaEntrada = medirDistanciaCM(TRIG_ENTRADA, ECHO_ENTRADA);

  if (distanciaEntrada > 0 && distanciaEntrada <= DISTANCIA_UMBRAL_ENTRADA_CM) {
    Serial.printf("Objeto detectado a %ld cm. Capturando...\n", distanciaEntrada);
    capturarYEnviar();
    delay(3000);
  }

  if (millis() - ultimaMedicionNivel >= INTERVALO_NIVEL_MS) {
    enviarNivelLlenado();
    ultimaMedicionNivel = millis();
  }

  delay(200);
}
