#include <ETH.h>
#include <WiFi.h>

#include "DHT.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "EEPROM.h"
#include <AsyncMqttClient.h>
extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}

//настройки подключение к сети Wifi
const char* ssid = "MikroTik-1EA2D2";
const char* password = "ferrari220";

#define EEPROM_SIZE 4 //количество байтов, к которым хотим получить доступ в EEPROM
#define DHTPIN 14     // контакт, к которому подключается DHT 
#define AIRPIN 27     //контакт датчика подачи воздуха
#define OILHEATPIN 32 // контакт включения подогревателя масла
#define STARTPIN 12  // контакт пуска горелки
#define AIRFLOWPIN 33 // контакт поддува вторичного воздуха
#define DHTTYPE DHT22   // DHT 11
#define ONE_WIRE_BUS 15 //контакт датчика 18б20
#define FLAMESENSORPIN 35 //вход датчика пламени
#define OILLOWSENSOREPIN 34 // вход низкого уровня датчика масла в бачке
#define OILHIGHSENSOREPIN 13 // вход высокого уровня датчика масла в бачке
#define OILPUMPPIN 23 //выход включения насоса масла
#define SPARKLEPIN 21 // выход подключения искры

#define MQTT_HOST IPAddress(212, 92, 170, 246) //адрес сервера MQTT
#define MQTT_PORT 1883 // порт сервера MQTT

// создаем объекты для управления MQTT-клиентом:
//Создаем объект для управления MQTT-клиентом и таймеры, которые понадобятся для повторного подключения к MQTT-брокеру или WiFi-роутеру, если связь вдруг оборвется.
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;


OneWire oneWire(ONE_WIRE_BUS); // Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);
DeviceAddress sensor1 = { 0x28, 0x4D, 0x82, 0x5, 0x5, 0x0, 0x0, 0xDD };
DeviceAddress sensor2 = { 0x28, 0xC1, 0xC6, 0x5, 0x5, 0x0, 0x0, 0xBC };
DeviceAddress sensor3 = { 0x28, 0x4, 0xC2, 0x5, 0x5, 0x0, 0x0, 0xD7 };
DeviceAddress sensor4 = { 0x28, 0x90, 0xC3, 0x5, 0x5, 0x0, 0x0, 0x77 };
DHT dht(DHTPIN, DHTTYPE);

IPAddress ip;

String SW_var = "";
String SW_var_temp = "";
String SW_var_temp_num = "";

// Нам нужно задать период таймера В МИЛЛИСЕКУНДАХ
// дней*(24 часов в сутках)*(60 минут в часе)*(60 секунд в минуте)*(1000 миллисекунд в секунде)
unsigned int period_DHT22 = 60000; // его же используем для обновления IP
unsigned int period_18b20 = 10000;
unsigned int period_flame_sensor = 2000;
unsigned int period_fuel_sensor = 10000;
unsigned long dht22 = 0; //переменные таймеров
unsigned long T18b20 = 0;
unsigned long flame_sensor = 0;
unsigned long fuel_sensor = 0;
byte x = 0; // Флаг состояния системы 0-стоп, 1-работа, 2 - авария
byte x1 = 0; // флаг состояния горелки 0 - не горит, 1 - горит,  2 - запуск, 3 - останов
byte y = 0; // Флаг состояния автоматического режима 0-ручной, 1-автоматический
byte a = 0; // Флаг состояния канала первичного воздуха 0-закрыт, 1-открыт
byte oh = 0; // Флаг состояния канала подогрева масла 0-выключен, 1-включен
byte af = 0; // флаг состояния канала вторичного воздуха 0-выключен, 1-включен
byte st = 0; // флаг состояния выключателя горелки 0-выключен, 1 - включен
byte fs = 0; // переменная состояния канала датчика пламени
byte oil = 0; // флаг состояния насоса подкачки масла 0-выключен, 1-включен
byte bl1 = 0; // флаг состояния датчика уровня масла в бачке 0-пустой, 1- полный 2-средний 3-неисправный
float oil_temp_hi; //температура масла для выключения тена
float oil_temp_low;// температура масла для включения тена
float water_temp_hi; //температура масла для выключения тена
float water_temp_low;// температура масла для включения тена

