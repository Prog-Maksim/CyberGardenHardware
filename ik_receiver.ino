#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "UniversalTelegramBotCustom.h"
#include "ArduinoJson.h"
#include "DHT.h"
#include <Stepper.h>
#include <IRremote.hpp>
#define IR_RECEIVE_PIN 23

#define LAMP_R_PORT 4
#define LAMP_G_PORT 2
#define LAMP_B_PORT 15

bool lampTurned = false;
bool waterTurned = false;
bool ventilationTurned = false;
byte lampR = 0, lampG = 0, lampB = 0;
/* Контакт, к которому подключен датчик */
#define DHTPIN 27
#define LIGHTLEVELPIN 32
#define WATERLEVELPIN 33
/* Тип датчика */
#define DHTTYPE DHT11
/* Создаем объект для работы с датчиком */
DHT dht(DHTPIN, DHTTYPE);

// Данные Wi-Fi
const char* ssid = "cybergarden";
const char* password = "cybergarden";

// Токен бота Telegram
#define BOT_TOKEN "******"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Таймер для проверки сообщений
unsigned long lastTimeBotRan = 0;
const int botRequestDelay = 1000;

//void (*receiveSignals)() = &receiveIr;
const int stepsPerRevolution = 2048;
Stepper myStepper(stepsPerRevolution, 5, 18, 19, 21);

int maxRotorSpeed = 0;
int currentRotorSpeed = 0;
int rotorSavedSpeed = 0;

#define PUMP_PIN 14

bool autoLight = false;
bool autoWater = false;
short turnLight = 0;
short turnWater = 0;

void setup() {
  Serial.begin(115200);
  dht.begin();
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver
  IrReceiver.enableIRIn();
  pinMode(LAMP_R_PORT, OUTPUT);
  pinMode(LAMP_G_PORT, OUTPUT);
  pinMode(LAMP_B_PORT, OUTPUT);

  pinMode(PUMP_PIN, OUTPUT);

  myStepper.setSpeed(currentRotorSpeed);

  if (!lampTurned) {
    analogWrite(LAMP_R_PORT, 0);
    analogWrite(LAMP_G_PORT, 0);
    analogWrite(LAMP_B_PORT, 0);
  }
  startWiFi();
}

void loop() {
  // delay(1000);
  // float t = dht.readTemperature();
  // Serial.print(F("%  Температура: "));
  // Serial.print(t);
  // Serial.print(F("°C "));
  // Serial.print("\n");
  // float h = dht.readHumidity();
  // Serial.print(F("Влажность: "));
  // Serial.print(h);
  // Serial.print(F("%"));
  // Serial.print("\n");
  // int l = analogRead(LIGHTLEVELPIN);
  // Serial.print(F("Свет: "));
  // Serial.println(l);
  // int w = analogRead(WATERLEVELPIN);
  // Serial.print(F("Уровень воды: "));
  // Serial.println(w);


  // if (millis() - lastTimeBotRan > botRequestDelay) {
  //   // Быстрая проверка новых сообщений без длительных операций
  //   int numNewMessages = bot.getUpdates(bot.last_message_received + 1, receiveIr);
    
  //   if (numNewMessages) {
  //     Serial.println("Найдены новые сообщения: " + String(numNewMessages));
      
  //     // Обрабатываем только по одному сообщению за цикл для уменьшения блокировки
  //     for (int i = 0; i < min(numNewMessages, 1); i++) { // Ограничиваем обработку 1 сообщением за цикл
  //       String chat_id = String(bot.messages[i].chat_id);
  //       String text = bot.messages[i].text;
  //       String from_name = bot.messages[i].from_name;
        
  //       Serial.println("Обрабатываем сообщение от: " + from_name);
  //       Serial.println("Текст: " + text);
        
  //       // Быстрая обработка команды
  //       handleCommand(text, chat_id);
        
  //       // После обработки одного сообщения выходим, чтобы не блокировать ИК
  //       break;
  //     }
  //   }
  //   lastTimeBotRan = millis();
  // }

  receiveIr();
  
  // Делаем шаг — полный оборот
  myStepper.step(stepsPerRevolution);

  // Увеличиваем скорость на 1, пока не достигнем maxSpeed
  if (currentRotorSpeed < maxRotorSpeed) {
    currentRotorSpeed++;
    myStepper.setSpeed(currentRotorSpeed);
  } else if (currentRotorSpeed > maxRotorSpeed) {
    currentRotorSpeed = maxRotorSpeed;
    myStepper.setSpeed(currentRotorSpeed);
  }

  if (autoLight) {
    if (analogRead(LIGHTLEVELPIN) >= turnLight) {
      lampTurned = true;
      analogWrite(LAMP_R_PORT, lampR);
      analogWrite(LAMP_G_PORT, lampG);
      analogWrite(LAMP_B_PORT, lampB);
    } else {
      lampTurned = false;
      analogWrite(LAMP_R_PORT, 0);
      analogWrite(LAMP_G_PORT, 0);
      analogWrite(LAMP_B_PORT, 0);
    }
  }

  if (autoWater) {
    if (int w = analogRead(WATERLEVELPIN) >= turnWater) {
      waterTurned = true;
      digitalWrite(PUMP_PIN, waterTurned);
    } else {
      waterTurned = false;
      digitalWrite(PUMP_PIN, waterTurned);
    }
  }

  //Serial.println("i'm alive!");
}

