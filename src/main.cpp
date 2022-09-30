#include <Arduino.h>
#include <Arduino_JSON.h>
#include <U8g2lib.h>
// #include <WiFi.h>
#include <WiFiManager.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "ESPAsyncWebServer.h"

// Oled display pins
#define CLK_PIN 22
#define DATA_PIN 21
#define RESET_PIN -1

#define LightStatus 25
#define Switch 26

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, CLK_PIN, DATA_PIN,
                                           RESET_PIN);

// Update area left position (in tiles)
const uint8_t board_1_tile_area_x_pos = 6;
// Update area upper position (distance from top in tiles)
const uint8_t board_1_tile_area_y_pos = 1;
const uint8_t board_1_tile_area_width = 3;
// this will allow cour18 chars to fit into the area
const uint8_t board_1_tile_area_height = 2;

// Update area left position (in tiles)
const uint8_t board_2_tile_area_x_pos = 9;
// Update area upper position (distance from top in tiles)
const uint8_t board_2_tile_area_y_pos = 5;
const uint8_t board_2_tile_area_width = 4;
// this will allow cour18 chars to fit into the area
const uint8_t board_2_tile_area_height = 3;

// Replace with your network credentials (STATION)
// const char *ssid = "ThuHuong-2.4G";
// const char *password = "62749820";

uint8_t remoteAddress[] = {0x7C, 0x9E, 0xBD, 0x48, 0x71, 0xE8};
uint8_t sensorBoardAddress[] = {0xC8, 0xF0, 0x9E, 0x9E, 0xF1, 0x88};
uint8_t AllBroadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

const char board1_address[18] = {"7c:9e:bd:48:71:e8"};
const char board2_address[18] = {"c8:f0:9e:9e:f1:88"};

WiFiManager wm;

JSONVar board;

AsyncWebServer server(80);
AsyncEventSource events("/events");

bool LastReceivedStatus;
bool Command = false;

char temp[50];
char hum[50];

// Structure example to receive data
// Must match the sender structure
typedef struct {
    int id;
    char WiFiName[20];
    bool status = Command;
} struct_message1;

typedef struct {
    int id;
    char WiFiName[20];
    float temp;
    float hum;
    int readingId;
} struct_message2;

struct_message1 Board0;
struct_message1 Board1_Data;
struct_message2 Board2_Data;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success"
                                                  : "Delivery Fail");
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    display.clearBuffer();

    // Copies the sender mac address to a string
    char macStr[18];
    Serial.print("Packet received from: ");
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
             mac_addr[5]);
    Serial.println(macStr);

    if (strstr(macStr, board1_address)) {
        memcpy(&Board1_Data, incomingData, sizeof(Board1_Data));
        Serial.printf("Board ID %u: %u bytes\n", Board1_Data.id, len);
    }

    if (strstr(macStr, board2_address)) {
        memcpy(&Board2_Data, incomingData, sizeof(Board2_Data));
        sprintf(temp, "%4.2f", Board2_Data.temp);
        sprintf(hum, "%4.2f", Board2_Data.hum);
        Serial.printf("Board ID %u: %u bytes\n", Board2_Data.id, len);
        Serial.printf("Temperature: %4.2f \n", Board2_Data.temp);
        Serial.printf("Humidity: %4.2f \n", Board2_Data.hum);
        Serial.printf("readingID value: %d \n", Board2_Data.readingId);

        board["id"] = 1;
        board["temperature"] = Board2_Data.temp;
        board["humidity"] = Board2_Data.hum;
        board["readingId"] = String(Board2_Data.readingId);
        String jsonString = JSON.stringify(board);
        events.send(jsonString.c_str(), "new_readings", millis());
    }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP-NOW DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #2f4468; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .packet { color: #bebebe; }
    .card.temperature { color: #fd7e14; }
    .card.humidity { color: #1b78e2; }
  </style>
</head>
<body>
  <div class="topnav">
    <h3></h3>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> BOARD #1 - TEMPERATURE</h4><p><span class="reading"><span id="t1"></span> &deg;C</span></p><p class="packet">Reading ID: <span id="rt1"></span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> BOARD #1 - HUMIDITY</h4><p><span class="reading"><span id="h1"></span> &percnt;</span></p><p class="packet">Reading ID: <span id="rh1"></span></p>
      </div>
      <div class="card light">
        <h4><i class="fas fa-thermometer-half"></i> BOARD #2 - LIGHT</h4><p><span class="reading"><span id="t2"></span> &deg;C</span></p><p class="packet">Reading ID: <span id="rt2"></span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('new_readings', function(e) {
  console.log("new_readings", e.data);
  var obj = JSON.parse(e.data);
  document.getElementById("t"+obj.id).innerHTML = obj.temperature.toFixed(2);
  document.getElementById("h"+obj.id).innerHTML = obj.humidity.toFixed(2);
  document.getElementById("rt"+obj.id).innerHTML = obj.readingId;
  document.getElementById("rh"+obj.id).innerHTML = obj.readingId;
 }, false);
}
</script>
</body>
</html>)rawliteral";

