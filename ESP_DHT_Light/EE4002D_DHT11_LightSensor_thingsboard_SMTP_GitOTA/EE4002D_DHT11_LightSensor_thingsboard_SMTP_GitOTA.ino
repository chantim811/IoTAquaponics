#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// Email & Time Libs
#include <ESP8266WiFi.h>
#include <ESP_Mail_Client.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// thingsboard MQTT
#include <PubSubClient.h>

// Github OTA
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <CertStoreBearSSL.h>

// OTA versioning
const String FirmwareVer={"1.0"};   // Update here & bin_version.txt accordingly
#define URL_fw_Version "https://raw.githubusercontent.com/programmer131/ESP8266_ESP32_SelfUpdate/master/esp32_ota/bin_version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/programmer131/ESP8266_ESP32_SelfUpdate/master/esp32_ota/fw.bin"
const char* host = "raw.githubusercontent.com";
const int httpsPort = 443;

// DigiCert High Assurance EV Root CA
// Take from https://www.digicert.com/kb/digicert-root-certificates.htm
const char trustRoot[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j
ZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL
MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3
LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug
RVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm
+9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW
PNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM
xChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB
Ik5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3
hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg
EsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF
MAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA
FLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec
nzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z
eM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF
hS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2
Yzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe
vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep
+OkuE6N36B9K
-----END CERTIFICATE-----
)EOF";
X509List cert(trustRoot);

// DHT Pin settings
// ESP8266 note: use pins 3, 4, 5, 12, 13 or 14
#define DHTPIN 5     // Digital GPIO pin connected to the DHT sensor 
#define DHTTYPE    DHT11     // DHT 11

DHT_Unified dht(DHTPIN, DHTTYPE);

// Light Sensor (Photoresistor) Pin settings
const int lightSensorPin = A0;

const int short_delay = 300; // Use const for constants
const int long_delay = 5000; // Use const for constants
unsigned long previousMillis = 0; // Stores the last time the function was executed
const long interval = 30000; // 5 minutes in milliseconds (5 * 60 * 1000) = 300000

unsigned int readingId = 0;   // Data Msg Counter

// Temp/Hum/Light global variables
float temp = 0.0;
float hum = 0.0;
int light = 0;
const float tempThresh = 35.00;
const float humThresh = 85.00;
const int lightThresh = 250;
bool temp_flag = false;
bool hum_flag = false;

// ThingsBoard MQTT credentials
const char* mqtt_server = "mqtt.thingsboard.cloud"; 
const char* access_token = "kkt76ry15dl4cbf0edtf"; 
const char* telemetry_topic = "v1/devices/me/telemetry";
const char* attributes_topic = "v1/devices/me/attributes";
const char* clientId = "f5fec8a0-ed40-11ef-84d4-27bdd494c932"; 

// ----------------------------------------------------------------------------
#define WIFI_SSID "Here"     // CHANGE IT
#define WIFI_PASSWORD "passwordishere?"  // CHANGE IT

// the sender email credentials
#define SENDER_EMAIL "stockalertim@gmail.com" // CHANGE IT
#define SENDER_PASSWORD "qipe knqm hjzr azvh"  // CHANGE IT to your Google App password

// #define RECIPIENT_EMAIL "e0725941@u.nus.edu" // CHANGE IT
#define RECIPIENT_EMAIL "stockalertim@gmail.com" // CHANGE IT

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 587

// Replace with time zone offset in seconds
const long utcOffsetInSeconds = 8 * 3600;  // Example: UTC +3 hours

// Email & Time Instance
SMTPSession smtp;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// MQTT Client Instance
WiFiClient espClient;   // For secure WiFi SSL/TLS connection
WiFiClientSecure secureClient;
PubSubClient client(espClient);


// ----------------------------------------------------------------------------
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\n~~~~~WiFi Connected~~~~~");
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
}


// ----------------------------------------------------------------------------
// MQTT Connection, Send, Reconnection
void initMQTT() {
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();
}

