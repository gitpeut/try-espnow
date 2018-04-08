/*
 ESP-NOW based sensor using a BME280 temperature/pressure/humidity sensor
 Sends readings every 15 minutes to a server with a fixed mac address
 It takes about 215 milliseconds to wakeup, send a reading and go back to sleep, 
 and it uses about 70 milliAmps while awake and about 25 microamps while sleeping, 
 so it should last for a good year even AAA alkaline batteries. 
 Anthony Elder
 License: Apache License v2
*/
#include <ESP8266WiFi.h>

extern "C" {
  #include <espnow.h>
}
ADC_MODE(ADC_VCC); //vcc read

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>


#define RED 12
#define GREEN 14

/* Set a private Mac Address
 *  http://serverfault.com/questions/40712/what-range-of-mac-addresses-can-i-safely-use-for-my-virtual-machines
 * Note: the point of setting a specific MAC is so you can replace this Gateway ESP8266 device with a new one
 * and the new gateway will still pick up the remote sensors which are still sending to the old MAC 
 */
uint8_t mac[] = {0xba, 0xa2, 0x50, 0x55, 0x09, 0x56};
void initVariant() {
  //wifi_set_macaddr(SOFTAP_IF, &mac[0]);
}

// this is the MAC Address of the remote ESP server which receives these sensor readings
uint8_t remoteMac[] = {0xba, 0xa2, 0x50, 0x55, 0x09, 0x56};
uint8_t allMac[]    = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

#define WIFI_CHANNEL 4
#define SLEEP_SECS 20// 5*605 minutes
#define SEND_TIMEOUT 245  // 245 millis seconds timeout 

// keep in sync with slave struct
struct __attribute__((packed)) SENSOR_DATA {
    uint8_t type;
    float vcc;
    float temp;
    float pressure;
} sensorData;

Adafruit_BMP280 bmp280;

char sendBuffer[256];
int starttime, sendtime;

/*----------------------------------------------------------*/

void startBMP280() {
  Wire.begin( 4, 5); //sda = 0, scl = 2
  Serial.print("bmp280 init="); Serial.println(bmp280.begin(), HEX);

}

/*----------------------------------------------------------*/

void readBMP280(){
  sensorData.temp      = bmp280.readTemperature();
  sensorData.pressure  = bmp280.readPressure();
  
  //Serial.printf("temp=%5.2f, pressure=%8.1f\n", sensorData.temp, sensorData.pressure);

}

/*----------------------------------------------------------*/

void gotoSleep() {
  // add some randomness to avoid collisions with multiple devices
  //int sleepSecs = SLEEP_SECS + ((uint8_t)RANDOM_REG32/2); 
  Serial.printf("Up for %d ms, going to sleep for %d secs...\n", millis(), SLEEP_SECS); 
  ESP.deepSleep(SLEEP_SECS * 1000000, RF_NO_CAL);
}

/*----------------------------------------------------------*/

void sendAck( uint8_t* mac, uint8_t ack){

    sendtime = micros() - starttime;

    Serial.printf("- Send done, ack from %02X:%02X:%02X:%02X:%02X:%02X status = %x (%s), time used for send %d µs\n", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
        ack, ack?"error":"ok", sendtime);

    if ( ack == 0 ){
            digitalWrite(RED,   LOW );
            digitalWrite(GREEN, HIGH );
    }else{
            digitalWrite(GREEN, LOW );
            digitalWrite(RED,   HIGH );
    }
    //gotoSleep();

}

/*----------------------------------------------------------*/

int startEspNow(){
  int status;

  if ( !(status = esp_now_init())  ) {

    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(remoteMac, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0);
    esp_now_add_peer(allMac, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0);

    esp_now_register_send_cb( sendAck );
  }
  
return ( status );
}

/*----------------------------------------------------------*/

void setup() {
  Serial.begin(115200); Serial.println();

  // read sensor first before awake generates heat
  startBMP280();

  WiFi.mode(WIFI_STA); // Station mode for esp-now sensor node
  WiFi.disconnect();

  pinMode( RED, OUTPUT); 
  pinMode( GREEN, OUTPUT); 

  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);

  for ( int i=1 ; i < 5 ; ++i ){
      digitalWrite(GREEN, (i&1) );
      delay( 300 );        
  }

  Serial.printf("This mac: %s, ", WiFi.macAddress().c_str()); 
  Serial.printf("target mac: %02X:%02X:%02X:%02X:%02X:%02X", remoteMac[0], remoteMac[1], remoteMac[2], remoteMac[3], remoteMac[4], remoteMac[5]); 
  Serial.printf(", channel: %d\n", WIFI_CHANNEL); 

  sensorData.type=1;
  
  startEspNow();
  
}
/*----------------------------------------------------------*/

void loop() {


  readBMP280();

  sensorData.type     = 1;
  sensorData.vcc      = ESP.getVcc() / 1000.0;
  sensorData.temp     = sensorData.temp;
  sensorData.pressure = sensorData.pressure/100.0;
  
  uint8_t bs[sizeof(sensorData)];
  memcpy(bs, &sensorData, sizeof(sensorData));

    
  int buflen = sprintf( sendBuffer, "{\"Node\": \"%s\", \"VCC\" : %4.2f,\"Temperature\" : %5.2f,\"Pressure\" : %7.2f}",
        WiFi.macAddress().c_str(),
        ESP.getVcc() / 1000.0,
        sensorData.temp,
        sensorData.pressure );

         
  //Serial.printf("- Sending buffer ( size = %d )(stringbuffer length %d)\n", sizeof(sensorData), buflen );
  starttime = micros();
  
  //  esp_now_send(NULL, bs, sizeof( sensorData) ); // NULL means send to all peers
  esp_now_send(NULL, ( uint8_t *)&sendBuffer[0], buflen ); // NULL means send to all peers
  
  //Serial.println( sendBuffer ); 

  //gotoSleep();
  
  delay(500);  

}


