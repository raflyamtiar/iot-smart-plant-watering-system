/* Smart Plant Watering System With New BLYNK */
#define BLYNK_TEMPLATE_ID "TMPL6qh_XUmti"
#define BLYNK_TEMPLATE_NAME "Smart Plant Watering System"
#define BLYNK_PRINT Serial

// Include libraries
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "ThingSpeak.h" // Tambahkan library ThingSpeak

#define API_KEY "AIzaSyD0l38qPbt1hKArNnIaDNqC3ZWrnP4p1vg" //add your api firebase
#define DATABASE_URL "https://iot-app-leafy-1-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Initialize the LCD display
LiquidCrystal_I2C lcd(0x27, 16, 2);

// WiFi and Blynk credentials
char authblynk[] = "0TgT5KsVmUEPyPYUoKD83MZRYUmt_l10"; //add your blynk token
char ssid[] = "modal"; //add your wifi ssid
char pass[] = "        "; //add your wifi password

// ThingSpeak credentials
unsigned long myChannelNumber = 2807193; //add yout=r number thingspeak
const char *myWriteAPIKey = "RP3PO9UNU2I24HB5";//add your writeapi thingspeak
WiFiClient client;

// Initialize DHT11 sensor
#define DHTPIN D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Define component pins
#define sensor A0
#define waterPump D3

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

BlynkTimer timer;
bool Relay = 0;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// ThingSpeak update interval
const long ts_update_interval = 20000;
unsigned long lastTSUpdate = 0;

void setup() {
  Serial.begin(9600);
  pinMode(waterPump, OUTPUT);
  digitalWrite(waterPump, LOW);
  lcd.begin();
  lcd.backlight();
  dht.begin();

  Blynk.begin(authblynk, ssid, pass, "blynk.cloud", 80);

  lcd.setCursor(1, 0);
  lcd.print("System Loading");
  for (int a = 0; a <= 15; a++) {
    lcd.setCursor(a, 1);
    lcd.print(".");
    delay(200);
  }
  lcd.clear();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("signUp OK");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize ThingSpeak
  ThingSpeak.begin(client);

  // Set timers
  timer.setInterval(100L, soilMoistureSensor);
  timer.setInterval(200L, dhtSensorReadings);
}

// Handle button control for the water pump
BLYNK_WRITE(V1) {
  Relay = param.asInt();

  if (Relay == 1) {
    digitalWrite(waterPump, HIGH);
  } else {
    digitalWrite(waterPump, LOW);
  }
}

// Get soil moisture values
void soilMoistureSensor() {
  int value = analogRead(sensor);
  value = map(value, 0, 1024, 0, 100);
  value = (value - 100) * -1;

  Blynk.virtualWrite(V0, value);
  lcd.setCursor(0, 0);
  lcd.print("Moisture :");
  lcd.print(value);
  lcd.print(" ");
  lcd.print("%   ");

  if (Firebase.RTDB.setFloat(&fbdo, "sensor/soilMoisture", value)) {
    Serial.println();
    Serial.print("successfully saved to: " + fbdo.dataPath());
  } else {
    Serial.println("FAILED:" + fbdo.errorReason());
  }

  // Send data to ThingSpeak
  ThingSpeak.setField(1, value);
}

// Get DHT11 sensor readings
void dhtSensorReadings() {
  if (digitalRead(waterPump) == HIGH) {
    lcd.setCursor(0, 1);
    lcd.print("Pump Active!     ");
    return;
  }

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // Check if readings are valid
  if (isnan(humidity) || isnan(temperature)) {
    lcd.setCursor(0, 1);
    lcd.print("Sensor Error!    ");
    return;
  }

  // Format temperature and humidity to 1 decimal place
  temperature = round(temperature * 10) / 10.0;
  humidity = round(humidity * 10) / 10.0;

  // Display temperature and humidity on LCD
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print((char)223);
  lcd.print("C ");
  lcd.print("H:");
  lcd.print(humidity, 1);
  lcd.print("%   ");

  // Send data to Blynk
  Blynk.virtualWrite(V2, temperature);
  Blynk.virtualWrite(V3, humidity);

  if (Firebase.RTDB.setFloat(&fbdo, "sensor/humidity", humidity)) {
    Serial.println();
    Serial.print(humidity);
    Serial.print("successfully saved to: " + fbdo.dataPath());
    Serial.println(" (" + fbdo.dataType() + ")");
  } else {
    Serial.println("FAILED:" + fbdo.errorReason());
  }

  if (Firebase.RTDB.setFloat(&fbdo, "sensor/temperature", temperature)) {
    Serial.println();
    Serial.print(temperature);
    Serial.print("successfully saved to: " + fbdo.dataPath());
    Serial.println(" (" + fbdo.dataType() + ")");
  } else {
    Serial.println("FAILED:" + fbdo.errorReason());
  }

   // Format temperature and humidity before sending to ThingSpeak
  String formattedTemperature = String(temperature, 2); // Limit to 2 decimal places
  String formattedHumidity = String(humidity, 2);       // Limit to 2 decimal places

  ThingSpeak.setField(2, formattedTemperature);
  ThingSpeak.setField(3, formattedHumidity);
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 500 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    Blynk.run();
    timer.run();

    if (Firebase.RTDB.getBool(&fbdo, "control/pumpStatus")) {
      bool pumpStatus = fbdo.boolData();
      digitalWrite(waterPump, pumpStatus ? HIGH : LOW);
      Serial.print("Pump Status: ");
      Serial.println(pumpStatus ? "ON" : "OFF");

      // Send pumpStatus to ThingSpeak
      ThingSpeak.setField(4, pumpStatus ? 1 : 0);
    } else {
      Serial.println("FAILED to read Pump status: " + fbdo.errorReason());
    }
  }

  // Update ThingSpeak every 30 seconds
  if (millis() - lastTSUpdate >= ts_update_interval) {
    lastTSUpdate = millis();
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (x == 200) {
      Serial.println("Sukses Update Chanel.");
    } else {
      Serial.println("Update Chanel Bermasalah. HTTP error code " + String(x));
    }
    Serial.println();
  }
}
