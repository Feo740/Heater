/**@file heater.ino */
#include <driver/adc.h>
#include <ETH.h>
#include <WiFi.h>
#include "DHT.h"
#include "OneWire.h"
//#include "DallasTemperature.h"
#include "EEPROM.h"
#include <AsyncMqttClient.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <Adafruit_ADS1X15.h>
#include <Wire.h>

Adafruit_ADS1115 ads; ///< переменная для I2C модуля АЦП
int16_t adc0, adc1, adc2, adc3;
float volts0, volts1, volts2, volts3;

//работа с именем сети и паролем
char my_buffer[41]; ///< Массив для хранения символов из файлов конфигураций
char pass_buffer[25]; ///< Массив для хранения символов пароля сети
char net_buffer[25]; ///< Массив для хранения символов имени сети
char oil_buffer[41]; ///< Массив для хранения символов из файлов конфигураций
char in_water_buffer[41]; ///< Массив для хранения символов из файлов конфигураций
char out_water_buffer[41]; ///< Массив для хранения символов из файлов конфигураций
char air_buffer[41]; ///< Массив для хранения символов из файлов конфигураций

 char* ssid = &net_buffer[0];
 char* password = &pass_buffer[0];
 char* oil_number_18b20 = &oil_buffer[0];
 char* air_number_18b20 = &air_buffer[0];


#define EEPROM_SIZE 10 ///<количество байтов, к которым хотим получить доступ в EEPROM
#define DHTPIN 14     ///< контакт, к которому подключается DHT
#define AIRPIN 27     ///< контакт датчика подачи воздуха
#define OILHEATPIN 32 ///< контакт включения подогревателя масла
#define STARTPIN 12  ///<  контакт пуска горелки ЗАМЕНИТЬ
#define AIRFLOWPIN 25 ///< контакт поддува вторичного воздуха
#define DHTTYPE DHT22   ///<  DHT 11
//#define ONE_WIRE_BUS 15 //контакт датчика 18б20
#define FLAMESENSORPIN 35 ///< контакт входа датчика пламени
#define OILLOWSENSOREPIN 34 ///< контакт входа низкого уровня датчика масла в бачке
#define OILHIGHSENSOREPIN 13 ///< контакт входа высокого уровня датчика масла в бачке
#define OILPUMPPIN 22 ///< контакт выхода включения насоса масла
#define SPARKLEPIN 21 ///< контакт выхода подключения генератора искры


#define MQTT_HOST IPAddress(212, 92, 170, 246) ///< адрес сервера MQTT
#define MQTT_PORT 1883 ///< порт сервера MQTT

#define I2C_SDA 26 ///< контакт sda линии порта I2C
#define I2C_SCL 33 ///< контакт scl линии порта I2C
#define TEMPSENSORPIN 15 ///< контакт для подключения датчика температуры
OneWire ds(TEMPSENSORPIN);




/// создаем объекты для управления MQTT-клиентом:
///Создаем объект для управления MQTT-клиентом и таймеры, которые понадобятся для повторного подключения к MQTT-брокеру или WiFi-роутеру, если связь вдруг оборвется.
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;


byte t[8] = { 0, 0, 0, 0, 0, 0, 0, 0};
byte addr_oil_temp[8]; // финальный результат в массиве типа byte
byte addr_in_water_temp[8]; // финальный результат в массиве типа byte
byte addr_out_water_temp[8]; // финальный результат в массиве типа byte
byte addr_air_temp[8]; // финальный результат в массиве типа byte
byte addr1[8];
byte oil_temp_flag; // флаг наличия адреса в файле инициализации датчиков.
byte in_water_temp_flag; // флаг наличия адреса в файле инициализации датчиков.
byte out_water_temp_flag; // флаг наличия адреса в файле инициализации датчиков.
byte air_temp_flag; // флаг наличия адреса в файле инициализации датчиков.

String temp_number_reading = "!"; //текстовая переменная для хранения прочитанных адресов новых датчиков
/*
OneWire oneWire(ONE_WIRE_BUS); // Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);
DeviceAddress sensor1 = { 0x28, 0x4D, 0x82, 0x5, 0x5, 0x0, 0x0, 0xDD };// датчик температуры масла
DeviceAddress sensor2 = { 0x28, 0xC1, 0xC6, 0x5, 0x5, 0x0, 0x0, 0xBC };//датчик температуры подачи
DeviceAddress sensor3 = { 0x28, 0x4, 0xC2, 0x5, 0x5, 0x0, 0x0, 0xD7 };//датчик температуры обратки
DeviceAddress sensor4 = { 0x28, 0x90, 0xC3, 0x5, 0x5, 0x0, 0x0, 0x77 };//датчик температуры резервный
*/
DHT dht(DHTPIN, DHTTYPE);

IPAddress ip;

String SW_var = "";
String SW_var_temp = "";
String SW_var_temp_num = "";

// Нам нужно задать период таймера В МИЛЛИСЕКУНДАХ
// дней*(24 часов в сутках)*(60 минут в часе)*(60 секунд в минуте)*(1000 миллисекунд в секунде)
unsigned int period_DHT22 = 60000;  ///< таймер для датчика влажности
unsigned int period_18b20_1 = 30000;  ///< таймер для первого датчика температуры
unsigned int period_18b20_2 = 25000;  ///< таймер для второго датчика температуры
unsigned int period_18b20_3 = 20000;  ///< таймер для третьего датчика температуры
unsigned int period_18b20_4 = 23000;  ///< таймер для четвертого датчика температуры
unsigned int period_flame_sensor = 2000;  ///< таймер для датчика пламени
unsigned int period_fuel_sensor = 10000;  ///< таймер для датчика уровня топлива
unsigned int period_blink1 = 2000;  ///< таймер для светодиода - пинг
unsigned int period_fuel_tank = 0;  ///< таймер для наполнения бака по времени
unsigned int period_air_before = 5000;  ///< таймер для продувки печи перед стартом
unsigned int period_air_after = 5000; ///< таймер для продувки печи при останове
unsigned int period_air_ing = 5000; ///< таймер для диапазона меж продувкой и подачей искры
unsigned int period_sparkle_ing = 2000; ///< таймер продолжительности подачи искры
unsigned int period_between_sparkle_ing = 2000; ///< таймер периода между подачами искры
unsigned int period_18b20_read = 500; ///< таймер ожидания преобразования в датчике 18b20