void initDisplay() {
    display.begin();  // Initialize OLED display
    display.clearBuffer();
    display.setContrast(1);
    display.setFont(u8g2_font_ncenB08_tr);  // choose a suitable font
    display.drawStr(0, 10, "Board 1");
    display.drawStr(0, 20, "Light");
    display.drawStr(0, 40, "Board 2");
    display.drawStr(0, 50, "Temperature:");
    display.drawStr(0, 60, "Humidity:");
    display.sendBuffer();  // transfer internal memory to the display
    delay(1000);
}

void initWiFi() {
    // Set device as a Wi-Fi Station
    // Serial.print("\nConnecting to ");
    // Serial.println(ssid);
    // WiFi.begin(ssid, password);
    // while (WiFi.status() != WL_CONNECTED) {
    //     delay(500);
    //     Serial.print(".");
    // }

    if (!wm.autoConnect("AutoConnectAP", "password")) {
        Serial.println("Failed to connect");
        ESP.restart();
    }
    // if you get here you have connected to the WiFi
    Serial.println("Connected");

    // Print local IP address
    Serial.println("");
    Serial.print("Station IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Wi-Fi Channel: ");
    Serial.println(WiFi.channel());
}

void initEspNow() {
    if (esp_now_init() == ESP_OK) {
        Serial.println("ESP-NOW Init Success");
        esp_now_register_recv_cb(OnDataRecv);
        esp_now_register_send_cb(OnDataSent);
    } else {
        Serial.println("ESP-NOW Init Failed");
        delay(3000);
        ESP.restart();
    }
}

void registerPeers() {
    // register peers
    esp_now_peer_info_t peerInfo;
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    memset(&peerInfo, 0, sizeof(peerInfo));

    // register Device peer
    memcpy(peerInfo.peer_addr, remoteAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    memcpy(peerInfo.peer_addr, sensorBoardAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    memcpy(peerInfo.peer_addr, AllBroadcastAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }
}

void initServer() {
    // Start web server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });

    events.onConnect([](AsyncEventSourceClient *client) {
        if (client->lastId()) {
            Serial.printf(
                "Client reconnected! Last message ID that it got is: %u\n",
                client->lastId());
        }
        // send event with message "hello!", id current millis
        // and set reconnect delay to 1 second
        client->send("hello!", NULL, millis(), 10000);
    });
    server.addHandler(&events);
    server.begin();
}

void BroadcastSSID() {
    strcpy(Board0.WiFiName, (wm.getWiFiSSID()).c_str());
    strcpy(Board2_Data.WiFiName, (wm.getWiFiSSID()).c_str());
    Serial.print("Sending Wifi named ");
    Serial.println(Board0.WiFiName);
    Board0.id = 0;
    Board2_Data.id = 0;

    esp_err_t outcome0 =
        esp_now_send(remoteAddress, (uint8_t *)&Board0, sizeof(Board0));
    Serial.println(outcome0 == ESP_OK
                       ? "Successfully sent the wifi name to board 1"
                       : "Error sending the wifi name to board 1");

    delay(100);

    esp_err_t outcome1 = esp_now_send(
        sensorBoardAddress, (uint8_t *)&Board2_Data, sizeof(Board2_Data));
    Serial.println(outcome1 == ESP_OK
                       ? "Successfully sent the wifi name to board 2"
                       : "Error sending the wifi name to board 2");
}

void setup() {
    // Initialize Serial Monitor
    Serial.begin(115200);
    // Set the device as a Station and Soft Access Point simultaneously
    WiFi.mode(WIFI_AP_STA);
    pinMode(Switch, INPUT_PULLUP);
    pinMode(LightStatus, OUTPUT);

    initDisplay();
    initEspNow();
    registerPeers();

    if (wm.getWiFiSSID() != "") BroadcastSSID();

    initWiFi();
    initServer();
}

void loop() {
    digitalWrite(LightStatus, LastReceivedStatus);

    if (!digitalRead(Switch)) {
        // Command = !Command;
        Board0.id = 1;
        // Board0.status = Command;

        LastReceivedStatus ? Board0.status = false : Board0.status = true;

        esp_err_t outcome1 =
            esp_now_send(remoteAddress, (uint8_t *)&Board0, sizeof(Board0));
        Serial.println(outcome1 == ESP_OK ? "Sent with success"
                                          : "Error sending the data");

        delay(200);
    }

    if (Board1_Data.status != LastReceivedStatus) {
        Serial.println(Board1_Data.status ? "Light ON\n" : "Light OFF\n");
        LastReceivedStatus = Board1_Data.status;
    }

    Board1_Data.status ? display.drawStr(50, 20, "ON")
                       : display.drawStr(50, 20, "OFF");

    display.drawStr(75, 50, temp);
    display.drawStr(75, 60, hum);

    display.updateDisplayArea(board_1_tile_area_x_pos, board_1_tile_area_y_pos,
                              board_1_tile_area_width,
                              board_1_tile_area_height);

    display.updateDisplayArea(board_2_tile_area_x_pos, board_2_tile_area_y_pos,
                              board_2_tile_area_width,
                              board_2_tile_area_height);

    static unsigned long lastEventTime = millis();
    static const unsigned long EVENT_INTERVAL_MS = 5000;
    if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
        events.send("ping", NULL, millis());
        lastEventTime = millis();
    }

    delay(100);
}
