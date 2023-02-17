# Nebulo V2

## Supported sensors
* Nova PM SDS011 (PM2.5 and PM10)
* Groupe Tera NextPM (PM1, PM2.5 and PM10)
* CCS811 (COV)
* BME280 (Temperature and Humidity)

## Displays
* OLED SSD1306
* WS2812B RGB LED

## Features
* Gets measurements from a full range of sensors
* Transmits data with WiFi or LoRaWAN to different databases
* Fully configurable through a web interface

## Libraries
* bblanchon/ArduinoJson@6.18.3
* 2dom/PxMatrix LED MATRIX library@^1.8.2
* fastled/FastLED@^3.4.0
* mcci-catena/MCCI LoRaWAN LMIC library@^4.1.1
* ThingPulse/ESP8266 and ESP32 OLED driver for SSD1306 displays @ ^4.2.1

And the ESP32 platform librairies:
* Wire
* WiFi
* DNSServer
* WiFiClientSecure
* HTTPClient
* FS
* SPIFFS
* WebServer
* Update
* ESPmDNS

## Boards
The code is developped on a ESP32 DevC with 38 pins (equiped with a ESP-WROOM-32 module). More information about this board on the official [Espressif website](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-devkitc.html).

## Flashing

Please use Platformio to flash the board.
The .ini file should be able to get all the needed boards, platforms and libraries from the internet

## Pin mapping

# NebuloV2

