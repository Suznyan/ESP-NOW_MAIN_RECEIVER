# ESP-NOW MAIN RECEIVER 
Center board that ultilize WiFiManager, Asynchronous HTTP Webserver using .html, .css, .js files saved in SPIFFS. Communicate with 4 different ESP32 boards using ESP-NOW and a SH1106 128x64 OLED with u8g2 library.

To solve the channel problem when using ESP-NOW, before connect to the Wifi Access Point, this main board will attempt to send the saved AP name that the board connected to with WiFiManager to every peers through ESP-NOW and they will change their channel to the correctsponded channel the main board would use.