float temp_sensor = 0;
String var;
byte olsp = 0;
byte ohsp = 0;

//Функция подключения к WiFi
void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
             //  "Подключаемся к WiFi..."
  WiFi.begin(ssid, password);
  IPAddress ip = WiFi.localIP();
}

//Функция подключения к MQTT 
void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
             //  "Подключаемся к MQTT... "
  mqttClient.connect();
}

//Функция переподключения к Wifi и MQTT  при обрыве связи
void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");  //  "Подключились к WiFi"
      Serial.println("IP address: ");  //  "IP-адрес: "
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
                 //  "WiFi-связь потеряна"
      // делаем так, чтобы ESP32
      // не переподключалась к MQTT
      // во время переподключения к WiFi:
      xTimerStop(mqttReconnectTimer, 0);
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

// в этом фрагменте добавляем топики, 
// на которые будет подписываться ESP32:
void onMqttConnect(bool sessionPresent) {
  //Serial.println("Connected to MQTT.");  //  "Подключились по MQTT."
  //Serial.print("Session present: ");  //  "Текущая сессия: "
  //Serial.println(sessionPresent);
  // подписываем ESP32 на топик «esp32/led»:
  uint16_t packetIdSub = mqttClient.subscribe("phone/ALL", 0);
  //Serial.print("Subscribing at QoS 0, packetId: ");
         //  "Подписываемся при QoS 0, ID пакета: "
  //Serial.println(packetIdSub);
} 

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  //Serial.println("Disconnected from MQTT.");
             //  "Отключились от MQTT."
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

/*void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
             //  "Подписка подтверждена."
  Serial.print("  packetId: ");  //  "  ID пакета: "
  Serial.println(packetId);
  Serial.print("  qos: ");  //  "  Уровень качества обслуживания: "
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
            //  "Отписка подтверждена."
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
            //  "Публикация подтверждена."
  Serial.print("  packetId: ");
  Serial.println(packetId);
}*/

// этой функцией управляется то, что происходит
// при получении того или иного сообщения в топиках

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  String messageTemp;
  for (int i = 0; i < len; i++) {
    //Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  // проверяем, получено ли MQTT-сообщение в топике «phone/ALL»:
  if (strcmp(topic, "phone/ALL") == 0) {
    if (messageTemp == "1") {
      All_on();
      Serial.print("page1.bt6.val=0\xFF\xFF\xFF");
      Serial.print("ref page1\xFF\xFF\xFF");
    } else {
      All_off();
      Serial.print("page1.bt6.val=1\xFF\xFF\xFF");
      Serial.print("ref page1\xFF\xFF\xFF");
    }
  }
 
  /* Serial.println("Publish received.");
             //  "Опубликованные данные получены."
  Serial.print("  message: ");  //  "  сообщение: "
  Serial.println(messageTemp);
  Serial.print("  topic: ");  //  "  топик: "
  Serial.println(topic);
  Serial.print("  qos: ");  //  "  уровень обслуживания: "
  Serial.println(properties.qos);
  Serial.print("  dup: ");  //  "  дублирование сообщения: "
  Serial.println(properties.dup);
  Serial.print("  retain: ");  //  "сохраненные сообщения: "
  Serial.println(properties.retain);
  Serial.print("  len: ");  //  "  размер: "
  Serial.println(len);
  Serial.print("  index: ");  //  "  индекс: "
  Serial.println(index);
  Serial.print("  total: ");  //  "  суммарно: "
  Serial.println(total);  */
}

