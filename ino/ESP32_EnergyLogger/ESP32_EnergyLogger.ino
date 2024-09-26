#include <SPI.h>
#include <Wire.h>
//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "time.h"
#include <ESP32Time.h>
#include <Adafruit_ADS1X15.h>
#include <ESPmDNS.h>
//#include <NetworkUdp.h>  //Uncomment for boards addon esp32 >= 3.0.0 (Current 2.0.17)
#include <ArduinoOTA.h>
#include "client_secrets.h"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#define VOLTAGE_MULTIPLIER 2073000.0/10020.0*1.0633
#define CURRENT_MULTIPLIER 30.0

#define BTN_PIN 27
#define READY_PIN 17

#define TURNOFF_TIMER 10000     //Time for turning off screen when idling
#define REINIT_COOLDOWN 30000   //Cooldown for connection reinitialization when offline

#define LCDWidth                        display.getDisplayWidth()
#define ALIGN_CENTER(t)                 ((LCDWidth - (display.getUTF8Width(t))) / 2)
#define ALIGN_RIGHT(t)                  (LCDWidth -  display.getUTF8Width(t))
#define ALIGN_LEFT                      0

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


//######################################### BITMAPS #########################################

const uint8_t splashLogo[] = {0x3f,0xcf,0xc7,0xf,0x00,0x00,0x00,0x60,0x00,0x00,0x00,0x00,0x00,       
                              0xbf,0xdf,0xdf,0xf,0x00,0x00,0x00,0x60,0x00,0x00,0x00,0x00,0x00,        
                              0x83,0xd9,0xd8,0x00,0x00,0x00,0x00,0x60,0x00,0x00,0x00,0x00,0x00,      
                              0x83,0xc3,0xd8,0xe0,0xe3,0x79,0xfe,0x6c,0xe0,0xf1,0xf9,0x3c,0xf,
                              0xbf,0xcf,0xd8,0xef,0xf7,0x7b,0xff,0x6c,0xf0,0xfb,0xfd,0x7e,0xf,
                              0x3f,0xcf,0xdf,0x6f,0x36,0x1b,0xb3,0x67,0x30,0x9b,0xcd,0x66,0x3,
                              0x3,0xdc,0xcf,0x60,0xf6,0x1b,0xb3,0x67,0x30,0x9b,0xcd,0x7e,0x3, 
                              0x83,0xd9,0xc0,0x60,0x36,0x18,0xb3,0x67,0x30,0x9b,0xcd,0x6,0x3, 
                              0x83,0xd9,0xc0,0x60,0x36,0x1b,0xb3,0x67,0x30,0x9b,0xcd,0x66,0x3,
                              0xbf,0xdf,0xc0,0x6f,0xf6,0x1b,0xbf,0xe7,0xf3,0xfb,0xfd,0x7e,0x3,
                              0x3f,0xcf,0xc0,0x6f,0xe6,0x19,0x3e,0xe3,0xe3,0xf1,0xf9,0x3c,0x3,
                              0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x3,0x00,0x80,0xc1,0x00,0x00,
                              0x00,0x00,0x00,0x00,0x00,0x00,0xbf,0x3,0x00,0xf8,0xfd,0x00,0x00,
                              0x00,0x00,0x00,0x00,0x00,0x00,0x9e,0x1,0x00,0xf0,0x78,0x00,0x00};

const uint8_t wifiOnLogo[] = {0b01111110,
                              0b10000001,
                              0b00111100,
                              0b01000010,
                              0b00011000,
                              0b00100100,
                              0b00000000,
                              0b00011000};

const uint8_t wifiTransferLogo[] = {0b01111110,
                              0b10000001,
                              0b00110000,
                              0b00000100,
                              0b00111110,
                              0b00000000,
                              0b01111100,
                              0b00100000};

const uint8_t wifiOffLogo[] = {0b01111101,
                              0b10000010,
                              0b00110100,
                              0b01001010,
                              0b00010000,
                              0b00100100,
                              0b01000000,
                              0b10011000};  

const uint8_t lockedLogo[] = {0b00011100,
                              0b00100010,
                              0b00100010,
                              0b01111111,
                              0b01000001,
                              0b01000001,
                              0b01000001,
                              0b01111111};

const uint8_t readingLogo[] = {0b01111,
                              0b10001,
                              0b01111,
                              0b10001,
                              0b10001};           


//######################################### VAR DECLARATION #########################################  

const char* ssid = SSID_NAME;
const char* password = WIFI_PASSWORD;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

