// Скетч часов из четырех семисегментных индикаторов, построенных на базе адресной светодиодной ленты WS2811
// (для разделительных точек применены два адресных светодиода WS2812)
// Опционально подключаестя выносной температурный сенсор на чипе DS18B20 (реализовано для одного сенсора, но можно использовать несколько сенсоров)
// Автор Марченко Вадим (C) 21/02/2023 год.

#include <ESP8266WiFi.h>
//-----------------------------------R e a d  G P S-----------------------------------------------------------------
#include <TinyGPS++.h>
//#include "C:\Users\Marra3888\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.1.1\libraries\SoftwareSerial.h"
#include <SoftwareSerial.h>
#include <Timezone.h>
#include <Time.h>

//static const int RXPin = 2, TXPin = 0;//D4 - RX, D3 - TX
#define RXPin D9 //D4 //3
#define TXPin D10 //D3 //1

// The TinyGPS++ object
TinyGPSPlus gps;
// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin);
// Central European Time
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 180}; // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 2, 120};   // Central European Standard Time
Timezone CE(CEST, CET);
TimeChangeRule *tcr;
#define Plus2

static const uint32_t GPSBaud = 9600;
uint8_t h, m, s, dow, satelit;
uint8_t d, mm;
uint16_t y;


//#include <Wire.h>                     // Базовая библиотека шины I2C (используется для связи модуля часов реального времени)
//#include <iarduino_RTC.h>             // Библиотека часов реального времени 
#include <Adafruit_NeoPixel.h>        // Библиотека для управления адресными светодиодами 
#include <OneWire.h>                  // Бибиотека шины 1-Wire для работы температурных датчиков
#include <DallasTemperature.h>        // Библиотека температурного сенсора DS18B20
#include <EEPROMex.h>                 // Библиотека доступа к энергонезависимой памяти

//#include <MD_DS3231.h>

#define PIN 6                         // управление адресными светодиодами по этому пину
#define ONE_WIRE_BUS D4               // датчик температуры DS18B20 подключен к этому пину
#define TEMPERATURE_PRECISION 12      // точность измерений 9-бит низкая, 12-бит высокая

//iarduino_RTC time(RTC_DS3231);        // подключаем RTC модуль на базе чипа DS3231, используется аппаратная шина I2C
Adafruit_NeoPixel strip = Adafruit_NeoPixel(86, PIN, NEO_GRB + NEO_KHZ800); //Подключаем ленту на адресных светодиодах

OneWire oneWire(ONE_WIRE_BUS);        // подключаем шину 1-Wire, для передачи данных с сенсоров температуры
DallasTemperature sensors(&oneWire);  // подключаем температурные сенсоры

byte Mode = 0;                         //режимы:  по умолчанию 0 - отображение времени с заданными настройкамиbyte NumColor = 0;
//----------- переменные настроек, которые так же хранятся в ПЗУ контроллера и считываются при включении устройсва-------------
byte NowBrigtness = 185;              // текущая яркость элементов (начальное значение должно быть кратным 15)
byte ColorMode = 1;                  // текущий цветовой режим (13 режим - цвет меняется каждый час, полный цикл за 12 частов)
byte TempTime = 8;                    // длительность отображения температуры
//------------------------------------------------------
int temp10;                           // перомежуточная переменная температуры (используем множитель 10, что бы использовать тип integer
long int KeyPressed;                  // время в милисекундах, для сравнения с текущими милисекундами (используется для возврата в основной режим)
unsigned long eeprom_timer;           // таймер зашивки в энергонезависимую память параметров настройки часов
byte degree = 50;                    // процент яркости разделительных точек (исходя из геометрии яркость разделительных точек нужно уменьшать
boolean eeprom_flag;

void setup() {
  Serial.begin(9600);                     // подключаем сериал порт для вывода отладочной информации (отключено в целях экономии памяти)
  pinMode(2, INPUT_PULLUP);                 // подключаем кнопку 1 к пину и подтягиваем её к питанию
  pinMode(3, INPUT_PULLUP);                 // подключаем кнопку 2 к пину и подтягиваем её к питанию
  pinMode(4, INPUT_PULLUP);                 // подключаем кнопку 3 к пину и подтягиваем её к питанию
//  time.begin();                             // запускаем часы реального времени
  sensors.begin();                          // запускаем сенсоры температуры
  strip.begin();                            // запускаем адресные светодиоды
  strip.setBrightness(NowBrigtness);        // устанавливаем яркость светодиодной ленты
  strip.show();
//  Wire.begin();  
  // KeyPressed = millis();
  // освещаем ленту (по умолчанию нули-ничего не светится)
  // Отладочный способ установки времени через скетч
// time.settime(0,12,16,14,2,23,2);        // _ сек, _ мин, _ час, _ день месяца, _ месяц, _ год, день недели 0-вск 6-суб
//  updateEEPROM;                           // при первой прошивке раскоментировать для записи настроек в ПЗУ контроллера
  readEEPROM();                             // чтениt настроек из ПЗУ контроллера
//  pinMode(LED_BUILTIN, OUTPUT);  
}


