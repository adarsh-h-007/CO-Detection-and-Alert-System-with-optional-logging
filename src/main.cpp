//* ------------------------------ Header Files ------------------------------ *//
#include <Arduino.h>
#include<TFT_eSPI.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include<DHT.h>
#include <driver/adc.h>
#include<SoftwareSerial.h>
#include <TinyGPS++.h>
#include<pitches.h>
#include "time.h" //Only for logging
#include <ESP_Google_Sheet_Client.h>  //Only for logging
//* -------------------------------------------------------------------------- *//


/* --------------------------------- logging -------------------------------- */
#define WIFI_SSID "Your WiFi name"
#define WIFI_PASSWORD "Your WiFi password"

// Google Project ID
#define PROJECT_ID "your_project_id"

// Service Account's client email
#define CLIENT_EMAIL "your-client-email"

// Service Account's private key
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nYour Private Key\n-----END PRIVATE KEY-----\n";

// The ID of the spreadsheet where you'll publish the data
const char spreadsheetId[] = "Your spreadsheet id";

// Token Callback function
void tokenStatusCallback(TokenInfo info);

// NTP server to request epoch time
const char* ntpServer = "pool.ntp.org";

// Variable to save current epoch time
unsigned long epochTime; 

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}
/* ------------------------------- End Logging ------------------------------ */

//* ------------------------------- Thresholds ------------------------------- *//
float threshold_low = 10.0;     //The lower threshold beyond which actions(change color to yellow in display) are taken. The value is in ppm
float threshold_high = 20.0;    //The upper threshold beyond which actions(change color to red in display, sound buzzer, send text message) are taken. The value is in ppm

#define buzzerPin 19
unsigned long buzzerDuration = 1000;
//* -------------------------------------------------------------------------- *//

//* --------------------------- ST7735 TFT Display --------------------------- *//
TFT_eSPI tft = TFT_eSPI();
//* -------------------------------------------------------------------------- *//

//* ---------------------------------- MQ-7 ---------------------------------- *//
#define mq7_heater_pin_1 32
#define mq7_analog_pin_1 35

#define mq7_heater_pin_2 25
#define mq7_analog_pin_2 34

unsigned long startTime;
unsigned long elapsedTime;
unsigned long heater_duratio_1n = 55000;    //heater duration, don't change
unsigned long read_duratio_1n = 90000;      //cooling duration, don't change

enum State
{
  HEATING,
  MEASURING,
  DONE
};

State currentState = HEATING;

bool heatingMessagePrinted = false;
bool measuringMessagePrinted = false;

#define mq7_low_side_resistor 1000    //No need to change. Refer to the modification in ESPHome website
#define mq7_high_side_resistor 470    //No need to change. Refer to the modification in ESPHome website
#define mq7_supply_voltage 5.3        //Voltage of your power supply in Volts

#define mq7_clean_air_compensated_resistance_1_1  47823.27567     //VERY_IMPORTANT: This is the baseline that we use to compare. Run the sensor a few times in clean air and replace the value here with the compensated resistance that you got from running the sensor in clean air. Varies from sensor to sensor.
#define mq7_clean_air_compensated_resistance_1_2  2543.14696      //VERY_IMPORTANT: This is the baseline that we use to compare. Run the sensor a few times in clean air and replace the value here with the compensated resistance that you got from running the sensor in clean air. Varies from sensor to sensor.

float global_co_ppm_1;   //Global variable for text message, printing onto the display and so on
float global_co_ppm_2;   //Global variable for text message, printing onto the display and so on
//* -------------------------------------------------------------------------- *//

//* ---------------------------------- DHT11 --------------------------------- *//
#define dht1_pin 33
#define dht_type DHT11

DHT dht1(dht1_pin, dht_type);

#define dht2_pin 26
#define dht_type DHT11

DHT dht2(dht2_pin, dht_type);
//* -------------------------------------------------------------------------- *//