ESP32Time rtc(gmtOffset_sec);
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C display(U8G2_R2, 22, 21);
Adafruit_ADS1115 ads;

bool locked = false;
bool unlockRelease = false;
bool pressed = false;
bool isOn = true;
bool isUpdatingDisplay = false;
bool isReading = false;
bool isReadingVoltage = true;
bool isOnline = true;
unsigned int prevRead = 0;
unsigned int connectionTimer = 0;
unsigned int pressedTimer = 0;
unsigned int serialTimer;
unsigned int refreshTimer = 0;
unsigned int onTimer = 0;
unsigned int rmsTimer = 0;
unsigned int readingTimer = 0;
unsigned int utilTimer = 0;
unsigned int frameTimer = 0;

int frameCount = 0;
int tileCount = 0;
int sampleCount = 0;
double rmsBuffer = 0;

double rmsVoltage = 0;
double rmsCurrent = 0;
double avgPower = 0;
double whCounter = 0;
double whMonthCounter = 0;
double whDayCounter = 0;
double offlineWhCounter = 0;
double offlineWhMonthCounter = 0;
double offlineWhDayCounter = 0;

unsigned char page = 0;
unsigned long epoch;
bool DST = false;
int prevHour = 0;

char parseBuff[20];
char ntoaBuff[20];
char requestBuff[300];

//Modify as needed
const char* requestTemplate = "http://<SERVER_IP_ADDR>/energy/send_data?timestamp=%lu&voltage=%f&current=%f&energy=%f";
const char* apiQueryTime = "http://worldtimeapi.org/api/timezone/Europe/Rome";
const char* apiQueryData = "http://<SERVER_IP_ADDR>/energy/initialize";

volatile bool new_data = false;



//######################################### FUNCTIONS #########################################

//ISR data sample ready
void IRAM_ATTR NewDataReadyISR() {
  new_data = true;
}

void sendData(){
  //Send template filled
  HTTPClient http;
  unsigned long ts = rtc.getEpoch() - (DST ? daylightOffset_sec : 0) - gmtOffset_sec;
  sprintf(requestBuff,requestTemplate,ts,rmsVoltage,rmsCurrent,whCounter);
  http.begin(requestBuff);
  int httpResponseCode = http.GET();
  if(httpResponseCode<=0){
    http.end();
    initConnection();
  }
  http.end();
  //reset counters if needed
  int currHour = rtc.getHour(true);
  if(!currHour && prevHour == 23){
    whDayCounter = 0;
    offlineWhDayCounter = 0;
    if(rtc.getDay() == 1){
      whMonthCounter = 0;
      offlineWhMonthCounter = 0;
    } 
  }
  prevHour = currHour;
}


/* Performs 3 attempts to establish connection with ntp server for time
 * and with local server to send sampled data.
 * After 3 failed attempts logger enters in offline mode, display is cleared.
 * While trying to connect user can skip connection and enter manually in offline mode.
 */

void initConnection(){

  HTTPClient http;
  http.begin(apiQueryTime);

  int count = 0;

  while(1){
    if(WiFi.status() == WL_CONNECTED){
      int httpResponseCode = http.GET();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      if (httpResponseCode>0) {
        JSONVar payload = JSON.parse(http.getString());
        epoch = payload["unixtime"];
        DST = payload["dst"];
        epoch += DST ? daylightOffset_sec : 0;
        rtc.setTime(epoch,0);
        http.end();
        http.begin(apiQueryData);
        int httpResponseCode = http.GET();
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        if (httpResponseCode>0) {
          //Serial.println(http.getString());
          JSONVar payload = JSON.parse(http.getString());
          String status = payload["status"];
          if (status == "ok"){
            if(whCounter == 0){
              whCounter = payload["value"]["tot"];
              whDayCounter = payload["value"]["day"];
              whMonthCounter = payload["value"]["month"];
            }
            whCounter += offlineWhCounter;
            whDayCounter += offlineWhDayCounter;
            whMonthCounter += offlineWhMonthCounter;
            offlineWhCounter = 0;
            offlineWhDayCounter = 0;
            offlineWhMonthCounter = 0;
            isOnline = true;
            Serial.println("End initialization");
            break;
          }
          //Serial.println(payload["energy"]); 
        }
      }
    }
    if(isOnline){
      connectionTimer = millis();
      display.clearDisplay();
      display.display();
      break;
    }
    display.setFont(u8g2_font_6x10_mf);
    display.setFontDirection(0);
    display.setCursor(0,32);
    display.println("HOLD TO SKIP CONNECTION");
    display.sendBuffer();

    if(count >= 3 || digitalRead(BTN_PIN) == LOW){
      isOnline = false;
      connectionTimer = millis();
      display.clearDisplay();
      display.display();
      break;
    } 
    Serial.print("Attempt n°");
    Serial.print(count+1);
    count++;
  }
  http.end();
}