![nebulo_logo](https://aircarto.fr/images/Logo_Nebulo2.png)

New version of the air quality sensor Nebulo developped with [AtmoSud](https://www.atmosud.org/).

## Pinout Reference

|GPIO| devices | notes |
|----|-----|-----|
|GPIO0|📶 lora RESET| must be LOW to enter boot mode|
|GPI01| TX | USB Serial |
|GPIO2| unused | Inboard LED |
|GPIO3| RX | USB Serial |
|GPIO4| unused | notes |
|GPIO5| 📶 lora NSS | notes |
|GPIO6| &#x1F6D1;	 | integrated SPI flash |
|GPIO7| &#x1F6D1; | integrated SPI flash |
|GPIO8| &#x1F6D1; | integrated SPI flash |
|GPIO9| &#x1F6D1; | integrated SPI flash |
|GPIO10| &#x1F6D1; | integrated SPI flash |
|GPIO11| &#x1F6D1; | integrated SPI flash |
|GPIO12| unused | notes |
|GPIO13| unused | notes |
|GPIO14| unused | notes |
|GPIO15| unused | notes |
|GPIO16| unused | notes |
|GPIO17| unused | notes |
|GPIO18| 📶 lora SCK | notes |
|GPIO19| 📶 lora MISO | notes |
|GPIO21| SDA sensors | notes |
|GPIO22| SCL sensors | notes |
|GPIO23| 📶 lora MOSI | notes |
|GPIO25| unused | notes |
|GPIO26| 📶 lora DIO0 | notes |
|GPIO27|  | |
|GPIO32| NextPM RX | notes |
|GPIO33| 💡LEDs | notes |
|GPIO34| 📶 lora DIO2 | notes |
|GPIO35| 📶 lora DIO1 | notes |
|GPIO36|  | |
|GPIO39| NextPM TX | notes |

## PCB

You can find the PCB layout [here](https://oshwlab.com/pvuarambon/nebulov2_esp32).

## Configuration

The process is the same as for the Sensor.Community firmware.
On the first start, the sation won't find any known network and it will go in AP mode producing a moduleair-XXXXXXX network. Connect to it from a PC or a smartphone and open the address http://192.168.4.1.
If needed, the password is moduleaircfg.

For the WiFi connection: type your credentials
For the LoRaWAN connection : type the APPEUI, DEVEUI and APPKEY as created in the Helium or TTN console.

Choose the sensors, the displays and the API in the different tabs. For coding reason, it was not possible to use radios for the PM sensors and the CO2 sensors. Please don't check both sensors of the same type to avoid problems…

Please don't decrease the measuring interval to spare connection time.

If the checkbox "WiFi transmission" is not checked, the station will stay in AP mode for 10 minutes and then the LoRaWAN transmission will start. During those 10 minutes or after a restart, you can change the configuration.

If the checkbox "WiFi transmission" is checked, the sensor will be always accessible through your router with an IP addess : 192.168.<0 or more>.<100, 101, 102…>. In that case the data streams will use WiFi and not LoRaWAN (even if it is checked).

## LoRaWAN payload
The payload consists in a 24 bytes (declared as a 25 according to the LMIC library) array.

The value are initialised for the first uplink at the end of the void setup() which is send according to the LMIC library examples.

```
0x00, config = 0 (see below)
0xff, 0xff, sds PM10 = -1
0xff, 0xff, sds PM2.5 = -1
0xff, 0xff, npm PM10 = -1
0xff, 0xff, npm PM2.5 = -1
0xff, 0xff, npm PM1 = -1
0xff, 0xff, npm_nc -1
0xff, 0xff, npm_nc -1
0xff, 0xff, npm_nc -1
0xff, 0xff, ccs811 -1
0xff, 0x80, bme temp = -128
0xff,       bme rh = -1
0xff, 0xff, bme press = -1
```

Those default values will be replaced during the normal use of the station according to the selected sensors.

The first byte is the configuation summary, representeed as an array of 0/1 for false/true:

```
configlorawan[0] = cfg::sds_read;
configlorawan[1] = cfg::npm_read;
configlorawan[2] = cfg::bmx280_read;
configlorawan[3] = cfg::ccs811_read;
configlorawan[4] = cfg::has_led_value;
configlorawan[5] = cfg::has_led_connect;
configlorawan[6] = cfg::rgpd;
configlorawan[7] = cfg::has_wifi;
```

It produce single byte which will have to be decoded on server side.

For example:

10101110 (binary) = 0xAE (hexbyte) =174 (decimal)
The station as a SDS011, a BME280, activated LEDs for value, activated LEDs for connection state, RGPD (european privacy law) is checked, the WiFi is not activated.

The LoRaWAN server has to get the forecast data and transmit by downlink. Because the WiFi is not activated the uplink sensor data has to be sent to the databases.

If wifi is activated it is useless to decode the uplinks and transmit some downlinks because everything is already done though API calls and POST requests.

## WiFi payload

Example for transmited data:

`{"software_version" : "ModuleAirV2-V1-122021", "sensordatavalues" : [ {"value_type" : "NPM_P0", "value" : "1.84"}, {"value_type" : "NPM_P1", "value" : "2.80"}, {"value_type" : "NPM_P2", "value" : "2.06"}, {"value_type" : "NPM_N1", "value" : "27.25"}, {"value_type" : "NPM_N10", "value" : "27.75"}, {"value_type" : "NPM_N25", "value" : "27.50"}, {"value_type" : "BME280_temperature", "value" : "20.84"}, {"value_type" : "BME280_pressure", "value" : "99220.03"}, {"value_type" : "BME280_humidity", "value" : "61.66"}, {"value_type" : "samples", "value" : "138555"}, {"value_type" : "min_micro", "value" : "933"}, {"value_type" : "max_micro", "value" : "351024"}, {"value_type" : "interval", "value" : "145000"}, {"value_type" : "signal", "value" : "-71"} ]}`

## Payload formaters

**Uplink**

```
function Decoder(bytes, port) { 

var buf = new ArrayBuffer(bytes.length);
var view1 = new DataView(buf);
var view2 = new DataView(buf);

bytes.forEach(function (b, i) {
    view1.setUint8(i, b);
});

bytes.forEach(function (b, i) {
    view2.setInt8(i, b);
});


if (view1.getUint8(0) < 0 || view1.getUint8(0) > 255 || view1.getUint8(0) % 1 !== 0) {
      throw new Error(byte+ " does not fit in a byte");
  }
  
return {"configuration":("000000000" + view1.getUint8(0).toString(2)).substr(-8),"PM1_SDS":view2.getInt16(1).toString(),"PM2_SDS":view2.getInt16(3).toString(),"PM0_NPM":view1.getInt16(5).toString(),"PM1_NPM":view1.getInt16(7).toString(),"PM2_NPM":view1.getInt16(9).toString(),"N1_NPM":view1.getInt16(11).toString(),"N10_NPM":view1.getInt16(13).toString(),"N25_NPM":view1.getInt16(15).toString(),"VOC_CCS811":view2.getInt16(17).toString(),"T_BME":view2.getInt8(19).toString(),"H_BME":view2.getInt8(21).toString(),"P_BME":view2.getInt16(22).toString();  
}
```