//* --------------------------------- SIM800 --------------------------------- *//
String PhoneNo = "Your phone number with country code"; 

HardwareSerial gsmSerial(2);    //We use the built-in Serial Port 2 of the ESP32
//* -------------------------------------------------------------------------- *//

//* -------------------------------- NEO6M GPS ------------------------------- *//
int gpsRx = 13;
int gpsTx = 12;
int GPSBaud = 9600;

TinyGPSPlus gps;
SoftwareSerial gpsSerial(gpsRx, gpsTx);

String gmapLink;
//* -------------------------------------------------------------------------- *//

//* ----------------------------- Message String ----------------------------- *//
String message;   //The string that we send as the text message
//* -------------------------------------------------------------------------- *//

//* -------------------------- Function Prototyping -------------------------- *//
void updateRawData_1();
float calculateresistance_1(float raw_value_1);
float calculateCompensatedresistance_1(float resistance_1, float temperature_1, float humidity_1);
float calculateratio_1(float compensated_resistance_1);
float calculateCOppm_1(float ratio_1);
void updateRawData_2();
float calculateresistance_2(float raw_value_1);
float calculateCompensatedresistance_2(float resistance_2, float temperature_2, float humidity_2);
float calculateratio_2(float compensated_resistance_2);
float calculateCOppm_2(float ratio_2);
void sendSOS(String message);
void updateSerial();
String prepTextMsg(String gmapLink, float co_ppm_1, float co_ppm_2);
void dispLevelTFT(float co_in, float co_out);
void heatingBorder();
void coolingBorder();
void value_compare(float co_in, float co_out);
void buzzer(float co_in, float co_out);
void critical_alarm();
void non_critical_alarm();
//* -------------------------------------------------------------------------- *//




//* -------------------------------------------------------------------------- *//
//*                               Setup Function                               *//
//* -------------------------------------------------------------------------- *//

void setup(){
  Serial.begin(115200);
  dht1.begin();
  dht2.begin();

  gsmSerial.begin(115200);
    delay(3000);
  
  gpsSerial.begin(GPSBaud);

  /* ------------------------------- Buzzer test ------------------------------ */
  pinMode(19, OUTPUT);
  tone(19, 10000, 3000);

  //* ---------------------------- Initializing MQ7 ---------------------------- *//
  pinMode(mq7_heater_pin_1, OUTPUT);
  pinMode(mq7_heater_pin_2, OUTPUT);
  currentState = HEATING;
  startTime = millis();
  //* -------------------------------------------------------------------------- *//

  //* ----------------------- TFT initialization & layout ---------------------- *//
  tft.init();
  tft.fillScreen(TFT_BLACK);
  //* -------------------------------------------------------------------------- *//

  /* --------------------------------- logging -------------------------------- */
  //Configure time
    configTime(0, 0, ntpServer);

    GSheet.printf("ESP Google Sheet Client v%s\n\n", ESP_GOOGLE_SHEET_CLIENT_VERSION);

    // Connect to Wi-Fi
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    // Set the callback for Google API access token generation status (for debug only)
    GSheet.setTokenCallback(tokenStatusCallback);

    // Set the seconds to refresh the auth token before expire (60 to 3540, default is 300 seconds)
    GSheet.setPrerefreshSeconds(10 * 60);

    // Begin the access token generation for Google API authentication
    GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
    /* ------------------------------- End Logging ------------------------------ */
}



//* -------------------------------------------------------------------------- *//
//*                                Loop Function                               *//
//* -------------------------------------------------------------------------- *//