void loop() 
{
  static int zz = 0;
  if ((millis() - KeyPressed) > 9000) Mode = 0;  //если спустя 4 секунды кнопок не нажималось часы переходят в нормальный режим
  int key = get_key();                            // опрашиваем нажатие кнопки
  if (key)  
  {
    delay(330);                                   // после нажатия на любую кнопку делаем небольшую паузу, что бы успеть отпустить кнопку
    KeyPressed = millis();   // сбрасываем таймер последнего нажатия кнопки (для того что бы спустя 3 сек часы перешли в режим 0)
    eeprom_flag = true;
  }
  if (key == 2)   
  {                               // если нажата кнопка 2, то гоняем меню по кругу (выбора режима цвета, яркость, установка времени и даты
    if (Mode == 9) Mode = 0; else Mode++;
  }
  
        if  (zz > 150)  
        {
          Serial.print(RTC.h / 10);
          Serial.print(RTC.h % 10);
          Serial.print(" : ");
          Serial.print(RTC.m / 10);
          Serial.print(RTC.m % 10);
          Serial.print("  ");
          Serial.print(RTC.dd);
          Serial.print(" / ");
          Serial.print(RTC.mm);
          Serial.print(" / ");
          Serial.print(RTC.yyyy);          
          Serial.print("  ");
          Serial.println(RTC.dow);          
          zz = 0;
        } 
          zz++; 
            
  switch ( Mode ) 
  {
    case 0: 
    { 
                                   //---------------------- Нормальный режим "0": отображение времени и температуры ----------------------
        setcolor (ColorMode);                     // задаём цвет всех элементов в зависимости от цветового режима
//        time.gettime();                       // запрашиваем у модуля реального времени теущее время
        RTC.readTime();            

//          digitalWrite(LED_BUILTIN, LOW);                     
                                   
        if (( RTC.s) == ( 60 - TempTime)) 
        {                                         // раз в минуту проверяем наличие датчика
          sensors.requestTemperatures();        // Отправляем запрос на значение температуры
          temp10 = 10 * sensors.getTempCByIndex(0);
          Serial.println(temp10, DEC);
        }
        if ((( RTC.s) <= 60 - TempTime) or (temp10<-500))   
        {                                          // отображаем время в том числе если датчик не подключен
          digitout (0, RTC.h / 10 );          // отрисовываем цифры (выставляем черные точки)
          digitout (21, RTC.h % 10 );         // отрисовываем цифры (выставляем черные точки)
          digitout (44, RTC.m / 10 );       // отрисовываем цифры (выставляем черные точки)
          digitout (65, RTC.m % 10 );       // отрисовываем цифры (выставляем черные точки)
          if (( RTC.s) % 2 == 0) 
          {                                        // по четным секундам разделительные точки не светятся (можно сделать затухание)
            strip.setPixelColor(42, 0, 0, 0); strip.setPixelColor(43, 0, 0, 0); 
          }
                   
        }
        
        if ((key == 1) || (key == 3) || ((( RTC.s) > 60 - TempTime) and (temp10>-500)))   
        {                                                  // если нажали кнопку "1" или пришло время отображать температуру и датчик подключен
          sensors.requestTemperatures();                  // Отправляем запрос на значение температуры
          temp10 = 10 * sensors.getTempCByIndex(0);
          for (int i = 0; i < 86; i++) 
          {
            strip.setPixelColor(i, WheelRGB((40 - temp10 / 10) * 255 / 60 & 255));                          // цвет  зависит от температуры датчика
            if (i == 42 or i == 43)  strip.setPixelColor(i, WheelRBG((40 - temp10 / 10) * 255 / 60 & 255)); // для разделительных точек применяем модифицированное цветовое колесо
          }
          if (temp10 < -500) {            // если датчик отключен, то выводим "---с"
            digitout (0, 13 );            // отрисовываем "-" (выставляем черные точки)
            digitout (21, 13 );           // отрисовываем "-" (выставляем черные точки)
            digitout (44, 13 );           // отрисовываем "-" (выставляем черные точки)
            strip.setPixelColor(74,75,76, (0, 0, 0));   // знак "_" не рисуем
          }
          else
          {
            digitout (0, temp10 / 100 );             // отрисовываем цифры (выставляем черные точки)
            digitout (21, ((temp10) % 100) / 10 );   // отрисовываем цифры (выставляем черные точки)
            digitout (44, (temp10) % 10 );          // отрисовываем цифры (выставляем черные точки)
          }
          strip.setPixelColor(42, 0, 0, 0);        // затеняем верхнюю точку
          digitout (65, 11 );                     //симвод градуса Цельсия
          if (temp10 >= 0) 
          {                      // если температура выше нуля, то знак "_" не отображаем
            strip.setPixelColor(74,75,76, (0, 0, 0));   // знак "_"
          }

          //      if (key == 3)   {
          // - зарезервированно либо для вывода даты, либо для вывода температуры с второго датчика
          //    }
        }
        strip.show(); //освещаем режим 0
        break;
      }                                           //---------------------- конец отработки условия нормального режима "0" ------------------
    case 1: 
    {                                     //  Если режим "1" - установка цветовой схемы
        if (key == 1)   
        {                                                                   //Установка цвета
          if (ColorMode == 0) ColorMode = 26; else ColorMode--;
        }
        if (key == 3)   
        { //Гоняем цвет по кругу
          if (ColorMode == 26) ColorMode = 0; else ColorMode++;
        }
        setcolor (ColorMode);                     // задаём цвет всех элементов в зависимости от цветового режима
                                                  // - отображаем на экране номер цветового режима
        digitout (0, 12 );                        // отрисовываем цифры (выставляем черные точки)
        digitout (21, 13);                         // отрисовываем цифры (выставляем черные точки)
        digitout (44, ColorMode / 10 );           // отрисовываем цифры (выставляем черные точки)
        digitout (65, ColorMode % 10);            // отрисовываем цифры (выставляем черные точки)
        strip.setPixelColor(42, 0, 0, 0); strip.setPixelColor(43, 0, 0, 0);
        strip.show(); //освещаем режим
        break;
      }
    case 2: 
    {                               //  Если режим "2" - установка яркости
        if (key == 1)   
        {
          NowBrigtness = NowBrigtness - 15;  //меняем яркость от 0 до 255 с шагом 5
        }
        if (key == 3)   
        {
          NowBrigtness = NowBrigtness + 15;
        }
        setcolor (ColorMode);                     // задаём цвет всех элементов в зависимости от цветового режима
        if (NowBrigtness == 0)strip.setBrightness(1); else strip.setBrightness(NowBrigtness); ;      // устанавливаем яркость светодиодной ленты
                                                                                                      // - отображаем на экране величину яркости
        digitout (0, 14 );                        // отрисовываем цифры (выставляем черные точки)
        digitout (21, 13);                         // отрисовываем цифры (выставляем черные точки)
        digitout (44, (NowBrigtness / 15) / 10 );         // отрисовываем цифры (выставляем черные точки)
        digitout (65, (NowBrigtness / 15) % 10);          // отрисовываем цифры (выставляем черные точки)
        strip.setPixelColor(42, 0, 0, 0); strip.setPixelColor(43, 0, 0, 0);
        strip.show(); //освещаем режим
        break;
      }
    case 3: 
    {                               // Если режим "3"   - Установка времени отображения температуры

        if (key == 1)   
        { // Устанавливаем время показа температуры на -1 сек
          if (TempTime == 0) TempTime = 60; else TempTime = TempTime - 1 ;
        }
        if (key == 3)   
        { //Устанавливаем время показа температуры на +1 сек
          if (TempTime == 60) TempTime = 0; else TempTime = TempTime + 1;
        }
        setcolor (ColorMode);                     // задаём цвет всех элементов в зависимости от цветового режима
        digitout (0, 17);                      // отрисовываем цифры (выставляем черные точки)
        digitout (21, 17);                       // отрисовываем цифры (выставляем черные точки)
        digitout (44, TempTime / 10);       // отрисовываем цифры (выставляем черные точки)
        digitout (65, TempTime % 10);        // отрисовываем цифры (выставляем черные точки)
        strip.setPixelColor(42, 0, 0, 0); strip.setPixelColor(43, 0, 0, 0);
        strip.show(); //освещаем режим
        break;
      }
    case 4: 
    {                               // Если режим "4" - установка времени (час)
        if (key == 1)   
        { //уменьшаем часы
          if ((RTC.h) > 23) RTC.h = 23; 
          else RTC.h--; //установит времени (час)
          RTC.writeTime();          
        }
        if (key == 3)   
        { //увеличиваем часы
          if ((RTC.h) > 23) 
             RTC.h = 0;
          else RTC.h++; //установит времени (час)
          RTC.writeTime();
        }
        // задаём цвет всех элементов в зависимости от цветового режима
        RTC.readTime();                           // запрашиваем у модуля реального времени теущее время
        setcolor (ColorMode);                       // задаём цвет всех элементов в зависимости от цветового режима
        //       for (int i = 0; i < 14; i++) {               // задаём цвет часов серым
        //     strip.setPixelColor(i, 127, 127, 127);
        //     }
        if (( millis() % 1000 > 500))  
        {
          digitout (0, 15 );         // отрисовываем цифры (выставляем черные точки)
          digitout (21, 15 );         // отрисовываем цифры (выставляем черные точки)
        } 
        else
        {
          digitout (0, RTC.h / 10 );      // отрисовываем цифры (выставляем черные точки)
          digitout (21, RTC.h % 10 );      // отрисовываем цифры (выставляем черные точки)
        }
        digitout (44, RTC.h / 10 );      // отрисовываем цифры (выставляем черные точки)
        digitout (65, RTC.h % 10 );      // отрисовываем цифры (выставляем черные точки)
        //  if (( time.seconds) % 2 == 0)  {        // по четным секундам разделительные точки не светятся (можно сделать затухание)
        //  strip.setPixelColor(14, 0, 0, 0); strip.setPixelColor(15, 0, 0, 0);
        // }
        strip.show(); //освещаем режим
        break;
      }
    case 5: 
    {                               // Если режим "5" - установка времени (мин). При изменении минут секунды обнуляются
        if (key == 1)   
        { //уменьшаем минуты
          if ((RTC.m) > 59) RTC.m = 59; 
          else RTC.m--; //установит времени (мин)
          RTC.writeTime();
        }
        if (key == 3)   
        { //увеличиваем минуты
          if ((RTC.m) > 59) RTC.m = 0; 
          else RTC.m++; //установит времени (мин)
          RTC.writeTime();          
        }
        // задаём цвет всех элементов в зависимости от цветового режима
        RTC.readTime();                           // запрашиваем у модуля реального времени теущее время

        setcolor (ColorMode);                     // задаём цвет всех элементов в зависимости от цветового режима
        //     for (int i = 16; i < 30; i++) {               // задаём цвет минут серым
        //      strip.setPixelColor(i, 127, 127, 127);
        //   }

        if (( millis() % 1000 > 500))  
        {
          digitout (44, 15 );         // отрисовываем цифры (выставляем черные точки)
          digitout (65, 15 );         // отрисовываем цифры (выставляем черные точки)
        } 
        else
        {
          digitout (44, RTC.m / 10 );      // отрисовываем цифры (выставляем черные точки)
          digitout (65, RTC.m % 10 );      // отрисовываем цифры (выставляем черные точки)
        }
        digitout (0, RTC.h / 10 );      // отрисовываем цифры (выставляем черные точки)
        digitout (21, RTC.h % 10 );      // отрисовываем цифры (выставляем черные точки)

        //  if (( time.seconds) % 2 == 0)  {        // по четным секундам разделительные точки не светятся (можно сделать затухание)
        //  strip.setPixelColor(14, 0, 0, 0); strip.setPixelColor(15, 0, 0, 0);
        // }
        strip.show(); //освещаем режим
        break;
      }
    case 6: 
    {                               // Если режим "6" - Установка дня
        if (key == 1)   
        { //уменьшаем времени (мин)
          if ((RTC.dd) == 0) RTC.dd = 31; 
          else RTC.dd--; //установит дата)
          RTC.writeTime();
        }
        if (key == 3)   
        { //увеличиваем часы

          if ((RTC.dd) > 31) RTC.dd = 1; 
          else RTC.dd++; //установит дата
          RTC.writeTime();
        }
        // задаём цвет всех элементов в зависимости от цветового режима
        RTC.readTime();                           // запрашиваем у модуля реального времени теущее время
        setcolor (ColorMode);                       // задаём цвет всех элементов в зависимости от цветового режима
        //     for (int i = 0; i < 14; i++) {               // задаём цвет часов серым
        //       strip.setPixelColor(i, 127, 127, 127);
        //     }
        if (( millis() % 1000 > 500))  
        {
          digitout (0, 15 );         // отрисовываем цифры (выставляем черные точки)
          digitout (21, 15 );         // отрисовываем цифры (выставляем черные точки)
        } 
        else
        {
          digitout (0, RTC.dd / 10 );      // отрисовываем цифры (выставляем черные точки)
          digitout (21, RTC.dd % 10 );      // отрисовываем цифры (выставляем черные точки)
        }
        digitout (44, RTC.mm / 10 );      // отрисовываем цифры (выставляем черные точки)
        digitout (65, RTC.mm % 10 );      // отрисовываем цифры (выставляем черные точки)
        strip.setPixelColor(42, 0, 0, 0);     // Оставляем светиться нижнюю точку - разделитель дня и месяца
        strip.show(); //освещаем режим


        break;
      }
    case 7: 
    {                               // Если режим "7" - Установка месяца
        if (key == 1)   
        { //уменьшаем времени (мин)
          if ((RTC.mm) == 0) RTC.mm = 12; 
          else RTC.mm--; //установит дата)
          RTC.writeTime();
        }
        if (key == 3)   
        { //увеличиваем часы
          if ((RTC.mm) == 13) RTC.mm = 1; 
          else RTC.mm++; //установит дата)
          RTC.writeTime();
        }
        // задаём цвет всех элементов в зависимости от цветового режима
        RTC.readTime();                           // запрашиваем у модуля реального времени теущее время

        setcolor (ColorMode);                     // задаём цвет всех элементов в зависимости от цветового режима
        //       for (int i = 16; i < 86; i++) {               // задаём цвет минут серым
        //       strip.setPixelColor(i, 127, 127, 127);
        //     }

        if (( millis() % 1000 > 500))  
        {
          digitout (44, 15 );         // отрисовываем цифры (выставляем черные точки)
          digitout (65, 15 );         // отрисовываем цифры (выставляем черные точки)
        } else
        {
          digitout (44, RTC.mm / 10 );      // отрисовываем цифры (выставляем черные точки)
          digitout (65, RTC.mm % 10 );      // отрисовываем цифры (выставляем черные точки)
        }
        digitout (0, RTC.dd / 10 );      // отрисовываем цифры (выставляем черные точки)
        digitout (21, RTC.dd % 10 );      // отрисовываем цифры (выставляем черные точки)
        strip.setPixelColor(42, 0, 0, 0);     // Оставляем светиться нижнюю точку - разделитель дня и месяца
        strip.show(); //освещаем режим
        break;
      }
    case 8: 
    {                               // Если режим "8"-установка года
        if (key == 1)   
        { //уменьшаем год
          RTC.yyyy--; //установит дата)
          RTC.writeTime();
        }
        if (key == 3)   
        { //увеличиваем год
          RTC.yyyy++; //установит дата)
          RTC.writeTime();
        }
        setcolor (ColorMode);                     // задаём цвет всех элементов в зависимости от цветового режима

        //      for (int i = 0; i < 30; i++) {               // задаём цвет минут серым
        //        strip.setPixelColor(i, 127, 127, 127);
        //     }
        if (( millis() % 1000 > 500))
        {
          digitout (0, 15 );         // отрисовываем цифры (выставляем черные точки)
          digitout (21, 15 );
          digitout (44, 15 );         // отрисовываем цифры (выставляем черные точки)
          digitout (65, 15 );         // отрисовываем цифры (выставляем черные точки)
        } 
        else
        {
          digitout (0, 2);                      // отрисовываем цифры (выставляем черные точки)
          digitout (21, 0);                       // отрисовываем цифры (выставляем черные точки)
          digitout (44, (RTC.yyyy) / 10 );           // отрисовываем цифры (выставляем черные точки)
          digitout (65, (RTC.yyyy) % 10);           // отрисовываем цифры (выставляем черные точки)
        }
        strip.setPixelColor(42, 0, 0, 0); strip.setPixelColor(43, 0, 0, 0);
        strip.show(); //освещаем режим
        break;
      }
    case 9: 
    {                               // Если режим "9"   - Установка дня недели

        RTC.dow = RTC.calcDoW(RTC.yyyy, RTC.mm, RTC.dd);
 /*       if (key == 1)   
        { //уменьшаем времени (мин)
          if ((RTC.dow) == 0) RTC.dow = 7; 
          else RTC.dow--; //установит дата)
          RTC.writeTime();
        }
        if (key == 3)   
        { //увеличиваем часы
          if ((RTC.dow) == 8) RTC.dow = 1; 
          else RTC.dow++; //установит дата)
          RTC.writeTime();
        }
*/
        RTC.writeTime();        
        setcolor (ColorMode);                     // задаём цвет всех элементов в зависимости от цветового режима

        //      for (int i = 0; i < 86; i++) {               // задаём цвет минут серым
        //        strip.setPixelColor(i, 127, 127, 127);
        //     }
        if (( millis() % 1000 > 500))
        {
          digitout (65, 15 );         // отрисовываем цифры (выставляем черные точки)
        } 
        else
        {
          digitout (65, (RTC.dow));           // отрисовываем цифры (выставляем черные точки)
        }
        digitout (0, 16 );         // отрисовываем цифры (выставляем черные точки)
        digitout (21, 15 );
        digitout (44, 15 );
        strip.setPixelColor(42, 0, 0, 0); strip.setPixelColor(43, 0, 0, 0);
        strip.show(); //освещаем режим

        break;
      }
  }                                         // Конец обработки всех режимов
  eepromTick();                             // проверка не пора ли сохранить настройки
}                                           // Конец основного цикла


