void obnovlenie (){
  String var;
  uint16_t packetIdPub2;
// Обновляем положение тумблера "On/off"
  if (x == 1){
    var = "1";
    packetIdPub2 = mqttClient.publish("esp32/ALL", 1, true, var.c_str());
    }
  if (x == 0){
    var = "0";
    packetIdPub2 = mqttClient.publish("esp32/ALL", 1, true, var.c_str());
    }
// Обновляем положение тумблера "АВТО"
  if (y == 1){
    var = "1";
    packetIdPub2 = mqttClient.publish("esp32/AUTO", 1, true, var.c_str());
    }
  if (y == 0){
    var = "0";
    packetIdPub2 = mqttClient.publish("esp32/AUTO", 1, true, var.c_str());
    }  

// Обновляем положение тумблера канала подачи воздуха "A_on"
    if (a == 1){
    var = "1";
    packetIdPub2 = mqttClient.publish("esp32/A_on", 1, true, var.c_str());
    }
  if (a == 0){
    var = "0";
    packetIdPub2 = mqttClient.publish("esp32/AUTO", 1, true, var.c_str());
    }  
 

//Обноляем положение тумблера канала нагревателя "oh"
    if (oh == 1){
    var = "1";
    packetIdPub2 = mqttClient.publish("esp32/oh", 1, true, var.c_str());
    }
    if (oh == 0){
    var = "0";
    packetIdPub2 = mqttClient.publish("esp32/oh", 1, true, var.c_str());
    }  

//Обновляем положение тумблера канала поддува "AF_on"
    if (af == 1){
    var = "1";
    packetIdPub2 = mqttClient.publish("esp32/AF", 1, true, var.c_str());
    }
    if (af == 0){
    var = "0";
    packetIdPub2 = mqttClient.publish("esp32/AF", 1, true, var.c_str());
    }

//Канал подкачки масла "oil"
    if (oil == 1){
    var = "1";
    packetIdPub2 = mqttClient.publish("esp32/oil", 1, true, var.c_str());
    }
    if (oil == 0){
    var = "0";
    packetIdPub2 = mqttClient.publish("esp32/oil", 1, true, var.c_str());
    }


// нижняя граница температуры масла "LOT"
      mqttClient.publish("esp32/LOT", 1, true, oil_temp_low_txt.c_str());
// верхняя граница температуры масла "HOT"
      mqttClient.publish("esp32/HOT", 1, true, oil_temp_hi_txt.c_str());
//нижняя граница температуры тосола "WTL"
      mqttClient.publish("esp32/WTL", 1, true, water_temp_low_txt.c_str());
//нижняя граница температуры тосола "WTH"
      mqttClient.publish("esp32/WTH", 1, true, water_temp_hi_txt.c_str());

//Нижний уровень топлива "oil_level"
      if (bl1 == 0){
      var = "Low";
      packetIdPub2 = mqttClient.publish("esp32/oil_level", 1, true, var.c_str());
      }
      if (bl1 == 1){
      var = "Full";
      packetIdPub2 = mqttClient.publish("esp32/oil_level", 1, true, var.c_str());
      }
      if (bl1 == 2){
      var = "Middle";
      packetIdPub2 = mqttClient.publish("esp32/oil_level", 1, true, var.c_str());
      }

// Состояние горелки "system"
      if (x1 == 0){
      var = "Cold";
      packetIdPub2 = mqttClient.publish("esp32/system", 1, true, var.c_str());
      }
      if (x1 == 1){
      var = "Work in process";
      packetIdPub2 = mqttClient.publish("esp32/system", 1, true, var.c_str());
      }
      if (x1 == 2){
      var = "Start in process";
      packetIdPub2 = mqttClient.publish("esp32/system", 1, true, var.c_str());
      }
      if (x1 == 3){
      var = "Stop in process";
      packetIdPub2 = mqttClient.publish("esp32/system", 1, true, var.c_str());
      }

}      
