#include <Arduino.h>
#include <Arduino_JSON.h>
#include <SPIFFS.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "ESPAsyncWebServer.h"

//---------------------------
// Oled display pins
#define CLK_PIN 22
#define DATA_PIN 21
#define RESET_PIN -1
//---------------------------

//---------------------------
// Board 1 indicator
#define Board_1_State 23
#define Board_1_Switch 26

// Board 2 indicator
#define Board_2_State 32
#define Board_2_Switch 27

// Board 3 indicator
#define Board_3_State 33
#define Board_3_Switch 14

// Board 4 indicator
#define Board_4_State 25
#define Board_4_Switch 12
//---------------------------

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

uint8_t AllBroadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t board_1_address[] = {0x7C, 0x9E, 0xBD, 0x48, 0x71, 0xE8};
uint8_t board_2_address[] = {0xC8, 0xF0, 0x9E, 0x9E, 0xF1, 0x88};
uint8_t board_3_address[] = {0xC8, 0xF0, 0x9E, 0x9E, 0x5D, 0xD0};
uint8_t board_4_address[] = {0x10, 0x52, 0x1C, 0x5D, 0x51, 0xAC};

const char board_1_address_str[18] = {"7c:9e:bd:48:71:e8"};
const char board_2_address_str[18] = {"c8:f0:9e:9e:f1:88"};
const char board_3_address_str[18] = {"c8:f0:9e:9e:5d:d0"};
const char board_4_address_str[18] = {"10:52:1c:5d:51:ac"};

// Oled display object
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, CLK_PIN, DATA_PIN,
                                           RESET_PIN);

// Wifi manager object
WiFiManager wm;

JSONVar board1;
JSONVar board2;
JSONVar board3;
JSONVar board4;

AsyncWebServer server(80);
AsyncEventSource events("/events");

bool Board_1_Last_Received_State;
bool Board_2_Last_Received_State;
bool Board_3_Last_Received_State;
bool Board_4_Last_Received_State;

char temp_str[50];
char hum_str[50];

// Structure example to receive data
// Must match the sender structure
typedef struct {
    byte id : 4;
    byte WiFi_Channel;
    bool status;
} Device_State_Msg_Struct;

typedef struct {
    byte id : 4;
    byte WiFi_Channel;
    bool status;
    float temp;
    float hum;
    int readingId;
} Sensor_Reading_Msg_Struct;

esp_err_t outcome;

Device_State_Msg_Struct Board1_Data;
Device_State_Msg_Struct Board3_Data;
Device_State_Msg_Struct Board4_Data;
Sensor_Reading_Msg_Struct Board2_Data;

int32_t getWiFiChannel(const char *ssid) {
    if (int32_t n = WiFi.scanNetworks()) {
        for (uint8_t i = 0; i < n; i++) {
            if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
                return WiFi.channel(i);
            }
        }
    }
    return 0;
}

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

    if (strstr(macStr, board_1_address_str)) {
        memcpy(&Board1_Data, incomingData, sizeof(Board1_Data));
        Serial.printf("Board ID %u: %u bytes\n", Board1_Data.id, len);

        if (Board1_Data.status != Board_1_Last_Received_State) {
            Serial.println(Board1_Data.status ? "Light ON\n" : "Light OFF\n");
            Board_1_Last_Received_State = Board1_Data.status;
        }

        board1["id"] = Board1_Data.id;
        board1["state"] = Board1_Data.status ? "ON" : "OFF";

        String jsonString1 = JSON.stringify(board1);
        events.send(jsonString1.c_str(), "new_device_state", millis());
    }

    if (strstr(macStr, board_2_address_str)) {
        memcpy(&Board2_Data, incomingData, sizeof(Board2_Data));
        sprintf(temp_str, "%4.2f", Board2_Data.temp);
        sprintf(hum_str, "%4.2f", Board2_Data.hum);
        Serial.printf("Board ID %u: %u bytes\n", Board2_Data.id, len);
        Serial.printf("Temperature: %4.2f \n", Board2_Data.temp);
        Serial.printf("Humidity: %4.2f \n", Board2_Data.hum);
        Serial.printf("readingID value: %d \n", Board2_Data.readingId);

        if (Board2_Data.status != Board_2_Last_Received_State) {
            Serial.println(Board2_Data.status ? "Light ON\n" : "Light OFF\n");
            Board_2_Last_Received_State = Board2_Data.status;
        }

        board2["id"] = Board2_Data.id;
        board2["temperature"] = Board2_Data.temp;
        board2["humidity"] = Board2_Data.hum;
        board2["readingId"] = String(Board2_Data.readingId);
        String jsonString2 = JSON.stringify(board2);
        events.send(jsonString2.c_str(), "new_readings", millis());
    }

    if (strstr(macStr, board_3_address_str)) {
        memcpy(&Board3_Data, incomingData, sizeof(Board3_Data));
        Serial.printf("Board ID %u: %u bytes\n", Board3_Data.id, len);

        if (Board3_Data.status != Board_3_Last_Received_State) {
            Serial.println(Board3_Data.status ? "Light ON\n" : "Light OFF\n");
            Board_3_Last_Received_State = Board3_Data.status;
        }

        board3["id"] = Board3_Data.id;
        board3["state"] = Board3_Data.status ? "ON" : "OFF";

        String jsonString3 = JSON.stringify(board3);
        events.send(jsonString3.c_str(), "new_device_state", millis());
    }

    if (strstr(macStr, board_4_address_str)) {
        memcpy(&Board4_Data, incomingData, sizeof(Board4_Data));
        Serial.printf("Board ID %u: %u bytes\n", Board4_Data.id, len);
        Serial.println(Board4_Data.status ? "Light ON\n" : "Light OFF\n");

        if (Board4_Data.status != Board_4_Last_Received_State) {
            Serial.println(Board4_Data.status ? "Light ON\n" : "Light OFF\n");
            Board_4_Last_Received_State = Board4_Data.status;
        }

        board4["id"] = Board4_Data.id;
        board4["state"] = Board4_Data.status ? "ON" : "OFF";

        String jsonString4 = JSON.stringify(board4);
        events.send(jsonString4.c_str(), "new_device_state", millis());
    }
}

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

