// Language config
#define CURRENT_LANG INTL_LANG

// Wifi config
const char WLANSSID[] PROGMEM = "luftdaten";
const char WLANPWD[] PROGMEM = "";

// BasicAuth config
const char WWW_USERNAME[] PROGMEM = "admin";
const char WWW_PASSWORD[] PROGMEM = "";
#define WWW_BASICAUTH_ENABLED 0

// Sensor Wifi config (config mode)
#define FS_SSID "admin"
#define FS_PWD "nebulocfg"

#define HAS_WIFI 0
#define HAS_LORA 1
#define APPEUI "0000000000000000"
#define DEVEUI "0000000000000000"
#define APPKEY "00000000000000000000000000000000"
#define SEND2LORA 1

// Where to send the data?
#define SEND2SENSORCOMMUNITY 0
#define SSL_SENSORCOMMUNITY 0
#define SEND2MADAVI 0
#define SSL_MADAVI 0
#define SEND2CSV 0
#define SEND2CUSTOM 0


enum LoggerEntry {
    LoggerSensorCommunity,
    LoggerMadavi,
    LoggerCustom,
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


//  === pin assignments for dev kit board ===================================
#ifdef ESP32
#define ONEWIRE_PIN D32
#define PM_SERIAL_RX D27
#define PM_SERIAL_TX D33
// define serial interface pins for Next PM Sensor
#define NPM_SERIAL_RX D1
#define NPM_SERIAL_TX D2
#define PIN_CS D13

#if defined(FLIP_I2C_PMSERIAL) // exchange the pins of the ports to use external i2c connector for gps
#define I2C_PIN_SCL D23
#define I2C_PIN_SDA D19
#define GPS_SERIAL_RX D22
#define GPS_SERIAL_TX D21
#else
#define I2C_PIN_SCL D22
#define I2C_PIN_SDA D21
#define GPS_SERIAL_RX D19
#define GPS_SERIAL_TX D23
#endif
#define PPD_PIN_PM1 GPS_SERIAL_TX
#define PPD_PIN_PM2 GPS_SERIAL_RX
//#define RFM69_CS D0
//#define RFM69_RST D2
//#define RFM69_INT D4
#endif

// SDS011, the more expensive version of the particle sensor
#define SDS_READ 1
#define SDS_API_PIN 1

// Tera Sensor Next PM sensor
#define NPM_READ 0
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

