#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

// Update these with values suitable for your network.

const char* ssid = "infind";
const char* password = "1518wifi";
const char* mqtt_server = "192.168.1.189";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

char mensaje[128];  // cadena de 128 caracteres

int LED = 0;  // valor inicial del LED
ADC_MODE(ADC_VCC);  //  habilita lectura del voltaje de alimentacion

DHTesp dht; // objeto sensor


void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if ((String)topic=="infind/GRUPO7/led/cmd"){ // en el caso de que llegue una instruccion para el led

    // Deserializacion del json de entrada
    StaticJsonDocument<32> leveldoc;
    DeserializationError error = deserializeJson(leveldoc, payload);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    LED = (int)leveldoc["level"]; // guardado del nivel de intensidad indicado como int
    
    analogWrite(BUILTIN_LED,100-LED); // cambio de intensidad del led con lógica invertida

    // Serializacion del mensaje de salida
    StaticJsonDocument<16> doc;
    doc["led"] = LED;
    String outLED="";
    serializeJson(doc, outLED);
    client.publish("infind/GRUPO7/led/status",outLED.c_str());
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a client ID
    String clientId = "ESP8266Client-";
    clientId += String(ESP.getChipId());
    // Attempt to connect
    if (client.connect(clientId.c_str(),"infind/GRUPO7/conexion",1,true,"{\"online\":false}")) { // Definición de LWM en modo retenido
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("infind/GRUPO7/conexion","{\"online\":true}",true); // Mensaje de aviso de conexion en modo retenido
      // ... and resubscribe
      client.subscribe("infind/GRUPO7/led/cmd",1);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// funcion que empaqueta los datos solicitados en formato json, serializa y publica
void pubDatos(const unsigned long &uptime, const unsigned int &LED) {
    
    // String que contendra la serializacion
    String out_datos = "";
    
    // Lectura datos del sensor
    float hum = dht.getHumidity();
    float temp = dht.getTemperature();

    // Lectura voltaje placa
    float vcc = ESP.getVcc();

    // Lectura informacion conexion wifi
    String ssid = WiFi.SSID().c_str();
    IPAddress ip = WiFi.localIP();
    double rssi = WiFi.RSSI();

    // Creacion y publicacion del JSON
    StaticJsonDocument<192> doc;

      doc["Uptime"] = uptime;
      doc["Vcc"] = vcc;
      
      JsonObject DHT11 = doc.createNestedObject("DHT11");
      DHT11["temp"] = temp;
      DHT11["hum"] = hum;
      doc["LED"] = LED;
      
      JsonObject Wifi = doc.createNestedObject("Wifi");
      Wifi["SSId"] = ssid;
      Wifi["IP"] = ip;
      Wifi["RSSI"] = rssi;

    serializeJson(doc, out_datos);
    client.publish("infind/GRUPO7/datos",out_datos.c_str());
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  analogWriteRange(100);  //nuevo rango PWM (0-100)
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.setup(5, DHTesp::DHT11); // Connect DHT sensor to GPIO 5
  analogWrite(BUILTIN_LED,100-LED); // El led inicia apagado por logica invertida
}

void loop() {


  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg >10000) {
    lastMsg = now;    
    pubDatos(now,LED);
  }
}