void loop(){

  bool ready = GSheet.ready();  //For logging

   elapsedTime = millis() - startTime;

  switch (currentState) {
    case HEATING:
      digitalWrite(mq7_heater_pin_1, HIGH);
      digitalWrite(mq7_heater_pin_2, HIGH);

      if (!heatingMessagePrinted) {
        Serial.println("The sensors are heating!");
        heatingMessagePrinted = true;
        heatingBorder();    //Gives an orange border and rectangle in the middle of the dsplay to indicate that the sensor is heating  
      }
      if (elapsedTime >= heater_duratio_1n) {
        currentState = MEASURING;
        startTime = millis();
      }
      break;

    case MEASURING:
      digitalWrite(mq7_heater_pin_1, LOW);
      digitalWrite(mq7_heater_pin_2, LOW);

      if (!measuringMessagePrinted) {
        Serial.println("The sensors are measuring!");
        measuringMessagePrinted = true;
        coolingBorder();      //Gives an blue border and rectangle in the middle of the dsplay to indicate that the sensor is cooling  
      }
      if (elapsedTime >= read_duratio_1n) {
        currentState = DONE;
        startTime = millis();
      }
      break;

    case DONE:
      if (digitalRead(mq7_heater_pin_1) == LOW && digitalRead(mq7_heater_pin_2) == LOW) {
        updateRawData_1();      //Function that calculates the level of CO using the first sensor
        updateRawData_2();      //Function that calculates the level of CO using the second sensor
        Serial.println("Done\n");

        digitalWrite(mq7_heater_pin_1, HIGH);
        digitalWrite(mq7_heater_pin_2, HIGH);
        currentState = HEATING;
        startTime = millis();
        heatingMessagePrinted = false;
        measuringMessagePrinted = false;

        dispLevelTFT(global_co_ppm_1, global_co_ppm_2);   //Function that prints the values in ppm to the display 

        value_compare(global_co_ppm_1, global_co_ppm_2);  //Function that decides whether to send text message based on the thresholds set

        buzzer(global_co_ppm_1, global_co_ppm_2);         //Function that decides whether to sound the buzzer based on the thresholds set

        //sendSOS(prepTextMsg(gmapLink, global_co_ppm_1, global_co_ppm_2));     /*For demo purposes only. Uncomment to send a text message after every reading*/
      }
      break;

    default:
      break;
  }

  //* ---------------------------- Google Maps link ---------------------------- *//
  //Couldn't implement this as a standalone function for some reason, so this (⸝⸝⸝O﹏ O⸝⸝⸝)
  while (gpsSerial.available() > 0)
    if (gps.encode(gpsSerial.read())){
      gmapLink = "https://maps.google.com/?q=";
      gmapLink += String(gps.location.lat() , 6);
      gmapLink += ",";
      gmapLink += String(gps.location.lng() , 6);
    }
  //* -------------------------------------------------------------------------- *//
}




//* -------------------------------------------------------------------------- *//
//*              Function that updates MQ-7 data to Serial Monitor             *//
//* -------------------------------------------------------------------------- *//