/* Load graphics into display buffer according to current menu page
 * -Page title
 * -Value + m.u.
 * -Time
 * -Wifi status
 * -Reading indicator
 * -Lock status
 */

void loadBuffer(){

  //Load display menu page in buffer(refresh rate ~30Hz)

  if(isOn && !isUpdatingDisplay && millis() - refreshTimer > 30){
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_mf);
    display.setFontDirection(0);
    display.setCursor(0,7);
    String unit = "";             
    switch(page){
      case 0:
        display.print("CONSUMO GIORNO");
        unit = "kWh";
        break;
      case 1:
        display.print("CONSUMO MESE");
        unit = "kWh";
        break;
      case 2:
        display.print("CONSUMO TOTALE");
        unit = "kWh";
        break;
      case 3:
        display.print("TENSIONE RMS");
        unit = "V";
        break;
      case 4:
        display.print("CORRENTE RMS");
        unit = "A";
        break;
      case 5:
        display.print("POTENZA ISTANT");
        unit = "W";
        break;
    }
    display.setFontDirection(0);
    
    if(isOnline){
      display.setCursor(98,7);
      display.println(rtc.getTime("%H:%M"));
    } 
    else{
      display.setCursor(88,7);
      display.println("OFFLINE");
    }
    display.setCursor(110,32);
    display.println(unit);
    display.setFont(u8g2_font_crox3hb_tn);
  
    strcpy(parseBuff,".000");
    int offset = 0;
    double scaledWh = (whCounter+offlineWhCounter)*0.001;
    double scaledWhDay = (whDayCounter+offlineWhDayCounter)*0.001;
    double scaledWhMonth = (whMonthCounter+offlineWhMonthCounter)*0.001;


    //Display page

    switch(page){
      case 0:
        /*
        itoa(whDayCounter%1000,ntoaBuff,10);
        strcpy(parseBuff+4-strlen(ntoaBuff),ntoaBuff);
        offset = ALIGN_RIGHT(parseBuff)-22;
        display.setCursor(offset,32);
        display.print(parseBuff);
        display.setFont(u8g2_font_logisoso22_tf);
        display.setCursor(offset-display.getUTF8Width(itoa(whDayCounter/1000,parseBuff,10))+2, 32);
        display.print(whDayCounter/1000);
        break;
        */
        snprintf(ntoaBuff,6,"%.03f",scaledWhDay-floor(scaledWhDay));
        strcpy(parseBuff+1,ntoaBuff+2);
        offset = ALIGN_RIGHT(parseBuff)-22;
        display.setCursor(offset,32);
        display.print(parseBuff);
        display.setFont(u8g2_font_logisoso22_tf);
        display.setCursor(offset-display.getUTF8Width(itoa((int)(floor(scaledWhDay)),parseBuff,10))+2, 32);
        display.print((int)(floor(scaledWhDay)));
        break;
      case 1:
        /*
        itoa(whMonthCounter%1000,ntoaBuff,10);
        strcpy(parseBuff+4-strlen(ntoaBuff),ntoaBuff);
        offset = ALIGN_RIGHT(parseBuff)-22;
        display.setCursor(offset,32);
        display.print(parseBuff);
        display.setFont(u8g2_font_logisoso22_tf);
        display.setCursor(offset-display.getUTF8Width(itoa(whMonthCounter/1000,parseBuff,10))+2, 32);
        display.print(whMonthCounter/1000);
        break;
        */
        snprintf(ntoaBuff,6,"%.03f",scaledWhMonth-floor(scaledWhMonth));
        strcpy(parseBuff+1,ntoaBuff+2);
        offset = ALIGN_RIGHT(parseBuff)-22;
        display.setCursor(offset,32);
        display.print(parseBuff);
        display.setFont(u8g2_font_logisoso22_tf);
        display.setCursor(offset-display.getUTF8Width(itoa((int)(floor(scaledWhMonth)),parseBuff,10))+2, 32);
        display.print((int)(floor(scaledWhMonth)));
        break;
      case 2:
        /*
        itoa(whCounter%1000,ntoaBuff,10);
        strcpy(parseBuff+4-strlen(ntoaBuff),ntoaBuff);
        offset = ALIGN_RIGHT(parseBuff)-22;
        display.setCursor(offset,32);
        display.print(parseBuff);
        display.setFont(u8g2_font_logisoso22_tf);
        display.setCursor(offset-display.getUTF8Width(itoa(whCounter/1000,parseBuff,10))+2, 32);
        display.print(whCounter/1000);
        break;
        */
        snprintf(ntoaBuff,6,"%.03f",scaledWh-floor(scaledWh));
        strcpy(parseBuff+1,ntoaBuff+2);
        offset = ALIGN_RIGHT(parseBuff)-22;
        display.setCursor(offset,32);
        display.print(parseBuff);
        display.setFont(u8g2_font_logisoso22_tf);
        display.setCursor(offset-display.getUTF8Width(itoa((int)(floor(scaledWh)),parseBuff,10))+2, 32);
        display.print((int)(floor(scaledWh)));
        break;
      case 3:
        snprintf(ntoaBuff,6,"%.03f",rmsVoltage-floor(rmsVoltage));
        strcpy(parseBuff+1,ntoaBuff+2);
        offset = ALIGN_RIGHT(parseBuff)-22;
        display.setCursor(offset,32);
        display.print(parseBuff);
        display.setFont(u8g2_font_logisoso22_tf);
        display.setCursor(offset-display.getUTF8Width(itoa((int)(floor(rmsVoltage)),parseBuff,10))+2, 32);
        display.print((int)(floor(rmsVoltage)));
        break;
      case 4:
        snprintf(ntoaBuff,6,"%.03f",rmsCurrent-floor(rmsCurrent));
        strcpy(parseBuff+1,ntoaBuff+2);
        offset = ALIGN_RIGHT(parseBuff)-22;
        display.setCursor(offset,32);
        display.print(parseBuff);
        display.setFont(u8g2_font_logisoso22_tf);
        display.setCursor(offset-display.getUTF8Width(itoa((int)(floor(rmsCurrent)),parseBuff,10))+2, 32);
        display.print((int)(floor(rmsCurrent)));
        break;
      case 5:
        snprintf(ntoaBuff,6,"%.03f",avgPower-floor(avgPower));
        strcpy(parseBuff+1,ntoaBuff+2);
        offset = ALIGN_RIGHT(parseBuff)-22;
        display.setCursor(offset,32);
        display.print(parseBuff);
        display.setFont(u8g2_font_logisoso22_tf);
        display.setCursor(offset-display.getUTF8Width(itoa((int)(floor(avgPower)),parseBuff,10))+2, 32);
        display.print((int)(floor(avgPower)));
        break;
    }
    if(isReading) display.drawXBM(102,11,5,5,readingLogo);
    if(locked) display.drawXBM(110,13,8,8,lockedLogo);
    if(isOnline && (millis()/1000)%2) display.drawXBM(119,13,8,8,wifiOnLogo);
    else if(isOnline) display.drawXBM(119,13,8,8,wifiTransferLogo);
    else display.drawXBM(119,13,8,8,wifiOffLogo);

    //display.sendBuffer();
    //frameCount++;               //Uncomment for refresh rate monitoring
    isUpdatingDisplay = true;
    tileCount = 64;
    refreshTimer = millis();
  }
}