void receiveIr() {
  if (IrReceiver.decode()) {
      Serial.println(IrReceiver.decodedIRData.command); // Print "old" raw data
      Serial.println(IrReceiver.decodedIRData.decodedRawData);
      IrReceiver.printIRResultShort(&Serial); // Print complete received data in one line
      //IrReceiver.printIRSendUsage(&Serial);   // Print the statement required to send this data
      
      if (IrReceiver.decodedIRData.address == 0x11) {
        lampR = IrReceiver.decodedIRData.command;
        if (lampTurned)
          analogWrite(LAMP_R_PORT, lampR);
      } else if (IrReceiver.decodedIRData.address == 0x22) {
        lampG = IrReceiver.decodedIRData.command;
        if (lampTurned)
          analogWrite(LAMP_G_PORT, lampG);
      } else if (IrReceiver.decodedIRData.address == 0x33) {
        lampB = IrReceiver.decodedIRData.command;
        if (lampTurned)
          analogWrite(15, lampB);
      } else if (IrReceiver.decodedIRData.address == 0x44) {
        autoLight = false;
        if (IrReceiver.decodedIRData.command == 0) {
          lampTurned = false;
          analogWrite(LAMP_R_PORT, 0);
          analogWrite(LAMP_G_PORT, 0);
          analogWrite(LAMP_B_PORT, 0);
        } else {
          lampTurned = true;
          analogWrite(LAMP_R_PORT, lampR);
          analogWrite(LAMP_G_PORT, lampG);
          analogWrite(LAMP_B_PORT, lampB);
        }
      } else if (IrReceiver.decodedIRData.address == 0x55) {
        autoLight = true;
        short turnLight = (int)(1023 * IrReceiver.decodedIRData.command);
      } else if (IrReceiver.decodedIRData.address == 0x66) {
        autoWater = false;
        waterTurned = IrReceiver.decodedIRData.command;
        digitalWrite(PUMP_PIN, waterTurned);
      } else if (IrReceiver.decodedIRData.address == 0x77) {
        autoWater = true;
        short turnWater = (int)(1023 * IrReceiver.decodedIRData.command);
      } else if (IrReceiver.decodedIRData.address == 0x88) {
        ventilationTurned = IrReceiver.decodedIRData.command;
        if (ventilationTurned) {
          maxRotorSpeed = rotorSavedSpeed;
        } else {
          rotorSavedSpeed = maxRotorSpeed;
          maxRotorSpeed = 0;
        }
      }

      IrReceiver.resume(); // Enable receiving of the next value
  }
}

float getTemperature() {
  float t = dht.readTemperature();
  Serial.print(F("%  Температура: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print("\n");
  return t;
  //return 20.0 + (random(0, 200) / 10.0);
}

float getHumidity() {
  float h = dht.readHumidity();
  Serial.print(F("Влажность: "));
  Serial.print(h);
  Serial.print(F("%"));
  Serial.print("\n");
  return h;
  //return 40.0 + (random(0, 600) / 10.0);
}

int getLightLevel() {
  //stopWiFi();
  int l = analogRead(LIGHTLEVELPIN);
  Serial.print(F("Свет: "));
  Serial.println(l);
  //startWiFi();
  return l; //- data pin of sensor
}

int getWaterLevel() {
  //stopWiFi();
  int w = analogRead(WATERLEVELPIN);
  Serial.print(F("Уровень воды: "));
  Serial.println(w);
  //startWiFi();
  return w; //- data pin of sensor
}

// Функция для создания обычной клавиатуры
String createReplyKeyboard() {
  String keyboardJson = "[[\"🌡️ Температура\", \"💧 Влажность\"], [\"💡 Освещенность\", \"Заполненность\"], [\"📊 Все данные\"]]";
  return keyboardJson;
}

// void sendWelcomeMessage(String chat_id, String from_name = "") {
//   if (from_name != "") {
//     welcome = "Привет, " + from_name + "!\n" + welcome;
//   }
//   welcome += "Выберите параметр для отображения:";
// }

void handleCommand(String text, String chat_id) {
  if (text == "/start" || text == "start") {
    //sendWelcomeMessage(chat_id);
    bot.sendMessageWithReplyKeyboard(chat_id, "Выберите параметр для отображения:", "", createReplyKeyboard(), true);
  }
  else if (text == "🌡️ Температура") {
    float temperature = getTemperature();
    String message = "🌡️ Температура: " + String(temperature) + "°C";
    bot.sendMessage(chat_id, message, "");
  }
  else if (text == "💧 Влажность") {
    float humidity = getHumidity();
    String message = "💧 Влажность: " + String(humidity) + "%";
    bot.sendMessage(chat_id, message, "");
  }
  else if (text == "💡 Освещенность") {
    int light = getLightLevel();
    String message = "💡 Освещенность: " + String(light) + " лк";
    bot.sendMessage(chat_id, message, "");
  }
  else if (text == "Заполненность") {
    int water = getWaterLevel();
    String message = "💡 Освещенность: " + String(water) + " %";
    bot.sendMessage(chat_id, message, "");
  }
  else if (text == "📊 Все данные") {
    String allData = "📊 Все данные с датчиков:\n\n";
    allData += "🌡️ Температура: " + String(getTemperature()) + "°C\n";
    allData += "💧 Влажность: " + String(getHumidity()) + "%\n";
    allData += "💡 Освещенность: " + String(getLightLevel()) + " лм\n";
    allData += "Заполненность: " + String(getWaterLevel()) + "%";
    bot.sendMessage(chat_id, allData, "");
  }
  else {
    String reply = "Неизвестная команда\n\n";
    reply += "Используйте /start для начала работы";
    bot.sendMessage(chat_id, reply, "");
  }
}

void startWiFi() {
  // // Подключение к Wi-Fi
  Serial.print("Подключение к WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi подключен!");
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());
  
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  Serial.println("Бот запущен! Отправьте /start в Telegram");
}

void stopWiFi() {
  WiFi.disconnect(true);  // Отключиться от сети
  WiFi.mode(WIFI_OFF);    // Выключить WiFi полностью
  Serial.println("WiFi остановлен");
}
