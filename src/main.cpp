#include <Arduino.h>
#include <Arduino_JSON.h>
#include <SPIFFS.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "ESPAsyncWebServer.h"

#define DEFAULT_CHANNEL 1
#define WIFI_TRIGGER 13
#define DEBOUNCETIME 150

volatile int numberOfButtonInterrupts_1 = 0;
volatile bool lastState1;
volatile uint32_t debounceTimeout1 = 0;

volatile int numberOfButtonInterrupts_2 = 0;
volatile bool lastState2;
volatile uint32_t debounceTimeout2 = 0;

volatile int numberOfButtonInterrupts_3 = 0;
volatile bool lastState3;
volatile uint32_t debounceTimeout3 = 0;

volatile int numberOfButtonInterrupts_4 = 0;
volatile bool lastState4;
volatile uint32_t debounceTimeout4 = 0;

// Stores last time scheduled ping was executed
unsigned long previousMillis = 0;
// Interval at which to ping others
const long interval = 240000;

//---------------------------
// Oled display pins
#define CLK_PIN 22
#define DATA_PIN 21
#define RESET_PIN -1
//---------------------------

#define FAILED_LIMIT 5

//---------------------------
// Board 1 indicator
#define Board_1_State 32
#define Board_1_Switch 17

// Board 2 indicator
#define Board_2_State 33
#define Board_2_Switch 5

// Board 3 indicator
#define Board_3_State 25
#define Board_3_Switch 18

// Board 4 indicator
#define Board_4_State 26
#define Board_4_Switch 19
//---------------------------

// uint8_t AllBroadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t board_1_address[] = {0x7C, 0x9E, 0xBD, 0x48, 0x71, 0xE8};
uint8_t board_2_address[] = {0xC8, 0xF0, 0x9E, 0x9E, 0xF1, 0x88};
uint8_t board_3_address[] = {0xC8, 0xF0, 0x9E, 0x9E, 0x5D, 0xD0};
uint8_t board_4_address[] = {0x10, 0x52, 0x1C, 0x5D, 0x51, 0xAC};

uint8_t *pBoard1 = board_1_address;
uint8_t *pBoard2 = board_2_address;
uint8_t *pBoard3 = board_3_address;
uint8_t *pBoard4 = board_4_address;
uint8_t *AddressArr[4] = {pBoard1, pBoard2, pBoard3, pBoard4};

const char board_1_address_str[18] = {"7c:9e:bd:48:71:e8"};
const char board_2_address_str[18] = {"c8:f0:9e:9e:f1:88"};
const char board_3_address_str[18] = {"c8:f0:9e:9e:5d:d0"};
const char board_4_address_str[18] = {"10:52:1c:5d:51:ac"};

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, CLK_PIN, DATA_PIN,
                                           RESET_PIN);

WiFiManager wm;

JSONVar board1;
JSONVar board2;
JSONVar board3;
JSONVar board4;

AsyncWebServer server(80);
AsyncEventSource events("/events");
AsyncWebSocket ws("/ws");

esp_now_peer_info_t slave;
esp_err_t outcome;

bool portalRunning = false;
bool Board_1_Last_Received_State, Board_2_Last_Received_State,
    Board_3_Last_Received_State, Board_4_Last_Received_State;
bool Slave_1_On_Correct_Channel = true, Slave_2_On_Correct_Channel = true,
     Slave_3_On_Correct_Channel = true, Slave_4_On_Correct_Channel = true;

byte B1_failed_count, B2_failed_count, B3_failed_count, B4_failed_count;

char ip_str[20], str1[20], str2[20], str3[20], str4[20];

// Structure example to receive data
typedef struct {
    byte id : 4;
    byte WiFi_Channel;
    bool status;
} Default_Struct;

typedef struct {
    byte id : 4;
    byte WiFi_Channel;
    bool status;
    bool Motion_Detected;
} Board1_Data_Struct;

typedef struct {
    byte id : 4;
    byte WiFi_Channel;
    bool status;
    int temp;
    byte hum : 7;
    int readingId;
} Board2_Data_Struct;

