// Language config
#define CURRENT_LANG INTL_LANG

// Wifi config
const char WLANSSID[] PROGMEM = "luftdaten";
const char WLANPWD[] PROGMEM = "luftd4ten";

// BasicAuth config
const char WWW_USERNAME[] PROGMEM = "admin";
const char WWW_PASSWORD[] PROGMEM = "";
#define WWW_BASICAUTH_ENABLED 0

// Sensor Wifi config (config mode)
#define FS_SSID ""
#define FS_PWD "nebulocfg"

#define HAS_WIFI 1
#define HAS_LORA 0
const char APPEUI[] = "0000000000000000";
const char DEVEUI [] = "0000000000000000";
const char APPKEY[] = "00000000000000000000000000000000";

// Where to send the data?
#define SEND2SENSORCOMMUNITY 0
#define SSL_SENSORCOMMUNITY 0
#define SEND2MADAVI 0
#define SSL_MADAVI 0
#define SEND2CSV 0
#define SEND2CUSTOM 0
#define SEND2CUSTOM2 0

enum LoggerEntry {
    LoggerSensorCommunity,
    LoggerMadavi,
    LoggerCustom,
    LoggerCustom2,
    LoggerCount
};

struct LoggerConfig {
    uint16_t destport;
    uint16_t errors;
    void* session;
};

// IMPORTANT: NO MORE CHANGES TO VARIABLE NAMES NEEDED FOR EXTERNAL APIS
static const char HOST_MADAVI[] PROGMEM = "api-rrd.madavi.de";
static const char URL_MADAVI[] PROGMEM = "/data.php";
#define PORT_MADAVI 80

static const char HOST_SENSORCOMMUNITY[] PROGMEM = "api.sensor.community";
static const char URL_SENSORCOMMUNITY[] PROGMEM = "/v1/push-sensor-data/";
#define PORT_SENSORCOMMUNITY 80

static const char NTP_SERVER_1[] PROGMEM = "0.pool.ntp.org";
static const char NTP_SERVER_2[] PROGMEM = "1.pool.ntp.org";

// define own API
static const char HOST_CUSTOM[] PROGMEM = "192.168.234.1";
static const char URL_CUSTOM[] PROGMEM = "/data.php";
#define PORT_CUSTOM 80
#define USER_CUSTOM ""
#define PWD_CUSTOM ""
#define SSL_CUSTOM 0

// define own API
static const char HOST_CUSTOM2[] PROGMEM = "192.168.234.1";
static const char URL_CUSTOM2[] PROGMEM = "/data.php";
#define PORT_CUSTOM2 80
#define USER_CUSTOM2 ""
#define PWD_CUSTOM2 ""
#define SSL_CUSTOM2 0

//GPIO Pins
// the IO pins which can be used for what depends on the following:
//   - The board which is used
//     - onboard peripherials like LCD or LoRa chips which already occupy an IO pin
//     - the ESP32 module which is used
//         - the WROVER board uses the IOs 16 and 17 to access the PSRAW
//         - on WROOM boards the IOs 16 and 17 can be freely used
//   - if JTAG debugging shall be used
//   - some IOs have constraints
//     - configuration of ESP32 module configuration options ("strapping") like operating voltage and boot medium
//     - some IOs can only be used for inputs (34, 35, 36, 39)
// see https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
//     https://github.com/va3wam/TWIPi/blob/master/Eagle/doc/feather-pinout-map.pdf

//  === pin assignments for the LoRa module according to LMIC library ===================================

// ttgo:
// PIN_SX1276_NSS = 18
// PIN_SX1276_NRESET = 23 
// PIN_SX1276_DIO0 = 26
// PIN_SX1276_DIO1 = 33 
// PIN_SX1276_DIO2 = 32 
// PIN_SX1276_ANT_SWITCH_RX = UNUSED_PIN,
// PIN_SX1276_ANT_SWITCH_TX_BOOST = UNUSED_PIN,
// PIN_SX1276_ANT_SWITCH_TX_RFO = UNUSED_PIN,
// PIN_VDD_BOOST_ENABLE = UNUSED_PIN,


// Heltec:
// PIN_SX1276_NSS = 18,
// PIN_SX1276_NRESET = 14
// PIN_SX1276_DIO0 = 26
// PIN_SX1276_DIO1 = 35
// PIN_SX1276_DIO2 = 34
// PIN_SX1276_ANT_SWITCH_RX = UNUSED_PIN,
// PIN_SX1276_ANT_SWITCH_TX_BOOST = UNUSED_PIN,
// PIN_SX1276_ANT_SWITCH_TX_RFO = UNUSED_PIN,
// PIN_VDD_BOOST_ENABLE = UNUSED_PIN,

#if defined (ARDUINO_TTGO_LoRa32_v21new)
//#define ONEWIRE_PIN D25
#define PM_SERIAL_RX D19
#define PM_SERIAL_TX D23



#define I2C_PIN_SCL D22
#define I2C_PIN_SDA D21
#define GPS_SERIAL_RX D12
#define GPS_SERIAL_TX D13
#endif

#if defined(ARDUINO_HELTEC_WIFI_LORA_32_V2)

#define I2C_SCREEN_SCL D15
#define I2C_SCREEN_SDA D4
#define OLED_RESET D16
#define PM_SERIAL_RX D23
#define PM_SERIAL_TX D17

// #define ONEWIRE_PIN D32
// #define PM_SERIAL_RX D27
// #define PM_SERIAL_TX D33

#define I2C_PIN_SCL D22
#define I2C_PIN_SDA D21
#define GPS_SERIAL_RX D12
#define GPS_SERIAL_TX D13
#endif

// SDS011, the more expensive version of the particle sensor
#define SDS_READ 0
#define SDS_API_PIN 1

// Tera Sensor Next PM sensor
#define NPM_READ 1
#define NPM_API_PIN 1

// BMP280/BME280, temperature, pressure (humidity on BME280)
#define BMX280_READ 1
#define BMP280_API_PIN 3
#define BME280_API_PIN 11

// GPS, preferred Neo-6M
#define GPS_READ 0
#define GPS_API_PIN 9

// OLED Display SSD1306 connected?
#define HAS_DISPLAY 1
#define HAS_SSD1306 1

// OLED Display um 180° gedreht?
#define HAS_FLIPPED_DISPLAY 0

// Show wifi info on displays
#define DISPLAY_WIFI_INFO 1

// Show device info on displays
#define DISPLAY_DEVICE_INFO 1

// Set debug level for serial output?
#define DEBUG 3