//переменные счетчиков
unsigned long dht22 = 0; ///< Техническая переменная счетчика таймера
unsigned long blink1; ///< Техническая переменная счетчика таймера
unsigned long T18b20_1 = 0; ///< Техническая переменная счетчика таймера
unsigned long T18b20_2 = 0; ///< Техническая переменная счетчика таймера
unsigned long T18b20_3 = 0; ///< Техническая переменная счетчика таймера
unsigned long T18b20_4 = 0; ///< Техническая переменная счетчика таймера
unsigned long flame_sensor = 0; ///< Техническая переменная счетчика таймера
unsigned long fuel_sensor = 0;  ///< Техническая переменная счетчика таймера
unsigned long fuel_tank_var = 0;  ///< Техническая переменная счетчика таймера
unsigned long air_before = 0; ///< Техническая переменная счетчика таймера
unsigned long air_after = 0;  ///< Техническая переменная счетчика таймера
unsigned long air_ing = 0;  ///< Техническая переменная счетчика таймера
unsigned long sparkle_ing = 0;  ///< Техническая переменная счетчика таймера
unsigned long between_sparkle_ing = 0;  ///< Техническая переменная счетчика таймера
unsigned long read_18b20 = 0; ///< Техническая переменная счетчика таймера

byte x = 0; ///< Флаг состояния системы 0-стоп, 1-работа, 2 - авария
byte x1 = 0;  ///< флаг состояния горелки 0 - не горит, 1 - горит,  2 - запуск, 3 - останов
byte y = 0; ///< Флаг состояния автоматического режима 0-ручной, 1-автоматический
byte a = 0; ///< Флаг состояния канала первичного воздуха 0-закрыт, 1-открыт
byte oh = 0; ///< Флаг состояния канала подогрева масла 0-выключен, 1-включен
byte af = 0; ///< флаг состояния канала вторичного воздуха 0-выключен, 1-включен
byte st = 0; ///< флаг состояния выключателя горелки 0-выключен, 1 - включен
byte fs1 = 0; ///< переменная состояния канала датчика пламени
byte oil = 0; ///< флаг состояния насоса подкачки масла 0-выключен, 1-включен
byte bl1 = 0; ///< флаг состояния датчика уровня масла в бачке 0-пустой, 1- полный 2-средний 3-неисправный
byte sp = 0; ///< флаг состояния подачи искры
byte sparkle_item = 0; ///< счетчик еоличества попыток запуска горелки
byte var_blink1 = 0; ///< флаг для мигания светодиодом
byte error_flag = 0; ///< переменная кода ошибки 0-нет, 1-ошибка наполнения топливом


float fuel_tank = 0; ///< переменная хранения значения времени заправки бака масла для eeprom
float oil_temp_hi; ///< переменная хранения значения максимальной температуры масла для выключения тена
float oil_temp_low; ///< переменная хранения значения минимальной температуры масла для выключения тена
float water_temp_hi;  ///< переменная хранения значения максимальной температуры антифриза в системе
float water_temp_low; ///< переменная хранения значения минимальной температуры антифриза в системе

int mVperAmp = 100; ///< use 100 for 20A Module and 66 for 30A Module
double Voltage = 0;
double VRMS = 0;
double AmpsRMS = 0;

String fuel_tank_txt; ///< длительность периода наполения бака масла используется для MQTT сообщений
String period_air_before_txt; ///< длительность периода продувки котла перед стартом используется для MQTT сообщений
String period_air_after_txt; ///< длительность периода продувки котла после останова используется для MQTT сообщений
String period_air_ing_txt; ///< длительность периода первичного впрыска топлива перед стартом используется для MQTT сообщений
String period_between_sparkle_ing_txt; ///< длительность периода между искрами используется для MQTT сообщений
String period_sparkle_ing_txt; ///< длительность периода искры используется для MQTT сообщений
String oil_temp_hi_txt; ///< значение температуры масла для выключения тена используется для MQTT сообщений
String oil_temp_low_txt;  ///< значение температуры масла для включения тена используется для MQTT сообщений
String water_temp_hi_txt; ///< значение температуры масла для выключения тена используется для MQTT сообщений
String water_temp_low_txt;  ///< значение температуры масла для включения тена используется для MQTT сообщений
float temp_sensor = 0;
String var;
byte olsp = 0; ///< флаг датчика топлива нижний порог
byte ohsp = 0; ///< флаг датчика топлива верхний порог
byte sd_con = 0; ///< флаг подключенной флешки


/*!
 \brief функция вывода листа папок с флешки
  Делаем вывод списка папок на флешке
 */
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

/*!
 \brief функция чтения файлов конфигураций
  читаем файл конигураций
 */
void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    int i = 0;
    while(file.available()){
        my_buffer[i] = file.read();
        i++;
                  }
        file.close();
}

/*!
 \brief функция записи в файл конфигураций
  пишем в файл конигураций
 */
void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}



/*!
 \brief функция подключения к сети wifi
  осуществляет подключение к сети.
 */
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
      Serial.printf("SSID=");
      Serial.println(ssid);
      Serial.printf("PASS=");
      Serial.println(password);
      xTimerStop(mqttReconnectTimer, 0);
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

// в этом фрагменте добавляем топики,
// на которые будет подписываться ESP32:
void onMqttConnect(bool sessionPresent) {
  // подписываем ESP32 на топики «phone/ALL», "phone/AUTO":
  uint16_t packetIdSub = mqttClient.subscribe("phone/ALL", 0);
  uint16_t packetIdSub1 = mqttClient.subscribe("phone/AUTO", 0);
  uint16_t packetIdSub2 = mqttClient.subscribe("phone/fuel", 0);

}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
             //  "Отключились от MQTT."
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
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