typedef struct {
    byte id : 4;
    byte WiFi_Channel;
    bool status;
    int temp;
    int pres;
    int readingID;
} Board3_Data_Struct;

Board1_Data_Struct Board1_Data;
Board2_Data_Struct Board2_Data;
Board3_Data_Struct Board3_Data;
Default_Struct Board4_Data;

portMUX_TYPE mux1 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE mux2 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE mux3 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE mux4 = portMUX_INITIALIZER_UNLOCKED;

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
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
             mac_addr[5]);
    Serial.print("Last Packet Sent to: ");
    Serial.println(macStr);
    Serial.print("Last Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success"
                                                  : "Delivery Fail");
    // If sent failed raise flag base on the corresponding mac address
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("Raising flag");
        if (strstr(macStr, board_1_address_str)) {
            Slave_1_On_Correct_Channel = false;
            if (B1_failed_count >= FAILED_LIMIT) {
                Slave_1_On_Correct_Channel = true;
            } else
                B1_failed_count++;
        } else if (strstr(macStr, board_2_address_str)) {
            Slave_2_On_Correct_Channel = false;
            if (B2_failed_count >= FAILED_LIMIT) {
                Slave_2_On_Correct_Channel = true;
            } else
                B2_failed_count++;
        } else if (strstr(macStr, board_3_address_str)) {
            Slave_3_On_Correct_Channel = false;
            if (B3_failed_count >= FAILED_LIMIT) {
                Slave_3_On_Correct_Channel = true;
            } else
                B3_failed_count++;
        } else if (strstr(macStr, board_4_address_str)) {
            Slave_4_On_Correct_Channel = false;
            if (B4_failed_count >= FAILED_LIMIT) {
                Slave_4_On_Correct_Channel = true;
            } else
                B4_failed_count++;
        }
    } else {
        // If sent successfully remove the flag and reset count of said address
        if (strstr(macStr, board_1_address_str)) {
            Slave_1_On_Correct_Channel = true;
            B1_failed_count = 0;
        } else if (strstr(macStr, board_2_address_str)) {
            Slave_2_On_Correct_Channel = true;
            B2_failed_count = 0;
        } else if (strstr(macStr, board_3_address_str)) {
            Slave_3_On_Correct_Channel = true;
            B3_failed_count = 0;
        } else if (strstr(macStr, board_4_address_str)) {
            Slave_4_On_Correct_Channel = true;
            B4_failed_count = 0;
        }
    }
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    // Copies the sender mac address to a string
    char macStr[18];
    Serial.print("Packet received from: ");
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
             mac_addr[5]);
    Serial.println(macStr);

    if (strstr(macStr, board_1_address_str)) {
        Slave_1_On_Correct_Channel = true;
        B1_failed_count = 0;

        memcpy(&Board1_Data, incomingData, sizeof(Board1_Data));
        snprintf(str1, 20, "1.%s|%s", Board1_Data.status ? "ON" : "OFF",
                 Board1_Data.Motion_Detected ? "Detected" : "Not detected");
        Serial.printf("Board ID %u: %u bytes\n", Board1_Data.id, len);
        Serial.println(Board1_Data.Motion_Detected ? "Motion: Detected"
                                                   : "Motion: Not detected");

        if (Board1_Data.status != Board_1_Last_Received_State) {
            display.clearBuffer();
            Serial.println(Board1_Data.status ? "Light ON\n" : "Light OFF\n");
            Board_1_Last_Received_State = Board1_Data.status;
        }

        board1["id"] = 1;
        board1["state"] = Board1_Data.status ? "ON" : "OFF";
        board1["motion"] =
            Board1_Data.Motion_Detected ? "Detected" : "Not detected";

        String jsonString1 = JSON.stringify(board1);
        events.send(jsonString1.c_str(), "new_device_state", millis());
    }

    if (strstr(macStr, board_2_address_str)) {
        Slave_2_On_Correct_Channel = true;
        B2_failed_count = 0;

        memcpy(&Board2_Data, incomingData, sizeof(Board2_Data));
        snprintf(str2, 22, "2.%s|%.1f??C|%d%%",
                 Board2_Data.status ? "ON" : "OFF",
                 (float)Board2_Data.temp / 100, Board2_Data.hum);
        Serial.printf("Board ID %u: %u bytes\n", Board2_Data.id, len);
        Serial.printf("Temperature: %.1f\n", (float)Board2_Data.temp / 100);
        Serial.printf("Humidity: %d \n", Board2_Data.hum);
        Serial.printf("readingID value: %d \n", Board2_Data.readingId);

        if (Board2_Data.status != Board_2_Last_Received_State) {
            Serial.println(Board2_Data.status ? "Light ON\n" : "Light OFF\n");
            Board_2_Last_Received_State = Board2_Data.status;
        }

        board2["id"] = 2;
        board2["temperature"] = (float)Board2_Data.temp / 100;
        board2["humidity"] = Board2_Data.hum;
        board2["readingId"] = String(Board2_Data.readingId);
        board2["state"] = Board2_Data.status ? "ON" : "OFF";
        String jsonString2 = JSON.stringify(board2);
        events.send(jsonString2.c_str(), "b2new_readings", millis());
    }

    if (strstr(macStr, board_3_address_str)) {
        Slave_3_On_Correct_Channel = true;
        B3_failed_count = 0;

        memcpy(&Board3_Data, incomingData, sizeof(Board3_Data));
        snprintf(str3, 20, "3.%s|%.2f??C", Board3_Data.status ? "ON" : "OFF",
                 (float)Board3_Data.temp / 100);
        Serial.printf("Board ID %u: %u bytes\n", Board3_Data.id, len);
        Serial.printf("Temperature: %.2f\n", (float)Board3_Data.temp / 100);
        Serial.printf("Pressure: %.2f\n", (float)Board3_Data.pres / 100);
        Serial.printf("readingID value: %d \n", Board3_Data.readingID);

        if (Board3_Data.status != Board_3_Last_Received_State) {
            Serial.println(Board3_Data.status ? "Light ON\n" : "Light OFF\n");
            Board_3_Last_Received_State = Board3_Data.status;
        }

        board3["id"] = 3;
        board3["state"] = Board3_Data.status ? "ON" : "OFF";
        board3["temperature"] = (float)Board3_Data.temp / 100;
        board3["pressure"] = (float)Board3_Data.pres / 100;
        board3["readingId"] = String(Board3_Data.readingID);
        String jsonString3 = JSON.stringify(board3);
        events.send(jsonString3.c_str(), "b3new_reading", millis());
    }

    if (strstr(macStr, board_4_address_str)) {
        Slave_4_On_Correct_Channel = true;
        B4_failed_count = 0;

        memcpy(&Board4_Data, incomingData, sizeof(Board4_Data));
        snprintf(str4, 20, "4.%s|", Board4_Data.status ? "ON" : "OFF");
        Serial.printf("Board ID %u: %u bytes\n", Board4_Data.id, len);

        if (Board4_Data.status != Board_4_Last_Received_State) {
            Serial.println(Board4_Data.status ? "Light ON\n" : "Light OFF\n");
            Board_4_Last_Received_State = Board4_Data.status;
        }

        board4["id"] = 4;
        board4["state"] = Board4_Data.status ? "ON" : "OFF";
        String jsonString4 = JSON.stringify(board4);
        events.send(jsonString4.c_str(), "new_device_state", millis());
    }
}

