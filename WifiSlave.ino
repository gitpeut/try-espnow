/*Jut now */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
extern "C" {
  #include <espnow.h>
  #include "user_interface.h"
}

#include "creds.h"

String deviceMac;
uint8_t sendBuffer[256];

struct __attribute__((packed)) SENSOR_DATA {
    uint8_t type;
    float vcc;
    float temp;
    float pressure;
} sensorData;

volatile boolean haveReading = false;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
const char* esphostname = "Jutnow";
char espversion[40]= "Jutnow V1.0";


/*-----------------------------------------------------------------*/

void startwifi()
{
  
  WiFi.mode( WIFI_AP_STA ); 
  WiFi.hostname(esphostname);
  
   WiFi.begin(wifiID, wifiPWD );
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
   Serial.println("\nWiFi connected");
   Serial.print("IP address: ");
   Serial.println(WiFi.localIP());

  httpUpdater.setup(&server);

   //SERVER INIT
  //list directory
  //server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/", HTTP_GET, [](){
    server.send(404, "text/plain", espversion);
  });

    server.onNotFound([](){
      server.send(404, "text/plain", "Jutnow : FileNotFound");
    });

  server.begin();

  
}


/* Set a private Mac Address
 *  http://serverfault.com/questions/40712/what-range-of-mac-addresses-can-i-safely-use-for-my-virtual-machines
 * Note: the point of setting a specific MAC is so you can replace this Gateway ESP8266 device with a new one
 * and the new gateway will still pick up the remote sensors which are still sending to the old MAC 
 */

/*-----------------------------------------------------------------*/

uint8_t mac[] = {0xba, 0xba, 0xba, 0xba, 0xba, 0xba}; 
void setMyAPMac() {
  wifi_set_macaddr(SOFTAP_IF, &mac[0]);
}

/*-----------------------------------------------------------------*/

void receiveMessage( uint8_t *mac, uint8_t *data, uint8_t len ){
  char temp[32];

  sprintf( temp, "%02X:%02X%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]); 
  Serial.printf("-Message with lengh %d received from device: %s\n",len,temp);

if( len ){
  switch( data[0] ){
   case 1: 
        memcpy( &sensorData, data, len);
        sprintf((char *)sendBuffer,"{\"VCC\" : %5.2f,\"Temperature\" : %4.2f,\"Pressure\" : %6.2f}",
        sensorData.vcc,
        sensorData.temp,
        sensorData.pressure); 

        Serial.printf( "-%s-\n", sendBuffer );
         
        break;
        
   case '{':
        Serial.printf("- JSON message received:\n      %s\n", data);
        break;
         
   default:
        Serial.println("Funny message\n");
        Serial.printf("-%s-\n", data);
  }      
}
/*
   sprintf((char *)sendBuffer,"{\"VCC\" : %5.2f,\"Temperature\" : %4.2f,\"Pressure\" : %6.2f}",
        WiFi.macAddress().c_str(),
        ESP.getVcc() / 1000.0,
        sensorData.temp,
        sensorData.pressure/100.0 );
 */

  haveReading = true;
}

/*-----------------------------------------------------------------*/

void initEspNow() {
  
  while (esp_now_init()!=0) {
    Serial.println("*** ESP_Now init failed");
    delay(1000);
  }
  Serial.println("*** ESP_Now init succeeded.");

  esp_now_set_self_role( ESP_NOW_ROLE_SLAVE );
  esp_now_register_recv_cb( receiveMessage );

}

/*-----------------------------------------------------------------*/


/* Presently it doesn't seem posible to use both WiFi and ESP-NOW at the
 * same time. This gateway gets around that be starting just ESP-NOW and
 * when a message is received switching on WiFi to sending the MQTT message
 * to Watson, and then restarting the ESP. The restart is necessary as 
 * ESP-NOW doesn't seem to work again even after WiFi is disabled.
 * Hopefully this will get fixed in the ESP SDK at some point.
 */

void setup() {
  Serial.begin(115200); Serial.println();

  setMyAPMac();
  WiFi.mode( WIFI_AP_STA ); 

  initEspNow();

  startwifi();
  
  Serial.print("This node AP mac: "); Serial.println(WiFi.softAPmacAddress());
  Serial.print("This node STA mac: "); Serial.println(WiFi.macAddress());

}

/*-----------------------------------------------------------------*/

void loop() {
  
  server.handleClient();
  delay(1000);
  
}