void initWiFiManager() {
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
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
}

void registerPeers() {
    // register peers
    esp_now_peer_info_t peerInfo;
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    memset(&peerInfo, 0, sizeof(peerInfo));

    // register Board 1
    memcpy(peerInfo.peer_addr, board_1_address, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add Board 1");
        return;
    }

    // register Board 2
    memcpy(peerInfo.peer_addr, board_2_address, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add Board 2");
        return;
    }

    // register Board 3
    memcpy(peerInfo.peer_addr, board_3_address, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add Board 3");
        return;
    }

    // register Board 4
    memcpy(peerInfo.peer_addr, board_4_address, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add Board 4");
        return;
    }

    memcpy(peerInfo.peer_addr, AllBroadcastAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add broadcast address");
        return;
    }
}

void Send_To__Board_1() {
    outcome = esp_now_send(board_1_address, (uint8_t *)&Board1_Data,
                           sizeof(Board1_Data));
    Serial.println(outcome == ESP_OK ? "Successfully sent data to board 1"
                                     : "Error sending data to board 1");
}

void Send_To__Board_2() {
    outcome = esp_now_send(board_2_address, (uint8_t *)&Board2_Data,
                           sizeof(Board2_Data));
    Serial.println(outcome == ESP_OK ? "Successfully sent data to board 2"
                                     : "Error sending data to board 2");
}

void Send_To__Board_3() {
    outcome = esp_now_send(board_3_address, (uint8_t *)&Board3_Data,
                           sizeof(Board3_Data));
    Serial.println(outcome == ESP_OK ? "Successfully sent data to board 3"
                                     : "Error sending data to board 3");
}

void Send_To__Board_4() {
    outcome = esp_now_send(board_4_address, (uint8_t *)&Board4_Data,
                           sizeof(Board4_Data));
    Serial.println(outcome == ESP_OK ? "Successfully sent data to board 4"
                                     : "Error sending data to board 4");
}

void Broadcast_Channel() {
    int32_t channel = getWiFiChannel((wm.getWiFiSSID()).c_str());

    Board1_Data.WiFi_Channel = channel;
    Board2_Data.WiFi_Channel = channel;
    Board3_Data.WiFi_Channel = channel;
    Board4_Data.WiFi_Channel = channel;

    Serial.print("Sending Wifi channel: ");
    Serial.println(Board1_Data.WiFi_Channel);
    Board1_Data.id = 0;
    Board2_Data.id = 0;
    Board3_Data.id = 0;
    Board4_Data.id = 0;

    Send_To__Board_1();
    delay(100);
    Send_To__Board_2();
    delay(100);
    Send_To__Board_3();
    delay(100);
    Send_To__Board_4();
}

void initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS Init success");
}

