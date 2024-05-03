#include <WiFi.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <RoboCore_Vespa.h>

// Variáveis
struct ServoSetting {
  uint8_t min;
  uint8_t max;
  uint8_t current;
};

AsyncWebServer server(80);
AsyncWebSocket ws("/websocket");

const uint8_t PIN_LED = 15;

VespaMotors motors;
VespaServo servos[4];
ServoSetting servoSettings[4] = { { 0, 180, 180 }, { 0, 180, 90 }, { 0, 180, 90 }, { 20, 170, 90 } };
VespaBattery battery;

uint8_t critic_battery = 0xFF;
const uint32_t BATTERY_UPDATE_INTERVAL = 5000;       // [ms]
const uint32_t DISCONNECTION_UPDATE_INTERVAL = 100;  // [ms]
const uint32_t LED_INTERVAL_HIGH = 1000;             // [ms]
const uint32_t LED_INTERVAL_LOW = 500;               // [ms]
uint32_t battery_update_timeout, timeout_disconnection, timeout_led_battery;
bool enable_vespa_reset = true;

// Contratos de métodos
void handleWebSocketMessage(uint32_t, void *, uint8_t *, size_t);
void handleWebSocketEvent(AsyncWebSocket *, AsyncWebSocketClient *, AwsEventType, void *, uint8_t *, size_t);

void updateBatteryVoltage();
void controlCriticalLED();
void resetVespa();

void setup() {
  // Configura a comunicação serial
  Serial.begin(115200);

  // Configura o LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  // Configura e inicializa os servos
  servos[0].attach(VESPA_SERVO_S1, VESPA_SERVO_PULSE_WIDTH_MIN, VESPA_SERVO_PULSE_WIDTH_MAX);
  servos[1].attach(VESPA_SERVO_S2, VESPA_SERVO_PULSE_WIDTH_MIN, VESPA_SERVO_PULSE_WIDTH_MAX);
  servos[2].attach(VESPA_SERVO_S3, VESPA_SERVO_PULSE_WIDTH_MIN, VESPA_SERVO_PULSE_WIDTH_MAX);
  servos[3].attach(VESPA_SERVO_S4, VESPA_SERVO_PULSE_WIDTH_MIN, VESPA_SERVO_PULSE_WIDTH_MAX);
  for (uint8_t i = 0; i < 4; i++) {
    servos[i].write(servoSettings[i].current);
  }

  // Configura o Access Point (AP) Wi-Fi
  const char *mac = WiFi.macAddress().c_str();
  char ssid[] = "W3PO-xxxxx";
  char *senha = "robowisky";

  // Atualiza o SSID com base no MAC
  memcpy(ssid + 5, mac, 5);
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(ssid, senha)) {
    // Trava a execução
    while (true) {
      digitalWrite(PIN_LED, HIGH);
      delay(100);
      digitalWrite(PIN_LED, LOW);
      delay(100);
    }
  }

  ws.onEvent(handleWebSocketEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  // Verifica se é hora de atualizar a leitura da tensão da bateria
  if (millis() > battery_update_timeout) {
    updateBatteryVoltage();
  }

  // Controla o LED crítico
  controlCriticalLED();

  // Verifica se é necessário resetar os motores
  resetVespa();
}

void handleWebSocketMessage(uint32_t client, void *arg, uint8_t *data, size_t length) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == length && info->opcode == WS_TEXT) {
    data[length] = 0;

    // Verificar se é necessário controlar os motores
    if (strstr(reinterpret_cast<char *>(data), "speedLeft") != nullptr
        && strstr(reinterpret_cast<char *>(data), "speedRight") != nullptr) {

      StaticJsonDocument<JSON_OBJECT_SIZE(2)> json;
      DeserializationError erro = deserializeJson(json, data, length);

      int16_t speedLeft = json["speedLeft"].as<int16_t>();
      int16_t speedRight = json["speedRight"].as<int16_t>();

      motors.turn(speedLeft, speedRight);
    } else {
      motors.stop();
    }

    // Verificar se é necessário controlar os servos
    if (strstr(reinterpret_cast<char *>(data), "servant") != nullptr
        && strstr(reinterpret_cast<char *>(data), "position") != nullptr) {

      StaticJsonDocument<JSON_OBJECT_SIZE(2)> json;
      DeserializationError erro = deserializeJson(json, data, length);

      int16_t servant = json["servant"].as<int16_t>();
      int16_t position = json["position"].as<int16_t>();

      if (servant >= 0
          && servant <= 3
          && position >= servoSettings[servant].min
          && position <= servoSettings[servant].max) {
        servos[servant].write(position);
      }
    }
  }
}

void handleWebSocketEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t length) {

  if (type == WS_EVT_CONNECT) {
    digitalWrite(PIN_LED, HIGH);
    if (ws.count() > 1) {
      ws.close(client->id());
    }
  } else if (type == WS_EVT_DISCONNECT) {
    if (ws.count() == 0) {
      digitalWrite(PIN_LED, LOW);
    }
  } else if (type == WS_EVT_DATA) {
    handleWebSocketMessage(client->id(), arg, data, length);
  }
}

void updateBatteryVoltage() {

  uint32_t batteryVoltage = battery.readVoltage();

  // Verifica se a tensão está crítica e atualiza o LED
  if (batteryVoltage < 7000 && critic_battery == 0xFF) {
    critic_battery = LOW;
    digitalWrite(PIN_LED, critic_battery);
    timeout_led_battery = millis() + LED_INTERVAL_LOW;
  } else if (batteryVoltage >= 7000 && critic_battery < 0xFF) {
    critic_battery = 0xFF;  // reset
    digitalWrite(PIN_LED, ws.count() > 0 ? HIGH : LOW);
  }

  // Envia a leitura da tensão para os clientes conectados
  if (ws.count() > 0) {
    // cria a mensagem
    const int json_size = JSON_OBJECT_SIZE(1);  // objeto JSON com um membro
    StaticJsonDocument<json_size> json;
    json["battery"] = batteryVoltage;
    size_t message_length = measureJson(json);
    char message[message_length + 1];
    serializeJson(json, message, message_length + 1);
    message[message_length] = 0;  // EOS (mostly for debugging)

    // send the message
    ws.textAll(message, message_length);
  }

  battery_update_timeout = millis() + BATTERY_UPDATE_INTERVAL;  // Atualiza o tempo da próxima verificação
}

void controlCriticalLED() {
  if (millis() > timeout_led_battery && critic_battery < 0xFF) {
    if (critic_battery == LOW) {
      critic_battery = HIGH;
      timeout_led_battery = millis() + LED_INTERVAL_HIGH;
    } else {
      critic_battery = LOW;
      timeout_led_battery = millis() + LED_INTERVAL_LOW;
    }
    digitalWrite(PIN_LED, critic_battery);
  }
}

void resetVespa() {
  if (millis() > timeout_disconnection
      && ws.count() == 0
      && enable_vespa_reset) {

    motors.stop();
    for (uint8_t i = 0; i < 4; i++) {
      servos[i].write(servoSettings[i].current);
    }

    enable_vespa_reset = false;  // Reset
  }

  timeout_disconnection = millis() + DISCONNECTION_UPDATE_INTERVAL;  // Atualiza o tempo de desconexão
}