void setcolor (byte ColorMode) {
              // процедура, которая задаёт цвет всех элементов в зависимости от указанного цветового режима
  uint16_t i;
  if (ColorMode < 24) 
  {                     //12 цветов по цветовому колесу
    for (i = 0; i < 86; i++) 
    {
      strip.setPixelColor(i, WheelRGB((ColorMode * 255 / 24) & 255));
      if (i == 42 or i == 43)  strip.setPixelColor(i, WheelRBG((ColorMode * 255 / 24) & 255)); // для разделительных точек применяем модифицированное цветовое колесо
    }
  }
  if (ColorMode == 24) 
  {                    // цвет -белый
    for (i = 0; i < 86; i++) 
    {
      strip.setPixelColor(i, 255, 255, 255);
    }
  }

  if (ColorMode == 25) 
  {                    // цветовой режим -радуга с периодом 12 часов
    RTC.readTime();
    for (i = 0; i < 86; i++) 
    {
      strip.setPixelColor(i, WheelRGB(((RTC.h) * 255 / 12) & 255));                        // в этом режиме часы меняются от 1-12
      if (i == 42 or i == 43)  strip.setPixelColor(i, WheelRBG(((RTC.h) * 255) / 12 & 255)); // для разделительных точек применяем модифицированное цветовое колесо
    }
  }
    if (ColorMode == 26) 
    {                    // цветовой режим -радуга с периодом 24 часа
    RTC.readTime();
    for (i = 0; i < 86; i++) 
    {
      strip.setPixelColor(i, WheelRGB(((RTC.h) * 255 / 24) & 255));                        // в этом режиме часы меняются от 1-24
      if (i == 42 or i == 43)  strip.setPixelColor(i, WheelRBG(((RTC.h) * 255 / 24) & 255)); // для разделительных точек применяем модифицированное цветовое колесо
    }
  }

}           //- конец процедуры установки цвета



