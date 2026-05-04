#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

/*================ WIFI =================*/
const char* ssid = "UIGM";
const char* password = "IGMISTHEBEST";

/*================ MQTT =================*/
const char* mqtt_server = "10.0.0.21";
const int mqtt_port = 1883;
const char* mqtt_topic_uid = "rfid/uid";
const char* mqtt_topic_data = "rfid/data";

/*================ PIN =================*/
#define BUZZER D4             // GPIO2
SoftwareSerial RFID(D7, D6);  // RX, TX (RDM6300 TX → D7)

/*================ LCD =================*/
LiquidCrystal_I2C lcd(0x27, 16, 2);

/*================ MQTT =================*/
WiFiClient espClient;
PubSubClient client(espClient);

/*================ RFID =================*/
String uid = "";
String lastUID = "";
bool reading = false;

/*================ DATA =================*/
String namaServer = "";
String jamServer = "--:--:--";

/*================ STATE =================*/
bool tampilData = false;
unsigned long tampilTime = 0;
const unsigned long standbyDelay = 1000;

/*================ BUZZER NON BLOCK =================*/
bool buzzerOn = false;
unsigned long buzzerTime = 0;
unsigned long buzzerDuration = 0;

bool beep3Active = false;
int beepCount = 0;
bool beepOn = false;
unsigned long beepTimer = 0;

const unsigned int beepFreq = 2000;    // frekuensi beep
const unsigned int beepOnTime = 150;   // durasi bunyi
const unsigned int beepOffTime = 150;  // jeda antar beep



/*================ MQTT CALLBACK =================*/
void callback(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != mqtt_topic_data) return;

  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  namaServer = doc["nama"].as<String>();
  jamServer = doc["jam"].as<String>();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(namaServer);
  lcd.setCursor(0, 1);
  lcd.print(jamServer);
  if (namaServer == "TIDAK TERDAFTAR") {
    /*buzzerOn = true;
    buzzerTime = millis();
    tone(BUZZER, 800);
    buzzerDuration = 300; */
    startBeep3x();
  } else {
    buzzerOn = true;
    buzzerTime = millis();
    tone(BUZZER, 2000);
    buzzerDuration = 300; 
    //buzzerPlay(2000, 200);
  }
  tampilData = true;
  tampilTime = millis();

  //buzzerOn = true;
  //buzzerTime = millis();
  //tone(BUZZER, 2000);
}

/*================ WIFI =================*/
void setup_wifi() {
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.print("WiFi Connect");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
  }

  lcd.clear();
  lcd.print("WiFi OK");
  delay(1000);
}

/*================ MQTT RECONNECT =================*/
void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP8266_RFID")) {
      client.subscribe(mqtt_topic_data);
    } else {
      delay(2000);
    }
  }
}

/*================ SETUP =================*/
void setup() {
  Serial.begin(9600);

  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);

  RFID.begin(9600);

  Wire.begin(D2, D1);
  lcd.init();
  lcd.backlight();

  lcd.print("RFID MQTT");
  delay(1000);

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tempelkan");
  lcd.setCursor(0, 1);
  lcd.print("Kartu RFID");
}

/*================ LOOP =================*/
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  /*==== BACA RFID ====*/
  while (RFID.available()) {
    char c = RFID.read();

    if (c == 0x02) {
      uid = "";
      reading = true;
    } else if (c == 0x03) {
      reading = false;

      if (uid.length() > 0 && uid != lastUID) {
        lastUID = uid;
        client.publish(mqtt_topic_uid, uid.c_str());

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("UID:");
        lcd.setCursor(0, 1);
        lcd.print(uid);
      }
    } else if (reading) {
      uid += c;
    }
  }

  /*==== BUZZER OFF ====*/
  if (buzzerOn && millis() - buzzerTime > buzzerDuration) {
    buzzerOn = false;
    noTone(BUZZER);
  }

  /*==== KEMBALI STANDBY ====*/
  if (tampilData && millis() - tampilTime > standbyDelay) {
    tampilData = false;
    lastUID = "";
    uid = "";

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tempelkan");
    lcd.setCursor(0, 1);
    lcd.print("Kartu RFID");
  }


  handleBeep3x();
}

void startBeep3x() {
  beep3Active = true;
  beepCount = 0;
  beepOn = false;
  beepTimer = millis();
}

void handleBeep3x() {
  if (!beep3Active) return;

  unsigned long now = millis();

  if (!beepOn && now - beepTimer >= beepOffTime) {
    // mulai beep
    tone(BUZZER, beepFreq);
    beepOn = true;
    beepTimer = now;
  }

  if (beepOn && now - beepTimer >= beepOnTime) {
    // stop beep
    noTone(BUZZER);
    beepOn = false;
    beepTimer = now;
    beepCount++;

    if (beepCount >= 3) {
      beep3Active = false;  // selesai
    }
  }
}