void updateRawData_1() {
  float temperature_1 = dht1.readTemperature();
  float humidity_1 = dht1.readHumidity();

  analogReadResolution(12);
  analogSetPinAttenuation(mq7_analog_pin_1, ADC_0db);  
  
  float raw_value_1 = analogRead(mq7_analog_pin_1);
  
  float resistance_1 = calculateresistance_1(raw_value_1);
  float compensated_resistance_1 = calculateCompensatedresistance_1(resistance_1, temperature_1, humidity_1);
  float ratio_1 = calculateratio_1(compensated_resistance_1);
  float co_ppm_1 = calculateCOppm_1(ratio_1);

  global_co_ppm_1 = co_ppm_1;  //Global variable

  Serial.println("---------------------------------- MQ7 1 ---------------------------------");

  Serial.print("temperature_1: ");
  Serial.print(temperature_1);
  Serial.println(" °C");

  Serial.print("humidity_1: ");
  Serial.print(humidity_1);
  Serial.println(" %");

  Serial.print("MQ7_1 Raw Value: ");
  Serial.println(raw_value_1);

  Serial.print("MQ7_1 resistance_1: ");
  Serial.println(resistance_1);

  Serial.print("MQ7_1 Compensated resistance_1: ");
  Serial.println(compensated_resistance_1);

  Serial.print("MQ7_1 ratio_1: ");
  Serial.println(ratio_1);

  Serial.print("MQ7_1 Carbon Monoxide: ");
  Serial.println(co_ppm_1);


/* --------------------------------- logging -------------------------------- */
  FirebaseJson response;

  Serial.println("\nAppend spreadsheet values...");
  Serial.println("----------------------------");

  FirebaseJson valueRange;

  epochTime = getTime();

        valueRange.add("majorDimension", "COLUMNS");
        valueRange.set("values/[0]/[0]", epochTime);
        valueRange.set("values/[1]/[0]", raw_value_1);
        valueRange.set("values/[2]/[0]", resistance_1);
        valueRange.set("values/[3]/[0]", compensated_resistance_1);
        valueRange.set("values/[4]/[0]", co_ppm_1);

  // For Google Sheet API ref doc, go to https://developers.google.com/sheets/api/reference/rest/v4/spreadsheets.values/append
        // Append values to the spreadsheet
        bool success = GSheet.values.append(&response /* returned response */, spreadsheetId /* spreadsheet Id to append */, "Sheet1!A1" /* range to append */, &valueRange /* data range to append */);
        if (success){
            response.toString(Serial, true);
            valueRange.clear();
        }
        else{
            Serial.println(GSheet.errorReason());
        }
        Serial.println();
        Serial.println(ESP.getFreeHeap());
/* ------------------------------- End Logging ------------------------------ */
}

float calculateresistance_1(float raw_value_1) {
  return (raw_value_1 /  mq7_supply_voltage)* (mq7_low_side_resistor - mq7_high_side_resistor);
}

float calculateCompensatedresistance_1(float resistance_1, float temperature_1, float humidity_1) {
  return resistance_1 / ( (-0.01223333 * temperature_1) - (0.00609615 * humidity_1) + 1.70860897);
}

float calculateratio_1(float compensated_resistance_1) {
  return 100.0 * 1/(compensated_resistance_1 / mq7_clean_air_compensated_resistance_1_1);
}

float calculateCOppm_1(float ratio_1) {
  float ratio_1_ln = log(ratio_1 / 100.0);
  return exp(-0.685204 - (2.67936 * ratio_1_ln) - (0.488075 * ratio_1_ln * ratio_1_ln) - (0.07818 * ratio_1_ln * ratio_1_ln * ratio_1_ln));
}



void updateRawData_2() {

  delay(250);

  float temperature_2 = dht2.readTemperature();
  float humidity_2 = dht2.readHumidity();

  analogReadResolution(12);
  analogSetPinAttenuation(mq7_analog_pin_2, ADC_0db);
  
  float raw_value_2 = analogRead(mq7_analog_pin_2);

  float resistance_2 = calculateresistance_1(raw_value_2);
  float compensated_resistance_2 = calculateCompensatedresistance_2(resistance_2, temperature_2, humidity_2);
  float ratio_2 = calculateratio_2(compensated_resistance_2);
  float co_ppm_2 = calculateCOppm_2(ratio_2);

  global_co_ppm_2 = co_ppm_2; // Global variable 

  Serial.println("---------------------------------- MQ7 2 ---------------------------------");

  Serial.print("temperature_2: ");
  Serial.print(temperature_2);
  Serial.println(" °C");

  Serial.print("humidity_2: ");
  Serial.print(humidity_2);
  Serial.println(" %");

  Serial.print("MQ7_2 Raw Value: ");
  Serial.println(raw_value_2);

  Serial.print("MQ7_2 resistance_2: ");
  Serial.println(resistance_2);

  Serial.print("MQ7_2 Compensated resistance_2: ");
  Serial.println(compensated_resistance_2);

  Serial.print("MQ7_2 ratio_2: ");
  Serial.println(ratio_2);

  Serial.print("MQ7_2 Carbon Monoxide: ");
  Serial.println(co_ppm_2);

/* --------------------------------- logging -------------------------------- */
  FirebaseJson response;

  Serial.println("\nAppend spreadsheet values...");
  Serial.println("----------------------------");

  FirebaseJson valueRange;

  epochTime = getTime();

        valueRange.add("majorDimension", "COLUMNS");
        valueRange.set("values/[6]/[0]", raw_value_2);
        valueRange.set("values/[7]/[0]", resistance_2);
        valueRange.set("values/[8]/[0]", compensated_resistance_2);
        valueRange.set("values/[9]/[0]", co_ppm_2);

  // For Google Sheet API ref doc, go to https://developers.google.com/sheets/api/reference/rest/v4/spreadsheets.values/append
        // Append values to the spreadsheet
        bool success = GSheet.values.append(&response /* returned response */, spreadsheetId /* spreadsheet Id to append */, "Sheet1!G1" /* range to append */, &valueRange /* data range to append */);
        if (success){
            response.toString(Serial, true);
            valueRange.clear();
        }
        else{
            Serial.println(GSheet.errorReason());
        }
        Serial.println();
        Serial.println(ESP.getFreeHeap());
/* ------------------------------- End Logging ------------------------------ */
}

