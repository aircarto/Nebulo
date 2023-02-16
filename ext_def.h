// Language config
#define CURRENT_LANG INTL_LANG

// Wifi config
const char WLANSSID[] PROGMEM = "luftdaten";
const char WLANPWD[] PROGMEM = "luftd4ten";


#define TIME_FOR_WIFI_CONFIG 120000;
#define SENDING_INTERVALL_MS 120000;

// BasicAuth config
const char WWW_USERNAME[] PROGMEM = "admin";
const char WWW_PASSWORD[] PROGMEM = "";
#define WWW_BASICAUTH_ENABLED 0

// Sensor Wifi config (config mode)
#define FS_SSID ""
#define FS_PWD "nebuloledcfg"

#define HAS_WIFI 1
#define HAS_LORA 1


// const bool HAS_WIFI = true;
// const bool HAS_LORA = true;


const char APPEUI[] = "000";
const char DEVEUI [] = "000";
const char APPKEY[] = "000";

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

static const char NTP_SERVER_1[] PROGMEM = "ntp-p1.obspm.fr";
static const char NTP_SERVER_2[] PROGMEM = "ntp.obspm.fr";

// define own API
static const char HOST_CUSTOM[] PROGMEM = "data.nebulo.fr";
static const char URL_CUSTOM[] PROGMEM = "/wifi.php";
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

#if defined(ARDUINO_ESP32_DEV) and defined(KIT_V1)

#define PM_SERIAL_RX D39
#define PM_SERIAL_TX D4
#define CO2_SERIAL_RX D36
#define CO2_SERIAL_TX D12
#define I2C_PIN_SCL D22
#define I2C_PIN_SDA D21
#define LED_PIN D2

#endif

#if defined(ARDUINO_ESP32_DEV) and defined(KIT_C)

#define PM_SERIAL_RX D39
#define PM_SERIAL_TX D32 
#define CO2_SERIAL_RX D36
#define CO2_SERIAL_TX D27
#define I2C_PIN_SCL D22
#define I2C_PIN_SDA D21
#define LED_PIN D2

#endif

#if defined(ARDUINO_HELTEC_WIFI_LORA_32_V2)

#define I2C_SCREEN_SCL D15
#define I2C_SCREEN_SDA D4
#define OLED_RESET D16
#define PM_SERIAL_RX D23
#define PM_SERIAL_TX D17
#define I2C_PIN_SCL D22
#define I2C_PIN_SDA D21
#define LED_PIN D2

#endif

// SDS011, the more expensive version of the particle sensor
#define SDS_READ 0
#define SDS_API_PIN 1

// Tera Sensor Next PM sensor
#define NPM_READ 0
// #define NPM_FULLTIME 1
#define NPM_API_PIN 1

// BMP280/BME280, temperature, pressure (humidity on BME280)
#define BMX280_READ 0
#define BMP280_API_PIN 3
#define BME280_API_PIN 11

// CCS811, COV Sensor

#define CCS811_READ 0
// #define CCS811_API_PIN X

//Location

const char LATITUDE[] PROGMEM = "43.296";
const char LONGITUDE[] PROGMEM = "5.369";

// OLED Display SSD1306
#define HAS_SSD1306 1
#define DISPLAY_MEASURE 1
#define HAS_LED_VALUE 0
#define HAS_LED_CONNECT 0
#define LEDS_NB 16
#define BRIGHTNESS 50
#define GAMMA true
#define VALUE_DISPLAYED 2

// Show wifi info on displays
#define DISPLAY_WIFI_INFO 1

// Show wifi info on displays
#define DISPLAY_LORA_INFO 0

// Show device info on displays
#define DISPLAY_DEVICE_INFO 1

// Set debug level for serial output?
#define DEBUG 5

static const char URL_API_SENSORCOMMUNITY[] PROGMEM = "https://data.sensor.community/airrohr/v1/sensor/";
