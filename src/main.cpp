#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32-TWAI-CAN.hpp>

#define CAN_TX_PIN 2
#define CAN_RX_PIN 15
#define ledCAN 16
#define ledMQTT 17 // Renomeado do antigo ledSD

// CONEXAO WIFI
const char* ssid = "ESP32";
const char* password = "acesso123";
const char* mqtt_server = "10.45.99.154";

const TwaiSpeed CAN_SPEED = TWAI_SPEED_250KBPS;

struct Data {
  float voltage;
  float current;
  int soc;
  int tempBat;
  int rpm;
  float torque;
  int tempCtrl;
  int tempMotor;
  char mode[10];
  unsigned long timestamp;
};

QueueHandle_t dataQueue;
WiFiClient espClient;
PubSubClient client(espClient);

// Protótipos das funções
void reconnect();
void setupWifi();
void CanReader(void *parameter);
void MQTTTask(void *parameter);

void setup(){
  Serial.begin(115200);

  pinMode(ledCAN, OUTPUT);
  pinMode(ledMQTT, OUTPUT);
  digitalWrite(ledCAN, LOW);
  digitalWrite(ledMQTT, LOW);

  // Fila para 20 medições
  dataQueue = xQueueCreate(20, sizeof(Data));

  // 1. Inicializa Wi-Fi
  setupWifi();

  // 2. Configura Broker MQTT
  client.setServer(mqtt_server, 1883);

  // 3. Inicializa CAN
  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  if (!ESP32Can.begin(CAN_SPEED)) {
    Serial.println("Erro CAN!");
    while (1);
  }

  // Task Core 0: CAN (Prioridade Alta)
  xTaskCreatePinnedToCore(CanReader, "CAN_Task", 4096, NULL, 2, NULL, 0);

  // Task Core 1: MQTT (Prioridade Normal)
  xTaskCreatePinnedToCore(MQTTTask, "MQTT_Task", 8192, NULL, 1, NULL, 1);
}

void loop(){
  vTaskDelete(NULL);
}

void CanReader(void *parameter){
  Data sensorLocal = {0};
  CanFrame rx;

  for(;;){
    if(ESP32Can.readFrame(rx, 0)) { 
      digitalWrite(ledCAN, !digitalRead(ledCAN)); 

      if(rx.identifier == 0x121){ 
        sensorLocal.voltage = ((uint16_t)rx.data[0] << 8 | rx.data[1]) * 0.1f;
        sensorLocal.current = ((int16_t)(rx.data[2] << 8 | rx.data[3])) * 0.1f;
        sensorLocal.tempBat = (int8_t)rx.data[4];
        sensorLocal.soc     = rx.data[6];
      }
      else if (rx.identifier == 0x300) { 
        sensorLocal.rpm    = (rx.data[0] << 8) | rx.data[1];
        sensorLocal.torque = ((int16_t)(rx.data[2] << 8 | rx.data[3])) * 0.1f;
        sensorLocal.tempCtrl  = rx.data[6] - 40;
        sensorLocal.tempMotor = rx.data[7] * 2;
        
        if (rx.data[5] == 0x45) strcpy(sensorLocal.mode, "ECO");
        else if (rx.data[5] == 0x4D) strcpy(sensorLocal.mode, "STD");
        else if (rx.data[5] == 0x55) strcpy(sensorLocal.mode, "TURBO");
        else strcpy(sensorLocal.mode, "N/A");

        sensorLocal.timestamp = millis();
        xQueueSend(dataQueue, &sensorLocal, 0);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void MQTTTask(void *parameter){
  Data info;
  char payload[200];

  for(;;){
    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    if (xQueueReceive(dataQueue, &info, portMAX_DELAY)) {
      digitalWrite(ledMQTT, HIGH);

      // JSON completo com todos os seus campos
      snprintf(payload, sizeof(payload), 
        "{\"ts\":%lu,\"v\":%.1f,\"a\":%.1f,\"soc\":%d,\"rpm\":%d,\"tq\":%.1f,\"mod\":\"%s\",\"tB\":%d,\"tM\":%d,\"tC\":%d}",
        info.timestamp, info.voltage, info.current, info.soc, info.rpm, 
        info.torque, info.mode, info.tempBat, info.tempMotor, info.tempCtrl);

      if(client.publish("moto/telemetria", payload)) {
        Serial.print("MQTT Sent: "); Serial.println(payload);
      } else {
        Serial.println("Falha ao enviar MQTT");
      }
      
      digitalWrite(ledMQTT, LOW);
    }
    vTaskDelay(pdMS_TO_TICKS(10)); //folga para o Core 1
  }
}

void setupWifi(){
    delay(10);
    Serial.print("\nConectando ao WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Conectado!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    // Tenta conectar com ID único
    if (client.connect("ESP32_Moto_Client")) {
      Serial.println("conectado!");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s");
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}