/*void onMqttPublish(uint16_t packetId) {
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
    if (messageTemp == "1" && x != 2) {
      All_on();
      Serial.print("page1.bt6.val=0\xFF\xFF\xFF");
      //Serial.print("ref page1\xFF\xFF\xFF");
    } else {
      All_off();
      Serial.print("page1.bt6.val=1\xFF\xFF\xFF");
    //  Serial.print("ref page1\xFF\xFF\xFF");
    }
  }

  if (strcmp(topic, "phone/AUTO") == 0) {
    if (messageTemp == "1") {
      AUTO_on();
      Serial.print("page1.bt5.val=0\xFF\xFF\xFF");
      } else {
      Serial.print("page1.p2.pic=4\xFF\xFF\xFF");
      String var = String("page0.t14.txt=\"") + String("manual") + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("page1.bt5.val=1\xFF\xFF\xFF");
      ostanov();
    }
  }
  if (strcmp(topic, "phone/fuel") == 0) {
    if (messageTemp == "1") {
      fuel_obrabotka_on();
    }else {
      fuel_obrabotka_off();
    }
  }

}

  void fuel_obrabotka_on(){
    if(x == 1 && (bl1 == 0 || bl1 == 2)){
    oil = 1;
    fuel_tank_var = millis(); // запускаем таймер предохранительный
    var = "1";
    uint16_t packetIdPub2 = mqttClient.publish("esp32/oil", 1, true, var.c_str());
    digitalWrite(OILPUMPPIN, HIGH);
    Serial.print("p1.pic=5\xFF\xFF\xFF");
    Serial.print("bt3.val=0\xFF\xFF\xFF"); // переводим тумблер "подкачка вкл"
    indikacia("oilpump", 15);
    }
    if((x == 0 || bl1 == 1)){
    fuel_obrabotka_off();
    }
  }

  void fuel_obrabotka_off(){
    oil = 0; // выключаем флаг насоса подкачки масла
    var = "0";
    uint16_t packetIdPub2 = mqttClient.publish("esp32/oil", 1, true, var.c_str()); // синхронизация с НА
    digitalWrite(OILPUMPPIN, LOW); // выключили насос подкачки
    Serial.print("p1.pic=4\xFF\xFF\xFF"); // лампочку гасим
    Serial.print("bt3.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
  }
// функция проверяет подключена ли sd-карточка
void SD_connect (){
  //Работа с флешкартой
  if(!SD.begin()){
          Serial.println("Card Mount Failed");
          sd_con = 0;
          return;
      } else {
        sd_con = 1;
      }
      uint8_t cardType = SD.cardType();

      if(cardType == CARD_NONE){
          Serial.println("No SD card attached");
          sd_con = 0;
          return;
      }

      Serial.print("SD Card Type: ");
      if(cardType == CARD_MMC){
          Serial.println("MMC");
      } else if(cardType == CARD_SD){
          Serial.println("SDSC");
      } else if(cardType == CARD_SDHC){
          Serial.println("SDHC");
      } else {
          Serial.println("UNKNOWN");
      }

      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

/*!
 \brief функция первичной конфигурации системы
  \details Функция выполняется один раз и определяет общую стартовую конфигурацию системы при запуске.

  включаем интерфейс I2C_SC
  \snippet heater.ino 1
  инициализация EEPROM с определенным размером
  \snippet heater.ino 2
 */
void setup(void) {

//![1]
Wire.begin(I2C_SDA, I2C_SCL);
//![1]

ads.begin(); //работа с ацп модулем

  // чтение настроек с флэш-памяти
  //![2]
  EEPROM.begin(EEPROM_SIZE); // инициализация EEPROM с определенным размером
//![2]
  oil_temp_hi = EEPROM.read(1); // читаем последнее значение из флеш-памяти
  oil_temp_low = EEPROM.read(0); // читаем последнее значение из флеш-памяти
  water_temp_hi = EEPROM.read(2); // читаем последнее значение из флеш-памяти
  water_temp_low = EEPROM.read(3); // читаем последнее значение из флеш-памяти
  fuel_tank = EEPROM.read (4); // читаем данные из памяти - время дозаправки, если не сработает поплавок
  period_air_before = 1000 * int(EEPROM.read (5)); // Читаем данные из памяти - время продувки печи при старте
  period_air_after = 1000 * int(EEPROM.read (6)); // Читаем данные из памяти - время продувки печи при останове
  period_air_ing = 1000 * int(EEPROM.read (7)); // Читаем данные из памяти - время от впрыска воздуха до искры
  period_sparkle_ing = 1000 * int(EEPROM.read (8)); // Читаем данные из памяти - время поддержки искры
  period_between_sparkle_ing = 1000 * int(EEPROM.read (9)); // Читаем данные из памяти - время между подачей искры
  period_fuel_tank = fuel_tank*1000;
  dht22 = millis();
  T18b20_1 = millis();
  T18b20_2 = millis();
  T18b20_3 = millis();
  T18b20_4 = millis();
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
  fs1 = digitalRead(FLAMESENSORPIN);
  // читаем флешку если она есть
  SD_connect ();
    if (sd_con == 1){

    for (int i=0; i<25; i++) {
      net_buffer[i] = 0;
      my_buffer[i] = 0;
      pass_buffer[i] = 0;
    }

    readFile(SD, "/network_name.txt");
    for (int i=0; i<25; i++) {
      net_buffer[i] = my_buffer[i];
      my_buffer[i] = 0;
    }

    readFile(SD, "/network_password.txt");
    for (int i=0; i<25; i++) {
      pass_buffer[i] = my_buffer[i];
      my_buffer[i] = 0;
    }

    readFile(SD, "/oil_number.txt");
    for (int i=0; i<41; i++) {
      oil_buffer[i] = my_buffer[i];
      my_buffer[i] = 0;
    }

    if (oil_buffer[0] != '!'){  //если адрес не начинается с ключевого символа воскл знака, то адреса нет.
      oil_temp_flag = 0; // Флаг - нет датчика на линии.
    }
    if (oil_buffer[0] == '!'){
      oil_temp_flag = 1; // Флаг - нет датчика на линии.
      }
    number_obrabotka (oil_buffer);
    for (int i=0; i<8; i++){
       addr_oil_temp[i]=t[i];
       t[i]=0;
    }

    readFile(SD, "/air_number.txt");
    for (int i=0; i<41; i++) {
      air_buffer[i] = my_buffer[i];
      my_buffer[i] = 0;
    }
    if (air_buffer[0] != '!'){  //если адрес не начинается с ключевого символа воскл знака, то адреса нет.
      air_temp_flag = 0; // Флаг - нет датчика на линии.
          }
    if (air_buffer[0] == '!'){
      air_temp_flag = 1; // Флаг - есть датчик на линии.
          }
      number_obrabotka (air_buffer);
      for (int i=0; i<8; i++){
         addr_air_temp[i]=t[i];
         t[i]=0;
      }


  /*  readFile(SD, "/in_water_number.txt");
    for (int i=0; i<40; i++) {
      in_water_buffer[i] = my_buffer[i];
      my_buffer[i] = 0;
    }
    if (in_water_buffer[0] != '!'){  //если адрес не начинается с ключевого символа воскл знака, то адреса нет.
      in_water_temp_flag = 0; // Флаг - нет датчика на линии.
    }
    if (in_water_buffer[0] == '!'){
      in_water_temp_flag = 1; // Флаг - нет датчика на линии.
      number_obrabotka (in_water_buffer);
    }

    readFile(SD, "/out_water_number.txt");
    for (int i=0; i<40; i++) {
      out_water_buffer[i] = my_buffer[i];
      my_buffer[i] = 0;
    }
    if (out_water_buffer[0] != '!'){  //если адрес не начинается с ключевого символа воскл знака, то адреса нет.
      out_water_temp_flag = 0; // Флаг - нет датчика на линии.
    }
    if (out_water_buffer[0] == '!'){
      out_water_temp_flag = 1; // Флаг - нет датчика на линии.
      number_obrabotka (out_water_buffer);
    }*/



    Serial.printf("SSID=");
    Serial.println(ssid);
    Serial.printf("PASS=");
    Serial.println(password);
    /*Serial.printf("air_number=");
    Serial.println(air_number_18b20);*/
    Serial.printf("oil_number=");
    Serial.println(oil_number_18b20);

  }
  else {
    Serial.println("error SD connect");
  }


  // инициализация дисплея
  obnulenie();


  // настраиваем сеть
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
  connectToWifi();
  WiFi.onEvent(WiFiEvent);


  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  //mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  IPAddress ip = WiFi.localIP();
  Serial.print("page0.ip.txt=\"");
  Serial.print(ip);
  Serial.print(String("\"") + String("\xFF\xFF\xFF"));
}