void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("Connecting to ThingsBoard... ");
        if (client.connect(clientId, access_token, NULL)) {
            Serial.println("Connected!");
            client.subscribe("v1/devices/me/rpc/request/+");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" Trying again in 5 seconds...");
            delay(5000);
        }
    }
}

void sendTelemetry(float temp, float hum, float light) {
  // JSON document to hold the data
  DynamicJsonDocument doc(1024);
  doc["Reading ID"] = readingId;
  doc["Sensor Type"] = "Temp-Hum-Light";
  doc["Temperature"] = temp;
  doc["Humidity"] = hum;
  doc["Light Value"] = light;

  // Send JSON data to ThingsBoard
  String payload;
  serializeJson(doc, payload);
  // String payload = "{\"Temperature\": " + String(temp) + ", \"Humidity\": " + String(hum) + ", \"Light Value\": " + String(light) + "}";
  Serial.print("Sending payload: ");
  Serial.println(payload);

  client.publish(telemetry_topic, payload.c_str());

  delay(short_delay);
}

// Don't need for this, as no callback is required in this
// void on_message(char* topic, byte* payload, unsigned int length) {
//     Serial.print("Message arrived on topic: ");
//     Serial.println(topic);
// }


// ----------------------------------------------------------------------------
// Init SMTP Callback
void gmail_send(String subject, String textMsg) {
  // set the network reconnection option
  MailClient.networkReconnect(true);

  smtp.debug(1);

  smtp.callback(smtpCallback);
  Session_Config config;

  // set the session config
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = SENDER_EMAIL;
  config.login.password = SENDER_PASSWORD;
  config.login.user_domain = F("127.0.0.1");
  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

  // declare the message class
  SMTP_Message message;

  // set the message headers
  message.sender.name = F("ESP8266");
  message.sender.email = SENDER_EMAIL;
  message.subject = subject;
  message.addRecipient(F("To Whom It May Concern"), RECIPIENT_EMAIL);

  message.text.content = textMsg;
  message.text.transfer_encoding = "base64";
  message.text.charSet = F("utf-8");
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

  // set the custom message header
  message.addHeader(F("Message-ID: <abcde.fghij@gmail.com>"));

  // connect to the server
  if (!smtp.connect(&config, &secureClient)) {  // Pass wifiClient here
    Serial.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (!smtp.isLoggedIn()) {
    Serial.println("Not yet logged in.");
  } else {
    if (smtp.isAuthenticated())
      Serial.println("Successfully logged in.");
    else
      Serial.println("Connected with no Auth.");
  }

  // start sending Email and close the session
  if (!MailClient.sendMail(&smtp, &message))
    Serial.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
}

// callback function to get the Email sending status
void smtpCallback(SMTP_Status status) {
  // Serial.println(status.info());

  // print the sending result
  if (status.success()) {
    Serial.println("----------------");
    Serial.printf("Email sent success: %d\n", status.completedCount());
    Serial.printf("Email sent failed: %d\n", status.failedCount());
    Serial.println();

    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      SMTP_Result result = smtp.sendingResult.getItem(i);   // get the result item

      Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
      // Serial.printf("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());   //5 Hour lag
      Serial.printf("Recipient: %s\n", result.recipients.c_str());
      Serial.printf("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    // free the memory
    smtp.sendingResult.clear();
  }
}


// ----------------------------------------------------------------------------
// Initialise Sensor
void initSensor() {
  pinMode(lightSensorPin, INPUT);
  dht.begin();

  // Print temperature sensor details.
  sensor_t sensor;

  dht.temperature().getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  delay(short_delay);

  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
  Serial.println(F("------------------------------------"));
  delay(short_delay);
}


// ----------------------------------------------------------------------------
// Sensor Readings
sensors_event_t tempEvent, humEvent; // Declare tempEvent and humEvent globally

void DHTSensorRead() {
  unsigned long currentMillis = millis();

  dht.temperature().getEvent(&tempEvent);
  dht.humidity().getEvent(&humEvent);

  if (isnan(tempEvent.temperature)) {
      Serial.println(F("Error reading temperature!"));
  } else {
    temp_flag = true;
    temp = tempEvent.temperature;  // Store the value in global variable
  }

  if (isnan(humEvent.relative_humidity)) {
      Serial.println(F("Error reading humidity!"));
  } else {
    hum_flag = true;
    hum = humEvent.relative_humidity;  // Store the value in global variable
  }

  // For Debugging Purposes 
  if (temp_flag && hum_flag) {
    Serial.print("Data Point: ");
    Serial.println(readingId);
    Serial.print(F("Temperature: "));
    Serial.print(temp);
    Serial.println(F("°C"));
    Serial.print(F("Humidity: "));
    Serial.print(hum);
    Serial.println(F("%"));
  }

  // Email Data Send
  if (temp >= tempThresh || hum >= humThresh) {
    String subject = "Email Notification from Temp/Hum Sensor - ESP8266";
    String textMsg = "This is an email sent from ESP8266.\n";
    if (temp >= tempThresh && hum < humThresh) {
      textMsg += "The TEMPERATURE exceeded threshold: " + String(tempThresh) + "°C\n";
      textMsg += "Current temperature: " + String(temp) + "°C\n";
    } 
    if (temp < tempThresh && hum >= humThresh) {
      textMsg += "The HUMIDITY exceeded threshold: " + String(humThresh) + "%\n";
      textMsg += "Current humidity: " + String(hum) + "%\n";
    }
    if (temp >= tempThresh && hum >= humThresh) {
      textMsg += "Both TEMPERATURE and HUMIDITY exceeded threshold: " + String(tempThresh) + "°C  | " + String(humThresh) + "%\n";
      textMsg += "Current temperature: " + String(temp) + "°C\n";
      textMsg += "Current humidity: " + String(hum) + "%\n";
    }
    textMsg += "Time Recorded: " + timeClient.getFormattedTime();

    // Prevent Email Spam
    if (currentMillis - previousMillis >= interval) { 
      previousMillis = currentMillis; // Update the previous time
      gmail_send(subject, textMsg);
    }
    Serial.printf("Previous Email Time Elapse < %.2f mins\n", interval / 60000.0);
  }
  delay(short_delay);
}

void LightSensorRead() {
  light = analogRead(lightSensorPin); // Read the analog value (0 [dark]-4095 [bright]) from the light sensor
  unsigned long currentMillis = millis();

  Serial.print("Light Level: ");
  Serial.print(light);

  if (light < lightThresh) {
    Serial.println("  --> It's dark!");
  } else {
    Serial.println("  --> It's bright!");
  }
  Serial.println("-----");

  // Email Data Send
  if (light <= lightThresh) {
    String subject = "Email Notification from Temp/Hum Sensor - ESP8266";
    String textMsg = "This is an email sent from ESP8266.\n";
    textMsg += "The LIGHT sensor detected darkness below threshold: ";
    textMsg += String(lightThresh) + "\n";
    textMsg += "Current value: " + String(light) + "\n";
    textMsg += "Time Recorded: " + timeClient.getFormattedTime();

    // Prevent Email Spam
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis; // Update the previous time
      gmail_send(subject, textMsg);
    }
    Serial.printf("Previous Email Time Elapse < %.2f mins\n", interval / 60000.0);
  }
}


void setup() {
  Serial.begin(115200);

  initSensor();
  initWiFi();

  // Email SSL/TLS security
  secureClient.setInsecure();

  //thingsbaord server setup
  client.setServer(mqtt_server, 1883);
  // client.setCallback(on_message);

  timeClient.begin(); // Initialise NTPClient
}


void loop() {
  initMQTT();

  timeClient.update();  // Update timeClient to fetch current time

  DHTSensorRead();
  delay(short_delay);   // Wait for a short period before the next reading
  LightSensorRead();

  sendTelemetry(temp, hum, light);  // Sends every 5 secs because of long_delay

  readingId++;
  delay(long_delay);
}