/* Every 5 seconds begin to sample current and voltage data
 * Once required sample count or maximum sample time is reached
 * updates counters and send data to server.
 */

void readData(){
  //Start reading 

  if(millis()-readingTimer>5000){
    ads.setGain(GAIN_TWO);
    //ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, /*continuous=*/true);
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, /*continuous=*/true);
    new_data = false;
    isReading = true;
    isReadingVoltage = true;
    rmsBuffer = 0;
    sampleCount = 0;
    rmsTimer = millis();
    readingTimer = millis();
  }

  //Get sample from ADC
  if (isReading && new_data) {
    int16_t results = ads.getLastConversionResults();
    new_data = false;
    rmsBuffer += pow(ads.computeVolts(results),2);
    sampleCount++;
  }


  /* Checks if minimum sample count or sample time is reached.
   * Switch from voltage to current reading.
   * Updates counters and send sata if online, updates offline counter otherwise 
   */

  if(isReading && (millis()-rmsTimer>500 || sampleCount >= 430)){
    Serial.println(sampleCount);

    if(isReadingVoltage){
      rmsVoltage = sqrt(rmsBuffer/sampleCount);
      rmsVoltage *= VOLTAGE_MULTIPLIER;
      Serial.println(rmsVoltage);
      ads.setGain(GAIN_FOUR);
      ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_2_3, /*continuous=*/true);
      new_data = false;
      isReadingVoltage = false;
      rmsBuffer = 0;
      sampleCount = 0;
      rmsTimer = millis();
    }
    else if(!isReadingVoltage){
      rmsCurrent = sqrt(rmsBuffer/sampleCount);
      rmsCurrent *= CURRENT_MULTIPLIER;
      Serial.println(rmsCurrent);
      avgPower = rmsCurrent * rmsVoltage;
      unsigned int dtime = millis()-prevRead;
      prevRead = millis();
      float tempoffs = dtime*avgPower/3600000.0;
      if(isOnline){
        whCounter += tempoffs;
        whDayCounter += tempoffs;
        whMonthCounter += tempoffs;
        sendData();
      }
      else{
        offlineWhCounter += tempoffs;
        offlineWhDayCounter += tempoffs;
        offlineWhMonthCounter += tempoffs;
      }
      ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, /*continuous=*/false);
      isReading = false;
    }
  }
}