void SendTo(byte board) {
    board = board - 1;
    switch (board) {
        case 0:
            outcome = esp_now_send(AddressArr[board], (uint8_t *)&Board1_Data,
                                   sizeof(Board1_Data));
            break;
        case 1:
            outcome = esp_now_send(AddressArr[board], (uint8_t *)&Board2_Data,
                                   sizeof(Board2_Data));
            break;
        case 2:
            outcome = esp_now_send(AddressArr[board], (uint8_t *)&Board3_Data,
                                   sizeof(Board3_Data));
            break;
        case 3:
            outcome = esp_now_send(AddressArr[board], (uint8_t *)&Board4_Data,
                                   sizeof(Board4_Data));
            break;
    }

    if (outcome == ESP_OK) {
        Serial.printf("Successfully sent data to board %d\n", board + 1);
    } else {
        Serial.printf("Error sending data to board %d\n", board + 1);
    }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len &&
        info->opcode == WS_TEXT) {
        data[len] = 0;
        if (strcmp((char *)data, "toggle1") == 0) {
            Board1_Data.id = 1;
            Board1_Data.status = digitalRead(Board_1_State) ? false : true;
            SendTo(1);
        } else if ((strcmp((char *)data, "toggle2") == 0)) {
            Board2_Data.id = 2;
            Board2_Data.status = digitalRead(Board_2_State) ? false : true;
            SendTo(2);
        } else if ((strcmp((char *)data, "toggle3") == 0)) {
            Board3_Data.id = 3;
            Board3_Data.status = digitalRead(Board_3_State) ? false : true;
            SendTo(3);
        } else if ((strcmp((char *)data, "toggle4") == 0)) {
            Board4_Data.id = 4;
            Board4_Data.status = digitalRead(Board_4_State) ? false : true;
            SendTo(4);
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n",
                          client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void initWebSocket() {
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

void initServer() {
    initWebSocket();

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
    Serial.println("HTTP server Init Success");
}

void Broadcast_Channel_To(byte Board_Index, byte channel) {
    switch (Board_Index) {
        case 1:
            Board1_Data.id = 0;
            Board1_Data.WiFi_Channel = channel;
            SendTo(1);
            delay(100);
            break;
        case 2:
            Board2_Data.id = 0;
            Board2_Data.WiFi_Channel = channel;
            SendTo(2);
            delay(100);
            break;
        case 3:
            Board3_Data.id = 0;
            Board3_Data.WiFi_Channel = channel;
            SendTo(3);
            delay(100);
            break;
        case 4:
            Board4_Data.id = 0;
            Board4_Data.WiFi_Channel = channel;
            SendTo(4);
            delay(100);
            break;
    }
}

void initWiFiManager() {
    wm.setConfigPortalTimeout(30);
    wm.setConnectTimeout(20);
    // connect after portal save toggle
    // wm.setSaveConnect(false);  // do not connect, only save
    portalRunning = true;
    // invert theme, dark
    wm.setDarkMode(true);

    if (!wm.autoConnect("Suzuha", "password")) {
        Serial.println("Failed to connect");
        portalRunning = false;
        snprintf(ip_str, 20, "IP:");
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
        WiFi.printDiag(Serial);
        // return;
    } else {
        // if you get here you have connected to the WiFi
        Serial.println("Connected");
        portalRunning = false;

        // Print local IP address
        Serial.println("");
        Serial.print("Station IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Wi-Fi Channel: ");
        Serial.println(WiFi.channel());
        snprintf(ip_str, 20, "IP: %s", WiFi.localIP().toString().c_str());
    }
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
    slave.channel = DEFAULT_CHANNEL;
    slave.encrypt = false;
    memset(&slave, 0, sizeof(slave));

    for (int i = 0; i < 4; i++) {
        memcpy(slave.peer_addr, AddressArr[i], 6);
        if (esp_now_add_peer(&slave) != ESP_OK) {
            Serial.printf("Failed to add Board %d\n", i + 1);
            return;
        }
        Serial.printf("Pair success for board #%d\n", i + 1);
    }
}

void initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS Init success");
}

void CheckButton() {
    // is configuration portal requested?
    if (!digitalRead(WIFI_TRIGGER)) {
        if (!portalRunning) {
            Serial.println("Starting Portal");
            initWiFiManager();
        } else {
            Serial.println("Stopping Portal");
            initWiFiManager();
        }
    }
}

void IRAM_ATTR Button1_interrupt_handler() {
    portENTER_CRITICAL_ISR(&mux1);
    numberOfButtonInterrupts_1++;
    lastState1 = digitalRead(Board_1_Switch);
    debounceTimeout1 = xTaskGetTickCount();
    portEXIT_CRITICAL_ISR(&mux1);
}

void IRAM_ATTR Button2_interrupt_handler() {
    portENTER_CRITICAL_ISR(&mux2);
    numberOfButtonInterrupts_2++;
    lastState2 = digitalRead(Board_2_Switch);
    debounceTimeout2 = xTaskGetTickCount();
    portEXIT_CRITICAL_ISR(&mux2);
}

void IRAM_ATTR Button3_interrupt_handler() {
    portENTER_CRITICAL_ISR(&mux3);
    numberOfButtonInterrupts_3++;
    lastState3 = digitalRead(Board_3_Switch);
    debounceTimeout3 = xTaskGetTickCount();
    portEXIT_CRITICAL_ISR(&mux3);
}

void IRAM_ATTR Button4_interrupt_handler() {
    portENTER_CRITICAL_ISR(&mux4);
    numberOfButtonInterrupts_4++;
    lastState4 = digitalRead(Board_4_Switch);
    debounceTimeout4 = xTaskGetTickCount();
    portEXIT_CRITICAL_ISR(&mux4);
}

void displayingTask(void *parameter) {
    pinMode(Board_1_State, OUTPUT);
    pinMode(Board_2_State, OUTPUT);
    pinMode(Board_3_State, OUTPUT);
    pinMode(Board_4_State, OUTPUT);

    display.begin();  // Initialize OLED display
    display.setPowerSave(0);
    display.setContrast(1);
    display.enableUTF8Print();  // enable UTF8 support for the Arduino print()
    display.setFont(u8g2_font_t0_11_tf);  // choose a suitable font

    while (1) {
        digitalWrite(Board_1_State, Board_1_Last_Received_State);
        digitalWrite(Board_2_State, Board_2_Last_Received_State);
        digitalWrite(Board_3_State, Board_3_Last_Received_State);
        digitalWrite(Board_4_State, Board_4_Last_Received_State);

        display.clearBuffer();
        display.setCursor(0, 10);
        display.print(ip_str);
        display.setCursor(0, 25);
        display.print(str1);
        display.setCursor(0, 35);
        display.print(str2);
        display.setCursor(0, 45);
        display.print(str3);
        display.setCursor(0, 55);
        display.print(str4);
        display.sendBuffer();

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void ChannelingMonitorTask(void *parameter) {
    while (1) {
        while (!Slave_1_On_Correct_Channel && B1_failed_count < FAILED_LIMIT ||
               !Slave_2_On_Correct_Channel && B2_failed_count < FAILED_LIMIT ||
               !Slave_3_On_Correct_Channel && B3_failed_count < FAILED_LIMIT ||
               !Slave_4_On_Correct_Channel && B4_failed_count < FAILED_LIMIT) {
            Serial.println("!!Fixing connection!!");
            // WiFi.printDiag(Serial);
            if (WiFi.status() == WL_CONNECTED) {
                if (Slave_1_On_Correct_Channel)
                    Broadcast_Channel_To(1, DEFAULT_CHANNEL);
                if (Slave_2_On_Correct_Channel)
                    Broadcast_Channel_To(2, DEFAULT_CHANNEL);
                if (Slave_3_On_Correct_Channel)
                    Broadcast_Channel_To(3, DEFAULT_CHANNEL);
                if (Slave_4_On_Correct_Channel)
                    Broadcast_Channel_To(4, DEFAULT_CHANNEL);

                WiFi.disconnect();
            }
            esp_wifi_set_promiscuous(true);
            esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
            esp_wifi_set_promiscuous(false);

            Serial.println("----Test ping----");
            for (int i = 1; i <= 4; i++) {
                SendTo(i);
                delay(500);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void buttonReadTask(void *parameter) {
    String taskMessage = "Debounced ButtonRead Task running on core ";
    taskMessage = taskMessage + xPortGetCoreID();
    Serial.println(taskMessage);

    // set up button Pin
    pinMode(Board_1_Switch, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Board_1_Switch),
                    Button1_interrupt_handler, FALLING);

    pinMode(Board_2_Switch, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Board_2_Switch),
                    Button2_interrupt_handler, FALLING);

    pinMode(Board_3_Switch, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Board_3_Switch),
                    Button3_interrupt_handler, FALLING);

    pinMode(Board_4_Switch, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Board_4_Switch),
                    Button4_interrupt_handler, FALLING);

    uint32_t saveDebounceTimeout1, saveDebounceTimeout2, saveDebounceTimeout3,
        saveDebounceTimeout4;
    bool saveLastState1, saveLastState2, saveLastState3, saveLastState4;
    int save1, save2, save3, save4;

    // Enter RTOS Task Loop
    while (1) {
        portENTER_CRITICAL_ISR(&mux1);
        save1 = numberOfButtonInterrupts_1;
        saveDebounceTimeout1 = debounceTimeout1;
        saveLastState1 = lastState1;
        portEXIT_CRITICAL_ISR(&mux1);

        portENTER_CRITICAL_ISR(&mux2);
        save2 = numberOfButtonInterrupts_2;
        saveDebounceTimeout2 = debounceTimeout2;
        saveLastState2 = lastState2;
        portEXIT_CRITICAL_ISR(&mux2);

        portENTER_CRITICAL_ISR(&mux3);
        save3 = numberOfButtonInterrupts_3;
        saveDebounceTimeout3 = debounceTimeout3;
        saveLastState3 = lastState3;
        portEXIT_CRITICAL_ISR(&mux3);

        portENTER_CRITICAL_ISR(&mux4);
        save4 = numberOfButtonInterrupts_4;
        saveDebounceTimeout4 = debounceTimeout4;
        saveLastState4 = lastState4;
        portEXIT_CRITICAL_ISR(&mux4);

        bool currentState1 = digitalRead(Board_1_Switch);
        bool currentState2 = digitalRead(Board_2_Switch);
        bool currentState3 = digitalRead(Board_3_Switch);
        bool currentState4 = digitalRead(Board_4_Switch);

        if ((save1 != 0)                          // interrupt has triggered
            && (currentState1 == saveLastState1)  // pin is still in the same
            // state as when intr triggered
            && (millis() - saveDebounceTimeout1 >
                DEBOUNCETIME)) {  // and it has been low for at least
                                  // DEBOUNCETIME, then valid keypress
            if (currentState1 == LOW) {
                Board1_Data.id = 1;
                Board1_Data.status = Board_1_Last_Received_State ? false : true;
                SendTo(1);
            }
            portENTER_CRITICAL_ISR(
                &mux1);  // can't change it unless, atomic - Critical section
            numberOfButtonInterrupts_1 =
                0;  // acknowledge keypress and reset interrupt counter
            portEXIT_CRITICAL_ISR(&mux1);
        }

        if ((save2 != 0) && (currentState2 == saveLastState2) &&
            (millis() - saveDebounceTimeout2 > DEBOUNCETIME)) {
            if (currentState2 == LOW) {
                Board2_Data.id = 2;
                Board2_Data.status = Board_2_Last_Received_State ? false : true;
                SendTo(2);
            }
            portENTER_CRITICAL_ISR(&mux2);
            numberOfButtonInterrupts_2 = 0;
            portEXIT_CRITICAL_ISR(&mux2);
        }

        if ((save3 != 0) && (currentState3 == saveLastState3) &&
            (millis() - saveDebounceTimeout3 > DEBOUNCETIME)) {
            if (currentState3 == LOW) {
                Board3_Data.id = 3;
                Board3_Data.status = Board_3_Last_Received_State ? false : true;
                SendTo(3);
            }
            portENTER_CRITICAL_ISR(&mux3);
            numberOfButtonInterrupts_3 = 0;
            portEXIT_CRITICAL_ISR(&mux3);
        }

        if ((save4 != 0) && (currentState4 == saveLastState4) &&
            (millis() - saveDebounceTimeout4 > DEBOUNCETIME)) {
            if (currentState4 == LOW) {
                Board4_Data.id = 4;
                Board4_Data.status = Board_4_Last_Received_State ? false : true;
                SendTo(4);
            }
            portENTER_CRITICAL_ISR(&mux4);
            numberOfButtonInterrupts_4 = 0;
            portEXIT_CRITICAL_ISR(&mux4);
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void maintainWiFiTask(void *parameter) {
    while (1) {
        while (WiFi.status() != WL_CONNECTED &&
               Slave_1_On_Correct_Channel == true &&
               Slave_2_On_Correct_Channel == true &&
               Slave_3_On_Correct_Channel == true &&
               Slave_4_On_Correct_Channel == true) {
            Serial.println("No wifi connection, connecting");
            byte tmp_chan = getWiFiChannel((wm.getWiFiSSID()).c_str());
            // clean up ram after doing wifi channel scan
            WiFi.scanDelete();

            if (B1_failed_count < FAILED_LIMIT)
                Broadcast_Channel_To(1, tmp_chan);
            if (B2_failed_count < FAILED_LIMIT)
                Broadcast_Channel_To(2, tmp_chan);
            if (B3_failed_count < FAILED_LIMIT)
                Broadcast_Channel_To(3, tmp_chan);
            if (B4_failed_count < FAILED_LIMIT)
                Broadcast_Channel_To(4, tmp_chan);

            // Connect to saved AP
            initWiFiManager();
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void pingWebServerTask(void *parameter) {
    initServer();

    while (1) {
        ws.cleanupClients();

        static unsigned long lastEventTime = millis();
        static const unsigned long EVENT_INTERVAL_MS = 5000;
        if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
            // Serial.println("Ping web");
            events.send("ping", NULL, millis());
            lastEventTime = millis();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void pingPeersTask(void *parameter) {
    while (1) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            // Save the last scheduled ping
            previousMillis = currentMillis;

            Serial.println("Scheduled ping");
            if (B1_failed_count < FAILED_LIMIT) SendTo(1);
            if (B2_failed_count < FAILED_LIMIT) SendTo(2);
            if (B3_failed_count < FAILED_LIMIT) SendTo(3);
            if (B4_failed_count < FAILED_LIMIT) SendTo(4);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    // Initialize Serial Monitor
    Serial.begin(115200);
    // Set the device as a Station and Soft Access Point simultaneously
    WiFi.mode(WIFI_AP_STA);

    // reset settings - wipe credentials for testing
    // wm.resetSettings();
    pinMode(WIFI_TRIGGER, INPUT_PULLUP);

    initSPIFFS();
    initEspNow();
    registerPeers();

    if (wm.getWiFiIsSaved()) {
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);

        byte tmp_chan = getWiFiChannel((wm.getWiFiSSID()).c_str());
        // clean up ram after doing wifi channel scan
        WiFi.scanDelete();
        for (int i = 1; i <= 4; i++) {
            Broadcast_Channel_To(i, tmp_chan);
        }
    }

    initWiFiManager();

    xTaskCreatePinnedToCore(ChannelingMonitorTask, "FixChannel", 2048, NULL, 3,
                            NULL, 0);
    xTaskCreatePinnedToCore(buttonReadTask, "Buttons", 2048, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(pingPeersTask, "Ping Nodes", 2048, NULL, 1, NULL,
                            0);

    xTaskCreatePinnedToCore(displayingTask, "OLED&LEDs", 2048, NULL, 3, NULL,
                            1);
    xTaskCreatePinnedToCore(maintainWiFiTask, "MaintainWiFi", 4096, NULL, 2,
                            NULL, 1);
    xTaskCreatePinnedToCore(pingWebServerTask, "Ping Web", 2048, NULL, 1, NULL,
                            1);
}

void loop() {}