#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266httpUpdate.h>
#include "DHTesp.h"

// Parametros WiFi
const char* ssid = "infind";
const char* password = "1518wifi";
const char* mqtt_server = "192.168.1.189";

// Parametros OTA             >>>> SUSTITUIR IP <<<<<
#define HTTP_OTA_ADDRESS      F("192.168.1.189")       // Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update") // Path to update firmware
#define HTTP_OTA_PORT         1880                     // Port of update server
                                                       // Name of firmware
#define HTTP_OTA_VERSION      String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1) + ".nodemcu" 

int LED_OTA = 16;

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

//----------------------------------------------
//------------ FUNCIONES OTA -------------------
//----------------------------------------------

// funciones para progreso de OTA (cabeceras)
void progreso_OTA(int,int);
void final_OTA();
void inicio_OTA();
void error_OTA(int);

//-----------------------------------------------------
void intenta_OTA()
{ 
  Serial.println( "--------------------------" );  
  Serial.println( "Comprobando actualización:" );
  Serial.print(HTTP_OTA_ADDRESS);Serial.print(":");Serial.print(HTTP_OTA_PORT);Serial.println(HTTP_OTA_PATH);
  Serial.println( "--------------------------" );  
  ESPhttpUpdate.setLedPin(LED_OTA, LOW);
  ESPhttpUpdate.onStart(inicio_OTA);
  ESPhttpUpdate.onError(error_OTA);
  ESPhttpUpdate.onProgress(progreso_OTA);
  ESPhttpUpdate.onEnd(final_OTA);
  WiFiClient wClient; // Puede que haya que usar espClient en vez de esto en el swtich (?)
  switch(ESPhttpUpdate.update(wClient, HTTP_OTA_ADDRESS, HTTP_OTA_PORT, HTTP_OTA_PATH, HTTP_OTA_VERSION)) {
    case HTTP_UPDATE_FAILED:
      Serial.printf(" HTTP update failed: Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F(" El dispositivo ya está actualizado"));
      break;
    case HTTP_UPDATE_OK:
      Serial.println(F(" OK"));
      break;
    }
}

//-----------------------------------------------------
void final_OTA()
{
  Serial.println("Fin OTA. Reiniciando...");
}

void inicio_OTA()
{
  Serial.println("Nuevo Firmware encontrado. Actualizando...");
}

void error_OTA(int e)
{
  char cadena[64];
  snprintf(cadena,64,"ERROR: %d",e);
  Serial.println(cadena);
}

void progreso_OTA(int x, int todo)
{
  char cadena[256];
  int progress=(int)((x*100)/todo);
  if(progress%10==0)
  {
    snprintf(cadena,256,"Progreso: %d%% - %dK de %dK",progress,x/1024,todo/1024);
    Serial.println(cadena);
  }
}


//----------------------------------------------
//------------ RESTO DE FUNCIONES --------------
//----------------------------------------------

//-------------- Conexion WiFi -----------------

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

// ------------------- MQTT Callback ---------------------

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

//------------------ Conexion MQTT ---------------------

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

//----------- Publicacion de datos del NodeMCU -----------

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

//----------------------------------------------
//-------------- SETUP Y LOOP ------------------
//----------------------------------------------

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  analogWriteRange(100);  //nuevo rango PWM (0-100)
  Serial.begin(115200);
  setup_wifi();
  intenta_OTA(); // Busqueda de actualizaciones
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
