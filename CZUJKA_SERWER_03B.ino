
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <HTTPClient.h>

uint8_t channel = 0;
bool encrypt = false;
String centralPrefix = "centrala_";
String sensorPrefix = "czujnik_";
String centralPassword = "123456789";
const int maxSensors = 8;

String T_SSID = "LokumCBA";
String T_PASW = "bigbadziol";
String T_HTTP_TEST = "http://example.com/index.html";
const int MAX_TRY_CONNECT = 10;
HTTPClient http;


typedef struct sData {
  char sensorName[32];
  int mesure;
  int battery;
  int newSleepTime;
  char newSensorName[32];

} sData;

sData IncData;
sData OutData;


esp_now_peer_info_t ScanedSensors[ maxSensors] = {};
int NumScanSensors = 0;

int buttonPin = 4;
int buttonState = 0;

typedef struct sSensor{
   char name[32];
   int sectionNum;
   char mac[24];
}sSensor;

 sSensor KnownSensors[ maxSensors] = {};
 int NumKnowSensors = 0;
 
 

/*-----------------------------------------------------------------------------------------------------------------
   Starting AP , if sensor dont see "central" do nothing
   Init espNow
  ----------------------------------------------------------------------------------------------------------------/
*/
void InitESPNow() {
  String myMac = WiFi.macAddress();
  String mySSID = centralPrefix + myMac;
  //
  int conTry = MAX_TRY_CONNECT;
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP_STA);

  WiFi.begin(T_SSID.c_str(), T_PASW.c_str(), channel);
  Serial.printf("\n[ESP/WIFI]-> Try to connect : %s \n", T_SSID);
  while (WiFi.status() != WL_CONNECTED && conTry > 0) {
    delay(1000);
    conTry--;
    Serial.printf("[ESP/WIFI]-> Connection try : %d \n", conTry);
  };
  if (conTry == 0) {
    Serial.printf("[ESP/WIFI]-> WiFi NOT connected\n");
    abort();
  };
  Serial.printf("[ESP/WIFI]-> WiFi connected.\n");
  Serial.printf("[WIFI]->  IP: %s \n", WiFi.localIP().toString().c_str());
  Serial.printf("[WIFI]-> Mac: %s \n", WiFi.macAddress().c_str());
  Serial.printf("[WIFI]->Chan: %d \n", WiFi.channel());
  Serial.printf("[WIFI]->Mask: %s \n", WiFi.subnetMask().toString().c_str());
  Serial.printf("[WIFI]->Gate: %s \n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("[WIFI]->DNS : %s \n", WiFi.dnsIP().toString().c_str());
  //

  //WiFi.disconnect();
  bool result = WiFi.softAP(mySSID.c_str(), centralPassword.c_str(), channel, 0);
  if (result) {
    Serial.println("[ESP/AP] -> Config success , AP :" + String(mySSID));
    if (esp_now_init() == ESP_OK) {
      Serial.println("[ESP/Now] Init Success");
    }
    else {
      Serial.println("[ESP/Now] Init Failed");
      ESP.restart();
    };//now init
  } else { //ap ok
    Serial.println("[ESP/AP] -> Init failed , restart ");
    ESP.restart();
  };
};
/*-----------------------------------------------------------------------------------------------------------------

  ----------------------------------------------------------------------------------------------------------------/
*/
void HttpTest() {
  int HttpCode;
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(T_HTTP_TEST.c_str());
    HttpCode = http.GET();
    if (HttpCode > 0) {
      Serial.printf("[HTTP]-> CODE : &d \n", HttpCode);
      if (HttpCode == HTTP_CODE_OK){
        String HttpData = http.getString();
        Serial.println(HttpData);
      };
    }
    else
    {
      Serial.printf("[HTTP] -> ERROR :%s \n", http.errorToString(HttpCode).c_str());
    };//http error
   http.end(); 
  }//connected
  else
  {
    Serial.printf("[HTTP]-> Not connected to WIFI\n");
  };// not connected
};
/*-----------------------------------------------------------------------------------------------------------------

  ----------------------------------------------------------------------------------------------------------------/
*/
void ScanForSensors() {
  int8_t scanResults = WiFi.scanNetworks();
  int nos = 0;// num of sensors
  memset(ScanedSensors, 0, sizeof(ScanedSensors));
  NumScanSensors = 0;
  if (scanResults == 0) {
    Serial.println("No AP devices...");
  } else {
    for (int i = 0; i < scanResults; ++i) {
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);
      if (SSID.indexOf(sensorPrefix) == 0) {
        nos ++;
        Serial.printf("%d . %s -> %s  %d \n", nos, SSID.c_str() , BSSIDstr.c_str(), RSSI);
        int mac[6];

        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int j = 0; j < 6; ++j ) {
            ScanedSensors[NumScanSensors].peer_addr[j] = (uint8_t) mac[j];
          };
        }
        ScanedSensors[NumScanSensors].channel = channel;
        ScanedSensors[NumScanSensors].encrypt = encrypt;
        NumScanSensors++;
      };
    };
  };
  WiFi.scanDelete();
};
/*-----------------------------------------------------------------------------------------------------------------

  ----------------------------------------------------------------------------------------------------------------/
*/
void ManageSensors() {
  if (NumScanSensors > 0) {
    for (int i = 0; i < NumScanSensors; i++) {
      for (int j = 0; j < 6; ++j ) {
        Serial.print((uint8_t) ScanedSensors[i].peer_addr[j], HEX);
        if (j != 5) Serial.print(":");
      };
      Serial.print(" -> ");
      bool exists = esp_now_is_peer_exist(ScanedSensors[i].peer_addr);
      if (exists) {
        Serial.println("paired");
      } else {
        esp_err_t addStatus = esp_now_add_peer(&ScanedSensors[i]);
        if (addStatus == ESP_OK) {
          Serial.println("new pair");
        } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
          Serial.println("not init");
        } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
          Serial.println(" invalid argument");
        } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
          Serial.println("list full");
        } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
          Serial.println("out of memory");
        } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
          Serial.println("peer exists");
        } else {
          Serial.println("hm....");
        };
      }// new state
    } // for count sensors
  };// NumScanSensors >0
};
/*-----------------------------------------------------------------------------------------------------------------

  ----------------------------------------------------------------------------------------------------------------
*/
void sendDataAll() {
  String randomName;
  //OutData.mesure = 0;
  //OutData.battery = 0;
  //OutData. sensorName = .....
  randomName = sensorPrefix + String(random(100));
  strcpy(OutData.newSensorName, randomName.c_str());
  OutData.newSleepTime = 20 + random(15);


  for (int i = 0; i < NumScanSensors; i++) {
    const uint8_t *peer_addr = ScanedSensors[i].peer_addr;
    if (i == 0) { // print only for first slave
      Serial.printf("============================= \n");
      Serial.printf("Sending to all: \n");
      Serial.printf("New Name : %s \n", OutData.newSensorName);
      Serial.printf("New sleep time : %d \n", OutData.newSleepTime);
      Serial.printf("============================= \n");
    };
    esp_err_t result = esp_now_send(peer_addr, (uint8_t*) &OutData, sizeof(OutData));
    Serial.print("Send Status: ");
    if (result == ESP_OK) {
      Serial.println("Success");
    } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
      Serial.println("ESPNOW not Init.");
    } else if (result == ESP_ERR_ESPNOW_ARG) {
      Serial.println("Invalid Argument");
    } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
      Serial.println("Internal Error");
    } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
      Serial.println("ESP_ERR_ESPNOW_NO_MEM");
    } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
      Serial.println("Peer not found.");
    } else {
      Serial.println("Not sure what happened");
    }; // status
    //delay(100);
  }; // for num sensors
};
/*-----------------------------------------------------------------------------------------------------------------

  ----------------------------------------------------------------------------------------------------------------/
*/
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
/*-----------------------------------------------------------------------------------------------------------------

  ----------------------------------------------------------------------------------------------------------------/
*/
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println("------------------------------------");
  memcpy(&IncData, data, sizeof(IncData));
  Serial.printf("From : %s \n", macStr);
  Serial.printf("Name :: %s \n", IncData.sensorName);
  Serial.printf("Mesure: %d \n", IncData.mesure);
  Serial.printf("Battery: %d \n", IncData.battery);
  Serial.println("------------------------------------");
};
/*-----------------------------------------------------------------------------------------------------------------

  ----------------------------------------------------------------------------------------------------------------/
*/
void setup() {
  Serial.begin(115200);
  Serial.println("Serwer czujek 3.0b 1.0");
  InitESPNow();
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  //HttpTest();
};
/*-----------------------------------------------------------------------------------------------------------------

  ----------------------------------------------------------------------------------------------------------------/
*/

void loop() {
    
  ScanForSensors();
  if (NumScanSensors > 0) {
    ManageSensors();
    //sendDataAll();
  } else {
  }

  buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH) {
    sendDataAll();
  } else {
    //Serial.println("...");

  };
  yield();
};
