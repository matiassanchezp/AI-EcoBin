#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "TU_WIFI";
const char* password = "TU_PASSWORD";
const char* serverUrl = "http://TU_IP_O_DOMINIO:5000/classify";

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

#define TRIG_PIN 13
#define ECHO_PIN 12
const int DISTANCIA_UMBRAL_CM = 10;

long medirDistanciaCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
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
  } else {
    Serial.println("Error enviando imagen: " + String(httpResponseCode));
  }
  http.end();
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  configurarCamara();
  conectarWiFi();
}

void loop() {
  long distancia = medirDistanciaCM();
  if (distancia > 0 && distancia <= DISTANCIA_UMBRAL_CM) {
    Serial.printf("Objeto detectado a %ld cm. Capturando...\n", distancia);
    capturarYEnviar();
    delay(3000);
  }
  delay(200);
}
