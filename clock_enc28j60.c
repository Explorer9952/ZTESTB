#include <UIPEthernet.h>
#include <UIPUdp.h>
#include <LiquidCrystal_I2C.h>
#include <TimeLib.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
const char* ntpServer = "mntp1.tv.zala.by";
const int timeZoneOffset = 3;

EthernetUDP Udp;
unsigned int localPort = 8888;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

unsigned long lastSync = 0;
const unsigned long syncInterval = 3600000;
char temperature[6] = "--";
char windSpeed[6] = "--";
char pressure[6] = "--";


// Переключение между пятью параметрами
unsigned long lastSwitch = 0;
const unsigned long switchInterval = 10000; 
int displayMode = 0; // 0-temp,1-wind,2-pressure,3-humidity,4-visibility

const char daysOfWeek[7][10] = {
  "Sunday", "Monday", "Tuesday", "Wednesday",
  "Thursday", "Friday", "Saturday"
};

byte Degree[8] = {
  0b00110, 0b01001, 0b01001, 0b00110,
  0b00000, 0b00000, 0b00000, 0b00000
};

void print2digits(int number);
bool fetchWeather();
bool syncTime();
void sendNTPpacket();
void updateDisplay();

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Starting U-Boot");
  lcd.setCursor(0,1);
  delay(100);
  lcd.print("CPU running at 16MHz");
  lcd.setCursor(0,2);
  delay(100);
  lcd.print("DRAM : 2 KB");
  lcd.setCursor(0,3);
  delay(100);
  lcd.print("Booting image");
  delay(700);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Verifying sum OK");
  delay(100);
  lcd.setCursor(0,1);
  lcd.print("Uncompress Kernel OK");
  delay(100);
  lcd.setCursor(0,2);
  lcd.print("Starting kernel...");
  delay(100);
  lcd.setCursor(0,3);
  lcd.print("Console OFF");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("BusyBox v1.36.1");
  lcd.setCursor(0,1);
  lcd.print("root@clk $Cant..");
  lcd.setCursor(0,2);
  delay(100);
  lcd.print("STOP!");
  delay(1000);
  lcd.createChar(0, Degree);
  
  if (Ethernet.begin(mac) == 0) {
    lcd.clear();
    lcd.print("Error 1901");
    lcd.setCursor(0,1);
    lcd.print("Network cable is not");
    lcd.setCursor(0,2);
    lcd.print("plugged!");
    while(1);
  }
  
  Udp.begin(localPort);

  if(syncTime()) {
    lcd.clear();
    lcd.print("Time synced!");
    lcd.setCursor(0,1);
    lcd.print("Weather synced!");
    lcd.setCursor(0,2);
    lcd.print("By Explorer");
    delay(2000);
  } else {
    lcd.clear();
    lcd.print("Error 1901");
    lcd.setCursor(0,1);
    lcd.print("Network cable is not");
    lcd.setCursor(0,2);
    lcd.print("plugged!");
    while(1);
  }
  
  fetchWeather();
}

void loop() {
  static unsigned long lastUpdate = 0;

  if (millis() - lastSync > syncInterval && syncTime()) {
    lastSync = millis();
  }

  if (millis() - lastUpdate > 60000 && fetchWeather()) {
    lastUpdate = millis();
  }

  // Управление подсветкой по времени и месяцу
  static bool displayWasOn = true;
  int currentHour = hour();
  int currentMinute = minute();
  int currentMonth = month();
  bool shouldDisplayBeOn = false;

  if (currentMonth >= 5 && currentMonth <= 8) {
    // Май–Август: подсветка включена весь день
    shouldDisplayBeOn = true;
  } else {
    // Сентябрь–Апрель: подсветка с 6:00 до 10:30
    if ((currentHour > 6 && currentHour < 10) ||
        (currentHour == 6 && currentMinute >= 0) ||
        (currentHour == 10 && currentMinute <= 30)) {
      shouldDisplayBeOn = true;
    }
  }

  if (shouldDisplayBeOn && !displayWasOn) {
    lcd.backlight();
    displayWasOn = true;
  } else if (!shouldDisplayBeOn && displayWasOn) {
    lcd.noBacklight();
    displayWasOn = false;
  }

  // Обновление дисплея
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 1000) {
    lastDisplayUpdate = millis();
    updateDisplay();

    if (millis() - lastSwitch >= switchInterval) {
      lastSwitch = millis();
      displayMode = (displayMode + 1) % 3; // 3 режима: температура, ветер, давление
    }
  }
}