void setup(void) {


  // чтение настроек с флэш-памяти
  EEPROM.begin(EEPROM_SIZE); //инициализация EEPROM с определенным размером
  oil_temp_hi = EEPROM.read(1); // читаем последнее значение из флеш-памяти
  oil_temp_low = EEPROM.read(0); // читаем последнее значение из флеш-памяти
  water_temp_hi = EEPROM.read(2); // читаем последнее значение из флеш-памяти
  water_temp_low = EEPROM.read(3); // читаем последнее значение из флеш-памяти
  dht22 = millis();
  T18b20 = millis();
  flame_sensor = millis();
  fuel_sensor = millis();
  Serial.begin(9600);
  dht.begin();
  pinMode(AIRPIN, OUTPUT);
  pinMode(STARTPIN, OUTPUT);
  pinMode(AIRFLOWPIN, OUTPUT);
  pinMode(OILHEATPIN, OUTPUT);
  pinMode(OILPUMPPIN, OUTPUT);
  pinMode(FLAMESENSORPIN, INPUT);
  pinMode(OILLOWSENSOREPIN, INPUT);
  pinMode(OILHIGHSENSOREPIN, INPUT);
  pinMode(SPARKLEPIN, OUTPUT);
  digitalWrite(SPARKLEPIN, LOW);
  digitalWrite(OILPUMPPIN, LOW);
  digitalWrite(AIRPIN, LOW);
  digitalWrite(AIRFLOWPIN, LOW);
  digitalWrite(STARTPIN, LOW);
  digitalWrite(OILHEATPIN, LOW);
  fs = digitalRead(FLAMESENSORPIN);
  // инициализация дисплея
  obnulenie();

  // настраиваем сеть
  
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
  connectToWifi();
  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  //mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
 // mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  //mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
}


//перевод системы в начальное состояние
void obnulenie() {
  //Лампочки в красный свет
  Serial.print("page1.p1.pic=4\xFF\xFF\xFF");
  Serial.print("page1.p2.pic=4\xFF\xFF\xFF");
  Serial.print("page1.p3.pic=4\xFF\xFF\xFF");
  Serial.print("page1.p4.pic=4\xFF\xFF\xFF");
  Serial.print("page1.p5.pic=4\xFF\xFF\xFF");
  Serial.print("page1.p6.pic=4\xFF\xFF\xFF");
  Serial.print("page1.p7.pic=4\xFF\xFF\xFF");
  Serial.print("page1.p1.pic=4\xFF\xFF\xFF");
  //Тумблеры off
  Serial.print("page1.bt0.val=1\xFF\xFF\xFF");
  Serial.print("page1.bt1.val=1\xFF\xFF\xFF");
  Serial.print("page1.bt2.val=1\xFF\xFF\xFF");
  Serial.print("page1.bt3.val=1\xFF\xFF\xFF");
  Serial.print("page1.bt4.val=1\xFF\xFF\xFF");
  Serial.print("page1.bt5.val=1\xFF\xFF\xFF");
  Serial.print("page1.bt6.val=1\xFF\xFF\xFF");
  //  на дисплей значение по умолчанию
  indikacia("manual", 14);
  indikacia("off", 15);
  indikacia("------", 16);
  // выводим на дисплей значения настроек
  
  String temp1 = String(oil_temp_low, 2);
  String var = String("page2.low.txt=\"") + temp1 + String("\"") + String("\xFF\xFF\xFF");
  Serial.print(var);
  temp1 = String(oil_temp_hi, 2);
  var = String("page2.hi.txt=\"") + temp1 + String("\"") + String("\xFF\xFF\xFF");
  Serial.print(var);
  temp1 = String(water_temp_low, 2);
  var = String("page2.wlow.txt=\"") + temp1 + String("\"") + String("\xFF\xFF\xFF");
  Serial.print(var);
  temp1 = String(water_temp_hi, 2);
  var = String("page2.whi.txt=\"") + temp1 + String("\"") + String("\xFF\xFF\xFF");
  Serial.print(var);
  Serial.print("ref page2\xFF\xFF\xFF");
  
  
  Serial.print("page0.ip.txt=\""); 
  Serial.print(ip);
  Serial.print(String("\"") + String("\xFF\xFF\xFF"));
  Serial.print("ref page0\xFF\xFF\xFF");
  

  // проверяем уровень топлива
  fuellevel();
}