//######################################### SETUP #########################################


void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(READY_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(READY_PIN), NewDataReadyISR, FALLING);

  display.setBusClock(400000);
  display.begin();
  display.clearBuffer();
  /*
  display.setFontDirection(0);
  display.setCursor(50,20);
  display.setFont(u8g2_font_crox3hb_tn);
  display.println("Initializing...");
  */
  display.drawXBM(13,9,100,14,splashLogo);
  display.sendBuffer();
  delay(1000);

  WiFi.mode(WIFI_STA); //Optional
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");

  delay(1000);

  initConnection();

  ArduinoOTA.setHostname("ESP32EnergyLogger");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();

  /*
  while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(100);
  }

  HTTPClient http;
  const char* apiQuery = "http://worldtimeapi.org/api/timezone/Europe/Rome";
  http.begin(apiQuery);
  int httpResponseCode = http.GET();
  if (httpResponseCode>0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      JSONVar payload = JSON.parse(http.getString());
      epoch = payload["unixtime"];
      DST = payload["dst"];
      epoch += DST ? daylightOffset_sec : 0;
      rtc.setTime(epoch,0);
  }
  else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
  }
  */
  // Free resources

  if (!ads.begin(0x48)) {
    Serial.println("Failed to initialize ADS.");
    while (1);
  } 

  ads.setDataRate(RATE_ADS1115_860SPS); //860SPS
  //ads.setGain(GAIN_FOUR);               //+/-1.024V

  /*
  for(int i = 16; i<=33; i++){
      pinMode(i,INPUT_PULLUP);
  }
  */

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  /*
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.setRotation(2);
  display.display();
  delay(1000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  display.display();
  */
  
  //display.begin();
  //delay(1000);
  prevRead = 0;
}


//######################################### LOOP #########################################


void loop() {

  //Reinitialize connection every 30 seconds if not online
  if(!isOnline && millis()-connectionTimer> REINIT_COOLDOWN) initConnection();

  ArduinoOTA.handle();

  //Refresh rate monitor routine
  /*
  if(millis()-frameTimer>1000){
    Serial.print("Refresh Rate: ");
    Serial.println(frameCount);
    frameCount=0;
    frameTimer=millis();
  }
  */

  readData();

  loadBuffer();
  
  //Button state check
  
  if(!pressed && digitalRead(BTN_PIN) == LOW){
    pressedTimer = millis();
    onTimer = millis();
    delay(20);
    pressed = true;
  }

  if(pressed && digitalRead(BTN_PIN) == LOW && millis()-pressedTimer > 2000){
    locked = !locked;
    pressed = false;
    unlockRelease = true;
    pressedTimer = millis();
  }

  if(pressed && digitalRead(BTN_PIN) == HIGH){
    if(unlockRelease && millis()-pressedTimer > 20) unlockRelease = false;
    else{ 
      if(isOn) page = (page+1)%6;
      else isOn = true;
    }
    delay(20);
    pressed = false;
  }

  //Turn off screen when idle

  if(isOn && !locked && millis() - onTimer > TURNOFF_TIMER){
    isOn = false;
    display.clearDisplay();
    display.display();
  }

  //Display menu one tile at time

  if(isOn && isUpdatingDisplay){
    if(tileCount){
      tileCount--;
      display.updateDisplayArea(tileCount%16, tileCount/16, 1, 1);
    }
    else isUpdatingDisplay = false;
  }
}