/*!
 \brief функция инициализации экрана в стартовое состояние
  \details Функция выполняется один раз при старте и обеспечивает соответствие
  отображения информации на экране сотстоянию системы
   */
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
  temp1 = String(fuel_tank, 2);
  var = String("page2.ftl.txt=\"") + temp1 + String("\"") + String("\xFF\xFF\xFF");
  Serial.print(var);
  Serial.print("ref page2\xFF\xFF\xFF");

/*
  Serial.print("page0.ip.txt=\"");
  Serial.print(ip);
  Serial.print(String("\"") + String("\xFF\xFF\xFF"));
  //Serial.print("ref page0\xFF\xFF\xFF"); */
  x = 0;
  y = 0;
  x1 = 0;
  oil = 0;
  oh = 0;
  af = 0;
  a = 0;
  indikacia("_________", 16);
  // проверяем уровень топлива
  fuellevel();
}

/*!
 \brief функция проверки уровня топлива в баке.
 Производит опрос датчиков топлива, выводит состояние датчиков на экран.

   */
void fuellevel() {
  olsp = digitalRead(OILLOWSENSOREPIN); // считываем нижнй концевик датчика
  ohsp = digitalRead(OILHIGHSENSOREPIN);// считываем верхний концевик датчика
  if (olsp == 0 && ohsp == 1) { // если верхний показывает перелив, а нижний - дно
    indikacia("fuel error", 16);
    indikacia("------", 23);
    x = 2; // состояние системы в "авария"
    bl1 = 3; // состояние датчика уровня масла "неисправен"
    if (oil ==1){
      fuel_obrabotka_off();
    }
  }
  if (olsp == 0 && ohsp == 0) { // оба показывают дно
    indikacia("low", 23);
    bl1 = 0;
    String var = "0";
    uint16_t packetIdPub2 = mqttClient.publish("esp32/fuel", 1, true, var.c_str());
    }
  if (olsp == 1 && ohsp == 0) {
    indikacia("middle", 23);
    bl1 = 2;
    var = "50";
    uint16_t packetIdPub2 = mqttClient.publish("esp32/fuel", 1, true, var.c_str());
  }
  if (olsp == 1 && ohsp == 1) {
    indikacia("hi", 23);
    bl1 = 1;
    var = "100";
    uint16_t packetIdPub2 = mqttClient.publish("esp32/fuel", 1, true, var.c_str());
    if (oil != 0) { // если флаг насоса подкачки масла пока зывает включенны насос
      Serial.print("bt3.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
      Serial.print("page1.p1.pic=4\xFF\xFF\xFF"); // лампочку гасим
      digitalWrite(OILPUMPPIN, LOW); // выключили насос подкачки
      oil = 0;
    }
  }
}

/*!
 \brief функция отображения информации в строках состояния на экране
 \param [in] k  в функцию передаем содержимое строки для отображения
 \param [in] k1 в функцию передаем адрес текстовой ячейки в мониторе
 \details Передаем содержимое строки и адрес ячейки, фунцкия преобразует информацию в понятный для монитора формат и передает.
    */
void indikacia(String k, int k1) {

  String stringVar = String(k1);
  String var = String("page0.t") + stringVar + String(".txt=\"") + k + String("\"") + String("\xFF\xFF\xFF");
  Serial.print(var); //индикация на дисплее
  //Serial.print("ref page0\xFF\xFF\xFF"); // обновить страницу

}

/*!
 \brief реализация процедуры запуска горелки
  \details запуска горелки включает в себя процедуры проверки состояния горелки. выполнение подготовки к старту.
  Проверяется уровень топлива, если требуется, доливается. Проверяется температура топлива, если требуется, подогревается.
  Управление поддувом, искрообразованием, контроль результата запуска по датчику пламени.
   */
void zapusk() {
  //1й шаг проверка уровня масла
  if (bl1 == 0 && x != 2) { // если уровень масла минимум и система не в состоянии "Авария"
    if ((millis() - fuel_sensor) >= period_fuel_sensor) {
      fuel_sensor = millis();
      fuellevel();
    }
    if (oil == 0) {
      fuel_obrabotka_on();
    }
  }

  //2й шаг проверка температуры масла
  if (bl1 == 1 && temp_sensor < oil_temp_low && x != 2) { //если уровень масла достаточный, температура масла ниже минимальной
    if (oil == 1) { //если вдруг не выключен наcос подкачки масла
      fuel_obrabotka_off();
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
  if (bl1 == 1 && temp_sensor >= oil_temp_low && x != 2) { // масла достаточно, температура масла выше мин, продувка выкл

    if (oh == 1 && temp_sensor >= oil_temp_hi) { //если обогреватель включен при температуре выше максимальной - выключаем тен
      Serial.print("page1.bt1.val=1\xFF\xFF\xFF"); // переводим тумблер "нагрев выкл"
      Serial.print("page1.p4.pic=4\xFF\xFF\xFF"); // лампочку гасим
      digitalWrite(OILHEATPIN, LOW); // выключаем тэн
      oh = 0;
    }

    if (af != 1)
    {
    air_before = millis();
    digitalWrite(AIRFLOWPIN, HIGH);
    Serial.print("p5.pic=5\xFF\xFF\xFF");
    Serial.print("page1.bt2.val=0\xFF\xFF\xFF");
    indikacia("airflow", 15);
    af = 1;
    }

    if ((millis() - air_before) >= period_air_before)
    {
        if (a != 1)
        {
        air_ing = millis();
        digitalWrite(AIRPIN, HIGH);
        Serial.print("page1.p3.pic=5\xFF\xFF\xFF");
        Serial.print("page1.bt0.val=0\xFF\xFF\xFF");
        indikacia("airING", 15);
        a = 1;
        }

        if ((millis() - air_ing) >= period_air_ing)
        {
          if (sp != 1 && fs1 == 1 && sparkle_item <= 3)
          {
            if (sparkle_item == 0 || ((millis() - between_sparkle_ing) >= period_between_sparkle_ing)){
            sparkle_ing = millis();
            digitalWrite(SPARKLEPIN, HIGH);
            Serial.print("p6.pic=5\xFF\xFF\xFF");
            Serial.print("page1.bt4.val=0\xFF\xFF\xFF");
            indikacia("SPARKLE", 15);
            sp = 1;
          }
        }

    if ((millis() - sparkle_ing) >= period_sparkle_ing)
    {
      if (sp == 1)
      {
    sp = 0;
    digitalWrite(SPARKLEPIN, LOW);
    Serial.print("p6.pic=4\xFF\xFF\xFF");
    Serial.print("page1.bt4.val=1\xFF\xFF\xFF");
    fs1 = digitalRead(FLAMESENSORPIN);
    between_sparkle_ing = millis();
    sparkle_item = sparkle_item + 1;
      }

    // если горелка успешно стартовала
    if (fs1 == 0) {
      indikacia("fire", 15);
      x1 = 1; // флаг горелки в режим "горение"
      sparkle_item = 0; //обнуляем количество попыток дать искру
    }
    //если после трех попыток нет пламени
    if (sparkle_item > 3) {
      indikacia("not fire", 16);
      indikacia("error", 15);
      x1 = 3;
      sparkle_item = 0;
    }
}
    }
}
}

}

/*!
 \brief обработка ситуации неудачного старта горелки
  \details Если попытки поджига не привели к старту горелки система переходит в аварияный режиме
   */
void fireError() {
  indikacia("error fire", 16);
  x = 2; // флаг горелки в режим "авария"
  ostanov();
}

/*!
 \brief реализация процедуры останова горелки
  \details Функция выполняется один раз при останове горелки и обеспечивает поэтапную процедуру корректного останова горелки
   */
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
  fs1 = digitalRead(FLAMESENSORPIN);
  if (fs1 == 1) {                             // если пламени нет, система остановилась в штатном режиме
    x1 = 0;     // состояние горелки "не горит"
    String var = String("page0.t15.txt=\"") + String("fire off") + String("\"") + String("\xFF\xFF\xFF");
    Serial.print(var); //индикация на дисплее "горение стоп"
    x = 0;    // состояние системы "стоп"
    y = 0;    // режим горелки "ручной"
    obnulenie();

  } else {
    String var = String("page0.t15.txt=\"") + String("error fire off") + String("\"") + String("\xFF\xFF\xFF");
    Serial.print(var); //индикация на дисплее "горение не остановлено - ошибка"
    ////Serial.print("ref page0\xFF\xFF\xFF"); // обновить страницу
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
  fuel_obrabotka_off();
  //Выключаем режим "авто"
  Serial.print("bt5.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
  Serial.print("page1.p2.pic=4\xFF\xFF\xFF"); // лампочку гасим
  y = 0;
  //Выключаем систему
  Serial.print("bt6.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
  Serial.print("page1.p7.pic=4\xFF\xFF\xFF"); // лампочку гасим
  x = 0;
  var = "0";
  uint16_t packetIdPub2 = mqttClient.publish("esp32/ALL", 1, true, var.c_str());
  packetIdPub2 = mqttClient.publish("esp32/AUTO_on", 1, true, var.c_str());
}

/*!
 \brief функция включения системы
  \details Функция обрабатывает ситуацию получения от монитора команды
  тумблер "все вкл" от дисплея
   */
void All_on(){
       x = 1;
      Serial.print("page1.p7.pic=5\xFF\xFF\xFF");
      String var = String("page0.t15.txt=\"") + String("on") + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      ////Serial.print("ref page0\xFF\xFF\xFF");
      Serial.print("ref page1\xFF\xFF\xFF");
      var = "1";
      uint16_t packetIdPub2 = mqttClient.publish("esp32/ALL", 1, true, var.c_str());
  }

  /*!
   \brief функция выключения системы
    \details Функция обрабатывает ситуацию получения от монитора команды
    тумблер "все выкл" от дисплея
     */
void All_off(){
        if (x1 == 1) {
        ostanov();
      }
      else {
        x = 0;
        Serial.print("page1.p7.pic=4\xFF\xFF\xFF");
        String var = String("page0.t15.txt=\"") + String("off") + String("\"") + String("\xFF\xFF\xFF");
        Serial.print(var);
        ////Serial.print("ref page0\xFF\xFF\xFF");
        Serial.print("ref page1\xFF\xFF\xFF");
        var = "0";
        uint16_t packetIdPub2 = mqttClient.publish("esp32/ALL", 1, true, var.c_str());
        packetIdPub2 = mqttClient.publish("esp32/AUTO_on", 1, true, var.c_str());
        obnulenie();
      }
  }


  /*!
   \brief функция автоматический режим включить
    \details Функция обрабатывает процедуру включения горелки в автоматическом режиме
     */
void AUTO_on(){
  if(x == 1 && x1 == 0){
  var = "1";
  uint16_t packetIdPub2 = mqttClient.publish("esp32/AUTO_on", 1, true, var.c_str());
  y = 1; // переводим флаг "автоматический режим"
  Serial.print("page1.p2.pic=5\xFF\xFF\xFF"); // зеленая лампочка
  String var = String("page0.t14.txt=\"") + String("auto") + String("\"") + String("\xFF\xFF\xFF"); // пишем в дисплей строку режима
  Serial.print(var); //индикация на дисплее "автоматический"
  //Serial.print("ref page0\xFF\xFF\xFF"); // обновить страницу
  x1 = 2;
  }
  if (x == 0){
    Serial.print("page1.bt5.val=1\xFF\xFF\xFF");
    var = "0";
    uint16_t packetIdPub2 = mqttClient.publish("esp32/AUTO_on", 1, true, var.c_str());
  }
}

void AUTO_off(){
  y = 0;
  var = "0";
  uint16_t packetIdPub2 = mqttClient.publish("esp32/AUTO_on", 1, true, var.c_str());
  Serial.print("page1.p2.pic=4\xFF\xFF\xFF");
  String var = String("page0.t14.txt=\"") + String("manual") + String("\"") + String("\xFF\xFF\xFF");
  Serial.print(var);
  //Serial.print("ref page0\xFF\xFF\xFF"); //отправляем сформированную строку в дисплей
  ostanov();
}


void Read_18b20(byte addr[8], int t, byte flag){
  //переменные для датчиков температуры
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  float celsius, fahrenheit;
  String result;


  ds.reset();
  ds.select(addr);
  ds.write(0x44, 0);        // признак выбора режима питания 0-внешнее 1-паразитное
  //delay(1000);
  if ((millis() - read_18b20) >= period_18b20_read) {

  read_18b20 = millis(); // обнуляем таймер на полсекунды - обработка датчиком


  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // читаем результат

  for ( i = 0; i < 9; i++) {           // нам требуется 9 байтов
    data[i] = ds.read();
  }

  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
    if (flag == 1) {
  result = String(celsius);
} else{
  result = "**.**";
}
  indikacia(result, t);
if (t == 17){
  uint16_t packetIdPub2 = mqttClient.publish("esp32/temperature", 1, true, result.c_str());
}
if (t == 0){
  uint16_t packetIdPub2 = mqttClient.publish("esp32/temperature1", 1, true, result.c_str());
}
  return;
}
}

/*!
 \brief функция определения адреса датчика температуры.
  \details Функция возвращает массив - адрес подключенного устройства. Для
  корректной работы должен быть подключен только один датчик.
   */
void read_vin_18b20(byte addr[8]){
  ds.search(addr);
  ds.reset_search();
  }

void loop(void) {

// пинг проверки корректной работы системы
if ((millis() - blink1) >= period_blink1) {
  blink1 = millis();
  if (var_blink1 == 0){
    var_blink1 = 1;
    String var = String(var_blink1);
    mqttClient.publish("esp32/blink1", 1, true, var.c_str());
    }
  else {
    var_blink1 = 0;
    var = String(var_blink1);
    mqttClient.publish("esp32/blink1", 1, true, var.c_str());
    }
}

  //проверяем данные управление от дисплея
  if ( Serial.available() > 0 ) {
    SW_var = Serial.readStringUntil(0xFF);
    SW_var.remove(0, 1);
    String SW_var_temp = SW_var.substring(0, 3); // Выделяем идентификатор редактируемого поля параметра
    String SW_var_temp_num = SW_var.substring(4); // Выделяем значение параметра
    byte temp_num = SW_var.substring(4).toInt();

    //включение системы?
    if (SW_var.equals("ALL_on") && x != 2) {
      All_on();
    }
    if (SW_var.equals("ALL_off")) {
     All_off();
    }

    //режим работы автоматический или ручной?
    if (SW_var.equals("AUTO_on")) { //режим системы вкл, тумблер авто-вкл и горелка не горит
      AUTO_on();
    }

    if (SW_var.equals("AUTO_off")) {
      AUTO_off();
    }

    // канал подачи воздуха?
    if (SW_var.equals("A_on") && x == 1 && y != 1) {
      a = 1;
      var = "1";
      uint16_t packetIdPub2 = mqttClient.publish("esp32/A_on", 1, true, var.c_str());
      digitalWrite(AIRPIN, HIGH);
      Serial.print("p3.pic=5\xFF\xFF\xFF");
    }
    if (SW_var.equals("A_on") && x == 0) {
      Serial.print("page1.bt0.val=1\xFF\xFF\xFF");
    }
    if (SW_var.equals("A_off") && y != 1) {
      a = 0;
      var = "0";
      uint16_t packetIdPub2 = mqttClient.publish("esp32/A_on", 1, true, var.c_str());
      digitalWrite(AIRPIN, LOW);
      Serial.print("p3.pic=4\xFF\xFF\xFF");
    }

    // канал нагревателя?
    if (SW_var.equals("OilHeat_on") && x == 1 && (temp_sensor < oil_temp_hi) && y != 1) {
      oh = 1;
      var = "1";
      uint16_t packetIdPub2 = mqttClient.publish("esp32/oh", 1, true, var.c_str());
      digitalWrite(OILHEATPIN, HIGH);
      Serial.print("p4.pic=5\xFF\xFF\xFF");
      Serial.print("page1.bt1.val=0\xFF\xFF\xFF");
    }
    if (SW_var.equals("OilHeat_on") && (x == 0 || (temp_sensor >= oil_temp_low))) {
      Serial.print("page1.bt1.val=1\xFF\xFF\xFF");
    }
    if (SW_var.equals("OilHeat_off") && y != 1) {
      oh = 0;
      var = "0";
      uint16_t packetIdPub2 = mqttClient.publish("esp32/oh", 1, true, var.c_str());
      digitalWrite(OILHEATPIN, LOW);
      Serial.print("p4.pic=4\xFF\xFF\xFF");
    }
    if (temp_sensor >= oil_temp_hi && oh != 0) {
      oh = 0;
      var = "0";
      uint16_t packetIdPub2 = mqttClient.publish("esp32/oh", 1, true, var.c_str());
      digitalWrite(OILHEATPIN, LOW);
      Serial.print("p4.pic=4\xFF\xFF\xFF");
      Serial.print("page1.bt1.val=1\xFF\xFF\xFF");
    }

    // канал вторичного поддува?
    if (SW_var.equals("AF_on") && x == 1 && y != 1) {
      af = 1;
      var = "1";
      uint16_t packetIdPub2 = mqttClient.publish("esp32/AF", 1, true, var.c_str());
      digitalWrite(AIRFLOWPIN, HIGH);
      Serial.print("p5.pic=5\xFF\xFF\xFF");
    }
    if (SW_var.equals("AF_on") && x == 0) {
      Serial.print("page1.bt2.val=1\xFF\xFF\xFF");
    }
    if (SW_var.equals("AF_off")) {
      af = 0;
      var = "0";
      uint16_t packetIdPub2 = mqttClient.publish("esp32/AF", 1, true, var.c_str());
      digitalWrite(AIRFLOWPIN, LOW);
      Serial.print("p5.pic=4\xFF\xFF\xFF");
    }

    //канал накачки масла?
    if (SW_var.equals("OILPUMP_on")) {
      fuel_obrabotka_on();
    }
    if (SW_var.equals("OILPUMP_off")) {
      fuel_obrabotka_off();
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
      oil_temp_low_txt = SW_var_temp_num;
      //Отправляем новое значение в мобильный клиент
      uint16_t packetIdPub2 = mqttClient.publish("esp32/LOT", 1, true, SW_var_temp_num.c_str());
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
      oil_temp_hi_txt = SW_var_temp_num;
      //Отправляем новое значение в мобильный клиент
      uint16_t packetIdPub2 = mqttClient.publish("esp32/HOT", 1, true, SW_var_temp_num.c_str());
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
      water_temp_low_txt = SW_var_temp_num;
      //Отправляем новое значение в мобильный клиент
      uint16_t packetIdPub2 = mqttClient.publish("esp32/WTL", 1, true, SW_var_temp_num.c_str());
    }

    if (SW_var_temp.equals("WTH")) { // Если изменили верхнюю границу диапазона температуры теплоносителя
      //обновляем индикацию на дисплее
      String var = String("page2.whi.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш

      EEPROM.write(2, temp_num);
      EEPROM.commit();
      water_temp_hi = SW_var_temp_num.toInt();
      water_temp_hi_txt = SW_var_temp_num;
      //Отправляем новое значение в мобильный клиент
      uint16_t packetIdPub2 = mqttClient.publish("esp32/WTH", 1, true, SW_var_temp_num.c_str());
    }

    if (SW_var_temp.equals("FTL")){ // Если изменили предельное время дозаправки бака
       //обновляем индикацию на дисплее
      String var = String("page2.ftl.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш

      EEPROM.write(4, temp_num);
      EEPROM.commit();
      fuel_tank = SW_var_temp_num.toInt();
      fuel_tank_txt = SW_var_temp_num;
      //Отправляем новое значение в мобильный клиент
      uint16_t packetIdPub2 = mqttClient.publish("esp32/FTL", 1, true, SW_var_temp_num.c_str());
    }

    if (SW_var_temp.equals("AIRB")){ // Если изменили время продувки до старта горелки
       //обновляем индикацию на дисплее
      String var = String("page2.airb.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш

      EEPROM.write(5, temp_num);
      EEPROM.commit();
      period_air_before = 1000 * SW_var_temp_num.toInt();
      period_air_before_txt = SW_var_temp_num;
      //Отправляем новое значение в мобильный клиент
      uint16_t packetIdPub2 = mqttClient.publish("esp32/AIRB", 1, true, SW_var_temp_num.c_str());
    }

    if (SW_var_temp.equals("AIRA")){ // Если изменили время продувки посте останова горелки
       //обновляем индикацию на дисплее
      String var = String("page2.aira.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш

      EEPROM.write(6, temp_num);
      EEPROM.commit();
      period_air_after = 1000 * SW_var_temp_num.toInt();
      period_air_after_txt = SW_var_temp_num;
      //Отправляем новое значение в мобильный клиент
      uint16_t packetIdPub2 = mqttClient.publish("esp32/AIRA", 1, true, SW_var_temp_num.c_str());
    }
    if (SW_var_temp.equals("AIRING")){ // Если изменили время продувки посте останова горелки
       //обновляем индикацию на дисплее
      String var = String("page2.airing.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш

      EEPROM.write(7, temp_num);
      EEPROM.commit();
      period_air_ing = 1000 * SW_var_temp_num.toInt();
      period_air_ing_txt = SW_var_temp_num;
      //Отправляем новое значение в мобильный клиент
      uint16_t packetIdPub2 = mqttClient.publish("esp32/AIRING", 1, true, SW_var_temp_num.c_str());
    }

    if (SW_var_temp.equals("BSI")){ // Если изменили время между искрами
       //обновляем индикацию на дисплее
      String var = String("page2.bsi.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш

      EEPROM.write(9, temp_num);
      EEPROM.commit();
      period_between_sparkle_ing = 1000 * SW_var_temp_num.toInt();
      period_between_sparkle_ing_txt = SW_var_temp_num;
      //Отправляем новое значение в мобильный клиент
      uint16_t packetIdPub2 = mqttClient.publish("esp32/BSI", 1, true, SW_var_temp_num.c_str());
    }

    if (SW_var_temp.equals("SI")){ // Если изменили время продолжительности искры
       //обновляем индикацию на дисплее
      String var = String("page2.si.txt=\"") + SW_var_temp_num + String("\"") + String("\xFF\xFF\xFF");
      Serial.print(var);
      Serial.print("ref page2\xFF\xFF\xFF");
      // записываем новое значение во флеш

      EEPROM.write(8, temp_num);
      EEPROM.commit();
      period_sparkle_ing = 1000 * SW_var_temp_num.toInt();
      period_sparkle_ing_txt = SW_var_temp_num;
      //Отправляем новое значение в мобильный клиент
      uint16_t packetIdPub2 = mqttClient.publish("esp32/SI", 1, true, SW_var_temp_num.c_str());
    }

    //кнопка "запросить номер датчика температуры"
    if (SW_var.equals("number18b20")) {
     temp_number_reading="";
     read_vin_18b20(addr1);
     for (int i=0;i<8;i++){
     temp_number_reading += "0x";
     temp_number_reading += String(addr1[i],HEX);
     temp_number_reading += ",";
        }
      Serial.print("num18b20.txt=\"" + temp_number_reading + "\"" + "\xFF\xFF\xFF");
    }

    if (SW_var.equals("maslo")){
        unsigned char* buf = new unsigned char[100];
        temp_number_reading.getBytes(buf, 100, 0);
        const char *str2 = (const char*)buf;
        writeFile(SD, "/oil_number.txt", str2);
        /*readFile(SD, "/oil_number.txt");
        for (int i=0; i<40; i++) {
          oil_buffer[i] = my_buffer[i];
          my_buffer[i] = 0;
        }*/
      }

    if (SW_var.equals("battery")){
          unsigned char* buf = new unsigned char[100];
          temp_number_reading.getBytes(buf, 100, 0);
          const char *str2 = (const char*)buf;
          writeFile(SD, "/air_number.txt", str2);
          /*readFile(SD, "/air_number.txt");
          for (int i=0; i<40; i++) {
            air_buffer[i] = my_buffer[i];
            my_buffer[i] = 0;
          }*/
        }

    if (SW_var.equals("obratka")){
            unsigned char* buf = new unsigned char[100];
            temp_number_reading.getBytes(buf, 100, 0);
            const char *str2 = (const char*)buf;
            writeFile(SD, "/in_water_number.txt", str2);
            /*readFile(SD, "/air_number.txt");
            for (int i=0; i<40; i++) {
              air_buffer[i] = my_buffer[i];
              my_buffer[i] = 0;
            }*/
          }

    if (SW_var.equals("podacha")){
              unsigned char* buf = new unsigned char[100];
              temp_number_reading.getBytes(buf, 100, 0);
              const char *str2 = (const char*)buf;
              writeFile(SD, "/out_water_number.txt", str2);
              /*readFile(SD, "/air_number.txt");
              for (int i=0; i<40; i++) {
                air_buffer[i] = my_buffer[i];
                my_buffer[i] = 0;
              }*/
            }

  }

  // читаем DHT22
  if ((millis() - dht22) >= period_DHT22) {
    obnovlenie (); //таймер подходит раз в минуту - отправим ВСЕ данные состояния горелки на смартфон
    dht22 = millis();
    float h = dht.readHumidity(); // считывание данных о температуре и влажности
    delay(70);
    float t = dht.readTemperature();// считываем температуру в градусах Цельсия:
    delay(70);

    // проверяем, корректно ли прочитались данные,
    // и если нет, то выходим и пробуем снова:
    if (isnan(h) || isnan(t)) {
      //Serial.print("Failed to read from DHT sensor!"); // "Не удалось прочитать данные с датчика DHT!"
      h=0;
      t=0;
      String var3 = "t1.txt=\"" + String(h) + "\"";
      Serial.print(var3 + "\xFF\xFF\xFF");
      String var4 = "t8.txt=\"" + String(t) + "\"";
      Serial.print(var4 + "\xFF\xFF\xFF");
      //Serial.print("ref page0\xFF\xFF\xFF");
    } else {
      String var3 = "t1.txt=\"" + String(h) + "\"";
      Serial.print(var3 + "\xFF\xFF\xFF");
      String var4 = "t8.txt=\"" + String(t) + "\"";
      Serial.print(var4 + "\xFF\xFF\xFF");
      //Serial.println("ref page0\xFF\xFF\xFF");
    }

// отправляем данные MQTT
String var = String(h);
uint16_t packetIdPub2 = mqttClient.publish("esp32/DHT_HUM", 1, true, var.c_str());
var = String(t);
packetIdPub2 = mqttClient.publish("esp32/DHT_Temp", 1, true, var.c_str());

//заодно обновим IP
  IPAddress ip = WiFi.localIP();
  Serial.print("page0.ip.txt=\"");
  Serial.print(ip);
  Serial.print(String("\"") + String("\xFF\xFF\xFF"));
  //Serial.print("ref page0\xFF\xFF\xFF");
      }

  // Читаем датчик 18b20
  if ((millis() - T18b20_1) >= period_18b20_1) {
      Read_18b20(addr_air_temp, 17, air_temp_flag);
      T18b20_1 = millis(); // обнуляем таймер опроса датчика
            }
  if ((millis() - T18b20_2) >= period_18b20_2) {
      Read_18b20(addr_oil_temp, 0, oil_temp_flag);
      T18b20_2 = millis(); // обнуляем таймер опроса датчика
    }
  if ((millis() - T18b20_3) >= period_18b20_3) {
      Read_18b20(addr_out_water_temp, 3, out_water_temp_flag);
      T18b20_3 = millis(); // обнуляем таймер опроса датчика
                                          }
  if ((millis() - T18b20_4) >= period_18b20_4) {
      Read_18b20(addr_in_water_temp, 12, in_water_temp_flag);
      T18b20_4 = millis(); // обнуляем таймер опроса датчика
      }


  //проверяем датчик пламени, если что-то не так обрабатываем ошибку
  if ((millis() - flame_sensor) >= period_flame_sensor) {
    flame_sensor = millis();
    fs1 = digitalRead(FLAMESENSORPIN);
    if (x1 == 1 && fs1 == 1 || x1 == 0 && fs1 == 0) { //если состояние системы - работа и датчик пламени показывает отсутствие пламени
      fireError(); //ошибка  горение
    }
  }

  // Читаем датчик уровня топлива раз в 5 сек
  if ((millis() - fuel_sensor) >= period_fuel_sensor) {
    fuel_sensor = millis();
    fuellevel();
/*    //почитаем датчик ацп

    adc0 = ads.readADC_SingleEnded(0);
    adc1 = ads.readADC_SingleEnded(1);
    adc2 = ads.readADC_SingleEnded(2);
    adc3 = ads.readADC_SingleEnded(3);

    volts0 = ads.computeVolts(adc0);
    volts1 = ads.computeVolts(adc1);
    volts2 = ads.computeVolts(adc2);
    volts3 = ads.computeVolts(adc3);

    Serial.println("-----------------------------------------------------------");
    Serial.print("AIN0: "); Serial.print(adc0); Serial.print("  "); Serial.print(volts0); Serial.println("V");
    Serial.print("AIN1: "); Serial.print(adc1); Serial.print("  "); Serial.print(volts1); Serial.println("V");
    Serial.print("AIN2: "); Serial.print(adc2); Serial.print("  "); Serial.print(volts2); Serial.println("V");
    Serial.print("AIN3: "); Serial.print(adc3); Serial.print("  "); Serial.print(volts3); Serial.println("V");
*/
    //закончили прочитать
  }

  //проверка исправности датчика топлива по вермени подкачки
  if(((millis()-fuel_tank_var)>= period_fuel_tank) && oil == 1){
    oil = 0;
    Serial.print("bt3.val=1\xFF\xFF\xFF"); // переводим тумблер "подкачка выкл"
    Serial.print("page1.p1.pic=4\xFF\xFF\xFF"); // лампочку гасим
    digitalWrite(OILPUMPPIN, LOW); // Выключили насос подкачки
    x = 2;
    }

  //проверка уровня бачка на дозаправку
  if (bl1 == 0 && y == 1 && x == 1 && oil != 1) { // если режим системы - работа, уровень масла минимум, а режим при этом автоматический - подкачиваем.
    Serial.print("bt3.val=0\xFF\xFF\xFF"); // переводим тумблер "подкачка вкл"
    Serial.print("page1.p1.pic=5\xFF\xFF\xFF"); // лампочку зажигаем
    digitalWrite(OILPUMPPIN, HIGH); // Включили насос подкачки
    oil = 1;
    fuel_tank_var = millis(); // запускаем таймер предохранительный
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