float calculateresistance_2(float raw_value_2) {
  return (raw_value_2 /  mq7_supply_voltage)* (mq7_low_side_resistor - mq7_high_side_resistor);
}

float calculateCompensatedresistance_2(float resistance_2, float temperature_2, float humidity_2) {
  return resistance_2 / ( (-0.01223333 * temperature_2) - (0.00609615 * humidity_2) + 1.70860897);
}

float calculateratio_2(float compensated_resistance_2) {
  return 100.0 * 1/(compensated_resistance_2 / mq7_clean_air_compensated_resistance_1_2);
}

float calculateCOppm_2(float ratio_2) {
  float ratio_2_ln = log(ratio_2 / 100.0);
  return exp(-0.685204 - (2.67936 * ratio_2_ln) - (0.488075 * ratio_2_ln * ratio_2_ln) - (0.07818 * ratio_2_ln * ratio_2_ln * ratio_2_ln));
}



//* -------------------------------------------------------------------------- *//
//*              Function that prints the CO Level on the Display              *//
//* -------------------------------------------------------------------------- *//
void dispLevelTFT(float co_in, float co_out){

  tft.fillRect(2, 2, tft.width() - 5, tft.height() - 5, TFT_BLACK);

  if(co_in < threshold_low)
    tft.setTextColor(TFT_WHITE);
  else if(co_in > threshold_low && co_in < threshold_high)
    tft.setTextColor(TFT_ORANGE);
  else if(co_in > threshold_high)
    tft.setTextColor(TFT_RED);

  tft.drawString(String(co_in), 2, 10, 6);

  tft.drawString("ppm", 5, 58, 2);

  tft.setTextColor(TFT_CYAN);
  tft.drawString("Inside", 35, 53, 4);


  if(co_out < threshold_low)
    tft.setTextColor(TFT_WHITE);
  else if(co_out > threshold_low && co_out < threshold_high)
    tft.setTextColor(TFT_ORANGE);
  else if(co_out > threshold_high)
    tft.setTextColor(TFT_RED);
  tft.drawString(String(co_out), 2, 90, 6);

  tft.drawString("ppm", 5, 133, 2);

  tft.setTextColor(TFT_CYAN);
  tft.drawString("Outside", 35, 128, 4);
}

void heatingBorder(){
  tft.drawRect(0, 0, tft.width() - 1, tft.height() - 1, TFT_ORANGE);
  tft.fillRectHGradient(0, 80, 160, 4, TFT_RED, TFT_ORANGE);
}

