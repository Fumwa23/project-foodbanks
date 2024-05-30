// Libraries for the HX711
#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"

// Libraries for Google Sheets
#include <WiFi.h>
#include "time.h"
#include <ESP_Google_Sheet_Client.h>

//For SD/SD_MMC mounting helper
#include <GS_SDHelper.h>

// For secret keys
#include "secrets.h"

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;

// Token Callback function
void tokenStatusCallback(TokenInfo info);

// Variables to hold data for uploading
float CO2;
String CO2String;
String source;
float weight;
String Food;
String FoodCategory;

// NTP server to request epoch time
const char *ntpServer = "pool.ntp.org";

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

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 13;
const int LOADCELL_SCK_PIN = 15;
const int LED_PIN = 4;

HX711 scale;

// Setting up buttons
const int BUTTON_PIN = 2;
int buttonState = 1;

void setup() {
  Serial.begin(115200);
  //BUTTON SETUP ---------------------------------------------------------
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  //Flash once to show that it has started
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);

  //GOOGLE SHEETS SETUP ---------------------------------------------------------
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
  
  //WEIGHING SCALE SETUP ---------------------------------------------------------
  rtc_cpu_freq_config_t config;
  rtc_clk_cpu_freq_get_config(&config);
  rtc_clk_cpu_freq_to_config(RTC_CPU_FREQ_80M, &config);
  rtc_clk_cpu_freq_set_config_fast(&config);
  Serial.println("Otley Food Larder Software Version 1.0");

  Serial.println("Initializing the scale");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  Serial.println("Before setting up the scale:");
  Serial.print("read: \t\t");
  Serial.println(scale.read());      // print a raw reading from the ADC

  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));   // print the average of 20 readings from the ADC

  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));   // print the average of 5 readings from the ADC minus the tare weight (not set yet)

  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);  // print the average of 5 readings from the ADC minus tare weight (not set) divided
            // by the SCALE parameter (not set yet)
            
  scale.set_scale(-278.8);
  //scale.set_scale(-471.497);                      // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();               // reset the scale to 0

  Serial.println("After setting up the scale:");

  Serial.print("read: \t\t");
  Serial.println(scale.read());                 // print a raw reading from the ADC

  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));       // print the average of 20 readings from the ADC

  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));   // print the average of 5 readings from the ADC minus the tare weight, set with tare()

  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);        // print the average of 5 readings from the ADC minus tare weight, divided
            // by the SCALE parameter set with set_scale

  Serial.println("Readings:");

  //Flash three times to show that it has reset
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  buttonState = digitalRead(BUTTON_PIN);

  bool ready = GSheet.ready(); //Checking if the Google Sheet is ready to upload data

  float weight = scale.get_units(10);
  Serial.print("average:\t");
  Serial.println(weight, 5); // print the average of 10 readings to 5 decimal places

  scale.power_down();             // put the ADC in sleep mode

  //if (weight > 50.0 && ready && millis() - lastTime > timerDelay) {
  if (buttonState == LOW && ready && millis() - lastTime > timerDelay) {
    digitalWrite(LED_PIN, HIGH); 
    delay(500);
    digitalWrite(LED_PIN, LOW);

    lastTime = millis();

    FirebaseJson response;

    Serial.println("\nAppend spreadsheet values...");
    Serial.println("----------------------------");

    FirebaseJson valueRange;

    // New BME280 sensor readings
    Food = "Bag of apples";
    FoodCategory = "Fruit, Compostable";
    CO2 = random(100, 200);
    
    // make CO2 string into the value of CO2 plus the unit
    CO2String = String(CO2) + " fake data";
    source = "bakery";

    // Get timestamp
    epochTime = getTime();

    if (epochTime == 0) {
    Serial.println("Failed to obtain time");
    return;
    }

    // Create a time structure
    struct tm *timeInfo;

    // Convert epoch to a time structure
    time_t rawTime = (time_t)epochTime;
    timeInfo = localtime(&rawTime);

    // Create variables for formatted date and time
    char dateStr[9]; // dd/mm/yy -> 8 characters + null terminator
    char timeStr[9]; // hh:mm:ss -> 8 characters + null terminator

    // Format date and time
    strftime(dateStr, sizeof(dateStr), "%d/%m/%y", timeInfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeInfo);

    // Print formatted date and time
    Serial.print("Date: ");
    Serial.println(dateStr);
    Serial.print("Time: ");
    Serial.println(timeStr);

    valueRange.add("majorDimension", "COLUMNS"); //can also pass "ROWS" as an argument instead
    valueRange.set("values/[0]/[0]", dateStr);
    valueRange.set("values/[1]/[0]", timeStr);
    valueRange.set("values/[2]/[0]", Food);
    valueRange.set("values/[3]/[0]", FoodCategory);
    valueRange.set("values/[4]/[0]", weight);
    valueRange.set("values/[5]/[0]", source);
    valueRange.set("values/[6]/[0]", CO2String);

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
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  delay(1000);
  scale.power_up();
}

void tokenStatusCallback(TokenInfo info){ //Callback function for the token status for debugging
    if (info.status == token_status_error){
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
        GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
    }
    else{
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
    }
}