void initServer() {
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", String(), false);
    });

    // Route to load style.css file
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css");
    });

    // Route to load javasript file
    server.on("/event.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/event.js", "text/javascript");
    });

    // Route to set GPIO to HIGH
    server.on("/1/on", HTTP_GET, [](AsyncWebServerRequest *request) {
        Board1_Data.id = 1;
        Board1_Data.status = true;
        Send_To__Board_1();
        request->send(SPIFFS, "/index.html", String());
    });

    // Route to set GPIO to LOW
    server.on("/1/off", HTTP_GET, [](AsyncWebServerRequest *request) {
        Board1_Data.id = 1;
        Board1_Data.status = false;
        Send_To__Board_1();
        request->send(SPIFFS, "/index.html", String());
    });

    server.on("/2/on", HTTP_GET, [](AsyncWebServerRequest *request) {
        Board2_Data.id = 2;
        Board2_Data.status = true;
        Send_To__Board_2();
        request->send(SPIFFS, "/index.html", String());
    });

    // Route to set GPIO to LOW
    server.on("/2/off", HTTP_GET, [](AsyncWebServerRequest *request) {
        Board2_Data.id = 2;
        Board2_Data.status = false;
        Send_To__Board_2();
        request->send(SPIFFS, "/index.html", String());
    });

    server.on("/3/on", HTTP_GET, [](AsyncWebServerRequest *request) {
        Board3_Data.id = 3;
        Board3_Data.status = true;
        Send_To__Board_3();
        request->send(SPIFFS, "/index.html", String());
    });

    // Route to set GPIO to LOW
    server.on("/3/off", HTTP_GET, [](AsyncWebServerRequest *request) {
        Board3_Data.id = 3;
        Board3_Data.status = false;
        Send_To__Board_3();
        request->send(SPIFFS, "/index.html", String());
    });

    server.on("/4/on", HTTP_GET, [](AsyncWebServerRequest *request) {
        Board4_Data.id = 4;
        Board4_Data.status = true;
        Send_To__Board_4();
        request->send(SPIFFS, "/index.html", String());
    });

    // Route to set GPIO to LOW
    server.on("/4/off", HTTP_GET, [](AsyncWebServerRequest *request) {
        Board4_Data.id = 4;
        Board4_Data.status = false;
        Send_To__Board_4();
        request->send(SPIFFS, "/index.html", String());
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

void setup() {
    // Initialize Serial Monitor
    Serial.begin(115200);
    // Set the device as a Station and Soft Access Point simultaneously
    WiFi.mode(WIFI_AP_STA);
    pinMode(Board_1_Switch, INPUT_PULLUP);
    pinMode(Board_1_State, OUTPUT);
    pinMode(Board_2_Switch, INPUT_PULLUP);
    pinMode(Board_2_State, OUTPUT);
    pinMode(Board_3_Switch, INPUT_PULLUP);
    pinMode(Board_3_State, OUTPUT);
    pinMode(Board_4_Switch, INPUT_PULLUP);
    pinMode(Board_4_State, OUTPUT);

    initSPIFFS();
    initDisplay();
    initEspNow();
    registerPeers();

    if (wm.getWiFiSSID() != "") Broadcast_Channel();

    initWiFiManager();
    initServer();
}

void loop() {
    digitalWrite(Board_1_State, Board_1_Last_Received_State);
    digitalWrite(Board_2_State, Board_2_Last_Received_State);
    digitalWrite(Board_3_State, Board_3_Last_Received_State);
    digitalWrite(Board_4_State, Board_4_Last_Received_State);

    if (!digitalRead(Board_1_Switch)) {
        Board1_Data.id = 1;
        Board1_Data.status = Board_1_Last_Received_State ? false : true;
        Send_To__Board_1();
        delay(200);
    }

    if (!digitalRead(Board_2_Switch)) {
        Board2_Data.id = 2;
        Board2_Data.status = Board_2_Last_Received_State ? false : true;
        Send_To__Board_2();
        delay(200);
    }

    if (!digitalRead(Board_3_Switch)) {
        Board3_Data.id = 3;
        Board3_Data.status = Board_3_Last_Received_State ? false : true;
        Send_To__Board_3();
        delay(200);
    }

    if (!digitalRead(Board_4_Switch)) {
        Board4_Data.id = 4;
        Board4_Data.status = Board_4_Last_Received_State ? false : true;
        Send_To__Board_4();
        delay(200);
    }

    Board1_Data.status ? display.drawStr(50, 20, "ON")
                       : display.drawStr(50, 20, "OFF");

    display.drawStr(75, 50, temp_str);
    display.drawStr(75, 60, hum_str);

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