void coolingBorder(){
  tft.drawRect(0, 0, tft.width() - 1, tft.height() - 1, TFT_BLUE);
  tft.fillRectHGradient(0, 80, 160, 4, TFT_SKYBLUE, TFT_VIOLET);
}




//* -------------------------------------------------------------------------- *//
//*              Function that compares co level and takes action              *//
//* -------------------------------------------------------------------------- *//

void value_compare(float co_in , float co_out){
  
  if(co_in > threshold_high){
    sendSOS(prepTextMsg(gmapLink, global_co_ppm_1, global_co_ppm_2));
  }
}



//* -------------------------------------------------------------------------- */
//*                               Buzzer function                              */
//* -------------------------------------------------------------------------- */

void buzzer(float co_in, float co_out){

  if (co_in > threshold_high || co_out > threshold_high)
    critical_alarm();
  else if((co_in > threshold_low || co_out > threshold_low))
    non_critical_alarm();
}


void critical_alarm(){
  for (int i = 0; i <= 15; i++){
    tone(buzzerPin, NOTE_G, buzzerDuration);
  }
}

void non_critical_alarm(){
  for (int i = 0; i <= 2; i++){
    tone(buzzerPin, NOTE_E, buzzerDuration);
    tone(buzzerPin, NOTE_D, buzzerDuration);
    tone(buzzerPin, NOTE_C, buzzerDuration);
    tone(buzzerPin, NOTE_G, buzzerDuration);
  }
}

 //* -------------------------------------------------------------------------- *//
//*                           Function that sends SOS                          *//
//* -------------------------------------------------------------------------- *//



void sendSOS(String message){
  gsmSerial.begin(9600);
  Serial.println("Initializing SIM800L.....");
  delay(1000);
  gsmSerial.println("AT"); //Once the handshake test is successful, it will back to OK
  updateSerial();
  gsmSerial.println("AT+CSQ"); //Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  gsmSerial.println("AT+CCID"); //Read SIM information to confirm whether the SIM is plugged
  updateSerial();
  gsmSerial.println("AT+CREG?"); //Check whether it has registered in the network
  updateSerial();
  gsmSerial.println("AT+CMGF=1");
  updateSerial();
  gsmSerial.println("AT+CMGS=\""+PhoneNo+"\"\r");   //idk why this frikn error comes. The code compiles and runs fine though.
  updateSerial();
  gsmSerial.print(message);
  updateSerial();
  gsmSerial.write(26);
  updateSerial();
  Serial.println("-----------------------------------");
}




//* -------------------------------------------------------------------------- *//
//*              Function that updates the Serial monitor from GSM             *//
//* -------------------------------------------------------------------------- *//

void updateSerial()
{
  delay(500);
  while (Serial.available())
  {
    gsmSerial.write(Serial.read());//Forward what's received in the Serial to the Software Serial Port
  }
  while (gsmSerial.available())
  {
    Serial.write(gsmSerial.read());//Forward what's received in the Software Serial Port to Serial. Not necessary, ig.
  }
}



//* -------------------------------------------------------------------------- *//
//*                     Function that prepares text message                    *//
//* -------------------------------------------------------------------------- *//

String prepTextMsg(String gmapLink, float co_ppm_1, float co_ppm_2){
  // This is the body of the text that'll be sent through the GSM module. Make changes if youu want to alter the message contents  ;)
  message = "**Carbon Monoxide Levels**\n";
  message += "Inside : ";
  message += String(co_ppm_1);
  message += "\n";
  message += "Outside : ";
  message += String(co_ppm_2);
  message += "\n\n";
  message += "Location : ";
  message += gmapLink;
  return message;
}

/* ----------------------------- More Logging ;) ---------------------------- */
void tokenStatusCallback(TokenInfo info);
void tokenStatusCallback(TokenInfo info){
    if (info.status == token_status_error){
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
        GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
    }
    else{
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
    }
}
/* ------------------------------- End Logging ------------------------------ */