//Название ESP8266-01_RebootRouter
//Версия 2024-01-09
//Используется модуль ESP8266-01 в паре с модулём реле для ESP01.
//Для выкл-вкл питания роутера (передергивания питания) в случае отсутствия соединения с интернетом
//или по прошествии какого-то интервала времени (например каждые 6 часов).
//Отсутствие интернета определяется путём опроса сервисной html страницы (http://10.0.0.1/status) модема Yota
//и чтения уровня сигнала от вышки. 
//Если страница модема не открывается, если уровень сигнала < 1, будет перезагружен роутер.
//
//Автор turtlefly@yandex.ru
//https://github.com/TurtleFlyRU
//Страница проекта https://github.com/TurtleFlyRU/ESP8266-01_RebootRouter/
//
//Freeware
//
//Arduino IDE 2.2.1 
//Все библиотеки последних версий на дату 2024-01-09.
//
// Ссылки Yota модема
// http://10.0.0.1
// http://10.0.0.1/status
// http://10.0.0.1/network
// http://10.0.0.1/manualupdate
// http://10.0.0.1/dir

#include <Arduino.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

const char* devHostName = "ESP44-RELAY-ROUTER"; //Сетевое имя модуля, на ваше усмотрение
const char* wifissid     = ""; //Укажите имя вашей WIFI сети к которой вы будете подключаться
const char* wifipassword = ""; //Укажите пароль доступа к этой WIFI сети

const int CycleTime = 10000; //миллисекунды, длительность паузы в конце цикла опроса модема Yota
const int ResetByDay = 12; //количество софт-ресетов модуля esp в день
const int CycleLimit = (24*60*60*1000) / CycleTime / ResetByDay; // вычисляемый лимит циклов работы модуля, при превышении лимита будет перезагрузка модуля

int cycle = 0; // счётчик главного цикла loop
int counterPingFailed = 0; // счетчик неудавшихся обращений к сайту

//Создаём объект WiFiMulti ->  класса ESP8266WiFiMulti
ESP8266WiFiMulti WiFiMulti;

//Функции

/////////////////////////////////////
//HTTP_CONNECT подключение к 10.0.0.1
/////////////////////////////////////
int HTTP_CONNECT()
{
    WiFiClient client;
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");

    if (http.begin(client, "http://10.0.0.1/status?_=0"))
    {  // HTTP


      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) 
      {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) 
        {
          String payload = http.getString();
          //Serial.println(payload);
          return(SEARCH_NEEDLE("3GPP.SINR=", payload));
        }
      }else
      {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        counterPingFailed++;
        return 0;
      }

      http.end();
    }else
    {
      Serial.println("[HTTP] Unable to connect");
      counterPingFailed++;
      return 0;
    }
  return 0;
}


//////////////////////////////////////////////////////////////////////////
//////////////SEARCH_NEEDLE поиск подстроки 
/////////////в ответе от сервера, вернём IntSubResult >0 - нашли, 0 - нет
//////////////////////////////////////////////////////////////////////////
int SEARCH_NEEDLE(String needle, String line)
{ 
     String SubResult = "0";
     int IntSubResult = 0;
     //обрезка пробелов строки
     line.trim();
     //Поиск в строке, возвращаем порядковый номер символа в строке, если он найден
     int NeedleIndex = line.indexOf(needle);

     if(NeedleIndex != -1)
     {
        int NeedleLenght = needle.length();
        int LineLength = line.length();
        int SubStart = NeedleIndex + NeedleLenght;
        int SubEnd = NeedleIndex + NeedleLenght + 2;
        //извлечение командных данных без идентификатора начала коммандной строки, только данные
        SubResult = line.substring(SubStart, SubEnd);
        IntSubResult = SubResult.toInt();
        Serial.print("needle:");Serial.println(needle);
        Serial.print("NeedleIndex:");Serial.println(NeedleIndex);
        Serial.print("NeedleLenght:");Serial.println(NeedleLenght);
        Serial.print("LineLength:");Serial.println(LineLength);
        Serial.print("SubStart:");Serial.println(SubStart);
        Serial.print("SubEnd:");Serial.println(SubEnd);
        Serial.print("SubResult:");Serial.println(SubResult);
        Serial.print("IntSubResult:");Serial.println(IntSubResult);
     }else
     {
        Serial.println(needle + " НЕ найдено!");
        Serial.print("IntSubResult:");Serial.println(IntSubResult);
     }
return IntSubResult;
}

/////////////////////////////////////////
//Перередергивание (ВЫКЛ -> ВКЛ) реле
/////////////////////////////////////////
void ResetCoil()
{
  Serial.println("Router Power OFF");
  //Включаем реле - посылаем высокий уровень сигнала, т.е. у роутера пропадает питание.
  digitalWrite(0, HIGH); 
  delay(2000);
  //Выключаем реле. Через контакты NC снова идёт питание на роутер.
  digitalWrite(0, LOW);
  Serial.println("Router Power On"); 
  //Роутер загружается ка минимум 30 секунд, а мы подождём немного дольше для инициализации модема.
  delay(60000);
}


/////////////////////////////////////////
//////////////////SETUP//////////////////
/////////////////////////////////////////
void setup() 
{
  Serial.begin(115200);
  //Подключение к WIFI роутеру
  WiFi.mode(WIFI_STA);
  WiFi.hostname(devHostName);
  WiFiMulti.addAP(wifissid, wifipassword); 

  //Реле на GPIO0, на реле задействованы контакты NC, т.е. для размыкания надо подать HI
  //Установим GPIO0 output low таким образом реле не под напряжением, 
  //а через контакты NC сетевое напряжение питает роутер
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW); 

  delay(1000);
}


/////////////////////////////////////////////////
////////////////////LOOP/////////////////////////
/////////////////////////////////////////////////
void loop()
{
  cycle++; //увеличиваем счётчик циклов на 1

  //Если подключены то отслеживаем страницу модема
  if ((WiFiMulti.run() == WL_CONNECTED)) 
  {
    int YotaStatus = HTTP_CONNECT();
    Serial.print("YotaStatus:"); Serial.println(YotaStatus);
    
    if(YotaStatus == 0)
    {
      cycle = 0;
      ResetCoil();
    }
  }

  //Если количество повторов цикла достигло ограничения, 
  //то перезагрузка роутера и модуля ESP
  //Т.к. иногда бывает ситуация:
  //модем отвечает, подключение к вышке демонстрирует, 
  //а интернета нет. Помогает только ВЫКЛ/ВКЛ питания.
  //Конечно, можно еще пинговать какой-то интернет хост
  //так и было, но это не спасало от глюков.
  //Решение с лимитом циклов надёжнее и удобнее всего
  //при вашем отстутствии на объекте.
  if(cycle >= CycleLimit)
  {
    ResetCoil();
    cycle = 0;
    ESP.restart();
  }

  delay(CycleTime);  //пауза
}