//функция проверки уробня топлива в баке
void fuellevel() {
  olsp = digitalRead(OILLOWSENSOREPIN); // считываем нижнй концевик датчика
  ohsp = digitalRead(OILHIGHSENSOREPIN);// считываем верхний концевик датчика
  if (olsp == 0 && ohsp == 1) { // если верхний показывает перелив, а нижний - дно
    indikacia("fuel error", 16);
    indikacia("------", 23);
    x = 2; // состояние системы в "авария"
    bl1 = 3; // состояние датчика уровня масла "неисправен"
    Serial.print("bt3.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
    Serial.print("page1.p1.pic=4\xFF\xFF\xFF"); // лампочку гасим
    digitalWrite(OILPUMPPIN, LOW); // выключили насос подкачки
    oil = 0;  // выключаем флаг насоса подкачки масла
    obnulenie();
  }
  if (olsp == 0 && ohsp == 0) { // оба показывают дно
    indikacia("low", 23);
    bl1 = 0;
  }
  if (olsp == 1 && ohsp == 0) {
    indikacia("middle", 23);
    bl1 = 2;
  }
  if (olsp == 1 && ohsp == 1) {
    indikacia("hi", 23);
    bl1 = 1;
    if (oil != 0) { // если флаг насоса подкачки масла пока зывает включенны насос
      Serial.print("bt3.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
      Serial.print("page1.p1.pic=4\xFF\xFF\xFF"); // лампочку гасим
      digitalWrite(OILPUMPPIN, LOW); // выключили насос подкачки
      oil = 0;
    }
  }
}

// функция отображения информации в строках состояния
void indikacia(String k, int k1) {

  String stringVar = String(k1);
  String var = String("page0.t") + stringVar + String(".txt=\"") + k + String("\"") + String("\xFF\xFF\xFF");
  Serial.print(var); //индикация на дисплее
  Serial.print("ref page0\xFF\xFF\xFF"); // обновить страницу

}

void zapusk() {
  //1й шаг проверка уровня масла
  if (bl1 == 0 && x != 2) { // если уровень масла минимум и система не в состоянии "Авария"
    if ((millis() - fuel_sensor) >= period_fuel_sensor) {
      fuel_sensor = millis();
      fuellevel();
    }
    if (oil == 0) {
      oil = 1;
      Serial.print("bt3.val=0\xFF\xFF\xFF"); // переводим тумблер "подкачка вкл"
      Serial.print("page1.p1.pic=5\xFF\xFF\xFF"); // лампочку зажигаем
      digitalWrite(OILPUMPPIN, HIGH); // Включили насос подкачки
      indikacia("oilpump", 15);
    }
  }

  //2й шаг проверка температуры масла
  if (bl1 == 1 && temp_sensor < oil_temp_low && x != 2) { //если уровень масла достаточный, температура масла ниже минимальной
    if (oil == 1) { //если вдруг не выключен наcос подкачки масла
      oil = 0;
      Serial.print("bt3.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
      Serial.print("page1.p1.pic=4\xFF\xFF\xFF"); // лампочку гасим
      digitalWrite(OILPUMPPIN, LOW); // Выключили насос подкачки
    }
    if (oh == 0) { // если тен подогрева выключен
      digitalWrite(OILHEATPIN, HIGH); // включаем тэн
      Serial.print("page1.bt1.val=0\xFF\xFF\xFF"); // переводим тумблер "нагрев вкл"
      Serial.print("page1.p4.pic=5\xFF\xFF\xFF"); // лампочку зажигаем
      oh = 1;
      indikacia("oilheat", 15);
    }
  }
  //3й шаг включаем поддув
  if (bl1 == 1 && temp_sensor >= oil_temp_low && x != 2) { // масла достаточно, температура масла выше мин

    if (oh == 1 && temp_sensor >= oil_temp_hi) { //если обогреватель включен при температуре выше максимальной - выключаем тен
      Serial.print("page1.bt1.val=1\xFF\xFF\xFF"); // переводим тумблер "нагрев выкл"
      Serial.print("page1.p4.pic=4\xFF\xFF\xFF"); // лампочку гасим
      digitalWrite(OILHEATPIN, LOW); // выключаем тэн
      oh = 0;
    }

    af = 1;
    digitalWrite(AIRFLOWPIN, HIGH);
    Serial.print("p5.pic=5\xFF\xFF\xFF");
    Serial.print("page1.bt2.val=0\xFF\xFF\xFF");
    indikacia("airflow", 15);
    delay(35000);

    a = 1;
    digitalWrite(AIRPIN, HIGH);
    Serial.print("page1.p3.pic=5\xFF\xFF\xFF");
    Serial.print("page1.bt0.val=0\xFF\xFF\xFF");
    indikacia("airING", 15);
    delay(35000);

    Serial.print("p6.pic=5\xFF\xFF\xFF");
    Serial.print("page1.bt4.val=0\xFF\xFF\xFF");
    digitalWrite(SPARKLEPIN, HIGH);
    indikacia("SPARKLE", 15);
    delay (9500);

    digitalWrite(SPARKLEPIN, LOW);
    Serial.print("p6.pic=4\xFF\xFF\xFF");
    Serial.print("page1.bt4.val=1\xFF\xFF\xFF");
    fs = digitalRead(FLAMESENSORPIN);
    if (fs == 1) {                             // если пламени нет
      indikacia("double SPARKLE", 15);
      delay (9500);
      Serial.print("p6.pic=5\xFF\xFF\xFF");
      Serial.print("page1.bt4.val=0\xFF\xFF\xFF");
      digitalWrite(SPARKLEPIN, HIGH);
      delay (9500);
      digitalWrite(SPARKLEPIN, LOW);
      Serial.print("p6.pic=4\xFF\xFF\xFF");
      Serial.print("page1.bt4.val=1\xFF\xFF\xFF");
    }
    if (fs == 0) {
      indikacia("fire", 15);
      x1 = 1; // флаг горелки в режим "горение"
    }
    else {
      indikacia("not fire", 16);
      indikacia("error", 15);
      x1 = 3;
    }

  }



}

void fireError() {
  indikacia("error fire", 16);
  x = 2; // флаг горелки в режим "авария"
  ostanov();
}

void ostanov() {
  // выключаем клапан воздуха
  a = 0;
  digitalWrite(AIRPIN, LOW);
  Serial.print("page1.p3.pic=4\xFF\xFF\xFF");
  Serial.print("page1.bt0.val=1\xFF\xFF\xFF");

  //выключаем вторичный поддув
  af = 0;
  digitalWrite(AIRFLOWPIN, LOW);
  Serial.print("page1.p5.pic=4\xFF\xFF\xFF");
  Serial.print("page1.bt2.val=1\xFF\xFF\xFF");
  //проверяем потухла или нет
  fs = digitalRead(FLAMESENSORPIN);
  if (fs == 1) {                             // если пламени нет, система остановилась в штатном режиме
    x1 = 0;     // состояние горелки "не горит"
    String var = String("page0.t15.txt=\"") + String("fire off") + String("\"") + String("\xFF\xFF\xFF");
    Serial.print(var); //индикация на дисплее "горение"
    Serial.print("ref page0\xFF\xFF\xFF"); // обновить страницу
    x = 0;    // состояние системы "стоп"
    y = 0;    // режим горелки "ручной"
    obnulenie();

  } else {
    String var = String("page0.t15.txt=\"") + String("error fire off") + String("\"") + String("\xFF\xFF\xFF");
    Serial.print(var); //индикация на дисплее "горение"
    Serial.print("ref page0\xFF\xFF\xFF"); // обновить страницу
    x = 2; // флаг горелки в режим "авария"
  }


  //выключаем подогрев
  if (oh != 0){
  oh = 0;
  Serial.print("page1.bt1.val=1\xFF\xFF\xFF"); // переводим тумблер "нагрев выкл"
  Serial.print("page1.p1.pic=4\xFF\xFF\xFF"); // лампочку гасим
  digitalWrite(OILHEATPIN, LOW); // выключаем тэн
  }
  //выключаем подкачку
  Serial.print("bt3.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
  Serial.print("page1.p1.pic=4\xFF\xFF\xFF"); // лампочку гасим
  digitalWrite(OILPUMPPIN, LOW); // Выключили насос подкачки
  //Выключаем режим "авто"
  Serial.print("bt5.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
  Serial.print("page1.p2.pic=4\xFF\xFF\xFF"); // лампочку гасим
  y = 0;
  //Выключаем систему
  Serial.print("bt6.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
  Serial.print("page1.p7.pic=4\xFF\xFF\xFF"); // лампочку гасим
  x = 0;
}

void All_on(){
       x = 1;
      Serial.print("page1.p7.pic=5\xFF\xFF\xFF");
      String var = String("page0.t15.txt=\"") + String("on") + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page0\xFF\xFF\xFF");
      Serial.print("ref page1\xFF\xFF\xFF");
      var = "1";
      uint16_t packetIdPub2 = mqttClient.publish("esp32/ALL", 1, true, var.c_str());
  }

void All_off(){
        if (x1 == 1) {
        ostanov();
      }
      else {
        x = 0;
        Serial.print("page1.p7.pic=4\xFF\xFF\xFF");
        String var = String("page0.t15.txt=\"") + String("off") + String("\"") + String("\xFF\xFF\xFF");
        Serial.print(var);
        Serial.print("ref page0\xFF\xFF\xFF");
        Serial.print("ref page1\xFF\xFF\xFF");
        var = "0";
        uint16_t packetIdPub2 = mqttClient.publish("esp32/ALL", 1, true, var.c_str());
        obnulenie();
      }
  }
void loop(void) {

  //проверяем данные управление от дисплея
  if ( Serial.available() > 0 ) {
    SW_var = Serial.readStringUntil(0xFF);
    SW_var.remove(0, 1);
    String SW_var_temp = SW_var.substring(0, 3); // Выделяем идентификатор редактируемого поля параметра
    String SW_var_temp_num = SW_var.substring(4); // Выделяем значение параметра
    byte temp_num = SW_var.substring(4).toInt();

    //включение системы?
    if (SW_var.equals("ALL_on")) {
      All_on();
    }
    if (SW_var.equals("ALL_off")) {
     All_off();
    }

    //режим работы автоматический или ручной?
    if (SW_var.equals("AUTO_on") && x == 1 && x1 == 0) { //режим системы вкл, тумблер авто-вкл и горелка не горит
      y = 1; // переводим флаг "автоматический режим"
      Serial.print("page1.p2.pic=5\xFF\xFF\xFF"); // зеленая лампочка
      String var = String("page0.t14.txt=\"") + String("auto") + String("\"") + String("\xFF\xFF\xFF"); // пишем в дисплей строку режима
      Serial.print(var); //индикация на дисплее "автоматический"
      Serial.print("ref page0\xFF\xFF\xFF"); // обновить страницу
      x1 = 2;
    }
    if (SW_var.equals("AUTO_on") && x == 0) {
      Serial.print("page1.bt5.val=1\xFF\xFF\xFF");
    }
    if (SW_var.equals("AUTO_off")) {
      y = 0;
      Serial.print("page1.p2.pic=4\xFF\xFF\xFF");
      String var = String("page0.t14.txt=\"") + String("manual") + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page0\xFF\xFF\xFF"); //отправляем сформированную строку в дисплей
      ostanov();
    }

    // канал подачи воздуха?
    if (SW_var.equals("A_on") && x == 1 && y != 1) {
      a = 1;
      digitalWrite(AIRPIN, HIGH);
      Serial.print("p3.pic=5\xFF\xFF\xFF");
    }
    if (SW_var.equals("A_on") && x == 0) {
      Serial.print("page1.bt0.val=1\xFF\xFF\xFF");
    }
    if (SW_var.equals("A_off") && y != 1) {
      a = 0;
      digitalWrite(AIRPIN, LOW);
      Serial.print("p3.pic=4\xFF\xFF\xFF");
    }

    // канал нагревателя?
    if (SW_var.equals("OilHeat_on") && x == 1 && (temp_sensor < oil_temp_hi) && y != 1) {
      oh = 1;
      digitalWrite(OILHEATPIN, HIGH);
      Serial.print("p4.pic=5\xFF\xFF\xFF");
      Serial.print("page1.bt1.val=0\xFF\xFF\xFF");
    }
    if (SW_var.equals("OilHeat_on") && (x == 0 || (temp_sensor >= oil_temp_low))) {
      Serial.print("page1.bt1.val=1\xFF\xFF\xFF");
    }
    if (SW_var.equals("OilHeat_off") && y != 1) {
      oh = 0;
      digitalWrite(OILHEATPIN, LOW);
      Serial.print("p4.pic=4\xFF\xFF\xFF");
    }
    if (temp_sensor >= oil_temp_hi && oh != 0) {
      oh = 0;
      digitalWrite(OILHEATPIN, LOW);
      Serial.print("p4.pic=4\xFF\xFF\xFF");
      Serial.print("page1.bt1.val=1\xFF\xFF\xFF");
    }

    // канал вторичного поддува?
    if (SW_var.equals("AF_on") && x == 1 && y != 1) {
      af = 1;
      digitalWrite(AIRFLOWPIN, HIGH);
      Serial.print("p5.pic=5\xFF\xFF\xFF");
    }
    if (SW_var.equals("AF_on") && x == 0) {
      Serial.print("page1.bt2.val=1\xFF\xFF\xFF");
    }
    if (SW_var.equals("AF_off")) {
      af = 0;
      digitalWrite(AIRFLOWPIN, LOW);
      Serial.print("p5.pic=4\xFF\xFF\xFF");
    }

    //канал накачки масла?
    if (SW_var.equals("OILPUMP_on") && x == 1 && (bl1 == 0 || bl1 == 2)) {
      oil = 1;
      digitalWrite(OILPUMPPIN, HIGH);
      Serial.print("p1.pic=5\xFF\xFF\xFF");
    }
    if (SW_var.equals("OILPUMP_on") && (x == 0 || bl1 == 1)) {
      Serial.print("page1.bt3.val=1\xFF\xFF\xFF");
    }
    if (SW_var.equals("OILPUMP_off")) {
      oil = 0;
      digitalWrite(OILPUMPPIN, LOW);
      Serial.print("p1.pic=4\xFF\xFF\xFF");
    }

    //Канал искры?
    if (SW_var.equals("START_on") && x == 1 && y != 1) {
      Serial.print("p6.pic=5\xFF\xFF\xFF");
    }
    if (SW_var.equals("START_on") && x == 0) {
      Serial.print("page1.bt4.val=1\xFF\xFF\xFF");
    }
    if (SW_var.equals("START_off") && y != 1) {
      digitalWrite(STARTPIN, HIGH);
      Serial.print("p6.pic=4\xFF\xFF\xFF");
    }


    // изменение параметров горелки с монитора?
    if (SW_var_temp.equals("LOT")) { // Если изменили нижнюю границу диапазона температуры масла
      //обновляем индикацию на дисплее
      String var = String("page2.low.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш
         
      EEPROM.write(0, temp_num);
      EEPROM.commit();
      oil_temp_low = SW_var_temp_num.toInt();
    }

    if (SW_var_temp.equals("HOT")) { // Если изменили верхнюю границу диапазона температуры масла
      //обновляем индикацию на дисплее
      String var = String("page2.hi.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш
         
      EEPROM.write(1, temp_num);
      EEPROM.commit();
      oil_temp_hi = SW_var_temp_num.toInt();
    }

    if (SW_var_temp.equals("WTL")) { // Если изменили нижнюю границу диапазона температуры теплоносителя
      //обновляем индикацию на дисплее
      String var = String("page2.wlow.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш
           
      EEPROM.write(3, temp_num);
      EEPROM.commit();
      water_temp_low = SW_var_temp_num.toInt();
    }

    if (SW_var_temp.equals("WTH")) { // Если изменили верхнюю границу диапазона температуры теплоносителя
      //обновляем индикацию на дисплее
      String var = String("page2.whi.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш

      EEPROM.write(2, temp_num);
      EEPROM.commit();
      water_temp_hi = SW_var_temp_num.toInt();;
    }

  }

  // читаем DHT22
  if ((millis() - dht22) >= period_DHT22) {
    dht22 = millis();
    float h = dht.readHumidity(); // считывание данных о температуре и влажности
    delay(50);
    float t = dht.readTemperature();// считываем температуру в градусах Цельсия:
    delay(50);
    float f = dht.readTemperature(true);// считываем температуру в градусах Фаренгейта:
    delay(50);
    // проверяем, корректно ли прочитались данные,
    // и если нет, то выходим и пробуем снова:
    if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println("Failed to read from DHT sensor!"); // "Не удалось прочитать данные с датчика DHT!"
    } else {
      String var3 = "t1.txt=\"" + String(h) + "\"";
      String var4 = "t8.txt=\"" + String(t) + "\"";
      Serial.print(var3 + "\xFF\xFF\xFF");
      Serial.print(var4 + "\xFF\xFF\xFF");
      Serial.print("ref page0\xFF\xFF\xFF");
    }

    //заодно обновим IP

    IPAddress ip = WiFi.localIP();
    Serial.print("page0.ip.txt=\""); 
  Serial.print(ip);
  Serial.print(String("\"") + String("\xFF\xFF\xFF"));
  Serial.print("ref page0\xFF\xFF\xFF");
  }

  // Читаем датчик 18b20
  if ((millis() - T18b20) >= period_18b20) {
    T18b20 = millis();
    sensors.requestTemperatures(); // Send the command to get temperatures
    temp_sensor = sensors.getTempC(sensor1);
    String var = String(sensors.getTempC(sensor1), 2);
    String var2 = "t0.txt=\"" + var + "C" + "\"";
    Serial.print(var2 + "\xFF\xFF\xFF");
    String var3 = "ref page0";
    Serial.print(var3 + "\xFF\xFF\xFF"); //отправляем сформированную строку в дисплей
    // публикуем MQTT-сообщение в топике «esp32/temperature»
    uint16_t packetIdPub2 = mqttClient.publish("esp32/temperature", 1, true, var.c_str());
  }

  //проверяем датчик пламени, если что-то не так обрабатываем ошибку
  if ((millis() - flame_sensor) >= period_flame_sensor) {
    flame_sensor = millis();
    fs = digitalRead(FLAMESENSORPIN);
    if (x1 == 1 && fs == 1 || x1 == 0 && fs == 0) { //если состояние системы - работа и датчик пламени показывает отсутствие пламени
      fireError(); //ошибка  горение
    }
  }

  // Читаем датчик уровня топлива раз в 5 сек
  if ((millis() - fuel_sensor) >= period_fuel_sensor) {
    fuel_sensor = millis();
    fuellevel();
  }

  //проверка уровня бачка на дозаправку
  if (bl1 == 0 && y == 1 && x == 1) { // если режим системы - работа, уровень масла минимум, а режим при этом автоматический - подкачиваем.
    Serial.print("bt3.val=0\xFF\xFF\xFF"); // переводим тумблер "подкачка вкл"
    Serial.print("page1.p1.pic=5\xFF\xFF\xFF"); // лампочку зажигаем
    digitalWrite(OILPUMPPIN, HIGH); // Включили насос подкачки
    oil = 1;
  }

  //поддержка температуры масла
    if (y == 1 && x == 1 && temp_sensor >= oil_temp_hi && oh != 0) {  // если в автоматическом режиме температура выше максимальной
    Serial.print("page1.bt1.val=1\xFF\xFF\xFF"); // переводим тумблер "нагрев выкл"
    Serial.print("page1.p4.pic=4\xFF\xFF\xFF"); // лампочку гасим
    digitalWrite(OILHEATPIN, LOW); // выключаем тэн
    oh = 0;
  }
  if (bl1 != 0 && y == 1 && x == 1 && temp_sensor < oil_temp_low) { // // если в автоматическом режиме температура ниже минимальной и бак не пустой
    Serial.print("page1.bt1.val=0\xFF\xFF\xFF"); // переводим тумблер "нагрев вкл"
    Serial.print("page1.p4.pic=5\xFF\xFF\xFF"); // лампочку зажигаем
    digitalWrite(OILHEATPIN, HIGH); // включаем тэн
    oh = 1;
  }

  // проверка состояния горелки
  if (x1 == 2) { //проверка состояния горелки, если состояние - запуск
    zapusk();
  }
  if (x1 == 3) { //проверка состояния горелки, если состояние - останов
    ostanov();
  }




}
