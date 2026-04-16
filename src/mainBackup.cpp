#include <ESP32-TWAI-CAN.hpp>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "formulas.h"

#define CAN_TX_PIN 2
#define CAN_RX_PIN 15
#define SD_CS_PIN 5
#define ledCAN 16
#define ledSD 17

int taxaAmostragem = 50;
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

// Protótipos das funções
void CanReader(void *parameter);
void SDRecorder(void *parameter);

void setup(){
  Serial.begin(115200);

  pinMode(ledCAN, OUTPUT);
  digitalWrite(ledCAN, LOW);
  pinMode(ledSD, OUTPUT);
  digitalWrite(ledSD, LOW);


  // Fila para 20 medições (buffer de segurança)
  dataQueue = xQueueCreate(20, sizeof(Data));

  // Inicializa SD
  if(!SD.begin(SD_CS_PIN)) {
    Serial.println("Erro SD!");
  } else {
    // Se o arquivo não existe, cria com cabeçalho
    if (!SD.exists("/datalog.csv")) {
      File file = SD.open("/datalog.csv", FILE_WRITE);
      if (file) {
        file.println("Timestamp,Modo,RPM,Torque,Volts,Amps,SoC,TempBat,TempMot,TempCtrl");
        file.close();
      }
    }
  }

  // Inicializa CAN
  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  if (!ESP32Can.begin(CAN_SPEED)) {
    Serial.println("Erro CAN!");
    while (1);
  }

  // Task Core 0: Focada apenas em não perder pacotes CAN
  xTaskCreatePinnedToCore(CanReader, "CAN_Task", 4096, NULL, 2, NULL, 0);

  // Task Core 1: Focada em gravação em disco (mais lenta)
  xTaskCreatePinnedToCore(SDRecorder, "TaskSD", 8192, NULL, 1, NULL, 1);
}

void loop(){
  // Auto-deleta o loop para liberar o Core 1 totalmente para a TaskSD
  vTaskDelete(NULL);
}

void CanReader(void *parameter){
  Data sensorLocal = {0};
  twai_message_t rx;

  for(;;){
    if(ESP32Can.readFrame(&rx)) {
      digitalWrite(ledCAN, HIGH);

      if(is_battery_msg(rx.identifier)) { // Dados da Bateria
        sensorLocal.voltage = Voltage(rx.data[0], rx.data[1]);
        sensorLocal.current = Current(rx.data[2], rx.data[3]);
        sensorLocal.tempBat = TempBat(rx.data[4]);
        sensorLocal.soc     = Soc(rx.data[6]);
      }
      else if (is_motor_msg(rx.identifier)) { // Dados do Motor
        sensorLocal.rpm       = RPM(rx.data[0], rx.data[1]);
        sensorLocal.torque    = Torque(rx.data[2], rx.data[3]);
        sensorLocal.tempCtrl  = TempCtrl(rx.data[6]);
        sensorLocal.tempMotor = TempMotor(rx.data[7]);

        strcpy(sensorLocal.mode, Modo(rx.data[5]));

        sensorLocal.timestamp = millis();
        
        // Envia para a fila para ser gravado no Core 1
        xQueueSend(dataQueue, &sensorLocal, 0);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // Respiro para o RTOS
  }
}

void SDRecorder(void *parameter){
  Data recebido;

  for(;;){
    // Espera até que um dado novo chegue na fila
    if (xQueueReceive(dataQueue, &recebido, portMAX_DELAY)) {
      digitalWrite(ledSD, HIGH);

      File file = SD.open("/datalog.csv", FILE_APPEND);
      if (file) {
        // Gravando todos os parâmetros formatados
        file.printf("%lu,%s,%d,%.1f,%.1f,%.1f,%d,%d,%d,%d\n", 
                    recebido.timestamp, recebido.mode, recebido.rpm, 
                    recebido.torque, recebido.voltage, recebido.current,
                    recebido.soc, recebido.tempBat, recebido.tempMotor, recebido.tempCtrl);
        file.close();
      }
      digitalWrite(ledSD, LOW);
      
      // Feedback no Serial para acompanhamento
      Serial.printf("[%lu] Current: %.1f | V: %.1f | Modo: %s\n", 
                    recebido.timestamp, recebido.current, recebido.voltage, recebido.mode);
    }
  }
}