bool fetchWeather() {
  EthernetClient client;
  if (client.connect("api.openweathermap.org", 80)) {
    client.print(F("GET /data/2.5/weather?id=625665&units=metric&appid=eee274f24b6d4a4f5cf557d64dace8c0 HTTP/1.1\r\n"));
    client.print(F("Host: api.openweathermap.org\r\n"));
    client.print(F("Connection: close\r\n\r\n"));
    
    unsigned long timeout = millis();
    char c;
    bool foundTemp = false;
    bool foundWind = false;
    bool foundPressure = false;
    char tempBuf[10];
    char windBuf[10];
    char pressureBuf[10];
    byte bufPos = 0;

    while(client.connected() && millis() - timeout < 3000) {
      while(client.available()) {
        c = client.read();
        
        // Существующий парсинг температуры (без изменений)
        if(!foundTemp && c == '\"') {
          if(client.read() == 't' && client.read() == 'e' && client.read() == 'm' && 
             client.read() == 'p' && client.read() == '\"') {
            client.read();
            bufPos = 0;
            while(client.available() && bufPos < sizeof(tempBuf)-1) {
              c = client.read();
              if(c == ',' || c == '}') break;
              if(isdigit(c) || c == '.' || c == '-') {
                tempBuf[bufPos++] = c;
              }
            }
            tempBuf[bufPos] = '\0';
            snprintf(temperature, sizeof(temperature), "%s", tempBuf);
            foundTemp = true;

            // Парсинг давления (без изменений)
            while(client.available() && !foundPressure) {
              c = client.read();
              if(c == '\"') {
                if(client.read() == 'p' && client.read() == 'r' && client.read() == 'e' && 
                   client.read() == 's' && client.read() == 's' && client.read() == 'u' &&
                   client.read() == 'r' && client.read() == 'e' && client.read() == '\"') {
                  client.read();
                  bufPos = 0;
                  while(client.available() && bufPos < sizeof(pressureBuf)-1) {
                    c = client.read();
                    if(c == ',' || c == '}') break;
                    if(isdigit(c)) {
                      pressureBuf[bufPos++] = c;
                    }
                  }
                  pressureBuf[bufPos] = '\0';
                  snprintf(pressure, sizeof(pressure), "%s", pressureBuf);
                  foundPressure = true;
                  break;
                }
              }
            }
          }
        }
        
        // Парсинг ветра (без изменений)
        if(foundTemp && !foundWind && c == '\"') {
          if(client.read() == 'w' && client.read() == 'i' && client.read() == 'n' && 
             client.read() == 'd' && client.read() == '\"') {
            while(client.available()) {
              c = client.read();
              if(c == '\"') {
                if(client.read() == 's' && client.read() == 'p' && client.read() == 'e' && 
                   client.read() == 'e' && client.read() == 'd' && client.read() == '\"') {
                  client.read();
                  bufPos = 0;
                  while(client.available() && bufPos < sizeof(windBuf)-1) {
                    c = client.read();
                    if(c == ',' || c == '}') break;
                    if(isdigit(c) || c == '.' || c == '-') {
                      windBuf[bufPos++] = c;
                    }
                  }
                  windBuf[bufPos] = '\0';
                  snprintf(windSpeed, sizeof(windSpeed), "%s", windBuf);
                  foundWind = true;
                  break;
                }
              }
            }
          }
        }

        // Парсинг влажности (без изменений)


        // Новый парсинг видимости (добавлен в конец)

      }
    }
    client.stop();
    return foundTemp && foundWind && foundPressure;
  }
  strcpy(temperature, "Err");
  strcpy(windSpeed, "Err");
  strcpy(pressure, "Err");

  return false;
}

// Остальные функции БЕЗ ИЗМЕНЕНИЙ
bool syncTime() {
  sendNTPpacket();
  unsigned long start = millis();
  while(millis() - start < 2000) {
    if(Udp.parsePacket() >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900 = (unsigned long)packetBuffer[40] << 24 | 
                                   (unsigned long)packetBuffer[41] << 16 | 
                                   (unsigned long)packetBuffer[42] << 8 | 
                                   (unsigned long)packetBuffer[43];
      setTime(secsSince1900 - 2208988800UL + timeZoneOffset * 3600);
      return true;
    }
    delay(10);
  }
  return false;
}

void sendNTPpacket() {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0xE3;
  Udp.beginPacket(ntpServer, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void updateDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Time: "));
  print2digits(hour());
  lcd.print(F(":"));
  print2digits(minute());
  lcd.print(F(":"));
  print2digits(second());
  
  lcd.setCursor(0, 1);
  lcd.print(F("Date: "));
  print2digits(day());
  lcd.print(F("/"));
  print2digits(month());
  lcd.print(F("/"));
  lcd.print(year());
  
  lcd.setCursor(0, 2);
  lcd.print(F("Day: "));
  lcd.print(daysOfWeek[weekday()-1]); 
    
  lcd.setCursor(0, 3);  
  switch(displayMode) {
    case 0:
      lcd.print(F("Temp: "));
      lcd.print(temperature);
      lcd.write(byte(0));
      lcd.print("C");
      break;
    case 1:
      lcd.print(F("Wind: "));
      lcd.print(windSpeed);
      lcd.print(" m/s");
      break;
    case 2:
      lcd.print(F("Press: "));
      lcd.print(pressure);
      lcd.print(" hPa");
      break;

  }
}

void print2digits(int number) {
  if(number < 10) lcd.print(F("0"));
  lcd.print(number);
}