int get_key()   //Вывод нажатой клавиши
{
  if (digitalRead(4) == 0) return 1;
  if (digitalRead(3) == 0) return 2;
  if (digitalRead(2) == 0) return 3;
  return 0;
}

void tempout() 
{  //процедура отображения температуры

}

// Отображение цифр
void digitout(int digitposition, int digit) 
{
  switch ( digit) 
  {
    case 1: 
    {
        strip.setPixelColor(digitposition + 0, (0, 0, 0));
        strip.setPixelColor(digitposition+1, (0, 0, 0));
        strip.setPixelColor(digitposition+2, (0, 0, 0));
        strip.setPixelColor(digitposition + 3, (0, 0, 0));
        strip.setPixelColor(digitposition + 4, (0, 0, 0));
        strip.setPixelColor(digitposition + 5, (0, 0, 0));
        //strip.setPixelColor(digitposition + 6, (0, 0, 0));
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        strip.setPixelColor(digitposition + 9, (0, 0, 0));
        strip.setPixelColor(digitposition + 10, (0, 0, 0));
        strip.setPixelColor(digitposition + 11, (0, 0, 0));
        strip.setPixelColor(digitposition + 12, (0, 0, 0));
        strip.setPixelColor(digitposition + 13, (0, 0, 0));
        strip.setPixelColor(digitposition + 14, (0, 0, 0));
        strip.setPixelColor(digitposition + 15, (0, 0, 0));
        strip.setPixelColor(digitposition + 16, (0, 0, 0));
        strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 2: 
    {
        strip.setPixelColor(digitposition+0, (0, 0, 0));
        strip.setPixelColor(digitposition+1, (0, 0, 0));
        strip.setPixelColor(digitposition + 2, (0, 0, 0));
        //strip.setPixelColor(digitposition+3, (0, 0, 0));
        //strip.setPixelColor(digitposition+4, (0, 0, 0));
        //strip.setPixelColor(digitposition + 5, (0, 0, 0));
        //strip.setPixelColor(digitposition+6, (0, 0, 0));
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        //strip.setPixelColor(digitposition + 12, (0, 0, 0));
        //strip.setPixelColor(digitposition + 13, (0, 0, 0));
        //strip.setPixelColor(digitposition + 14, (0, 0, 0));
        //strip.setPixelColor(digitposition + 15, (0, 0, 0));
        //strip.setPixelColor(digitposition + 16, (0, 0, 0));
        //strip.setPixelColor(digitposition + 17, (0, 0, 0));
        strip.setPixelColor(digitposition + 18, (0, 0, 0));
        strip.setPixelColor(digitposition + 19, (0, 0, 0));
        strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 3: 
    {
        strip.setPixelColor(digitposition+0, (0, 0, 0));
        strip.setPixelColor(digitposition+1, (0, 0, 0));
        strip.setPixelColor(digitposition+2, (0, 0, 0));
        //strip.setPixelColor(digitposition+3, (0, 0, 0));
        //strip.setPixelColor(digitposition + 4, (0, 0, 0));
        //strip.setPixelColor(digitposition + 5, (0, 0, 0));
        //strip.setPixelColor(digitposition+6, (0, 0, 0));
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        strip.setPixelColor(digitposition + 12, (0, 0, 0));
        strip.setPixelColor(digitposition + 13, (0, 0, 0));
        strip.setPixelColor(digitposition + 14, (0, 0, 0));
        //strip.setPixelColor(digitposition + 15, (0, 0, 0));
        //strip.setPixelColor(digitposition + 16, (0, 0, 0));
        //strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
       break;
      }
    case 4: 
    {
        //strip.setPixelColor(digitposition + 0, (0, 0, 0));
        //strip.setPixelColor(digitposition+1, (0, 0, 0));
        //strip.setPixelColor(digitposition+2, (0, 0, 0));
        strip.setPixelColor(digitposition + 3, (0, 0, 0));
        strip.setPixelColor(digitposition + 4, (0, 0, 0));
        strip.setPixelColor(digitposition+5, (0, 0, 0));
        //strip.setPixelColor(digitposition+6, (0, 0, 0));;
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        strip.setPixelColor(digitposition + 12, (0, 0, 0));
        strip.setPixelColor(digitposition + 13, (0, 0, 0));
        strip.setPixelColor(digitposition + 14, (0, 0, 0));
        strip.setPixelColor(digitposition + 15, (0, 0, 0));
        strip.setPixelColor(digitposition + 16, (0, 0, 0));
        strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 5: 
    {
        //strip.setPixelColor(digitposition+0, (0, 0, 0));
        //strip.setPixelColor(digitposition + 1, (0, 0, 0));
        //strip.setPixelColor(digitposition+2, (0, 0, 0));
        //strip.setPixelColor(digitposition+3, (0, 0, 0));
        //strip.setPixelColor(digitposition + 4, (0, 0, 0));
        //strip.setPixelColor(digitposition+5, (0, 0, 0));
        strip.setPixelColor(digitposition+6, (0, 0, 0));
        strip.setPixelColor(digitposition + 7, (0, 0, 0));
        strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        strip.setPixelColor(digitposition + 12, (0, 0, 0));
        strip.setPixelColor(digitposition + 13, (0, 0, 0));
        strip.setPixelColor(digitposition + 14, (0, 0, 0));
        //strip.setPixelColor(digitposition + 15, (0, 0, 0));
        //strip.setPixelColor(digitposition + 16, (0, 0, 0));
        //strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }

    case 6: 
    {
        //strip.setPixelColor(digitposition+0, (0, 0, 0));
        //strip.setPixelColor(digitposition + 1, (0, 0, 0));
        //strip.setPixelColor(digitposition+2, (0, 0, 0));
        //strip.setPixelColor(digitposition+3, (0, 0, 0));
        //strip.setPixelColor(digitposition+4, (0, 0, 0));
        //strip.setPixelColor(digitposition+5, (0, 0, 0));
        strip.setPixelColor(digitposition+6, (0, 0, 0));
        strip.setPixelColor(digitposition + 7, (0, 0, 0));
        strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        //strip.setPixelColor(digitposition + 12, (0, 0, 0));
        //strip.setPixelColor(digitposition + 13, (0, 0, 0));
        //strip.setPixelColor(digitposition + 14, (0, 0, 0));
        //strip.setPixelColor(digitposition + 15, (0, 0, 0));
        //strip.setPixelColor(digitposition + 16, (0, 0, 0));
        //strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 7: 
    {
        strip.setPixelColor(digitposition+0, (0, 0, 0));
        strip.setPixelColor(digitposition+1, (0, 0, 0));
        strip.setPixelColor(digitposition+2, (0, 0, 0));
        //strip.setPixelColor(digitposition + 3, (0, 0, 0));
        //strip.setPixelColor(digitposition + 4, (0, 0, 0));
        //strip.setPixelColor(digitposition + 5, (0, 0, 0));
        //strip.setPixelColor(digitposition + 6, (0, 0, 0));
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        strip.setPixelColor(digitposition + 9, (0, 0, 0));
        strip.setPixelColor(digitposition + 10, (0, 0, 0));
        strip.setPixelColor(digitposition + 11, (0, 0, 0));
        strip.setPixelColor(digitposition + 12, (0, 0, 0));
        strip.setPixelColor(digitposition + 13, (0, 0, 0));
        strip.setPixelColor(digitposition + 14, (0, 0, 0));
        strip.setPixelColor(digitposition + 15, (0, 0, 0));
        strip.setPixelColor(digitposition + 16, (0, 0, 0));
        strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 8: 
    {
        //strip.setPixelColor(digitposition+0, (0, 0, 0));
        //strip.setPixelColor(digitposition+1, (0, 0, 0));
        //strip.setPixelColor(digitposition+2, (0, 0, 0));
        //strip.setPixelColor(digitposition+3, (0, 0, 0));
        //strip.setPixelColor(digitposition+4, (0, 0, 0));
        //strip.setPixelColor(digitposition+5, (0, 0, 0));
        //strip.setPixelColor(digitposition+6, (0, 0, 0));
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        //strip.setPixelColor(digitposition + 12, (0, 0, 0));
        //strip.setPixelColor(digitposition + 13, (0, 0, 0));
        //strip.setPixelColor(digitposition + 14, (0, 0, 0));
        //strip.setPixelColor(digitposition + 15, (0, 0, 0));
        //strip.setPixelColor(digitposition + 16, (0, 0, 0));
        //strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 9: 
    {
        //strip.setPixelColor(digitposition+0, (0, 0, 0));
        //strip.setPixelColor(digitposition+1, (0, 0, 0));
        //strip.setPixelColor(digitposition+2, (0, 0, 0));
        //strip.setPixelColor(digitposition+3, (0, 0, 0));
        //strip.setPixelColor(digitposition+4, (0, 0, 0));
        //strip.setPixelColor(digitposition+5, (0, 0, 0));
        //strip.setPixelColor(digitposition+6, (0, 0, 0));
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        strip.setPixelColor(digitposition + 12, (0, 0, 0));
        strip.setPixelColor(digitposition + 13, (0, 0, 0));
        strip.setPixelColor(digitposition + 14, (0, 0, 0));
        //strip.setPixelColor(digitposition + 15, (0, 0, 0));
        //strip.setPixelColor(digitposition + 16, (0, 0, 0));
        //strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 0: 
    {
        //strip.setPixelColor(digitposition+0, (0, 0, 0));
        //strip.setPixelColor(digitposition+1, (0, 0, 0));
        //strip.setPixelColor(digitposition+2, (0, 0, 0));
        //strip.setPixelColor(digitposition+3, (0, 0, 0));
        //strip.setPixelColor(digitposition+4, (0, 0, 0));
        //strip.setPixelColor(digitposition+5, (0, 0, 0));
        //strip.setPixelColor(digitposition+6, (0, 0, 0));
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        strip.setPixelColor(digitposition + 9, (0, 0, 0));
        strip.setPixelColor(digitposition + 10, (0, 0, 0));
        strip.setPixelColor(digitposition + 11, (0, 0, 0));
        //strip.setPixelColor(digitposition + 12, (0, 0, 0));
        //strip.setPixelColor(digitposition + 13, (0, 0, 0));
        //strip.setPixelColor(digitposition + 14, (0, 0, 0));
        //strip.setPixelColor(digitposition + 15, (0, 0, 0));
        //strip.setPixelColor(digitposition + 16, (0, 0, 0));
        //strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 11: 
    {          //символ градуса цельсия и знак минус _
        strip.setPixelColor(digitposition+0, (0, 0, 0));
        strip.setPixelColor(digitposition+1, (0, 0, 0));
        strip.setPixelColor(digitposition+2, (0, 0, 0));
        //strip.setPixelColor(digitposition+3, (0, 0, 0));
        //strip.setPixelColor(digitposition+4, (0, 0, 0));
        //strip.setPixelColor(digitposition+5, (0, 0, 0));
        //strip.setPixelColor(digitposition+6, (0, 0, 0));
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        strip.setPixelColor(digitposition + 9, (0, 0, 0));
        strip.setPixelColor(digitposition + 10, (0, 0, 0));
        strip.setPixelColor(digitposition + 11, (0, 0, 0));
        //strip.setPixelColor(digitposition + 12, (0, 0, 0));
        //strip.setPixelColor(digitposition + 13, (0, 0, 0));
        //strip.setPixelColor(digitposition + 14, (0, 0, 0));
        strip.setPixelColor(digitposition + 15, (0, 0, 0));
        strip.setPixelColor(digitposition + 16, (0, 0, 0));
        strip.setPixelColor(digitposition + 17, (0, 0, 0));
        strip.setPixelColor(digitposition + 18, (0, 0, 0));
        strip.setPixelColor(digitposition + 19, (0, 0, 0));
        strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 12: 
    {          //символ "C"  "color"
        //strip.setPixelColor(digitposition+0, (0, 0, 0));
        //strip.setPixelColor(digitposition+1, (0, 0, 0));
        //strip.setPixelColor(digitposition+2, (0, 0, 0));
        strip.setPixelColor(digitposition+3, (0, 0, 0));
        strip.setPixelColor(digitposition+4, (0, 0, 0));
        strip.setPixelColor(digitposition+5, (0, 0, 0));
        strip.setPixelColor(digitposition+6, (0, 0, 0));
        strip.setPixelColor(digitposition + 7, (0, 0, 0));
        strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        //strip.setPixelColor(digitposition + 12, (0, 0, 0));
        //strip.setPixelColor(digitposition + 13, (0, 0, 0));
        //strip.setPixelColor(digitposition + 14, (0, 0, 0));
        //strip.setPixelColor(digitposition + 15, (0, 0, 0));
        //strip.setPixelColor(digitposition + 16, (0, 0, 0));
        //strip.setPixelColor(digitposition + 17, (0, 0, 0));
        strip.setPixelColor(digitposition + 18, (0, 0, 0));
        strip.setPixelColor(digitposition + 19, (0, 0, 0));
        strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 13: 
    {          //символ "-"
        strip.setPixelColor(digitposition+0, (0, 0, 0));
        strip.setPixelColor(digitposition+1, (0, 0, 0));
        strip.setPixelColor(digitposition+2, (0, 0, 0));
        strip.setPixelColor(digitposition+3, (0, 0, 0));
        strip.setPixelColor(digitposition+4, (0, 0, 0));
        strip.setPixelColor(digitposition+5, (0, 0, 0));
        strip.setPixelColor(digitposition+6, (0, 0, 0));
        strip.setPixelColor(digitposition + 7, (0, 0, 0));
        strip.setPixelColor(digitposition + 8, (0, 0, 0));
        strip.setPixelColor(digitposition + 9, (0, 0, 0));
        strip.setPixelColor(digitposition + 10, (0, 0, 0));
        strip.setPixelColor(digitposition + 11, (0, 0, 0));
        strip.setPixelColor(digitposition + 12, (0, 0, 0));
        strip.setPixelColor(digitposition + 13, (0, 0, 0));
        strip.setPixelColor(digitposition + 14, (0, 0, 0));
        strip.setPixelColor(digitposition + 15, (0, 0, 0));
        strip.setPixelColor(digitposition + 16, (0, 0, 0));
        strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 14: 
    {          //символ "b" "яркость"
        strip.setPixelColor(digitposition+0, (0, 0, 0));
        strip.setPixelColor(digitposition+1, (0, 0, 0));
        strip.setPixelColor(digitposition+2, (0, 0, 0));
        strip.setPixelColor(digitposition+3, (0, 0, 0));
        strip.setPixelColor(digitposition+4, (0, 0, 0));
        strip.setPixelColor(digitposition+5, (0, 0, 0));
        //strip.setPixelColor(digitposition+6, (0, 0, 0));
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        //strip.setPixelColor(digitposition + 12, (0, 0, 0));
        //strip.setPixelColor(digitposition + 13, (0, 0, 0));
        //strip.setPixelColor(digitposition + 14, (0, 0, 0));
        //strip.setPixelColor(digitposition + 15, (0, 0, 0));
        //strip.setPixelColor(digitposition + 16, (0, 0, 0));
        //strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 15: 
    {          //символ " " "пустота"
        strip.setPixelColor(digitposition+0, (0, 0, 0));
        strip.setPixelColor(digitposition+1, (0, 0, 0));
        strip.setPixelColor(digitposition+2, (0, 0, 0));
        strip.setPixelColor(digitposition+3, (0, 0, 0));
        strip.setPixelColor(digitposition+4, (0, 0, 0));
        strip.setPixelColor(digitposition+5, (0, 0, 0));
        strip.setPixelColor(digitposition+6, (0, 0, 0));
        strip.setPixelColor(digitposition + 7, (0, 0, 0));
        strip.setPixelColor(digitposition + 8, (0, 0, 0));
        strip.setPixelColor(digitposition + 9, (0, 0, 0));
        strip.setPixelColor(digitposition + 10, (0, 0, 0));
        strip.setPixelColor(digitposition + 11, (0, 0, 0));
        strip.setPixelColor(digitposition + 12, (0, 0, 0));
        strip.setPixelColor(digitposition + 13, (0, 0, 0));
        strip.setPixelColor(digitposition + 14, (0, 0, 0));
        strip.setPixelColor(digitposition + 15, (0, 0, 0));
        strip.setPixelColor(digitposition + 16, (0, 0, 0));
        strip.setPixelColor(digitposition + 17, (0, 0, 0));
        strip.setPixelColor(digitposition + 18, (0, 0, 0));
        strip.setPixelColor(digitposition + 19, (0, 0, 0));
        strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 16: 
    {          //символ "d" "день недели"
        strip.setPixelColor(digitposition+0, (0, 0, 0));
        strip.setPixelColor(digitposition+1, (0, 0, 0));
        strip.setPixelColor(digitposition+2, (0, 0, 0));
        //strip.setPixelColor(digitposition+3, (0, 0, 0));
        //strip.setPixelColor(digitposition+4, (0, 0, 0));
        //strip.setPixelColor(digitposition+5, (0, 0, 0));
        //strip.setPixelColor(digitposition+6, (0, 0, 0));
        //strip.setPixelColor(digitposition + 7, (0, 0, 0));
        //strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        //strip.setPixelColor(digitposition + 12, (0, 0, 0));
        //strip.setPixelColor(digitposition + 13, (0, 0, 0));
        //strip.setPixelColor(digitposition + 14, (0, 0, 0));
        strip.setPixelColor(digitposition + 15, (0, 0, 0));
        strip.setPixelColor(digitposition + 16, (0, 0, 0));
        strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }
    case 17: 
    {          //символ "t"
        strip.setPixelColor(digitposition+0, (0, 0, 0));
        strip.setPixelColor(digitposition+1, (0, 0, 0));
        strip.setPixelColor(digitposition+2, (0, 0, 0));
        strip.setPixelColor(digitposition+3, (0, 0, 0));
        strip.setPixelColor(digitposition+4, (0, 0, 0));
        strip.setPixelColor(digitposition+5, (0, 0, 0));
        strip.setPixelColor(digitposition+6, (0, 0, 0));
        strip.setPixelColor(digitposition + 7, (0, 0, 0));
        strip.setPixelColor(digitposition + 8, (0, 0, 0));
        //strip.setPixelColor(digitposition + 9, (0, 0, 0));
        //strip.setPixelColor(digitposition + 10, (0, 0, 0));
        //strip.setPixelColor(digitposition + 11, (0, 0, 0));
        //strip.setPixelColor(digitposition + 12, (0, 0, 0));
        //strip.setPixelColor(digitposition + 13, (0, 0, 0));
        //strip.setPixelColor(digitposition + 14, (0, 0, 0));
        //strip.setPixelColor(digitposition + 15, (0, 0, 0));
        //strip.setPixelColor(digitposition + 16, (0, 0, 0));
        //strip.setPixelColor(digitposition + 17, (0, 0, 0));
        //strip.setPixelColor(digitposition + 18, (0, 0, 0));
        //strip.setPixelColor(digitposition + 19, (0, 0, 0));
        //strip.setPixelColor(digitposition + 20, (0, 0, 0));
        break;
      }

  }
}

// функция цветового колеса, которое возвращает RGB цвет для ленты (аргументу 0-255 соответсует поворот колеса от 0 до 359 градусов)
uint32_t WheelRGB(byte WheelPos) 
{
  WheelPos = 255 - WheelPos;          // разделительные точки между часами и мунутами на базе WS2812 с измененными цветами G и B

  if (WheelPos < 85) 
  {
    return strip.Color((255 - WheelPos * 3) , (WheelPos * 3), 0);
  }
  if (WheelPos < 170) 
  {
    WheelPos -= 85;
    return strip.Color(0,  (255 - WheelPos * 3) ,  (WheelPos * 3) );
  }
  WheelPos -= 170;
  return strip.Color( (WheelPos * 3), 0, (255 - WheelPos * 3));
}

uint32_t WheelRBG(byte WheelPos) 
{    // Это колесо специально для разделительных точек (транспонированы между собой синий и зеленый цвета)
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) 
  {
    return strip.Color(degree * (255 - WheelPos * 3) / 100, 0, degree * ( WheelPos * 3) / 100);
  }
  if (WheelPos < 170) 
  {
    WheelPos -= 85;
    return strip.Color(0, degree * (WheelPos * 3) / 100, degree * (255 - WheelPos * 3) / 100);
  }
  WheelPos -= 170;
  return strip.Color(degree * (WheelPos * 3) / 100, degree * (255 - WheelPos * 3) / 100, 0);
}

void updateEEPROM() 
{
  EEPROM.updateByte(1, NowBrigtness);
  EEPROM.updateByte(2, ColorMode);
  EEPROM.updateByte(3, TempTime);
}
void readEEPROM() {
  NowBrigtness = EEPROM.readByte(1);
  ColorMode = EEPROM.readByte(2);
  TempTime = EEPROM.readByte(3);
}
void eepromTick() 
{
  if (eeprom_flag)
    if (millis() - KeyPressed > 60000) {  // 60 секунд после последнего нажатия с пульта
      eeprom_flag = false;
      updateEEPROM();
    }
}
