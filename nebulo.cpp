
#include <WString.h>
#include <pgmspace.h>

#define SOFTWARE_VERSION_STR "Nebulo-V1-122021"
String SOFTWARE_VERSION(SOFTWARE_VERSION_STR);

#include <Arduino.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <arduino_lmic_hal_boards.h>

// includes ESP32 libraries
#define FORMAT_SPIFFS_IF_FAILED true
#include <FS.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HardwareSerial.h>
#include <hwcrypto/sha.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// includes external libraries

#include "./oledfont.h" // avoids including the default Arial font, needs to be included before SSD1306.h
#include <SSD1306Wire.h>
//#include <SSD1306.h>

#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_DECODE_UNICODE 0
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <StreamString.h>
#include <TinyGPS++.h>
#include "./bmx280_i2c.h"

// includes files
#include "./intl.h"
#include "./utils.h"
#include "defines.h"
#include "ext_def.h"
#include "html-content.h"

// configuration
namespace cfg
{
	unsigned debug = DEBUG;

	unsigned time_for_wifi_config = 600000;
	unsigned sending_intervall_ms = 145000;

	char current_lang[3];

	// credentials for basic auth of internal web server
	bool www_basicauth_enabled = WWW_BASICAUTH_ENABLED;
	char www_username[LEN_WWW_USERNAME];
	char www_password[LEN_CFG_PASSWORD];

	// wifi credentials
	char wlanssid[LEN_WLANSSID];
	char wlanpwd[LEN_CFG_PASSWORD];

	// credentials of the sensor in access point mode
	char fs_ssid[LEN_FS_SSID] = FS_SSID;
	char fs_pwd[LEN_CFG_PASSWORD] = FS_PWD;

	//main config

	bool has_wifi = HAS_WIFI;
	bool has_lora = HAS_LORA;
	char appeui[LEN_APPEUI];
	char deveui[LEN_DEVEUI];
	char appkey[LEN_APPKEY];

	// (in)active sensors
	bool sds_read = SDS_READ;
	bool npm_read = NPM_READ;
	bool bmx280_read = BMX280_READ;
	char height_above_sealevel[8] = "0";
	bool gps_read = GPS_READ;

	// send to "APIs"
	bool send2dusti = SEND2SENSORCOMMUNITY;
	bool send2madavi = SEND2MADAVI;
	bool send2custom = SEND2CUSTOM;
	bool send2custom2 = SEND2CUSTOM2;
	bool send2csv = SEND2CSV;

	// (in)active displays
	bool has_ssd1306 = HAS_SSD1306;

	bool display_wifi_info = DISPLAY_WIFI_INFO;
	bool display_lora_info = DISPLAY_LORA_INFO;
	bool display_device_info = DISPLAY_DEVICE_INFO;

	// API settings
	bool ssl_madavi = SSL_MADAVI;
	bool ssl_dusti = SSL_SENSORCOMMUNITY;

	//API AirCarto
	char host_custom[LEN_HOST_CUSTOM];
	char url_custom[LEN_URL_CUSTOM];
	bool ssl_custom = SSL_CUSTOM;
	unsigned port_custom = PORT_CUSTOM;
	char user_custom[LEN_USER_CUSTOM] = USER_CUSTOM;
	char pwd_custom[LEN_CFG_PASSWORD] = PWD_CUSTOM;

	//API AtmoSud
	char host_custom2[LEN_HOST_CUSTOM2];
	char url_custom2[LEN_URL_CUSTOM2];
	bool ssl_custom2 = SSL_CUSTOM2;
	unsigned port_custom2 = PORT_CUSTOM2;
	char user_custom2[LEN_USER_CUSTOM2] = USER_CUSTOM2;
	char pwd_custom2[LEN_CFG_PASSWORD] = PWD_CUSTOM2;

	//First load
	void initNonTrivials(const char *id)
	{
		strcpy(cfg::current_lang, CURRENT_LANG);
		strcpy_P(appeui, APPEUI);
		strcpy_P(deveui, DEVEUI);
		strcpy_P(appkey, APPKEY);
		strcpy_P(www_username, WWW_USERNAME);
		strcpy_P(www_password, WWW_PASSWORD);
		strcpy_P(wlanssid, WLANSSID);
		strcpy_P(wlanpwd, WLANPWD);
		strcpy_P(host_custom, HOST_CUSTOM);
		strcpy_P(url_custom, URL_CUSTOM);
		strcpy_P(host_custom2, HOST_CUSTOM2);
		strcpy_P(url_custom2, URL_CUSTOM2);

		//AJOUTER LORA ICI?

		if (!*fs_ssid)
		{
			strcpy(fs_ssid, SSID_BASENAME);
			strcat(fs_ssid, id);
		}
	}
}

// define size of the config JSON
#define JSON_BUFFER_SIZE 2300 //REVOIR LA TAILLE

LoggerConfig loggerConfigs[LoggerCount];

// test variables
long int sample_count = 0;
bool bmx280_init_failed = false;
bool gps_init_failed = false;
bool nebulo_selftest_failed = false;

WebServer server(80);

// include JSON config reader
#include "./nebulo-cfg.h"

/*****************************************************************
 * Display definitions                                           *
 *****************************************************************/

SSD1306Wire *oled_ssd1306 = nullptr; //as pointer

/*****************************************************************
 * Serial declarations                                           *
 *****************************************************************/

#define serialSDS (Serial1)
#define serialGPS (&(Serial2)) //as pointer
#define serialNPM (Serial1)

/*****************************************************************
 * BMP/BME280 declaration                                        *
 *****************************************************************/
BMX280 bmx280;

/*****************************************************************
 * GPS declaration                                               *
 *****************************************************************/
TinyGPSPlus gps;

// time management varialbles
bool send_now = false;
unsigned long starttime;
unsigned long time_point_device_start_ms;
unsigned long starttime_SDS;
unsigned long starttime_GPS;
unsigned long starttime_NPM;
unsigned long last_NPM;
unsigned long act_micro;
unsigned long act_milli;
unsigned long last_micro = 0;
unsigned long min_micro = 1000000000;
unsigned long max_micro = 0;

//bool is_SDS_running = true;
bool is_SDS_running;

// To read SDS responses

enum
{
	SDS_REPLY_HDR = 10,
	SDS_REPLY_BODY = 8
} SDS_waiting_for;

bool is_NPM_running;

// To read NPM responses
enum
{
	NPM_REPLY_HEADER_16 = 16,
	NPM_REPLY_STATE_16 = 14,
	NPM_REPLY_BODY_16 = 13,
	NPM_REPLY_CHECKSUM_16 = 1
} NPM_waiting_for_16;  //for concentration

enum
{
	NPM_REPLY_HEADER_4 = 4,
	NPM_REPLY_STATE_4 = 2,
	NPM_REPLY_CHECKSUM_4 = 1
} NPM_waiting_for_4; //for change

enum
{
	NPM_REPLY_HEADER_5 = 5,
	NPM_REPLY_STATE_5 = 3,
	NPM_REPLY_DATA_5 = 2,
	NPM_REPLY_CHECKSUM_5 = 1
} NPM_waiting_for_5; //for fan speed

enum
{
	NPM_REPLY_HEADER_6 = 6,
	NPM_REPLY_STATE_6 = 4,
	NPM_REPLY_DATA_6 = 3,
	NPM_REPLY_CHECKSUM_6 = 1
} NPM_waiting_for_6; // for version

unsigned long sending_time = 0;
unsigned long last_update_attempt;
int last_update_returncode;
int last_sendData_returncode;

// data variables
float last_value_BMX280_T = -128.0;
float last_value_BMX280_P = -1.0;
float last_value_BME280_H = -1.0;

uint32_t sds_pm10_sum = 0;
uint32_t sds_pm25_sum = 0;
uint32_t sds_val_count = 0;
uint32_t sds_pm10_max = 0;
uint32_t sds_pm10_min = 20000;
uint32_t sds_pm25_max = 0;
uint32_t sds_pm25_min = 20000;

uint32_t npm_pm1_sum = 0;
uint32_t npm_pm10_sum = 0;
uint32_t npm_pm25_sum = 0;
uint32_t npm_pm1_sum_pcs = 0;
uint32_t npm_pm10_sum_pcs = 0;
uint32_t npm_pm25_sum_pcs = 0;
uint16_t npm_val_count = 0;
uint16_t npm_pm1_max = 0;
uint16_t npm_pm1_min = 20000;
uint16_t npm_pm10_max = 0;
uint16_t npm_pm10_min = 20000;
uint16_t npm_pm25_max = 0;
uint16_t npm_pm25_min = 20000;
uint16_t npm_pm1_max_pcs = 0;
uint16_t npm_pm1_min_pcs = 60000;
uint16_t npm_pm10_max_pcs = 0;
uint16_t npm_pm10_min_pcs = 60000;
uint16_t npm_pm25_max_pcs = 0;
uint16_t npm_pm25_min_pcs = 60000;
bool newCmdNPM = true;

float last_value_SDS_P1 = -1.0;
float last_value_SDS_P2 = -1.0;
float last_value_NPM_P0 = -1.0;
float last_value_NPM_P1 = -1.0;
float last_value_NPM_P2 = -1.0;
float last_value_NPM_N0 = -1.0;
float last_value_NPM_N1 = -1.0;
float last_value_NPM_N2 = -1.0;
float last_value_GPS_alt = -1000.0;
double last_value_GPS_lat = -200.0;
double last_value_GPS_lon = -200.0;
String last_value_GPS_timestamp;
String last_data_string;
int last_signal_strength;
int last_disconnect_reason;

//chipID variables
String esp_chipid;
//String esp_mac_id;
String last_value_SDS_version;
String last_value_NPM_version;

unsigned long SDS_error_count;
unsigned long NPM_error_count;
unsigned long WiFi_error_count;

unsigned long last_page_load = millis();

bool wificonfig_loop = false;
uint8_t sntp_time_set;

unsigned long count_sends = 0;
unsigned long last_display_millis = 0;
uint8_t next_display_count = 0;

// info
struct struct_wifiInfo
{
	char ssid[LEN_WLANSSID];
	uint8_t encryptionType;
	int32_t RSSI;
	int32_t channel;
};

struct struct_wifiInfo *wifiInfo;
uint8_t count_wifiInfo;

// IPAddress addr_static_ip;
// IPAddress addr_static_subnet;
// IPAddress addr_static_gateway;
// IPAddress addr_static_dns;

#define msSince(timestamp_before) (act_milli - (timestamp_before))

const char data_first_part[] PROGMEM = "{\"software_version\": \"" SOFTWARE_VERSION_STR "\", \"sensordatavalues\":[";
const char JSON_SENSOR_DATA_VALUES[] PROGMEM = "sensordatavalues";

static String displayGenerateFooter(unsigned int screen_count)
{
	String display_footer;
	for (unsigned int i = 0; i < screen_count; ++i)
	{
		display_footer += (i != (next_display_count % screen_count)) ? " . " : " o ";
	}
	return display_footer;
}

/*****************************************************************
 * display values                                                *
 *****************************************************************/
static void display_debug(const String &text1, const String &text2)
{
	debug_outln_info(F("output debug text to displays..."));

	if (oled_ssd1306)
	{
		oled_ssd1306->clear();
		oled_ssd1306->displayOn();
		oled_ssd1306->setTextAlignment(TEXT_ALIGN_LEFT);
		oled_ssd1306->drawString(0, 12, text1);
		oled_ssd1306->drawString(0, 24, text2);
		oled_ssd1306->display();
	}
}

/*****************************************************************
 * read SDS011 sensor serial and firmware date                   *
 *****************************************************************/
static String SDS_version_date()
{

	if (cfg::sds_read && !last_value_SDS_version.length())
	{
		debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(DBG_TXT_SDS011_VERSION_DATE));
		is_SDS_running = SDS_cmd(PmSensorCmd::Start);
		delay(250);
		serialSDS.flush();
		// Query Version/Date
		SDS_rawcmd(0x07, 0x00, 0x00);
		delay(400);
		const constexpr uint8_t header_cmd_response[2] = {0xAA, 0xC5};
		while (serialSDS.find(header_cmd_response, sizeof(header_cmd_response)))
		{
			uint8_t data[8];
			unsigned r = serialSDS.readBytes(data, sizeof(data));
			if (r == sizeof(data) && data[0] == 0x07 && SDS_checksum_valid(data))
			{
				char tmp[20];
				snprintf_P(tmp, sizeof(tmp), PSTR("%02d-%02d-%02d(%02x%02x)"),
						   data[1], data[2], data[3], data[4], data[5]);
				last_value_SDS_version = tmp;
				break;
			}
		}
		debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(DBG_TXT_SDS011_VERSION_DATE));
	}

	return last_value_SDS_version;
}


/*****************************************************************
 * NPM functions     *
 *****************************************************************/

static uint8_t NPM_get_state(){
		uint8_t result;
		NPM_waiting_for_4 = NPM_REPLY_HEADER_4;
		debug_outln_info(F("State NPM..."));
		NPM_cmd(PmSensorCmd2::State);

		while (!serialNPM.available())
		{
			debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
		}

        while (serialNPM.available() >= NPM_waiting_for_4)
        {
            const uint8_t constexpr header[2] = {0x81, 0x16};
            uint8_t state[1];
            uint8_t checksum[1];
            uint8_t test[4];

            switch (NPM_waiting_for_4)
            {
            case NPM_REPLY_HEADER_4:
                if (serialNPM.find(header, sizeof(header)))
                    NPM_waiting_for_4 = NPM_REPLY_STATE_4;
                break;
            case NPM_REPLY_STATE_4:
                serialNPM.readBytes(state, sizeof(state));
                NPM_state(state[0]);
				result = state[0];
                NPM_waiting_for_4 = NPM_REPLY_CHECKSUM_4;
                break;
            case NPM_REPLY_CHECKSUM_4:
                serialNPM.readBytes(checksum, sizeof(checksum));
                memcpy(test, header, sizeof(header));
                memcpy(&test[sizeof(header)], state, sizeof(state));
                memcpy(&test[sizeof(header) + sizeof(state)], checksum, sizeof(checksum));
                NPM_data_reader(test, 4);
				NPM_waiting_for_4 = NPM_REPLY_HEADER_4;
                if (NPM_checksum_valid_4(test)){
                    debug_outln_info(F("Checksum OK..."));	
				}
                break;
            }
        }
		return result;
}

static bool NPM_start_stop(){
		bool result;
		NPM_waiting_for_4 = NPM_REPLY_HEADER_4;
		debug_outln_info(F("Switch start/stop NPM..."));
		NPM_cmd(PmSensorCmd2::Change);

		while (!serialNPM.available())
		{
			debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
		}

        while (serialNPM.available() >= NPM_waiting_for_4)
        {
            const uint8_t constexpr header[2] = {0x81, 0x15};
            uint8_t state[1];
            uint8_t checksum[1];
            uint8_t test[4];

            switch (NPM_waiting_for_4)
            {
            case NPM_REPLY_HEADER_4:
                if (serialNPM.find(header, sizeof(header)))
                    NPM_waiting_for_4 = NPM_REPLY_STATE_4;
                break;
            case NPM_REPLY_STATE_4:
                serialNPM.readBytes(state, sizeof(state));
                NPM_state(state[0]);

				if (bitRead(state[0], 0) == 0){
				debug_outln_info(F("NPM start..."));
						result = true;
				}else{
				debug_outln_info(F("NPM stop..."));	
						result = false;
				}


                NPM_waiting_for_4 = NPM_REPLY_CHECKSUM_4;
                break;
            case NPM_REPLY_CHECKSUM_4:
                serialNPM.readBytes(checksum, sizeof(checksum));
                memcpy(test, header, sizeof(header));
                memcpy(&test[sizeof(header)], state, sizeof(state));
                memcpy(&test[sizeof(header) + sizeof(state)], checksum, sizeof(checksum));
                NPM_data_reader(test, 4);
				NPM_waiting_for_4 = NPM_REPLY_HEADER_4;
                if (NPM_checksum_valid_4(test)){
                    debug_outln_info(F("Checksum OK..."));
				}
                break;
            }
        }
return result; //ATTENTION
}

static void NPM_version_date()
{
		//debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(DBG_TXT_NPM_VERSION_DATE));
		delay(250);
		NPM_waiting_for_6 = NPM_REPLY_HEADER_6;
		debug_outln_info(F("Version NPM..."));
		NPM_cmd(PmSensorCmd2::Version);

		while (!serialNPM.available())
		{
			debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
		}

		while (serialNPM.available() >= NPM_waiting_for_6)
		{
         	const uint8_t constexpr header[2] = {0x81, 0x17};
            uint8_t state[1];
            uint8_t data[2];
            uint8_t checksum[1];
            uint8_t test[6];

switch (NPM_waiting_for_6)
            {
            case NPM_REPLY_HEADER_6:
                if (serialNPM.find(header, sizeof(header)))
                    NPM_waiting_for_6 = NPM_REPLY_STATE_6;
                break;
            case NPM_REPLY_STATE_6:
                serialNPM.readBytes(state, sizeof(state));
                NPM_state(state[0]);
                NPM_waiting_for_6 = NPM_REPLY_DATA_6;
                break;
          case NPM_REPLY_DATA_6:
 if (serialNPM.readBytes(data, sizeof(data)) == sizeof(data)){
				NPM_data_reader(data, 2);
				uint16_t NPMversion = word(data[0], data[1]);
				last_value_NPM_version = String(NPMversion);
				//debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(DBG_TXT_NPM_VERSION_DATE));
				debug_outln_info(F("Next PM Firmware: "), last_value_NPM_version);
 }
                NPM_waiting_for_6 = NPM_REPLY_CHECKSUM_6;
                break;
            case NPM_REPLY_CHECKSUM_6:
                serialNPM.readBytes(checksum, sizeof(checksum));
                memcpy(test, header, sizeof(header));
                memcpy(&test[sizeof(header)], state, sizeof(state));
                memcpy(&test[sizeof(header) + sizeof(state)], data, sizeof(data));
				memcpy(&test[sizeof(header) + sizeof(state) + sizeof(data)], checksum, sizeof(checksum));
                NPM_data_reader(test, 6);
				NPM_waiting_for_6 = NPM_REPLY_HEADER_6;
                if (NPM_checksum_valid_6(test)){
                    debug_outln_info(F("Checksum OK..."));
				}
                break;
            }
		}	
}



static void NPM_fan_speed()
{

		NPM_waiting_for_5 = NPM_REPLY_HEADER_5;
		debug_outln_info(F("Set fan speed to 50 %..."));
		NPM_cmd(PmSensorCmd2::Speed);

		while (!serialNPM.available())
		{
			debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
		}

		while (serialNPM.available() >= NPM_waiting_for_5)
		{
         const uint8_t constexpr header[2] = {0x81, 0x21};
            uint8_t state[1];
            uint8_t data[1];
            uint8_t checksum[1];
            uint8_t test[5];

switch (NPM_waiting_for_5)
            {
            case NPM_REPLY_HEADER_5:
                if (serialNPM.find(header, sizeof(header)))
                    NPM_waiting_for_5 = NPM_REPLY_STATE_5;
                break;
            case NPM_REPLY_STATE_5:
                serialNPM.readBytes(state, sizeof(state));
                NPM_state(state[0]);
                NPM_waiting_for_5 = NPM_REPLY_DATA_5;
                break;
          case NPM_REPLY_DATA_5:
 if (serialNPM.readBytes(data, sizeof(data)) == sizeof(data)){
				NPM_data_reader(data, 1);
 }
                NPM_waiting_for_5 = NPM_REPLY_CHECKSUM_5;
                break;
            case NPM_REPLY_CHECKSUM_5:
                serialNPM.readBytes(checksum, sizeof(checksum));
                memcpy(test, header, sizeof(header));
                memcpy(&test[sizeof(header)], state, sizeof(state));
                memcpy(&test[sizeof(header) + sizeof(state)], data, sizeof(data));
				memcpy(&test[sizeof(header) + sizeof(state) + sizeof(data)], checksum, sizeof(checksum));
                NPM_data_reader(test, 5);
				NPM_waiting_for_5 = NPM_REPLY_HEADER_5;
                if (NPM_checksum_valid_5(test)){
                    debug_outln_info(F("Checksum OK..."));
				}
                break;
            }
		}	
}



/*****************************************************************
 * disable unneeded NMEA sentences, TinyGPS++ needs GGA, RMC     *
 *****************************************************************/
static void disable_unneeded_nmea()
{
	serialGPS->println(F("$PUBX,40,GLL,0,0,0,0*5C")); // Geographic position, latitude / longitude
													  //	serialGPS->println(F("$PUBX,40,GGA,0,0,0,0*5A"));       // Global Positioning System Fix Data
	serialGPS->println(F("$PUBX,40,GSA,0,0,0,0*4E")); // GPS DOP and active satellites
													  //	serialGPS->println(F("$PUBX,40,RMC,0,0,0,0*47"));       // Recommended minimum specific GPS/Transit data
	serialGPS->println(F("$PUBX,40,GSV,0,0,0,0*59")); // GNSS satellites in view
	serialGPS->println(F("$PUBX,40,VTG,0,0,0,0*5E")); // Track made good and ground speed
}

/*****************************************************************
 * write config to spiffs                                        *
 *****************************************************************/
static bool writeConfig()
{
	DynamicJsonDocument json(JSON_BUFFER_SIZE);
	debug_outln_info(F("Saving config..."));
	json["SOFTWARE_VERSION"] = SOFTWARE_VERSION;

	for (unsigned e = 0; e < sizeof(configShape) / sizeof(configShape[0]); ++e)
	{
		ConfigShapeEntry c;
		memcpy_P(&c, &configShape[e], sizeof(ConfigShapeEntry));
		switch (c.cfg_type)
		{
		case Config_Type_Bool:
			json[c.cfg_key()].set(*c.cfg_val.as_bool);
			break;
		case Config_Type_UInt:
		case Config_Type_Time:
			json[c.cfg_key()].set(*c.cfg_val.as_uint);
			break;
		case Config_Type_Password:
		case Config_Type_Hex:
		case Config_Type_String:
			json[c.cfg_key()].set(c.cfg_val.as_str);
			break;
		};
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

	SPIFFS.remove(F("/config.json.old"));
	SPIFFS.rename(F("/config.json"), F("/config.json.old"));

	File configFile = SPIFFS.open(F("/config.json"), "w");
	if (configFile)
	{
		serializeJson(json, configFile);
		configFile.close();
		debug_outln_info(F("Config written successfully."));
	}
	else
	{
		debug_outln_error(F("failed to open config file for writing"));
		return false;
	}

#pragma GCC diagnostic pop

	return true;
}

/*****************************************************************
 * read config from spiffs                                       *
 *****************************************************************/

/* backward compatibility for the times when we stored booleans as strings */
static bool boolFromJSON(const DynamicJsonDocument &json, const __FlashStringHelper *key)
{
	if (json[key].is<const char *>())
	{
		return !strcmp_P(json[key].as<const char *>(), PSTR("true"));
	}
	return json[key].as<bool>();
}

static void readConfig(bool oldconfig = false)
{
	bool rewriteConfig = false;

	String cfgName(F("/config.json"));
	if (oldconfig)
	{
		cfgName += F(".old");
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	File configFile = SPIFFS.open(cfgName, "r");
	if (!configFile)
	{
		if (!oldconfig)
		{
			return readConfig(true /* oldconfig */);
		}

		debug_outln_error(F("failed to open config file."));
		return;
	}

	debug_outln_info(F("opened config file..."));
	DynamicJsonDocument json(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json, configFile.readString());
	configFile.close();
#pragma GCC diagnostic pop

	if (!err)
	{
		serializeJsonPretty(json, Debug);
		debug_outln_info(F("parsed json..."));
		for (unsigned e = 0; e < sizeof(configShape) / sizeof(configShape[0]); ++e)
		{
			ConfigShapeEntry c;
			memcpy_P(&c, &configShape[e], sizeof(ConfigShapeEntry));
			if (json[c.cfg_key()].isNull())
			{
				continue;
			}
			switch (c.cfg_type)
			{
			case Config_Type_Bool:
				*(c.cfg_val.as_bool) = boolFromJSON(json, c.cfg_key());
				break;
			case Config_Type_UInt:
			case Config_Type_Time:
				*(c.cfg_val.as_uint) = json[c.cfg_key()].as<unsigned int>();
				break;
			case Config_Type_String:
			case Config_Type_Hex:
			case Config_Type_Password:
				strncpy(c.cfg_val.as_str, json[c.cfg_key()].as<const char *>(), c.cfg_len);
				c.cfg_val.as_str[c.cfg_len] = '\0';
				break;
			};
		}
		String writtenVersion(json["SOFTWARE_VERSION"].as<const char *>());
		if (writtenVersion.length() && writtenVersion[0] == 'N' && SOFTWARE_VERSION != writtenVersion)
		{
			debug_outln_info(F("Rewriting old config from: "), writtenVersion);
			// would like to do that, but this would wipe firmware.old which the two stage loader
			// might still need
			// SPIFFS.format();
			rewriteConfig = true;
		}
		if (cfg::sending_intervall_ms < READINGTIME_SDS_MS)
		{
			cfg::sending_intervall_ms = READINGTIME_SDS_MS;
		}
		if (boolFromJSON(json, F("bmp280_read")) || boolFromJSON(json, F("bme280_read")))
		{
			cfg::bmx280_read = true;
			rewriteConfig = true;
		}
	}
	else
	{
		debug_outln_error(F("failed to load json config"));

		if (!oldconfig)
		{
			return readConfig(true /* oldconfig */);
		}
	}

	if (rewriteConfig)
	{
		writeConfig();
	}
}

static void init_config()
{

	debug_outln_info(F("mounting FS..."));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

	bool spiffs_begin_ok = SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);

#pragma GCC diagnostic pop

	if (!spiffs_begin_ok)
	{
		debug_outln_error(F("failed to mount FS"));
		return;
	}
	readConfig();
}

/*****************************************************************
 * Prepare information for data Loggers                          *
 *****************************************************************/
static void createLoggerConfigs()
{

	auto new_session = []()
	{ return nullptr; };

	if (cfg::send2dusti)
	{
		loggerConfigs[LoggerSensorCommunity].destport = 80;
		if (cfg::ssl_dusti)
		{
			loggerConfigs[LoggerSensorCommunity].destport = 443;
			loggerConfigs[LoggerSensorCommunity].session = new_session();
		}
	}
	loggerConfigs[LoggerMadavi].destport = PORT_MADAVI;
	if (cfg::send2madavi && cfg::ssl_madavi)
	{
		loggerConfigs[LoggerMadavi].destport = 443;
		loggerConfigs[LoggerMadavi].session = new_session();
	}
	loggerConfigs[LoggerCustom].destport = cfg::port_custom;
	if (cfg::send2custom && (cfg::ssl_custom || (cfg::port_custom == 443)))
	{
		loggerConfigs[LoggerCustom].session = new_session();
	}
	loggerConfigs[LoggerCustom2].destport = cfg::port_custom2;
	if (cfg::send2custom2 && (cfg::ssl_custom2 || (cfg::port_custom2 == 443)))
	{
		loggerConfigs[LoggerCustom2].session = new_session();
	}
}

/*****************************************************************
 * dew point helper function                                     *
 *****************************************************************/
static float dew_point(const float temperature, const float humidity)
{
	float dew_temp;
	const float k2 = 17.62;
	const float k3 = 243.12;

	dew_temp = k3 * (((k2 * temperature) / (k3 + temperature)) + log(humidity / 100.0f)) / (((k2 * k3) / (k3 + temperature)) - log(humidity / 100.0f));

	return dew_temp;
}

/*****************************************************************
 * dew point helper function                                     *
 *****************************************************************/
static float pressure_at_sealevel(const float temperature, const float pressure)
{
	float pressure_at_sealevel;

	pressure_at_sealevel = pressure * pow(((temperature + 273.15f) / (temperature + 273.15f + (0.0065f * readCorrectionOffset(cfg::height_above_sealevel)))), -5.255f);

	return pressure_at_sealevel;
}

/*****************************************************************
 * html helper functions                                         *
 *****************************************************************/
static void start_html_page(String &page_content, const String &title)
{
	last_page_load = millis();

	RESERVE_STRING(s, LARGE_STR);
	s = FPSTR(WEB_PAGE_HEADER);
	s.replace("{t}", title);
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), s);

	server.sendContent_P(WEB_PAGE_HEADER_HEAD);

	s = FPSTR(WEB_PAGE_HEADER_BODY);
	s.replace("{t}", title);
	if (title != " ")
	{
		s.replace("{n}", F("&raquo;"));
	}
	else
	{
		s.replace("{n}", emptyString);
	}
	s.replace("{id}", esp_chipid);
	//s.replace("{macid}", esp_mac_id);
	//s.replace("{mac}", WiFi.macAddress());
	page_content += s;
}

static void end_html_page(String &page_content)
{
	if (page_content.length())
	{
		server.sendContent(page_content);
	}
	server.sendContent_P(WEB_PAGE_FOOTER);
}

static void add_form_input(String &page_content, const ConfigShapeId cfgid, const __FlashStringHelper *info, const int length)
{
	RESERVE_STRING(s, MED_STR);
	s = F("<tr>"
		  "<td title='[&lt;= {l}]'>{i}:&nbsp;</td>"
		  "<td style='width:{l}em'>"
		  "<input type='{t}' name='{n}' id='{n}' placeholder='{i}' value='{v}' maxlength='{l}'/>"
		  "</td></tr>");
	String t_value;
	ConfigShapeEntry c;
	memcpy_P(&c, &configShape[cfgid], sizeof(ConfigShapeEntry));
	switch (c.cfg_type)
	{
	case Config_Type_UInt:
		t_value = String(*c.cfg_val.as_uint);
		s.replace("{t}", F("number"));
		break;
	case Config_Type_Time:
		t_value = String((*c.cfg_val.as_uint) / 1000);
		s.replace("{t}", F("number"));
		break;
	case Config_Type_Password:
		s.replace("{t}", F("password"));
		info = FPSTR(INTL_PASSWORD);
	case Config_Type_Hex:
		s.replace("{t}", F("hex"));
	default:
		t_value = c.cfg_val.as_str;
		t_value.replace("'", "&#39;");
		s.replace("{t}", F("text"));
	}
	s.replace("{i}", info);
	s.replace("{n}", String(c.cfg_key()));
	s.replace("{v}", t_value);
	s.replace("{l}", String(length));
	page_content += s;
}

static String form_checkbox(const ConfigShapeId cfgid, const String &info, const bool linebreak)
{
	RESERVE_STRING(s, MED_STR);
	s = F("<label for='{n}'>"
		  "<input type='checkbox' name='{n}' value='1' id='{n}' {c}/>"
		  "<input type='hidden' name='{n}' value='0'/>"
		  "{i}</label><br/>");
	if (*configShape[cfgid].cfg_val.as_bool)
	{
		s.replace("{c}", F(" checked='checked'"));
	}
	else
	{
		s.replace("{c}", emptyString);
	};
	s.replace("{i}", info);
	s.replace("{n}", String(configShape[cfgid].cfg_key()));
	if (!linebreak)
	{
		s.replace("<br/>", emptyString);
	}
	return s;
}

static String form_submit(const String &value)
{
	String s = F("<tr>"
				 "<td>&nbsp;</td>"
				 "<td>"
				 "<input type='submit' name='submit' value='{v}' />"
				 "</td>"
				 "</tr>");
	s.replace("{v}", value);
	return s;
}

static String form_select_lang()
{
	String s_select = F(" selected='selected'");
	String s = F("<tr>"
				 "<td>" INTL_LANGUAGE ":&nbsp;</td>"
				 "<td>"
				 "<select id='current_lang' name='current_lang'>"
				 "<option value='FR'>Français (FR)</option>"
				 "<option value='EN'>English (EN)</option>"
				 "</select>"
				 "</td>"
				 "</tr>");

	s.replace("'" + String(cfg::current_lang) + "'>", "'" + String(cfg::current_lang) + "'" + s_select + ">");
	return s;
}

static void add_warning_first_cycle(String &page_content)
{
	String s = FPSTR(INTL_TIME_TO_FIRST_MEASUREMENT);
	unsigned int time_to_first = cfg::sending_intervall_ms - msSince(starttime);
	if (time_to_first > cfg::sending_intervall_ms)
	{
		time_to_first = 0;
	}
	s.replace("{v}", String(((time_to_first + 500) / 1000)));
	page_content += s;
}

static void add_age_last_values(String &s)
{
	s += "<b>";
	unsigned int time_since_last = msSince(starttime);
	if (time_since_last > cfg::sending_intervall_ms)
	{
		time_since_last = 0;
	}
	s += String((time_since_last + 500) / 1000);
	s += FPSTR(INTL_TIME_SINCE_LAST_MEASUREMENT);
	s += FPSTR(WEB_B_BR_BR);
}

/*****************************************************************
 * Webserver request auth: prompt for BasicAuth
 *
 * -Provide BasicAuth for all page contexts except /values and images
 *****************************************************************/
static bool webserver_request_auth()
{
	if (cfg::www_basicauth_enabled && !wificonfig_loop)
	{
		debug_outln_info(F("validate request auth..."));
		if (!server.authenticate(cfg::www_username, cfg::www_password))
		{
			server.requestAuthentication(BASIC_AUTH, "Sensor Login", F("Authentication failed"));
			return false;
		}
	}
	return true;
}

static void sendHttpRedirect()
{
	server.sendHeader(F("Location"), F("http://192.168.4.1/config"));
	server.send(302, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), emptyString);
}

/*****************************************************************
 * Webserver root: show all options                              *
 *****************************************************************/
static void webserver_root()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		sendHttpRedirect();
	}
	else
	{
		if (!webserver_request_auth())
		{
			return;
		}

		RESERVE_STRING(page_content, XLARGE_STR);
		start_html_page(page_content, emptyString);
		debug_outln_info(F("ws: root ..."));

		// Enable Pagination
		page_content += FPSTR(WEB_ROOT_PAGE_CONTENT);
		page_content.replace(F("{t}"), FPSTR(INTL_CURRENT_DATA));
		page_content.replace(F("{s}"), FPSTR(INTL_DEVICE_STATUS));
		page_content.replace(F("{conf}"), FPSTR(INTL_CONFIGURATION));
		page_content.replace(F("{restart}"), FPSTR(INTL_RESTART_SENSOR));
		page_content.replace(F("{debug}"), FPSTR(INTL_DEBUG_LEVEL));
		end_html_page(page_content);
	}
}

/*****************************************************************
 * Webserver config: show config page                            *
 *****************************************************************/
static void webserver_config_send_body_get(String &page_content)
{
	auto add_form_checkbox = [&page_content](const ConfigShapeId cfgid, const String &info)
	{
		page_content += form_checkbox(cfgid, info, true);
	};

	auto add_form_checkbox_sensor = [&add_form_checkbox](const ConfigShapeId cfgid, __const __FlashStringHelper *info)
	{
		add_form_checkbox(cfgid, add_sensor_type(info));
	};

	debug_outln_info(F("begin webserver_config_body_get ..."));
	page_content += F("<form method='POST' action='/config' style='width:100%;'>\n"
					  "<input class='radio' id='r1' name='group' type='radio' checked>"
					  "<input class='radio' id='r2' name='group' type='radio'>"
					  "<input class='radio' id='r3' name='group' type='radio'>"
					  "<input class='radio' id='r4' name='group' type='radio'>"
					  "<input class='radio' id='r5' name='group' type='radio'>"
					  "<div class='tabs'>"
					  "<label class='tab' id='tab1' for='r1'>" INTL_WIFI_SETTINGS "</label>"
					  "<label class='tab' id='tab2' for='r2'>");
	page_content += FPSTR(INTL_LORA_SETTINGS);
	page_content += F("</label>"
					  "<label class='tab' id='tab3' for='r3'>");
	page_content += FPSTR(INTL_MORE_SETTINGS);
	page_content += F("</label>"
					  "<label class='tab' id='tab4' for='r4'>");
	page_content += FPSTR(INTL_SENSORS);
	page_content += F(
		"</label>"
		"<label class='tab' id='tab5' for='r5'>APIs"
		"</label></div><div class='panels'>"
		"<div class='panel' id='panel1'>");

	if (wificonfig_loop)
	{ // scan for wlan ssids
		page_content += F("<div id='wifilist'>" INTL_WIFI_NETWORKS "</div><br/>");
	}
	add_form_checkbox(Config_has_wifi, FPSTR(INTL_WIFI_ACTIVATION));
	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_wlanssid, FPSTR(INTL_FS_WIFI_NAME), LEN_WLANSSID - 1);
	add_form_input(page_content, Config_wlanpwd, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += F("<hr/>\n<br/><b>");

	page_content += FPSTR(INTL_AB_HIER_NUR_ANDERN);
	page_content += FPSTR(WEB_B_BR);
	page_content += FPSTR(BR_TAG);

	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);
	page_content = emptyString;

	add_form_checkbox(Config_www_basicauth_enabled, FPSTR(INTL_BASICAUTH));
	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_www_username, FPSTR(INTL_USER), LEN_WWW_USERNAME - 1);
	add_form_input(page_content, Config_www_password, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += FPSTR(BR_TAG);

	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);

	if (!wificonfig_loop)
	{
		page_content = FPSTR(INTL_FS_WIFI_DESCRIPTION);
		page_content += FPSTR(BR_TAG);

		page_content += FPSTR(TABLE_TAG_OPEN);
		add_form_input(page_content, Config_fs_ssid, FPSTR(INTL_FS_WIFI_NAME), LEN_FS_SSID - 1);
		add_form_input(page_content, Config_fs_pwd, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD - 1);
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);

		// Paginate page after ~ 1500 Bytes
		server.sendContent(page_content);
	}

	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(2));
	page_content += FPSTR(WEB_LF_B);
	page_content += FPSTR(INTL_LORA_EXPLANATION);
	page_content += FPSTR(WEB_B_BR_BR);
	add_form_checkbox(Config_has_lora, FPSTR(INTL_LORA_ACTIVATION));
	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_appeui, FPSTR("APPEUI"), LEN_APPEUI - 1);
	add_form_input(page_content, Config_deveui, FPSTR("DEVEUI"), LEN_DEVEUI - 1);
	add_form_input(page_content, Config_appkey, FPSTR("APPKEY"), LEN_APPKEY - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	server.sendContent(page_content);
	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(3));

	//add_form_checkbox(Config_has_display, FPSTR(INTL_DISPLAY));
	add_form_checkbox(Config_has_ssd1306, FPSTR(INTL_SSD1306));

	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);
	page_content = emptyString;

	add_form_checkbox(Config_display_wifi_info, FPSTR(INTL_DISPLAY_WIFI_INFO));
	add_form_checkbox(Config_display_lora_info, FPSTR(INTL_DISPLAY_LORA_INFO));
	add_form_checkbox(Config_display_device_info, FPSTR(INTL_DISPLAY_DEVICE_INFO));

	server.sendContent(page_content);
	page_content = FPSTR(WEB_BR_LF_B);
	page_content += F(INTL_FIRMWARE "</b>&nbsp;");

	page_content += FPSTR(TABLE_TAG_OPEN);
	page_content += form_select_lang();
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);

	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_debug, FPSTR(INTL_DEBUG_LEVEL), 1);
	add_form_input(page_content, Config_sending_intervall_ms, FPSTR(INTL_MEASUREMENT_INTERVAL), 5);
	add_form_input(page_content, Config_time_for_wifi_config, FPSTR(INTL_DURATION_ROUTER_MODE), 5);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);

	server.sendContent(page_content);

	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(4));
	add_form_checkbox_sensor(Config_sds_read, FPSTR(INTL_SDS011));

	// Paginate page after ~ 1500 Bytes  //ATTENTION RYTHME PAGINATION !
	server.sendContent(page_content);
	page_content = emptyString;

	add_form_checkbox_sensor(Config_bmx280_read, FPSTR(INTL_BMX280));

	// // Paginate page after ~ 1500 Bytes
	// server.sendContent(page_content);
	// page_content = emptyString;

	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_height_above_sealevel, FPSTR(INTL_HEIGHT_ABOVE_SEALEVEL), LEN_HEIGHT_ABOVE_SEALEVEL - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);

	page_content += FPSTR(WEB_BR_LF_B);
	page_content += FPSTR(INTL_MORE_SENSORS);
	page_content += FPSTR(WEB_B_BR);

	add_form_checkbox_sensor(Config_npm_read, FPSTR(INTL_NPM));
	add_form_checkbox(Config_gps_read, FPSTR(INTL_NEO6M));

	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);
	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(5));

	page_content += tmpl(FPSTR(INTL_SEND_TO), F("APIs"));
	page_content += FPSTR(BR_TAG);
	page_content += form_checkbox(Config_send2dusti, FPSTR(WEB_SENSORCOMMUNITY), false);
	page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
	page_content += form_checkbox(Config_ssl_dusti, FPSTR(WEB_HTTPS), false);
	page_content += FPSTR(WEB_BRACE_BR);
	page_content += form_checkbox(Config_send2madavi, FPSTR(WEB_MADAVI), false);
	page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
	page_content += form_checkbox(Config_ssl_madavi, FPSTR(WEB_HTTPS), false);
	page_content += FPSTR(WEB_BRACE_BR);
	add_form_checkbox(Config_send2csv, FPSTR(WEB_CSV));

	server.sendContent(page_content);
	page_content = emptyString;

	page_content += FPSTR(BR_TAG);
	page_content += form_checkbox(Config_send2custom, FPSTR(INTL_SEND_TO_OWN_API), false);
	page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
	page_content += form_checkbox(Config_ssl_custom, FPSTR(WEB_HTTPS), false);
	page_content += FPSTR(WEB_BRACE_BR);

	server.sendContent(page_content);
	page_content = FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_host_custom, FPSTR(INTL_SERVER), LEN_HOST_CUSTOM - 1);
	add_form_input(page_content, Config_url_custom, FPSTR(INTL_PATH), LEN_URL_CUSTOM - 1);
	add_form_input(page_content, Config_port_custom, FPSTR(INTL_PORT), MAX_PORT_DIGITS);
	add_form_input(page_content, Config_user_custom, FPSTR(INTL_USER), LEN_USER_CUSTOM - 1);
	add_form_input(page_content, Config_pwd_custom, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);

	page_content += FPSTR(BR_TAG);
	page_content += form_checkbox(Config_send2custom2, FPSTR(INTL_SEND_TO_OWN_API2), false);
	page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
	page_content += form_checkbox(Config_ssl_custom2, FPSTR(WEB_HTTPS), false);
	page_content += FPSTR(WEB_BRACE_BR);

	server.sendContent(page_content);

	page_content = FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_host_custom2, FPSTR(INTL_SERVER2), LEN_HOST_CUSTOM2 - 1);
	add_form_input(page_content, Config_url_custom2, FPSTR(INTL_PATH2), LEN_URL_CUSTOM2 - 1);
	add_form_input(page_content, Config_port_custom2, FPSTR(INTL_PORT2), MAX_PORT_DIGITS2);
	add_form_input(page_content, Config_user_custom2, FPSTR(INTL_USER2), LEN_USER_CUSTOM2 - 1);
	add_form_input(page_content, Config_pwd_custom2, FPSTR(INTL_PASSWORD2), LEN_CFG_PASSWORD2 - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += F("</div></div>");
	page_content += form_submit(FPSTR(INTL_SAVE_AND_RESTART));
	page_content += FPSTR(BR_TAG);
	page_content += FPSTR(WEB_BR_FORM);
	if (wificonfig_loop)
	{ // scan for wlan ssids
		page_content += F("<script>window.setTimeout(load_wifi_list,1000);</script>");
	}
	server.sendContent(page_content);
	page_content = emptyString;
}

static void webserver_config_send_body_post(String &page_content)
{
	String masked_pwd;

	for (unsigned e = 0; e < sizeof(configShape) / sizeof(configShape[0]); ++e)
	{
		ConfigShapeEntry c;
		memcpy_P(&c, &configShape[e], sizeof(ConfigShapeEntry));
		const String s_param(c.cfg_key());
		if (!server.hasArg(s_param))
		{
			continue;
		}
		const String server_arg(server.arg(s_param));

		switch (c.cfg_type)
		{
		case Config_Type_UInt:
			*(c.cfg_val.as_uint) = server_arg.toInt();
			break;
		case Config_Type_Time:
			*(c.cfg_val.as_uint) = server_arg.toInt() * 1000;
			break;
		case Config_Type_Bool:
			*(c.cfg_val.as_bool) = (server_arg == "1");
			break;
		case Config_Type_String:
			strncpy(c.cfg_val.as_str, server_arg.c_str(), c.cfg_len);
			c.cfg_val.as_str[c.cfg_len] = '\0';
			break;
		case Config_Type_Password:
			if (server_arg.length())
			{
				server_arg.toCharArray(c.cfg_val.as_str, LEN_CFG_PASSWORD);
			}
			break;
		case Config_Type_Hex:
			strncpy(c.cfg_val.as_str, server_arg.c_str(), c.cfg_len);
			c.cfg_val.as_str[c.cfg_len] = '\0';
			break;
		}
	}

	page_content += FPSTR(INTL_SENSOR_IS_REBOOTING);

	server.sendContent(page_content);
	page_content = emptyString;
}

static void sensor_restart()
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

	SPIFFS.end();

#pragma GCC diagnostic pop

	if (cfg::npm_read)
	{
		serialNPM.end();
	}
	else
	{
		serialSDS.end();
	}
	debug_outln_info(F("Restart."));
	delay(500);
	ESP.restart();
	// should not be reached
	while (true)
	{
		yield();
	}
}

static void webserver_config()
{
	if (!webserver_request_auth())
	{
		return;
	}

	debug_outln_info(F("ws: config page ..."));

	server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
	server.sendHeader(F("Pragma"), F("no-cache"));
	server.sendHeader(F("Expires"), F("0"));
	// Enable Pagination (Chunked Transfer)
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);

	RESERVE_STRING(page_content, XLARGE_STR);

	start_html_page(page_content, FPSTR(INTL_CONFIGURATION));
	if (wificonfig_loop)
	{ // scan for wlan ssids
		page_content += FPSTR(WEB_CONFIG_SCRIPT);
	}

	if (server.method() == HTTP_GET)
	{
		webserver_config_send_body_get(page_content);
	}
	else
	{
		webserver_config_send_body_post(page_content);
	}
	end_html_page(page_content);

	if (server.method() == HTTP_POST)
	{
		display_debug(F("Writing config"), emptyString);
		if (writeConfig())
		{
			display_debug(F("Writing config"), F("and restarting"));
			sensor_restart();
		}
	}
}

/*****************************************************************
 * Webserver wifi: show available wifi networks                  *
 *****************************************************************/
static void webserver_wifi()
{
	String page_content;

	debug_outln_info(F("wifi networks found: "), String(count_wifiInfo));
	if (count_wifiInfo == 0)
	{
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(INTL_NO_NETWORKS);
		page_content += FPSTR(BR_TAG);
	}
	else
	{
		std::unique_ptr<int[]> indices(new int[count_wifiInfo]);
		debug_outln_info(F("ws: wifi ..."));
		for (unsigned i = 0; i < count_wifiInfo; ++i)
		{
			indices[i] = i;
		}
		for (unsigned i = 0; i < count_wifiInfo; i++)
		{
			for (unsigned j = i + 1; j < count_wifiInfo; j++)
			{
				if (wifiInfo[indices[j]].RSSI > wifiInfo[indices[i]].RSSI)
				{
					std::swap(indices[i], indices[j]);
				}
			}
		}
		int duplicateSsids = 0;
		for (int i = 0; i < count_wifiInfo; i++)
		{
			if (indices[i] == -1)
			{
				continue;
			}
			for (int j = i + 1; j < count_wifiInfo; j++)
			{
				if (strncmp(wifiInfo[indices[i]].ssid, wifiInfo[indices[j]].ssid, sizeof(wifiInfo[0].ssid)) == 0)
				{
					indices[j] = -1; // set dup aps to index -1
					++duplicateSsids;
				}
			}
		}

		page_content += FPSTR(INTL_NETWORKS_FOUND);
		page_content += String(count_wifiInfo - duplicateSsids);
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(TABLE_TAG_OPEN);
		//if (n > 30) n=30;
		for (int i = 0; i < count_wifiInfo; ++i)
		{
			if (indices[i] == -1)
			{
				continue;
			}
			// Print SSID and RSSI for each network found
			page_content += wlan_ssid_to_table_row(wifiInfo[indices[i]].ssid, ((wifiInfo[indices[i]].encryptionType == WIFI_AUTH_OPEN) ? " " : u8"🔒"), wifiInfo[indices[i]].RSSI);
		}
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += FPSTR(BR_TAG);
	}
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver root: show latest values                            *
 *****************************************************************/
static void webserver_values()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		sendHttpRedirect();
		return;
	}

	RESERVE_STRING(page_content, XLARGE_STR);
	start_html_page(page_content, FPSTR(INTL_CURRENT_DATA));
	const String unit_Deg("°");
	const String unit_P("hPa");
	const String unit_T("°C");
	const String unit_CO2("ppm");
	const String unit_NC();
	const String unit_LA(F("dB(A)"));
	float dew_point_temp;

	const int signal_quality = calcWiFiSignalQuality(last_signal_strength);
	debug_outln_info(F("ws: values ..."));
	if (!count_sends)
	{
		page_content += F("<b style='color:red'>");
		add_warning_first_cycle(page_content);
		page_content += FPSTR(WEB_B_BR_BR);
	}
	else
	{
		add_age_last_values(page_content);
	}

	auto add_table_pm_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float &value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -1, 1, 0), F("µg/m³"));
	};

	auto add_table_nc_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -1, 1, 0), F("#/cm³"));
	};

	auto add_table_t_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -128, 1, 0), "°C");
	};

	auto add_table_h_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -1, 1, 0), "%");
	};

	auto add_table_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const String &value, const String &unit)
	{
		add_table_row_from_value(page_content, sensor, param, value, unit);
	};

	server.sendContent(page_content);
	page_content = F("<table cellspacing='0' cellpadding='5' class='v'>\n"
					 "<thead><tr><th>" INTL_SENSOR "</th><th> " INTL_PARAMETER "</th><th>" INTL_VALUE "</th></tr></thead>");

	if (cfg::sds_read)
	{
		add_table_pm_value(FPSTR(SENSORS_SDS011), FPSTR(WEB_PM25), last_value_SDS_P2);
		add_table_pm_value(FPSTR(SENSORS_SDS011), FPSTR(WEB_PM10), last_value_SDS_P1);
		page_content += FPSTR(EMPTY_ROW);
	}
	if (cfg::npm_read)
	{
		add_table_pm_value(FPSTR(SENSORS_NPM), FPSTR(WEB_PM1), last_value_NPM_P0);
		add_table_pm_value(FPSTR(SENSORS_NPM), FPSTR(WEB_PM25), last_value_NPM_P2);
		add_table_pm_value(FPSTR(SENSORS_NPM), FPSTR(WEB_PM10), last_value_NPM_P1);
		add_table_nc_value(FPSTR(SENSORS_NPM), FPSTR(WEB_NC1k0), last_value_NPM_N0);
		add_table_nc_value(FPSTR(SENSORS_NPM), FPSTR(WEB_NC2k5), last_value_NPM_N2);
		add_table_nc_value(FPSTR(SENSORS_NPM), FPSTR(WEB_NC10), last_value_NPM_N1);
		page_content += FPSTR(EMPTY_ROW);
	}

	if (cfg::bmx280_read)
	{
		const char *const sensor_name = (bmx280.sensorID() == BME280_SENSOR_ID) ? SENSORS_BME280 : SENSORS_BMP280;
		add_table_t_value(FPSTR(sensor_name), FPSTR(INTL_TEMPERATURE), last_value_BMX280_T);
		add_table_value(FPSTR(sensor_name), FPSTR(INTL_PRESSURE), check_display_value(last_value_BMX280_P / 100.0f, (-1 / 100.0f), 2, 0), unit_P);
		add_table_value(FPSTR(sensor_name), FPSTR(INTL_PRESSURE_AT_SEALEVEL), last_value_BMX280_P != -1.0f ? String(pressure_at_sealevel(last_value_BMX280_T, last_value_BMX280_P / 100.0f), 2) : "-", unit_P);
		if (bmx280.sensorID() == BME280_SENSOR_ID)
		{
			add_table_h_value(FPSTR(sensor_name), FPSTR(INTL_HUMIDITY), last_value_BME280_H);
			dew_point_temp = dew_point(last_value_BMX280_T, last_value_BME280_H);
			add_table_value(FPSTR(sensor_name), FPSTR(INTL_DEW_POINT), isnan(dew_point_temp) ? "-" : String(dew_point_temp, 1), unit_T);
		}
		page_content += FPSTR(EMPTY_ROW);
	}
	if (cfg::gps_read)
	{
		add_table_value(FPSTR(WEB_GPS), FPSTR(INTL_LATITUDE), check_display_value(last_value_GPS_lat, -200.0, 6, 0), unit_Deg);
		add_table_value(FPSTR(WEB_GPS), FPSTR(INTL_LONGITUDE), check_display_value(last_value_GPS_lon, -200.0, 6, 0), unit_Deg);
		add_table_value(FPSTR(WEB_GPS), FPSTR(INTL_ALTITUDE), check_display_value(last_value_GPS_alt, -1000.0, 2, 0), "m");
		add_table_value(FPSTR(WEB_GPS), FPSTR(INTL_TIME_UTC), last_value_GPS_timestamp, emptyString);
		page_content += FPSTR(EMPTY_ROW);
	}

	server.sendContent(page_content);
	page_content = emptyString;

	add_table_value(F("WiFi"), FPSTR(INTL_SIGNAL_STRENGTH), String(last_signal_strength), "dBm");
	add_table_value(F("WiFi"), FPSTR(INTL_SIGNAL_QUALITY), String(signal_quality), "%");

	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += FPSTR(BR_TAG);
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver root: show device status
 *****************************************************************/
static void webserver_status()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		sendHttpRedirect();
		return;
	}

	RESERVE_STRING(page_content, XLARGE_STR);
	start_html_page(page_content, FPSTR(INTL_DEVICE_STATUS));

	debug_outln_info(F("ws: status ..."));
	server.sendContent(page_content);
	page_content = F("<table cellspacing='0' cellpadding='5' class='v'>\n"
					 "<thead><tr><th> " INTL_PARAMETER "</th><th>" INTL_VALUE "</th></tr></thead>");
	String versionHtml(SOFTWARE_VERSION);
	versionHtml += F("/ST:");
	versionHtml += String(!nebulo_selftest_failed);
	versionHtml += '/';
	versionHtml.replace("/", FPSTR(BR_TAG));
	add_table_row_from_value(page_content, FPSTR(INTL_FIRMWARE), versionHtml);
	add_table_row_from_value(page_content, F("Free Memory"), String(ESP.getFreeHeap()));
	time_t now = time(nullptr);
	add_table_row_from_value(page_content, FPSTR(INTL_TIME_UTC), ctime(&now));
	add_table_row_from_value(page_content, F("Uptime"), delayToString(millis() - time_point_device_start_ms));
	if (cfg::sds_read)
	{
		page_content += FPSTR(EMPTY_ROW);
		add_table_row_from_value(page_content, FPSTR(SENSORS_SDS011), last_value_SDS_version);
	}
	if (cfg::npm_read)
	{
		page_content += FPSTR(EMPTY_ROW);
		add_table_row_from_value(page_content, FPSTR(SENSORS_NPM), last_value_NPM_version);
	}
	page_content += FPSTR(EMPTY_ROW);
	page_content += F("<tr><td colspan='2'><b>" INTL_ERROR "</b></td></tr>");
	String wifiStatus(WiFi_error_count);
	wifiStatus += '/';
	wifiStatus += String(last_signal_strength);
	wifiStatus += '/';
	wifiStatus += String(last_disconnect_reason);
	add_table_row_from_value(page_content, F("WiFi"), wifiStatus);

	if (last_update_returncode != 0)
	{
		add_table_row_from_value(page_content, F("OTA Return"),
								 last_update_returncode > 0 ? String(last_update_returncode) : HTTPClient::errorToString(last_update_returncode));
	}
	for (unsigned int i = 0; i < LoggerCount; ++i)
	{
		if (loggerConfigs[i].errors)
		{
			const __FlashStringHelper *logger = loggerDescription(i);
			if (logger)
			{
				add_table_row_from_value(page_content, logger, String(loggerConfigs[i].errors));
			}
		}
	}

	if (last_sendData_returncode != 0)
	{
		add_table_row_from_value(page_content, F("Data Send Return"),
								 last_sendData_returncode > 0 ? String(last_sendData_returncode) : HTTPClient::errorToString(last_sendData_returncode));
	}
	if (cfg::sds_read)
	{
		add_table_row_from_value(page_content, FPSTR(SENSORS_SDS011), String(SDS_error_count));
	}
	if (cfg::npm_read)
	{
		add_table_row_from_value(page_content, FPSTR(SENSORS_NPM), String(NPM_error_count));
	}
	server.sendContent(page_content);
	page_content = emptyString;

	if (count_sends > 0)
	{
		page_content += FPSTR(EMPTY_ROW);
		add_table_row_from_value(page_content, F(INTL_NUMBER_OF_MEASUREMENTS), String(count_sends));
		if (sending_time > 0)
		{
			add_table_row_from_value(page_content, F(INTL_TIME_SENDING_MS), String(sending_time), "ms");
		}
	}

	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver read serial ring buffer                             *
 *****************************************************************/
static void webserver_serial()
{
	String s(Debug.popLines());

	server.send(s.length() ? 200 : 204, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), s);
}

/*****************************************************************
 * Webserver set debug level                                     *
 *****************************************************************/
static void webserver_debug_level()
{
	if (!webserver_request_auth())
	{
		return;
	}

	RESERVE_STRING(page_content, LARGE_STR);
	start_html_page(page_content, FPSTR(INTL_DEBUG_LEVEL));

	if (server.hasArg("lvl"))
	{
		debug_outln_info(F("ws: debug level ..."));

		const int lvl = server.arg("lvl").toInt();
		if (lvl >= 0 && lvl <= 5)
		{
			cfg::debug = lvl;
			page_content += F("<h3>");
			page_content += FPSTR(INTL_DEBUG_SETTING_TO);
			page_content += ' ';

			const __FlashStringHelper *lvlText;
			switch (lvl)
			{
			case DEBUG_ERROR:
				lvlText = F(INTL_ERROR);
				break;
			case DEBUG_WARNING:
				lvlText = F(INTL_WARNING);
				break;
			case DEBUG_MIN_INFO:
				lvlText = F(INTL_MIN_INFO);
				break;
			case DEBUG_MED_INFO:
				lvlText = F(INTL_MED_INFO);
				break;
			case DEBUG_MAX_INFO:
				lvlText = F(INTL_MAX_INFO);
				break;
			default:
				lvlText = F(INTL_NONE);
			}

			page_content += lvlText;
			page_content += F(".</h3>");
		}
	}

	page_content += F("<br/><pre id='slog' class='panels'>");
	page_content += Debug.popLines();
	page_content += F("</pre>");
	page_content += F("<script>"
					  "function slog_update() {"
					  "fetch('/serial').then(r => r.text()).then((r) => {"
					  "document.getElementById('slog').innerText += r;}).catch(err => console.log(err));};"
					  "setInterval(slog_update, 3000);"
					  "</script>");
	page_content += F("<h4>");
	page_content += FPSTR(INTL_DEBUG_SETTING_TO);
	page_content += F("</h4>"
					  "<table style='width:100%;'>"
					  "<tr><td style='width:25%;'><a class='b' href='/debug?lvl=0'>" INTL_NONE "</a></td>"
					  "<td style='width:25%;'><a class='b' href='/debug?lvl=1'>" INTL_ERROR "</a></td>"
					  "<td style='width:25%;'><a class='b' href='/debug?lvl=3'>" INTL_MIN_INFO "</a></td>"
					  "<td style='width:25%;'><a class='b' href='/debug?lvl=5'>" INTL_MAX_INFO "</a></td>"
					  "</tr><tr>"
					  "</tr>"
					  "</table>");

	end_html_page(page_content);
}

/*****************************************************************
 * Webserver remove config                                       *
 *****************************************************************/
static void webserver_removeConfig()
{
	if (!webserver_request_auth())
	{
		return;
	}

	RESERVE_STRING(page_content, LARGE_STR);
	start_html_page(page_content, FPSTR(INTL_DELETE_CONFIG));
	debug_outln_info(F("ws: removeConfig ..."));

	if (server.method() == HTTP_GET)
	{
		page_content += FPSTR(WEB_REMOVE_CONFIG_CONTENT);
	}
	else
	{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		// Silently remove the desaster backup
		SPIFFS.remove(F("/config.json.old"));
		if (SPIFFS.exists(F("/config.json")))
		{ //file exists
			debug_outln_info(F("removing config.json..."));
			if (SPIFFS.remove(F("/config.json")))
			{
				page_content += F("<h3>" INTL_CONFIG_DELETED ".</h3>");
			}
			else
			{
				page_content += F("<h3>" INTL_CONFIG_CAN_NOT_BE_DELETED ".</h3>");
			}
		}
		else
		{
			page_content += F("<h3>" INTL_CONFIG_NOT_FOUND ".</h3>");
		}
#pragma GCC diagnostic pop
	}
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver reset NodeMCU                                       *
 *****************************************************************/
static void webserver_reset()
{
	if (!webserver_request_auth())
	{
		return;
	}

	String page_content;
	page_content.reserve(512);

	start_html_page(page_content, FPSTR(INTL_RESTART_SENSOR));
	debug_outln_info(F("ws: reset ..."));

	if (server.method() == HTTP_GET)
	{
		page_content += FPSTR(WEB_RESET_CONTENT);
	}
	else
	{
		sensor_restart();
	}
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver data.json                                           *
 *****************************************************************/
static void webserver_data_json()
{
	String s1;
	unsigned long age = 0;

	debug_outln_info(F("ws: data json..."));
	if (!count_sends)
	{
		s1 = FPSTR(data_first_part);
		s1 += "]}";
		age = cfg::sending_intervall_ms - msSince(starttime);
		if (age > cfg::sending_intervall_ms)
		{
			age = 0;
		}
		age = 0 - age;
	}
	else
	{
		s1 = last_data_string;
		age = msSince(starttime);
		if (age > cfg::sending_intervall_ms)
		{
			age = 0;
		}
	}
	String s2 = F(", \"age\":\"");
	s2 += String((long)((age + 500) / 1000));
	s2 += F("\", \"sensordatavalues\"");
	s1.replace(F(", \"sensordatavalues\""), s2);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_JSON), s1);
}

/*****************************************************************
 * Webserver metrics endpoint                                    *
 *****************************************************************/
static void webserver_metrics_endpoint()
{
	debug_outln_info(F("ws: /metrics"));
	RESERVE_STRING(page_content, XLARGE_STR);
	page_content = F("software_version{version=\"" SOFTWARE_VERSION_STR "\",$i} 1\nuptime_ms{$i} $u\nsending_intervall_ms{$i} $s\nnumber_of_measurements{$i} $c\n");
	String id(F("node=\"" SENSOR_BASENAME));
	id += esp_chipid;
	id += '\"';
	page_content.replace("$i", id);
	page_content.replace("$u", String(msSince(time_point_device_start_ms)));
	page_content.replace("$s", String(cfg::sending_intervall_ms));
	page_content.replace("$c", String(count_sends));
	DynamicJsonDocument json2data(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json2data, last_data_string);
	if (!err)
	{
		for (JsonObject measurement : json2data[FPSTR(JSON_SENSOR_DATA_VALUES)].as<JsonArray>())
		{
			page_content += measurement["value_type"].as<const char *>();
			page_content += '{';
			page_content += id;
			page_content += "} ";
			page_content += measurement["value"].as<const char *>();
			page_content += '\n';
		}
		page_content += F("last_sample_age_ms{");
		page_content += id;
		page_content += "} ";
		page_content += String(msSince(starttime));
		page_content += '\n';
	}
	else
	{
		debug_outln_error(FPSTR(DBG_TXT_DATA_READ_FAILED));
	}
	page_content += F("# EOF\n");
	debug_outln(page_content, DEBUG_MED_INFO);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), page_content);
}

/*****************************************************************
 * Webserver Images                                              *
 *****************************************************************/

static void webserver_favicon()
{
	server.sendHeader(F("Cache-Control"), F("max-age=2592000, public"));

	// server.send_P(200, TXT_CONTENT_TYPE_IMAGE_PNG,
	// 			  LUFTDATEN_INFO_LOGO_PNG, LUFTDATEN_INFO_LOGO_PNG_SIZE);

	// server.send_P(200, TXT_CONTENT_TYPE_IMAGE_SVG,
	// 			  AIRCARTO_INFO_LOGO_SVG, AIRCARTO_INFO_LOGO_SVG_SIZE);

	server.send_P(200, TXT_CONTENT_TYPE_IMAGE_PNG,
				  AIRCARTO_INFO_LOGO_PNG, AIRCARTO_INFO_LOGO_PNG_SIZE);
}

/*****************************************************************
 * Webserver page not found                                      *
 *****************************************************************/
static void webserver_not_found()
{
	last_page_load = millis();
	debug_outln_info(F("ws: not found ..."));
	if (WiFi.status() != WL_CONNECTED)
	{
		if ((server.uri().indexOf(F("success.html")) != -1) || (server.uri().indexOf(F("detect.html")) != -1))
		{
			server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), FPSTR(WEB_IOS_REDIRECT));
		}
		else
		{
			sendHttpRedirect();
		}
	}
	else
	{
		server.send(404, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), F("Not found."));
	}
}

static void webserver_static()
{
	server.sendHeader(F("Cache-Control"), F("max-age=2592000, public"));

	if (server.arg(String('r')) == F("logo"))
	{
		//  server.send_P(200, TXT_CONTENT_TYPE_IMAGE_PNG,
		//  			  LUFTDATEN_INFO_LOGO_PNG, LUFTDATEN_INFO_LOGO_PNG_SIZE);

		// server.send_P(200, TXT_CONTENT_TYPE_IMAGE_SVG,
		// 			  AIRCARTO_INFO_LOGO_SVG, AIRCARTO_INFO_LOGO_SVG_SIZE);

		server.send_P(200, TXT_CONTENT_TYPE_IMAGE_PNG,
					  AIRCARTO_INFO_LOGO_PNG, AIRCARTO_INFO_LOGO_PNG_SIZE);
	}
	else if (server.arg(String('r')) == F("css"))
	{
		server.send_P(200, TXT_CONTENT_TYPE_TEXT_CSS,
					  WEB_PAGE_STATIC_CSS, sizeof(WEB_PAGE_STATIC_CSS) - 1);
	}
	else
	{
		webserver_not_found();
	}
}

/*****************************************************************
 * Webserver setup                                               *
 *****************************************************************/
static void setup_webserver()
{
	server.on("/", webserver_root);
	server.on(F("/config"), webserver_config);
	server.on(F("/wifi"), webserver_wifi);
	server.on(F("/values"), webserver_values);
	server.on(F("/status"), webserver_status);
	server.on(F("/generate_204"), webserver_config);
	server.on(F("/fwlink"), webserver_config);
	server.on(F("/debug"), webserver_debug_level);
	server.on(F("/serial"), webserver_serial);
	server.on(F("/removeConfig"), webserver_removeConfig);
	server.on(F("/reset"), webserver_reset);
	server.on(F("/data.json"), webserver_data_json);
	server.on(F("/metrics"), webserver_metrics_endpoint);
	server.on(F("/favicon.ico"), webserver_favicon);
	server.on(F(STATIC_PREFIX), webserver_static);
	server.onNotFound(webserver_not_found);

	//debug_outln_info(F("Starting Webserver... "), WiFi.localIP().toString());
	debug_outln_info(F("Starting Webserver... "));
	server.begin();
}

static int selectChannelForAp()
{
	std::array<int, 14> channels_rssi;
	std::fill(channels_rssi.begin(), channels_rssi.end(), -100);

	for (unsigned i = 0; i < std::min((uint8_t)14, count_wifiInfo); i++)
	{
		if (wifiInfo[i].RSSI > channels_rssi[wifiInfo[i].channel])
		{
			channels_rssi[wifiInfo[i].channel] = wifiInfo[i].RSSI;
		}
	}

	if ((channels_rssi[1] < channels_rssi[6]) && (channels_rssi[1] < channels_rssi[11]))
	{
		return 1;
	}
	else if ((channels_rssi[6] < channels_rssi[1]) && (channels_rssi[6] < channels_rssi[11]))
	{
		return 6;
	}
	else
	{
		return 11;
	}
}

/*****************************************************************
 * WifiConfig                                                    *
 *****************************************************************/
static void wifiConfig()
{
	debug_outln_info(F("Starting WiFiManager"));
	debug_outln_info(F("AP ID: "), String(cfg::fs_ssid));
	debug_outln_info(F("Password: "), String(cfg::fs_pwd));

	wificonfig_loop = true;

	WiFi.disconnect(true);
	debug_outln_info(F("scan for wifi networks..."));
	int8_t scanReturnCode = WiFi.scanNetworks(false /* scan async */, true /* show hidden networks */);
	if (scanReturnCode < 0)
	{
		debug_outln_error(F("WiFi scan failed. Treating as empty. "));
		count_wifiInfo = 0;
	}
	else
	{
		count_wifiInfo = (uint8_t)scanReturnCode;
	}

	delete[] wifiInfo;
	wifiInfo = new struct_wifiInfo[std::max(count_wifiInfo, (uint8_t)1)];

	for (unsigned i = 0; i < count_wifiInfo; i++)
	{
		String SSID;
		uint8_t *BSSID;

		memset(&wifiInfo[i], 0, sizeof(struct_wifiInfo));
		WiFi.getNetworkInfo(i, SSID, wifiInfo[i].encryptionType, wifiInfo[i].RSSI, BSSID, wifiInfo[i].channel);
		SSID.toCharArray(wifiInfo[i].ssid, sizeof(wifiInfo[0].ssid));
	}

	// Use 13 channels if locale is not "EN"
	wifi_country_t wifi;
	wifi.policy = WIFI_COUNTRY_POLICY_MANUAL;
	strcpy(wifi.cc, INTL_LANG);
	wifi.nchan = (INTL_LANG[0] == 'E' && INTL_LANG[1] == 'N') ? 11 : 13;
	wifi.schan = 1;

	WiFi.mode(WIFI_AP);
	const IPAddress apIP(192, 168, 4, 1);
	WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
	WiFi.softAP(cfg::fs_ssid, cfg::fs_pwd, selectChannelForAp());
	// In case we create a unique password at first start
	debug_outln_info(F("AP Password is: "), cfg::fs_pwd);

	DNSServer dnsServer;
	// Ensure we don't poison the client DNS cache
	dnsServer.setTTL(0);
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(53, "*", apIP); // 53 is port for DNS server

	setup_webserver();

	// 10 minutes timeout for wifi config
	last_page_load = millis();
	while ((millis() - last_page_load) < cfg::time_for_wifi_config + 500)
	{
		dnsServer.processNextRequest();
		server.handleClient();
		yield();
	}

	WiFi.softAPdisconnect(true);

	wifi.policy = WIFI_COUNTRY_POLICY_MANUAL;
	strcpy(wifi.cc, INTL_LANG);
	wifi.nchan = 13;
	wifi.schan = 1;

	//The station mode starts only if WiFi communication is enabled.

	if (cfg::has_wifi)
	{

		WiFi.mode(WIFI_STA);

		dnsServer.stop();
		delay(100);

		debug_outln_info(FPSTR(DBG_TXT_CONNECTING_TO), cfg::wlanssid);

		WiFi.begin(cfg::wlanssid, cfg::wlanpwd);
	}
	debug_outln_info(F("---- Result Webconfig ----"));
	debug_outln_info(F("WiFi: "), cfg::has_wifi);
	debug_outln_info(F("LoRa: "), cfg::has_lora);
	debug_outln_info(F("APPEUI: "), cfg::appeui);
	debug_outln_info(F("DEVEUI: "), cfg::deveui);
	debug_outln_info(F("APPKEY: "), cfg::appkey);
	debug_outln_info(F("WLANSSID: "), cfg::wlanssid);
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_info_bool(F("SDS: "), cfg::sds_read);
	debug_outln_info_bool(F("NPM: "), cfg::npm_read);
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_info_bool(F("SensorCommunity: "), cfg::send2dusti);
	debug_outln_info_bool(F("Madavi: "), cfg::send2madavi);
	debug_outln_info_bool(F("CSV: "), cfg::send2csv);
	debug_outln_info_bool(F("AirCarto: "), cfg::send2custom);
	debug_outln_info_bool(F("AtmoSud: "), cfg::send2custom2);
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	//debug_outln_info_bool(F("Display: "), cfg::has_display);
	debug_outln_info_bool(F("Display: "), cfg::has_ssd1306);
	debug_outln_info(F("Debug: "), String(cfg::debug));
	wificonfig_loop = false; //VOIR ICI
}

static void waitForWifiToConnect(int maxRetries)
{
	int retryCount = 0;
	while ((WiFi.status() != WL_CONNECTED) && (retryCount < maxRetries))
	{
		delay(500);
		debug_out(".", DEBUG_MIN_INFO);
		++retryCount;
	}
}

/*****************************************************************
 * WiFi auto connecting script                                   *
 *****************************************************************/

//static WiFiEventHandler disconnectEventHandler;

static void connectWifi()
{
	display_debug(F("Connecting to"), String(cfg::wlanssid));

	if (WiFi.getAutoConnect())
	{
		WiFi.setAutoConnect(false);
	}
	if (!WiFi.getAutoReconnect())
	{
		WiFi.setAutoReconnect(true);
	}

	// Use 13 channels for connect to known AP
	wifi_country_t wifi;
	wifi.policy = WIFI_COUNTRY_POLICY_MANUAL;
	strcpy(wifi.cc, INTL_LANG);
	wifi.nchan = 13;
	wifi.schan = 1;

	WiFi.mode(WIFI_STA);

	WiFi.setHostname(cfg::fs_ssid);

	WiFi.begin(cfg::wlanssid, cfg::wlanpwd); // Start WiFI

	debug_outln_info(FPSTR(DBG_TXT_CONNECTING_TO), cfg::wlanssid);

	waitForWifiToConnect(40);
	debug_outln_info(emptyString);
	if (WiFi.status() != WL_CONNECTED)
	{
		String fss(cfg::fs_ssid);
		display_debug(fss.substring(0, 16), fss.substring(16));

		wifi.policy = WIFI_COUNTRY_POLICY_AUTO;

		wifiConfig();
		if (WiFi.status() != WL_CONNECTED)
		{
			waitForWifiToConnect(20);
			debug_outln_info(emptyString);
		}
	}
	debug_outln_info(F("WiFi connected, IP is: "), WiFi.localIP().toString());
	last_signal_strength = WiFi.RSSI();

	if (MDNS.begin(cfg::fs_ssid))
	{
		MDNS.addService("http", "tcp", 80);
		MDNS.addServiceTxt("http", "tcp", "PATH", "/config");
	}
}

static WiFiClient *getNewLoggerWiFiClient(const LoggerEntry logger)
{

	WiFiClient *_client;
	if (loggerConfigs[logger].session)
	{
		_client = new WiFiClientSecure;
	}
	else
	{
		_client = new WiFiClient;
	}
	return _client;
}

/*****************************************************************
 * send data to rest api                                         *
 *****************************************************************/
static unsigned long sendData(const LoggerEntry logger, const String &data, const int pin, const char *host, const char *url)
{

	unsigned long start_send = millis();
	const __FlashStringHelper *contentType;
	int result = 0;

	String s_Host(FPSTR(host));
	String s_url(FPSTR(url));

	switch (logger)
	{
	default:
		contentType = FPSTR(TXT_CONTENT_TYPE_JSON);
		break;
	}

	std::unique_ptr<WiFiClient> client(getNewLoggerWiFiClient(logger));

	HTTPClient http;
	http.setTimeout(20 * 1000);
	http.setUserAgent(SOFTWARE_VERSION + '/' + esp_chipid);
	http.setReuse(false);
	bool send_success = false;
	if (logger == LoggerCustom && (*cfg::user_custom || *cfg::pwd_custom))
	{
		http.setAuthorization(cfg::user_custom, cfg::pwd_custom);
	}
	if (http.begin(*client, s_Host, loggerConfigs[logger].destport, s_url, !!loggerConfigs[logger].session))
	{
		http.addHeader(F("Content-Type"), contentType);
		http.addHeader(F("X-Sensor"), String(F(SENSOR_BASENAME)) + esp_chipid);
		//http.addHeader(F("X-MAC-ID"), String(F(SENSOR_BASENAME)) + esp_mac_id);
		if (pin)
		{
			http.addHeader(F("X-PIN"), String(pin));
		}

		result = http.POST(data);

		if (result >= HTTP_CODE_OK && result <= HTTP_CODE_ALREADY_REPORTED)
		{
			debug_outln_info(F("Succeeded - "), s_Host);
			send_success = true;
		}
		else if (result >= HTTP_CODE_BAD_REQUEST)
		{
			debug_outln_info(F("Request failed with error: "), String(result));
			debug_outln_info(F("Details:"), http.getString());
		}
		http.end();
	}
	else
	{
		debug_outln_info(F("Failed connecting to "), s_Host);
	}

	if (!send_success && result != 0)
	{
		loggerConfigs[logger].errors++;
		last_sendData_returncode = result;
	}

	return millis() - start_send;
}

/*****************************************************************
 * send single sensor data to sensor.community api                *
 *****************************************************************/
static unsigned long sendSensorCommunity(const String &data, const int pin, const __FlashStringHelper *sensorname, const char *replace_str)
{
	unsigned long sum_send_time = 0;

	if (cfg::send2dusti && data.length())
	{
		RESERVE_STRING(data_sensorcommunity, LARGE_STR);
		data_sensorcommunity = FPSTR(data_first_part);

		debug_outln_info(F("## Sending to sensor.community - "), sensorname);
		data_sensorcommunity += data;
		data_sensorcommunity.remove(data_sensorcommunity.length() - 1);
		data_sensorcommunity.replace(replace_str, emptyString);
		data_sensorcommunity += "]}";
		sum_send_time = sendData(LoggerSensorCommunity, data_sensorcommunity, pin, HOST_SENSORCOMMUNITY, URL_SENSORCOMMUNITY);
	}

	return sum_send_time;
}

/*****************************************************************
 * send data as csv to serial out                                *
 *****************************************************************/
static void send_csv(const String &data)
{
	DynamicJsonDocument json2data(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json2data, data);
	debug_outln_info(F("CSV Output: "), data);
	if (!err)
	{
		String headline = F("Timestamp_ms;");
		String valueline(act_milli);
		valueline += ';';
		for (JsonObject measurement : json2data[FPSTR(JSON_SENSOR_DATA_VALUES)].as<JsonArray>())
		{
			headline += measurement["value_type"].as<const char *>();
			headline += ';';
			valueline += measurement["value"].as<const char *>();
			valueline += ';';
		}
		static bool first_csv_line = true;
		if (first_csv_line)
		{
			if (headline.length() > 0)
			{
				headline.remove(headline.length() - 1);
			}
			Debug.println(headline);
			first_csv_line = false;
		}
		if (valueline.length() > 0)
		{
			valueline.remove(valueline.length() - 1);
		}
		Debug.println(valueline);
	}
	else
	{
		debug_outln_error(FPSTR(DBG_TXT_DATA_READ_FAILED));
	}
}

/*****************************************************************
 * read BMP280/BME280 sensor values                              *
 *****************************************************************/
static void fetchSensorBMX280(String &s)
{
	const char *const sensor_name = (bmx280.sensorID() == BME280_SENSOR_ID) ? SENSORS_BME280 : SENSORS_BMP280;
	debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(sensor_name));

	bmx280.takeForcedMeasurement();
	const auto t = bmx280.readTemperature();
	const auto p = bmx280.readPressure();
	const auto h = bmx280.readHumidity();
	if (isnan(t) || isnan(p))
	{
		last_value_BMX280_T = -128.0;
		last_value_BMX280_P = -1.0;
		last_value_BME280_H = -1.0;
		debug_outln_error(F("BMP/BME280 read failed"));
	}
	else
	{
		//last_value_BMX280_T = t + readCorrectionOffset(cfg::temp_correction);
		last_value_BMX280_T = t;
		last_value_BMX280_P = p;
		if (bmx280.sensorID() == BME280_SENSOR_ID)
		{
			add_Value2Json(s, F("BME280_temperature"), FPSTR(DBG_TXT_TEMPERATURE), last_value_BMX280_T);
			add_Value2Json(s, F("BME280_pressure"), FPSTR(DBG_TXT_PRESSURE), last_value_BMX280_P);
			last_value_BME280_H = h;
			add_Value2Json(s, F("BME280_humidity"), FPSTR(DBG_TXT_HUMIDITY), last_value_BME280_H);
		}
		else
		{
			add_Value2Json(s, F("BMP280_pressure"), FPSTR(DBG_TXT_PRESSURE), last_value_BMX280_P);
			add_Value2Json(s, F("BMP280_temperature"), FPSTR(DBG_TXT_TEMPERATURE), last_value_BMX280_T);
		}
	}
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(sensor_name));
}

/*****************************************************************
 * read SDS011 sensor values                                     *
 *****************************************************************/
static void fetchSensorSDS(String &s)
{
	if (cfg::sending_intervall_ms > (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS) &&
		msSince(starttime) < (cfg::sending_intervall_ms - (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS)))
	{
		if (is_SDS_running)
		{
			is_SDS_running = SDS_cmd(PmSensorCmd::Stop);
		}
	}
	else
	{
		if (!is_SDS_running)
		{
			is_SDS_running = SDS_cmd(PmSensorCmd::Start);
			SDS_waiting_for = SDS_REPLY_HDR;
		}

		while (serialSDS.available() >= SDS_waiting_for)
		{
			const uint8_t constexpr hdr_measurement[2] = {0xAA, 0xC0};
			uint8_t data[8];

			switch (SDS_waiting_for)
			{
			case SDS_REPLY_HDR:
				if (serialSDS.find(hdr_measurement, sizeof(hdr_measurement)))
					SDS_waiting_for = SDS_REPLY_BODY;
				break;
			case SDS_REPLY_BODY:
				debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(SENSORS_SDS011));
				if (serialSDS.readBytes(data, sizeof(data)) == sizeof(data) && SDS_checksum_valid(data))
				{
					uint32_t pm25_serial = data[0] | (data[1] << 8);
					uint32_t pm10_serial = data[2] | (data[3] << 8);

					if (msSince(starttime) > (cfg::sending_intervall_ms - READINGTIME_SDS_MS))
					{
						sds_pm10_sum += pm10_serial;
						sds_pm25_sum += pm25_serial;
						UPDATE_MIN_MAX(sds_pm10_min, sds_pm10_max, pm10_serial);
						UPDATE_MIN_MAX(sds_pm25_min, sds_pm25_max, pm25_serial);
						debug_outln_verbose(F("PM10 (sec.) : "), String(pm10_serial / 10.0f));
						debug_outln_verbose(F("PM2.5 (sec.): "), String(pm25_serial / 10.0f));
						sds_val_count++;
					}
				}
				debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(SENSORS_SDS011));
				SDS_waiting_for = SDS_REPLY_HDR;
				break;
			}
		}
	}
	if (send_now)
	{
		last_value_SDS_P1 = -1;
		last_value_SDS_P2 = -1;
		if (sds_val_count > 2)
		{
			sds_pm10_sum = sds_pm10_sum - sds_pm10_min - sds_pm10_max;
			sds_pm25_sum = sds_pm25_sum - sds_pm25_min - sds_pm25_max;
			sds_val_count = sds_val_count - 2;
		}
		if (sds_val_count > 0)
		{
			last_value_SDS_P1 = float(sds_pm10_sum) / (sds_val_count * 10.0f);
			last_value_SDS_P2 = float(sds_pm25_sum) / (sds_val_count * 10.0f);
			add_Value2Json(s, F("SDS_P1"), F("PM10:  "), last_value_SDS_P1);
			add_Value2Json(s, F("SDS_P2"), F("PM2.5: "), last_value_SDS_P2);
			debug_outln_info(FPSTR(DBG_TXT_SEP));
			if (sds_val_count < 3)
			{
				SDS_error_count++;
			}
		}
		else
		{
			SDS_error_count++;
		}
		sds_pm10_sum = 0;
		sds_pm25_sum = 0;
		sds_val_count = 0;
		sds_pm10_max = 0;
		sds_pm10_min = 20000;
		sds_pm25_max = 0;
		sds_pm25_min = 20000;
		if ((cfg::sending_intervall_ms > (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS)))
		{

			if (is_SDS_running)
			{
				is_SDS_running = SDS_cmd(PmSensorCmd::Stop);
			}
		}
	}
}

/*****************************************************************
 * read Tera Sensor Next PM sensor sensor values                 *
 *****************************************************************/
static void fetchSensorNPM(String &s)
{

	//debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(SENSORS_NPM));
	if (cfg::sending_intervall_ms > (WARMUPTIME_NPM_MS + READINGTIME_NPM_MS) && msSince(starttime) < (cfg::sending_intervall_ms - (WARMUPTIME_NPM_MS + READINGTIME_NPM_MS)))
	{
		if (is_NPM_running)
		{
			debug_outln_info(F("Change NPM to stop..."));
			is_NPM_running = NPM_start_stop();
		}
	}
	else
	{
		if (!is_NPM_running)
		{
			debug_outln_info(F("Change NPM to start..."));
			is_NPM_running = NPM_start_stop();
			NPM_waiting_for_16 = NPM_REPLY_HEADER_16;
		}

	//if (){}
		if (msSince(starttime) > (cfg::sending_intervall_ms - READINGTIME_NPM_MS)){ //DIMINUER LE READING TIME

		debug_outln_info(F("Concentration NPM..."));
		NPM_cmd(PmSensorCmd2::Concentration);
		while (!serialNPM.available())
		{
			debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
		}

		while (serialNPM.available() >= NPM_waiting_for_16)
		{
			const uint8_t constexpr header[2] = {0x81, 0x11};
			uint8_t state[1];
			uint8_t data[12];
			uint8_t checksum[1];
			uint8_t test[16];

			switch (NPM_waiting_for_16)
			{
			case NPM_REPLY_HEADER_16:
				if (serialNPM.find(header, sizeof(header)))
					NPM_waiting_for_16 = NPM_REPLY_STATE_16;
				break;
			case NPM_REPLY_STATE_16:
				serialNPM.readBytes(state, sizeof(state));
				NPM_state(state[0]);
				NPM_waiting_for_16 = NPM_REPLY_BODY_16;
				break;
			case NPM_REPLY_BODY_16:
				if (serialNPM.readBytes(data, sizeof(data)) == sizeof(data))
				{
					NPM_data_reader(data, 12);
					uint16_t N1_serial = word(data[0], data[1]);
					uint16_t N25_serial = word(data[2], data[3]);
					uint16_t N10_serial = word(data[4], data[5]);

					uint16_t pm1_serial = word(data[6], data[7]);
					uint16_t pm25_serial = word(data[8], data[9]);
					uint16_t pm10_serial = word(data[10], data[11]);

					//if (msSince(starttime) > (cfg::sending_intervall_ms - READINGTIME_NPM_MS)){
					

						debug_outln_verbose(F("PM1 (μg/m3) : "), String(pm1_serial / 10.0f));
						debug_outln_verbose(F("PM2.5 (μg/m3): "), String(pm25_serial / 10.0f));
						debug_outln_verbose(F("PM10 (μg/m3) : "), String(pm10_serial / 10.0f));

						debug_outln_verbose(F("PM1 (pcs/mL) : "), String(N1_serial));
						debug_outln_verbose(F("PM2.5 (pcs/mL): "), String(N25_serial));
						debug_outln_verbose(F("PM10 (pcs/mL) : "), String(N10_serial));

						npm_pm1_sum += pm1_serial;
						npm_pm25_sum += pm25_serial;
						npm_pm10_sum += pm10_serial;

						npm_pm1_sum_pcs += N1_serial;
						npm_pm25_sum_pcs += N25_serial;
						npm_pm10_sum_pcs += N10_serial;

						UPDATE_MIN_MAX(npm_pm1_min, npm_pm1_max, pm1_serial);
						UPDATE_MIN_MAX(npm_pm25_min, npm_pm25_max, pm25_serial);
						UPDATE_MIN_MAX(npm_pm10_min, npm_pm10_max, pm10_serial);

						UPDATE_MIN_MAX(npm_pm1_min_pcs, npm_pm1_max_pcs, N1_serial);
						UPDATE_MIN_MAX(npm_pm25_min_pcs, npm_pm25_max_pcs, N25_serial);
						UPDATE_MIN_MAX(npm_pm10_min_pcs, npm_pm10_max_pcs, N10_serial);

						debug_outln_info(F("Next PM Measure..."));
						npm_val_count++;
						debug_outln(String(npm_val_count), DEBUG_MAX_INFO);
					//}
				}
				NPM_waiting_for_16 = NPM_REPLY_CHECKSUM_16;
				break;
			case NPM_REPLY_CHECKSUM_16:
				serialNPM.readBytes(checksum, sizeof(checksum));
				memcpy(test, header, sizeof(header));
				memcpy(&test[sizeof(header)], state, sizeof(state));
				memcpy(&test[sizeof(header) + sizeof(state)], data, sizeof(data));
				memcpy(&test[sizeof(header) + sizeof(state) + sizeof(data)], checksum, sizeof(checksum));
				NPM_data_reader(test, 16);
				if (NPM_checksum_valid_16(test))
					debug_outln_info(F("Checksum OK..."));
				NPM_waiting_for_16 = NPM_REPLY_HEADER_16;
				break;
			}
		}
	}
	}

	if (send_now)
	{
		last_value_NPM_P0 = -1.0f;
		last_value_NPM_P1 = -1.0f;
		last_value_NPM_P2 = -1.0f;
		last_value_NPM_N0 = -1.0f;
		last_value_NPM_N1 = -1.0f;
		last_value_NPM_N2 = -1.0f;

		if (npm_val_count > 2)
		{
			npm_pm1_sum = npm_pm1_sum - npm_pm1_min - npm_pm1_max;
			npm_pm10_sum = npm_pm10_sum - npm_pm10_min - npm_pm10_max;
			npm_pm25_sum = npm_pm25_sum - npm_pm25_min - npm_pm25_max;
			npm_pm1_sum_pcs = npm_pm1_sum_pcs - npm_pm1_min_pcs - npm_pm1_max_pcs;
			npm_pm10_sum_pcs = npm_pm10_sum_pcs - npm_pm10_min_pcs - npm_pm10_max_pcs;
			npm_pm25_sum_pcs = npm_pm25_sum_pcs - npm_pm25_min_pcs - npm_pm25_max_pcs;
			npm_val_count = npm_val_count - 2;
		}
		if (npm_val_count > 0)
		{
			last_value_NPM_P0 = float(npm_pm1_sum) / (npm_val_count * 10.0f);
			last_value_NPM_P1 = float(npm_pm10_sum) / (npm_val_count * 10.0f);
			last_value_NPM_P2 = float(npm_pm25_sum) / (npm_val_count * 10.0f);

			last_value_NPM_N0 = float(npm_pm1_sum_pcs) / npm_val_count;
			last_value_NPM_N1 = float(npm_pm10_sum_pcs) / npm_val_count;
			last_value_NPM_N2 = float(npm_pm25_sum_pcs) / npm_val_count;

			add_Value2Json(s, F("NPM_P0"), F("PM1: "), last_value_NPM_P0);
			add_Value2Json(s, F("NPM_P1"), F("PM10:  "), last_value_NPM_P1);
			add_Value2Json(s, F("NPM_P2"), F("PM2.5: "), last_value_NPM_P2);

			add_Value2Json(s, F("NPM_N1"), F("NC1.0: "), last_value_NPM_N0);
			add_Value2Json(s, F("NPM_N10"), F("NC10:  "), last_value_NPM_N1);
			add_Value2Json(s, F("NPM_N25"), F("NC2.5: "), last_value_NPM_N2);

			debug_outln_info(FPSTR(DBG_TXT_SEP));
			if (npm_val_count < 3)
			{
				NPM_error_count++;
			}
		}
		else
		{
			NPM_error_count++;
		}

		npm_pm1_sum = 0;
		npm_pm10_sum = 0;
		npm_pm25_sum = 0;

		npm_val_count = 0;

		npm_pm1_max = 0;
		npm_pm1_min = 20000;
		npm_pm10_max = 0;
		npm_pm10_min = 20000;
		npm_pm25_max = 0;
		npm_pm25_min = 20000;

		npm_pm1_sum_pcs = 0;
		npm_pm10_sum_pcs = 0;
		npm_pm25_sum_pcs = 0;

		npm_pm1_max_pcs = 0;
		npm_pm1_min_pcs = 60000;
		npm_pm10_max_pcs = 0;
		npm_pm10_min_pcs = 60000;
		npm_pm25_max_pcs = 0;
		npm_pm25_min_pcs = 60000;

		if (cfg::sending_intervall_ms > (WARMUPTIME_NPM_MS + READINGTIME_NPM_MS))
		{
			if (is_NPM_running)
			{
				debug_outln_info(F("Change NPM to stop after measure..."));
				is_NPM_running = NPM_start_stop();
			}
		}
	}

	//debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(SENSORS_NPM));
}

/*****************************************************************
 * read GPS sensor values                                        *
 *****************************************************************/
static __noinline void fetchSensorGPS(String &s)
{
	debug_outln_verbose(FPSTR(DBG_TXT_START_READING), "GPS");

	if (gps.location.isUpdated())
	{
		if (gps.location.isValid())
		{
			last_value_GPS_lat = gps.location.lat();
			last_value_GPS_lon = gps.location.lng();
		}
		else
		{
			last_value_GPS_lat = -200;
			last_value_GPS_lon = -200;
			debug_outln_verbose(F("Lat/Lng INVALID"));
		}
		if (gps.altitude.isValid())
		{
			last_value_GPS_alt = gps.altitude.meters();
		}
		else
		{
			last_value_GPS_alt = -1000;
			debug_outln_verbose(F("Altitude INVALID"));
		}
		if (!gps.date.isValid())
		{
			debug_outln_verbose(F("Date INVALID"));
		}
		if (!gps.time.isValid())
		{
			debug_outln_verbose(F("Time: INVALID"));
		}
		if (gps.date.isValid() && gps.time.isValid())
		{
			char gps_datetime[37];
			snprintf_P(gps_datetime, sizeof(gps_datetime), PSTR("%04d-%02d-%02dT%02d:%02d:%02d.%03d"),
					   gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second(), gps.time.centisecond());
			last_value_GPS_timestamp = gps_datetime;
		}
		else
		{
			//define a default value
			last_value_GPS_timestamp = F("1970-01-01T00:00:00.000");
		}
	}

	if (send_now)
	{
		debug_outln_info(F("Lat: "), String(last_value_GPS_lat, 6));
		debug_outln_info(F("Lng: "), String(last_value_GPS_lon, 6));
		debug_outln_info(F("DateTime: "), last_value_GPS_timestamp);

		add_Value2Json(s, F("GPS_lat"), String(last_value_GPS_lat, 6));
		add_Value2Json(s, F("GPS_lon"), String(last_value_GPS_lon, 6));
		add_Value2Json(s, F("GPS_height"), F("Altitude: "), last_value_GPS_alt);
		add_Value2Json(s, F("GPS_timestamp"), last_value_GPS_timestamp);
		debug_outln_info(FPSTR(DBG_TXT_SEP));
	}

	if (count_sends > 0 && gps.charsProcessed() < 10)
	{
		debug_outln_error(F("No GPS data received: check wiring"));
		gps_init_failed = true;
	}

	debug_outln_verbose(FPSTR(DBG_TXT_END_READING), "GPS");
}

/*****************************************************************
 * display values                                                *
 *****************************************************************/
static void display_values()
{
	float t_value = -128.0;
	float h_value = -1.0;
	float p_value = -1.0;
	String t_sensor, h_sensor, p_sensor;
	float pm01_value = -1.0;
	float pm25_value = -1.0;
	float pm10_value = -1.0;
	String pm01_sensor;
	String pm10_sensor;
	String pm25_sensor;
	float nc010_value = -1.0;
	float nc025_value = -1.0;
	float nc100_value = -1.0;
	String la_sensor;
	double lat_value = -200.0;
	double lon_value = -200.0;
	double alt_value = -1000.0;
	String display_header;
	String display_lines[3] = {"", "", ""};
	uint8_t screen_count = 0;
	uint8_t screens[8];
	int line_count = 0;
	//debug_outln_info(F("output values to display..."));

	if (cfg::npm_read)
	{
		pm01_value = last_value_NPM_P0;
		pm10_value = last_value_NPM_P1;
		pm25_value = last_value_NPM_P2;
		pm01_sensor = FPSTR(SENSORS_NPM);
		pm10_sensor = FPSTR(SENSORS_NPM);
		pm25_sensor = FPSTR(SENSORS_NPM);
		nc010_value = last_value_NPM_N0;
		nc100_value = last_value_NPM_N1;
		nc025_value = last_value_NPM_N2;
	}

	if (cfg::sds_read)
	{
		pm10_sensor = FPSTR(SENSORS_SDS011);
		pm25_sensor = FPSTR(SENSORS_SDS011);
		pm10_value = last_value_SDS_P1;
		pm25_value = last_value_SDS_P2;
	}

	if (cfg::bmx280_read)
	{
		t_sensor = p_sensor = FPSTR(SENSORS_BMP280);
		t_value = last_value_BMX280_T;
		p_value = last_value_BMX280_P;
		if (bmx280.sensorID() == BME280_SENSOR_ID)
		{
			h_sensor = t_sensor = FPSTR(SENSORS_BME280);
			h_value = last_value_BME280_H;
		}
	}

	if (cfg::gps_read)
	{
		lat_value = last_value_GPS_lat;
		lon_value = last_value_GPS_lon;
		alt_value = last_value_GPS_alt;
	}
	if (cfg::sds_read)
	{
		screens[screen_count++] = 1;
	}
	if (cfg::npm_read)
	{
		screens[screen_count++] = 2;
	}
	if (cfg::bmx280_read)
	{
		screens[screen_count++] = 3;
	}

	if (cfg::gps_read)
	{
		screens[screen_count++] = 4;
	}
	if (cfg::display_wifi_info && cfg::has_wifi)
	{
		screens[screen_count++] = 5; // Wifi info
	}
	if (cfg::display_device_info)
	{
		screens[screen_count++] = 6; // chipID, firmware and count of measurements
	}
	if (cfg::display_lora_info && cfg::has_lora)
	{
		screens[screen_count++] = 7; // Lora info
	}

	// update size of "screens" when adding more screens!
	//if (cfg::has_display)
	if (cfg::has_ssd1306)
	{
		switch (screens[next_display_count % screen_count])
		{
		case 1:
			display_header = FPSTR(SENSORS_SDS011);
			display_lines[0] = std::move(tmpl(F("PM2.5: {v} µg/m³"), check_display_value(pm25_value, -1, 1, 6)));
			display_lines[1] = std::move(tmpl(F("PM10: {v} µg/m³"), check_display_value(pm10_value, -1, 1, 6)));
			display_lines[2] = emptyString;
			break;
		case 2:
			display_header = FPSTR(SENSORS_NPM);
			display_lines[0] = std::move(tmpl(F("PM1: {v} µg/m³"), check_display_value(pm01_value, -1, 1, 6)));
			display_lines[1] = std::move(tmpl(F("PM2.5: {v} µg/m³"), check_display_value(pm25_value, -1, 1, 6)));
			display_lines[2] = std::move(tmpl(F("PM10: {v} µg/m³"), check_display_value(pm10_value, -1, 1, 6)));
			//display_lines[3] = "NC: " + check_display_value(nc010_value, -1, 0, 3) + " " + check_display_value(nc025_value, -1, 0, 3) + " " + check_display_value(nc100_value, -1, 0, 3);
			break;
		case 3:
			display_header = t_sensor;
			// if (h_sensor && t_sensor != h_sensor)
			// {
			// 	display_header += " / " + h_sensor;
			// }
			// if ((h_sensor && p_sensor && (h_sensor != p_sensor)) || (h_sensor == "" && p_sensor && (t_sensor != p_sensor)))
			// {
			// 	display_header += " / " + p_sensor;
			// }
			if (t_sensor != "")
			{
				display_lines[line_count] = "Temp.: ";
				display_lines[line_count] += check_display_value(t_value, -128, 1, 6);
				display_lines[line_count++] += " °C";
			}
			if (h_sensor != "")
			{
				display_lines[line_count] = "Hum.:  ";
				display_lines[line_count] += check_display_value(h_value, -1, 1, 6);
				display_lines[line_count++] += " %";
			}
			if (p_sensor != "")
			{
				display_lines[line_count] = "Pres.: ";
				display_lines[line_count] += check_display_value(p_value / 100, (-1 / 100.0), 1, 6);
				display_lines[line_count++] += " hPa";
			}
			while (line_count < 3)
			{
				display_lines[line_count++] = emptyString;
			}
			break;
		case 4:
			display_header = "NEO6M";
			display_lines[0] = "Lat: ";
			display_lines[0] += check_display_value(lat_value, -200.0, 6, 10);
			display_lines[1] = "Lon: ";
			display_lines[1] += check_display_value(lon_value, -200.0, 6, 10);
			display_lines[2] = "Alt: ";
			display_lines[2] += check_display_value(alt_value, -1000.0, 2, 10);
			break;
		case 5:
			display_header = F("Wifi info");
			display_lines[0] = "IP: ";
			display_lines[0] += WiFi.localIP().toString();
			display_lines[1] = "SSID: ";
			display_lines[1] += WiFi.SSID();
			display_lines[2] = std::move(tmpl(F("Signal: {v} %"), String(calcWiFiSignalQuality(last_signal_strength))));
			break;
		case 6:
			display_header = F("Device Info");
			display_lines[0] = "ID: ";
			display_lines[0] += esp_chipid;
			display_lines[1] = "FW: ";
			display_lines[1] += SOFTWARE_VERSION;
			display_lines[2] = F("Measurements: ");
			display_lines[2] += String(count_sends);
			break;
		case 7:
			display_header = F("LoRaWAN Info");
			display_lines[0] = "APPEUI: ";
			display_lines[0] += cfg::appeui;
			display_lines[1] = "DEVEUI: ";
			display_lines[1] += cfg::deveui;
			display_lines[2] = "APPKEY: ";
			display_lines[2] += cfg::appkey;
			break;
		}

		if (oled_ssd1306)
		{
			oled_ssd1306->clear();
			oled_ssd1306->displayOn();
			oled_ssd1306->setTextAlignment(TEXT_ALIGN_CENTER);
			oled_ssd1306->drawString(64, 1, display_header);
			oled_ssd1306->setTextAlignment(TEXT_ALIGN_LEFT);
			oled_ssd1306->drawString(0, 16, display_lines[0]);
			oled_ssd1306->drawString(0, 28, display_lines[1]);
			oled_ssd1306->drawString(0, 40, display_lines[2]);
			oled_ssd1306->setTextAlignment(TEXT_ALIGN_CENTER);
			oled_ssd1306->drawString(64, 52, displayGenerateFooter(screen_count));
			oled_ssd1306->display();
		}
	}

	// ----5----0----5----0
	// PM10/2.5: 1999/999
	// T/H: -10.0°C/100.0%
	// T/P: -10.0°C/1000hPa

	yield();
	next_display_count++;
}

/*****************************************************************
 * Init LCD/OLED display                                         *
 *****************************************************************/
static void init_display()
{
	//if (cfg::has_display && cfg::has_ssd1306)
	if (cfg::has_ssd1306)

	{

#if defined(ARDUINO_TTGO_LoRa32_v21new)
		oled_ssd1306 = new SSD1306Wire(0x3c, I2C_PIN_SDA, I2C_PIN_SCL);
#endif

#if defined(ARDUINO_HELTEC_WIFI_LORA_32_V2)
		oled_ssd1306 = new SSD1306Wire(0x3c, I2C_SCREEN_SDA, I2C_SCREEN_SCL);
#endif

		oled_ssd1306->init();
		oled_ssd1306->flipScreenVertically(); //ENLEVER ???
		oled_ssd1306->clear();
		oled_ssd1306->displayOn();
		oled_ssd1306->setTextAlignment(TEXT_ALIGN_CENTER);
		oled_ssd1306->drawString(64, 1, "START");
		oled_ssd1306->display();

		// reset back to 100k as the OLEDDisplay initialization is
		// modifying the I2C speed to 400k, which overwhelms some of the
		// sensors.
		Wire.setClock(100000);
		//Wire.setClockStretchLimit(150000);
	}
}
/*****************************************************************
 * Init BMP280/BME280                                            *
 *****************************************************************/
static bool initBMX280(char addr)
{
	debug_out(String(F("Trying BMx280 sensor on ")) + String(addr, HEX), DEBUG_MIN_INFO);

	if (bmx280.begin(addr))
	{
		debug_outln_info(FPSTR(DBG_TXT_FOUND));
		bmx280.setSampling(
			BMX280::MODE_FORCED,
			BMX280::SAMPLING_X1,
			BMX280::SAMPLING_X1,
			BMX280::SAMPLING_X1);
		return true;
	}
	else
	{
		debug_outln_info(FPSTR(DBG_TXT_NOT_FOUND));
		return false;
	}
}

/*****************************************************************
   Functions
 *****************************************************************/

static void powerOnTestSensors()
{

	if (cfg::sds_read)
	{
		debug_outln_info(F("Read SDS...: "), SDS_version_date());
		SDS_cmd(PmSensorCmd::ContinuousMode);
		delay(100);
		debug_outln_info(F("Stopping SDS011..."));
		is_SDS_running = SDS_cmd(PmSensorCmd::Stop);
	}

	if (cfg::npm_read)
	{
		uint8_t test_state;
		delay(15000); //wait a bit to be sure Next PM is ready to receive instructions.
		test_state = NPM_get_state();
		if (test_state == 0x00){
			debug_outln_info(F("NPM already started..."));
		}else if (test_state == 0x01){
			debug_outln_info(F("Force start NPM...")); // to read the firmware version
			is_NPM_running = NPM_start_stop();
		}else {
			if (bitRead(test_state, 1) == 1){
				debug_outln_info(F("Degraded state"));
			}else{
				debug_outln_info(F("Default state"));
			}
			if (bitRead(test_state, 2) == 1){
				debug_outln_info(F("Not ready"));
			}
			if (bitRead(test_state, 3) == 1){
				debug_outln_info(F("Heat error"));
			}
			if (bitRead(test_state, 4) == 1){
				debug_outln_info(F("T/RH error"));
			}
			if (bitRead(test_state, 5) == 1){
				debug_outln_info(F("Fan error"));

				// if (bitRead(test_state, 0) == 1){
				// 	debug_outln_info(F("Force start NPM..."));
				// 	is_NPM_running = NPM_start_stop();
				// 	delay(5000);
				// }
				// NPM_fan_speed();
				// delay(5000);
			}
			if (bitRead(test_state, 6) == 1){
				debug_outln_info(F("Memory error"));
			}
			if (bitRead(test_state, 7) == 1){
				debug_outln_info(F("Laser error"));
			}
			if (bitRead(test_state, 0) == 0){
				debug_outln_info(F("NPM already started..."));
			}else{
				//if(!is_NPM_running){
				debug_outln_info(F("Force start NPM..."));
				is_NPM_running = NPM_start_stop();
			//}
			}
		}

		delay(1000);
		NPM_version_date();
		delay(5000);
		is_NPM_running = NPM_start_stop();
	}

	if (cfg::bmx280_read)
	{
		debug_outln_info(F("Read BMxE280..."));
		if (!initBMX280(0x76) && !initBMX280(0x77))
		{
			debug_outln_error(F("Check BMx280 wiring"));
			bmx280_init_failed = true;
		}
	}
}

static void logEnabledAPIs()
{
	debug_outln_info(F("Send to :"));
	if (cfg::send2dusti)
	{
		debug_outln_info(F("sensor.community"));
	}

	if (cfg::send2madavi)
	{
		debug_outln_info(F("Madavi.de"));
	}

	if (cfg::send2csv)
	{
		debug_outln_info(F("Serial as CSV"));
	}

	if (cfg::send2custom)
	{
		debug_outln_info(F("AirCarto API"));
	}
	if (cfg::send2custom2)
	{
		debug_outln_info(F("Atmosud API"));
	}
}

static void logEnabledDisplays()
{
	//if (cfg::has_display || cfg::has_ssd1306)
	if (cfg::has_ssd1306)

	{
		debug_outln_info(F("Show on OLED..."));
	}
}

static void setupNetworkTime()
{
	// server name ptrs must be persisted after the call to configTime because internally
	// the pointers are stored see implementation of lwip sntp_setservername()
	static char ntpServer1[18], ntpServer2[18];
	strcpy_P(ntpServer1, NTP_SERVER_1);
	strcpy_P(ntpServer2, NTP_SERVER_2);
	configTime(0, 0, ntpServer1, ntpServer2);
}

static unsigned long sendDataToOptionalApis(const String &data)
{
	unsigned long sum_send_time = 0;

	if (cfg::send2madavi)
	{
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("madavi.de: "));
		sum_send_time += sendData(LoggerMadavi, data, 0, HOST_MADAVI, URL_MADAVI);
	}

	if (cfg::send2custom)
	{
		String data_to_send = data;
		data_to_send.remove(0, 1);
		String data_4_custom(F("{\"nebuloid\": \""));
		data_4_custom += esp_chipid;
		data_4_custom += "\", ";
		data_4_custom += data_to_send;
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("aircarto api: "));
		sum_send_time += sendData(LoggerCustom, data_4_custom, 0, cfg::host_custom, cfg::url_custom);
	}

	if (cfg::send2custom2)
	{
		String data_to_send = data;
		data_to_send.remove(0, 1);
		String data_4_custom(F("{\"nebuloid\": \""));
		data_4_custom += esp_chipid;
		data_4_custom += "\", ";
		data_4_custom += data_to_send;
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("atmosud api: "));
		sum_send_time += sendData(LoggerCustom2, data_4_custom, 0, cfg::host_custom2, cfg::url_custom2);
	}

	if (cfg::send2csv)
	{
		debug_outln_info(F("## Sending as csv: "));
		send_csv(data);
	}

	return sum_send_time;
}




















/*****************************************************************
 * Helium LoRaWAN                  *
 *****************************************************************/

// /* OTAA para*/ EXEMPLE
// uint8_t DevEui[] = {0x60, 0x81, 0xF9, 0x7B, 0x1A, 0x5B, 0x67, 0x63};
// uint8_t AppEui[] = {0x60, 0x81, 0xF9, 0x22, 0x43, 0x9D, 0x24, 0xD9};
// uint8_t AppKey[] = {0xCA, 0x22, 0x28, 0xAE, 0x0C, 0xC8, 0x6C, 0x6E, 0x31, 0x77, 0xA9, 0x8D, 0xCD, 0xF0, 0xDC, 0xBB};

//ATTENTION ON PREND LE LSB

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes.

//6081F97B1A5B6763 A RETOURNER ET CONVERTIR EN HEX

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply).

static u1_t PROGMEM appeui_hex[8] = {};
static u1_t PROGMEM deveui_hex[8] = {};

// DANS LE MEME ORDRE
static u1_t PROGMEM appkey_hex[16] = {};

//DEPLACER LES MEMCPY!!!

void os_getArtEui(u1_t *buf) { memcpy_P(buf, appeui_hex, 8); }

void os_getDevEui(u1_t *buf) { memcpy_P(buf, deveui_hex, 8); }

void os_getDevKey(u1_t *buf) { memcpy_P(buf, appkey_hex, 16); }

//DEFINIR LA SIZE AJOUTER PM1!!!!!!!!!!!!!!!
static uint8_t datalora_sds[21] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t datalora_npm[25] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = cfg::sending_intervall_ms; // Replaced with cfg::time_send

// //For Generic ESP32 with Generic Lora
// #if defined(ESP32) and not defined(ARDUINO_HELTEC_WIFI_LORA_32_V2) and not defined(ARDUINO_TTGO_LoRa32_v21new)
// // Pin mapping
// const lmic_pinmap lmic_pins = {
// 	.nss = 15,
// 	.rxtx = LMIC_UNUSED_PIN,
// 	.rst = LMIC_UNUSED_PIN,
// 	.dio = {4, 5, LMIC_UNUSED_PIN},
// };
// #endif

// #if defined(ARDUINO_HELTEC_WIFI_LORA_32_V2)
// 			const lmic_pinmap lmic_pins = {
//     		.nss = 18,
//     		.rxtx = LMIC_UNUSED_PIN,
//     		.rst = 14,
//     		.dio = {26, 35, 34}
// 			};

// 			//const lmic_pinmap *pPinMap = &lmic_pins;

// 			#endif

// 			#if defined(ARDUINO_TTGO_LoRa32_v21new)
			
			

// 			#endif

// //OU AU DESSUS?
// 			//For Generic ESP32 with Generic Lora
// 			#if defined(ESP32) and not defined(ARDUINO_HELTEC_WIFI_LORA_32_V2) and not defined(ARDUINO_TTGO_LoRa32_v21new)
// 			// Pin mapping
// 			const lmic_pinmap lmic_pins = {
// 				.nss = 15,
// 				.rxtx = LMIC_UNUSED_PIN,
// 				.rst = LMIC_UNUSED_PIN,
// 				.dio = {4, 5, LMIC_UNUSED_PIN},
// 			};

// 			//const lmic_pinmap *pPinMap = &lmic_pins;

// 			#endif

void ToByteArray()
{
	String appeui_str = cfg::appeui;
	String deveui_str = cfg::deveui;
	String appkey_str = cfg::appkey;
	//  Debug.println(appeui_str);
	//  Debug.println(deveui_str);
	//  Debug.println(appkey_str);

	int j = 1;
	int k = 1;
	int l = 0;

//6081F97B1A5B6763

	for (unsigned int i = 0; i < appeui_str.length(); i += 2)
	{
		String byteString = appeui_str.substring(i, i + 2); //ICI? 1
		 //Debug.println(byteString);
		byte byte = (char)strtol(byteString.c_str(), NULL, 16);
		  //Debug.println(byte,HEX);
		appeui_hex[(appeui_str.length() / 2) - j] = byte; //reverse
		j += 1;
	}

//6081F922439D24D9
	for (unsigned int i = 0; i < deveui_str.length(); i += 2)
	{
		String byteString = deveui_str.substring(i, i + 2);
		//  Debug.println(byteString);
		byte byte = (char)strtol(byteString.c_str(), NULL, 16);
		//  Debug.println(byte, HEX);
		deveui_hex[(deveui_str.length() / 2) - k] = byte; //reverse
		k += 1;
	}
//CA2228AE0CC86C6E3177A98DCDF0DCBB

	for (unsigned int i = 0; i < appkey_str.length(); i += 2)
	{
		String byteString = appkey_str.substring(i, i + 2);
		//  Debug.println(byteString);
		byte byte = (char)strtol(byteString.c_str(), NULL, 16);
		//  Debug.println(byte, HEX);
		//appkey_hex[(appkey_str.length() / 2) - 1 - l] = byte; // reverse
		appkey_hex[l] = byte; //not reverse
		l += 1;
	}
}

void printHex2(unsigned v)
{
	v &= 0xff;
	if (v < 16)
		Serial.print('0');
	//debug_outln_info(F("\nChipId: "), esp_chipid);
	Debug.print(v, HEX);
}

void do_send(osjob_t *j)
{
	// Check if there is not a current TX/RX job running
	if (LMIC.opmode & OP_TXRXPEND)
	{
		Debug.println(F("OP_TXRXPEND, not sending"));
	}
	else
	{
		// Prepare upstream data transmission at the next possible time.

		if (cfg::sds_read)
		{
			LMIC_setTxData2(1, datalora_sds, sizeof(datalora_sds) - 1, 0);
		}

		if (cfg::npm_read)
		{
			LMIC_setTxData2(1, datalora_npm, sizeof(datalora_npm) - 1, 0);
		}

		// u1_t port is the FPort used for the transmission. Default is 1.
		// You can send different kind of data using different FPorts,
		// so the payload decoder can extract the information depending on the port used.

		Debug.println(F("Packet queued"));
	}
	// Next TX is scheduled after TX_COMPLETE event.
}

void onEvent(ev_t ev)
{
	Debug.print(os_getTime());
	Debug.print(": ");
	switch (ev)
	{
	case EV_SCAN_TIMEOUT:
		Debug.println(F("EV_SCAN_TIMEOUT"));
		break;
	case EV_BEACON_FOUND:
		Debug.println(F("EV_BEACON_FOUND"));
		break;
	case EV_BEACON_MISSED:
		Debug.println(F("EV_BEACON_MISSED"));
		break;
	case EV_BEACON_TRACKED:
		Debug.println(F("EV_BEACON_TRACKED"));
		break;
	case EV_JOINING:
		Debug.println(F("EV_JOINING"));
		break;
	case EV_JOINED:
		Debug.println(F("EV_JOINED"));
		{
			u4_t netid = 0;
			devaddr_t devaddr = 0;
			u1_t nwkKey[16];
			u1_t artKey[16];
			LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
			Debug.print("netid: ");
			Debug.println(netid, DEC);
			Debug.print("devaddr: ");
			Debug.println(devaddr, HEX);
			Debug.print("AppSKey: ");
			for (size_t i = 0; i < sizeof(artKey); ++i)
			{
				if (i != 0)
					Debug.print("-");
				printHex2(artKey[i]);
			}
			Debug.println("");
			Debug.print("NwkSKey: ");
			for (size_t i = 0; i < sizeof(nwkKey); ++i)
			{
				if (i != 0)
					Debug.print("-");
				printHex2(nwkKey[i]);
			}
			Debug.println();
		}
		// Disable link check validation (automatically enabled
		// during join, but because slow data rates change max TX
		// size, we don't use it in this example.
		LMIC_setLinkCheckMode(0);
		break;
	/*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_RFU1:
        ||     Serial.println(F("EV_RFU1"));
        ||     break;
        */
	case EV_JOIN_FAILED:
		Debug.println(F("EV_JOIN_FAILED"));
		break;
	case EV_REJOIN_FAILED:
		Debug.println(F("EV_REJOIN_FAILED"));
		break;
		break;
	case EV_TXCOMPLETE:
		Debug.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
		if (LMIC.txrxFlags & TXRX_ACK)
			Debug.println(F("Received ack"));
		if (LMIC.dataLen)
		{
			Debug.println(F("Received "));
			Debug.println(LMIC.dataLen);
			Debug.println(F(" bytes of payload"));
		}
		// Schedule next transmission
		os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
		break;
	case EV_LOST_TSYNC:
		Debug.println(F("EV_LOST_TSYNC"));
		break;
	case EV_RESET:
		Debug.println(F("EV_RESET"));
		break;
	case EV_RXCOMPLETE:
		// data received in ping slot
		Debug.println(F("EV_RXCOMPLETE"));
		break;
	case EV_LINK_DEAD:
		Debug.println(F("EV_LINK_DEAD"));
		break;
	case EV_LINK_ALIVE:
		Debug.println(F("EV_LINK_ALIVE"));
		break;
	/*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_SCAN_FOUND:
        ||    Serial.println(F("EV_SCAN_FOUND"));
        ||    break;
        */
	case EV_TXSTART:
		Debug.println(F("EV_TXSTART"));
		break;
	case EV_TXCANCELED:
		Debug.println(F("EV_TXCANCELED"));
		break;
	case EV_RXSTART:
		/* do not print anything -- it wrecks timing */
		break;
	case EV_JOIN_TXCOMPLETE:
		Debug.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
		break;

	default:
		Debug.print(F("Unknown event: "));
		Debug.println((unsigned)ev);
		break;
	}
}

static void prepareTxFrame()
{

	union float_2_byte
	{
		float temp_float;
		byte temp_byte[4];
	} u;

	if (cfg::sds_read)
	{
		u.temp_float = last_value_SDS_P1;

		datalora_sds[0] = u.temp_byte[0];
		datalora_sds[1] = u.temp_byte[1];
		datalora_sds[2] = u.temp_byte[2];
		datalora_sds[3] = u.temp_byte[3];

		u.temp_float = last_value_SDS_P2;

		datalora_sds[4] = u.temp_byte[0];
		datalora_sds[5] = u.temp_byte[1];
		datalora_sds[6] = u.temp_byte[2];
		datalora_sds[7] = u.temp_byte[3];

		u.temp_float = last_value_BMX280_T;

		datalora_sds[8] = u.temp_byte[0];
		datalora_sds[9] = u.temp_byte[1];
		datalora_sds[10] = u.temp_byte[2];
		datalora_sds[11] = u.temp_byte[3];

		u.temp_float = last_value_BMX280_P;

		datalora_sds[12] = u.temp_byte[0];
		datalora_sds[13] = u.temp_byte[1];
		datalora_sds[14] = u.temp_byte[2];
		datalora_sds[15] = u.temp_byte[3];

		u.temp_float = last_value_BME280_H;

		datalora_sds[16] = u.temp_byte[0];
		datalora_sds[17] = u.temp_byte[1];
		datalora_sds[18] = u.temp_byte[2];
		datalora_sds[19] = u.temp_byte[3];

		Debug.printf("HEX values:\n");
		for (int i = 0; i < 20; i++)
		{
			Debug.printf(" %02x", datalora_sds[i]);
			if (i == 19)
			{
				Debug.printf("\n");
			}
		}
	}
	if (cfg::npm_read)
	{
		u.temp_float = last_value_NPM_P0;

		datalora_npm[0] = u.temp_byte[0];
		datalora_npm[1] = u.temp_byte[1];
		datalora_npm[2] = u.temp_byte[2];
		datalora_npm[3] = u.temp_byte[3];

		u.temp_float = last_value_NPM_P1;

		datalora_npm[4] = u.temp_byte[0];
		datalora_npm[5] = u.temp_byte[1];
		datalora_npm[6] = u.temp_byte[2];
		datalora_npm[7] = u.temp_byte[3];

		u.temp_float = last_value_NPM_P2;

		datalora_npm[8] = u.temp_byte[0];
		datalora_npm[9] = u.temp_byte[1];
		datalora_npm[10] = u.temp_byte[2];
		datalora_npm[11] = u.temp_byte[3];

		u.temp_float = last_value_BMX280_T;

		datalora_npm[12] = u.temp_byte[0];
		datalora_npm[13] = u.temp_byte[1];
		datalora_npm[14] = u.temp_byte[2];
		datalora_npm[15] = u.temp_byte[3];

		u.temp_float = last_value_BMX280_P;

		datalora_npm[16] = u.temp_byte[0];
		datalora_npm[17] = u.temp_byte[1];
		datalora_npm[18] = u.temp_byte[2];
		datalora_npm[19] = u.temp_byte[3];

		u.temp_float = last_value_BME280_H;

		datalora_npm[20] = u.temp_byte[0];
		datalora_npm[21] = u.temp_byte[1];
		datalora_npm[22] = u.temp_byte[2];
		datalora_npm[23] = u.temp_byte[3];

		Debug.printf("HEX values:\n");
		for (int i = 0; i < 24; i++)
		{
			Debug.printf(" %02x", datalora_npm[i]);
			if (i == 23)
			{
				Debug.printf("\n");
			}
		}
	}
}

/*****************************************************************
 * The Setup                                                     *
 *****************************************************************/

void setup(void)
{

	Debug.begin(9600); // Output to Serial at 9600 baud
					   //----------------------------------------------

	// uint64_t chipid_num;
	// chipid_num = ESP.getEfuseMac();
	// Debug.printf("ESP32ChipID=%04X", (uint16_t)(chipid_num >> 32));	 //print High 2 bytes
	// Debug.printf("%08X\n", (uint32_t)chipid_num);					 //print Low 4bytes.
	esp_chipid = String((uint16_t)(ESP.getEfuseMac() >> 32), HEX); // for esp32
	esp_chipid += String((uint32_t)ESP.getEfuseMac(), HEX);
	esp_chipid.toUpperCase();
	cfg::initNonTrivials(esp_chipid.c_str());
	WiFi.persistent(false);

	debug_outln_info(F("Nebulo: " SOFTWARE_VERSION_STR "/"), String(CURRENT_LANG));
	//----------------------------------------------

	init_config();

#if defined(ESP32) and not defined(ARDUINO_HELTEC_WIFI_LORA_32_V2) and not defined(ARDUINO_TTGO_LoRa32_v21new)
	Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL);
#endif

#if defined(ARDUINO_TTGO_LoRa32_v21new)
	Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL);
#endif

	// #if defined(ARDUINO_HELTEC_WIFI_LORA_32_V2)
	//Heltec.begin(true /*DisplayEnable Enable*/, true /*LoRa Disable*/, true /*Serial Enable*/, true /*PABOOST Enable*/, 470E6 /**/);
	// if (cfg::has_lora)
	// {
	// 	Heltec.begin(true, true, false); // Serial normal
	// }
	// else
	// {
	// 	Heltec.begin(true, false, false); // Serial normal
	// }

	// delay(2000);
	// oled_ssd1306->clear();
	// oled_ssd1306->drawString(0, 0, "NEBULO STARTS");
	// oled_ssd1306->display();
	// delay(2000);

#if defined(ARDUINO_HELTEC_WIFI_LORA_32_V2)
	pinMode(OLED_RESET, OUTPUT);
	digitalWrite(OLED_RESET, LOW); // set GPIO16 low to reset OLED
	delay(50);
	digitalWrite(OLED_RESET, HIGH); // while OLED is running, must set GPIO16 in high、
	Wire.begin(I2C_SCREEN_SDA, I2C_SCREEN_SCL);
	Wire1.begin(I2C_PIN_SDA, I2C_PIN_SCL);
#endif

	if (cfg::npm_read)
	{
		serialNPM.begin(115200, SERIAL_8E1, PM_SERIAL_RX, PM_SERIAL_TX);
		Debug.println("Read Next PM... serialNPM 115200 8E1");
		serialNPM.setTimeout(400);
	}
	else
	{
		serialSDS.begin(9600, SERIAL_8N1, PM_SERIAL_RX, PM_SERIAL_TX);
		Debug.println("No Next PM... serialSDS 9600 8N1");
		serialSDS.setTimeout((4 * 12 * 1000) / 9600);
	}

	init_display();

	// ToByteArray();

	// Debug.printf("APPEUI:\n");
	// for (int i = 0; i < 8; i++)
	// {
	// 	Debug.printf(" %02x", appeui_hex[i]);
	// 	if (i == 7)
	// 	{
	// 		Debug.printf("\n");
	// 	}
	// }

	// Debug.printf("DEVEUI:\n");
	// for (int i = 0; i < 8; i++)
	// {
	// 	Debug.printf(" %02x", deveui_hex[i]);
	// 	if (i == 7)
	// 	{
	// 		Debug.printf("\n");
	// 	}
	// }

	// Debug.printf("APPKEY:\n");
	// for (int i = 0; i < 16; i++)
	// {
	// 	Debug.printf(" %02x", appkey_hex[i]);
	// 	if (i == 15)
	// 	{
	// 		Debug.printf("\n");
	// 	}
	// }

	debug_outln_info(F("\nChipId: "), esp_chipid);
	//debug_outln_info(F("\nMAC Id: "), esp_mac_id);

	if (cfg::gps_read)
	{
		serialGPS->begin(9600, SERIAL_8N1, GPS_SERIAL_RX, GPS_SERIAL_TX);
		debug_outln_info(F("Read GPS..."));
		disable_unneeded_nmea();
	}

	// always start the Webserver on void setup to get access to the sensor

	if (cfg::has_wifi)
	{
		setupNetworkTime();
	}

	connectWifi();
	setup_webserver();
	createLoggerConfigs();
	logEnabledAPIs();
	powerOnTestSensors();
	logEnabledDisplays();

	delay(50);

	starttime = millis(); // store the start time
	last_update_attempt = time_point_device_start_ms = starttime;

	if (cfg::npm_read)
	{
		last_display_millis = starttime_NPM = starttime;
	}
	else
	{
		last_display_millis = starttime_SDS = starttime;
	}

	if (cfg::has_lora)
	{

		ToByteArray();

		Debug.printf("APPEUI:\n");
		for (int i = 0; i < 8; i++)
		{
			Debug.printf(" %02x", appeui_hex[i]);
			if (i == 7)
			{
				Debug.printf("\n");
			}
		}

		Debug.printf("DEVEUI:\n");
		for (int i = 0; i < 8; i++)
		{
			Debug.printf(" %02x", deveui_hex[i]);
			if (i == 7)
			{
				Debug.printf("\n");
			}
		}

		Debug.printf("APPKEY:\n");
		for (int i = 0; i < 16; i++)
		{
			Debug.printf(" %02x", appkey_hex[i]);
			if (i == 15)
			{
				Debug.printf("\n");
			}
		}

		if (!strcmp(cfg::appeui, "0000000000000000") && !strcmp(cfg::deveui, "0000000000000000") && !strcmp(cfg::appkey, "00000000000000000000000000000000"))
		{

			#if defined(ARDUINO_HELTEC_WIFI_LORA_32_V2) or defined(ARDUINO_TTGO_LoRa32_v21new)
			
			
			//const lmic_pinmap *pPinMap = Arduino_LMIC::GetPinmap_ThisBoard();
			auto* pinMap = Arduino_LMIC::GetPinmap_ThisBoard();





			#endif

		



			// if (pPinMap == nullptr) {
       
            // Debug.println(F("Board not known to library; add pinmap or update getconfig_thisboard.cpp"));
        
    		// 	}

			//Debug.println(pPinMap);
			
			//os_init_ex(pPinMap);
			os_init_ex(pinMap);
			// Reset the MAC state. Session and pending data transfers will be discarded.

 			//os_init();


			LMIC_reset();
			// allow much more clock error than the X/1000 default. See:
			// https://github.com/mcci-catena/arduino-lorawan/issues/74#issuecomment-462171974
			// https://github.com/mcci-catena/arduino-lmic/commit/42da75b56#diff-16d75524a9920f5d043fe731a27cf85aL633
			// the X/1000 means an error rate of 0.1%; the above issue discusses using values up to 10%.
			// so, values from 10 (10% error, the most lax) to 1000 (0.1% error, the most strict) can be used.
			LMIC_setClockError(1 * MAX_CLOCK_ERROR / 40);

			LMIC_setLinkCheckMode(0);
			LMIC_setDrTxpow(DR_SF7, 14); //BONNE OPTION????

			//Set the data rate to Spreading Factor 7.  This is the fastest supported rate for 125 kHz channels, and it
			// minimizes air time and battery power. Set the transmission power to 14 dBi (25 mW).

			// Start job (sending automatically starts OTAA too)
			do_send(&sendjob); 
			
			
			
			//ATTENTION PREMIER START!!!!!
		}
	}
}

/*****************************************************************
 * And action                                                    *
 *****************************************************************/
void loop(void)
{
	String result_PPD, result_SDS, result_PMS, result_HPM, result_NPM;
	String result_GPS, result_DNMS;

	unsigned sum_send_time = 0;

	act_micro = micros();
	act_milli = millis();
	send_now = msSince(starttime) > cfg::sending_intervall_ms;

	// Wait at least 30s for each NTP server to sync

	// ATTENTION SNTP SI LORA

	if (cfg::has_wifi)
	{
		if (!sntp_time_set && send_now && msSince(time_point_device_start_ms) < 1000 * 2 * 30 + 5000)
		{
			debug_outln_info(F("NTP sync not finished yet, skipping send"));
			send_now = false;
			starttime = act_milli;
		}
	}

	sample_count++;
	if (last_micro != 0)
	{
		unsigned long diff_micro = act_micro - last_micro;
		UPDATE_MIN_MAX(min_micro, max_micro, diff_micro);
	}
	last_micro = act_micro;

	if (cfg::npm_read)
	{
		if ((msSince(starttime_NPM) > SAMPLETIME_NPM_MS) || send_now)
		{
			starttime_NPM = act_milli;
			fetchSensorNPM(result_NPM);
		}
	}

	if (cfg::sds_read)
	{
		if ((msSince(starttime_SDS) > SAMPLETIME_SDS_MS) || send_now)
		{
			starttime_SDS = act_milli;
			fetchSensorSDS(result_SDS);
		}
	}

	if (cfg::gps_read && !gps_init_failed)
	{
		// process serial GPS data..
		while (serialGPS->available() > 0)
		{
			gps.encode(serialGPS->read());
		}

		if ((msSince(starttime_GPS) > SAMPLETIME_GPS_MS) || send_now)
		{
			// getting GPS coordinates
			fetchSensorGPS(result_GPS);
			starttime_GPS = act_milli;
		}
	}

	//if ((msSince(last_display_millis) > DISPLAY_UPDATE_INTERVAL_MS) && (cfg::has_display && cfg::has_ssd1306 ))
	if ((msSince(last_display_millis) > DISPLAY_UPDATE_INTERVAL_MS) && (cfg::has_ssd1306))

	{
		display_values();
		last_display_millis = act_milli;
	}

	server.handleClient();
	yield();

	if (send_now)
	{
		if (cfg::has_wifi)
		{

			last_signal_strength = WiFi.RSSI();
			RESERVE_STRING(data, LARGE_STR);
			data = FPSTR(data_first_part);
			RESERVE_STRING(result, MED_STR);

			if (cfg::sds_read)
			{
				data += result_SDS;
				sum_send_time += sendSensorCommunity(result_SDS, SDS_API_PIN, FPSTR(SENSORS_SDS011), "SDS_");
			}
			if (cfg::npm_read)
			{
				data += result_NPM;
				sum_send_time += sendSensorCommunity(result_NPM, NPM_API_PIN, FPSTR(SENSORS_NPM), "NPM_");
				Debug.println(data);
			}

			if (cfg::bmx280_read && (!bmx280_init_failed))
			{
				// getting temperature, humidity and pressure (optional)
				fetchSensorBMX280(result);
				data += result;
				if (bmx280.sensorID() == BME280_SENSOR_ID)
				{
					sum_send_time += sendSensorCommunity(result, BME280_API_PIN, FPSTR(SENSORS_BME280), "BME280_");
				}
				else
				{
					sum_send_time += sendSensorCommunity(result, BMP280_API_PIN, FPSTR(SENSORS_BMP280), "BMP280_");
				}
				result = emptyString;
				Debug.println(data);
			}

			if (cfg::gps_read)
			{
				data += result_GPS;
				sum_send_time += sendSensorCommunity(result_GPS, GPS_API_PIN, F("GPS"), "GPS_");
				result = emptyString;
			}

			add_Value2Json(data, F("samples"), String(sample_count));
			add_Value2Json(data, F("min_micro"), String(min_micro));
			add_Value2Json(data, F("max_micro"), String(max_micro));
			add_Value2Json(data, F("interval"), String(cfg::sending_intervall_ms));
			add_Value2Json(data, F("signal"), String(last_signal_strength));

			if ((unsigned)(data.lastIndexOf(',') + 1) == data.length())
			{
				data.remove(data.length() - 1);
			}
			data += "]}";

			yield();
			Debug.println(data);
			sum_send_time += sendDataToOptionalApis(data);

			//MODELE => CHANGER POUR PMS!!!

			//{"software_version" : "Nebulo-V1-122021", "sensordatavalues" : [ {"value_type" : "NPM_P0", "value" : "1.84"}, {"value_type" : "NPM_P1", "value" : "2.80"}, {"value_type" : "NPM_P2", "value" : "2.06"}, {"value_type" : "NPM_N1", "value" : "27.25"}, {"value_type" : "NPM_N10", "value" : "27.75"}, {"value_type" : "NPM_N25", "value" : "27.50"}, {"value_type" : "BME280_temperature", "value" : "20.84"}, {"value_type" : "BME280_pressure", "value" : "99220.03"}, {"value_type" : "BME280_humidity", "value" : "61.66"}, {"value_type" : "samples", "value" : "138555"}, {"value_type" : "min_micro", "value" : "933"}, {"value_type" : "max_micro", "value" : "351024"}, {"value_type" : "interval", "value" : "145000"}, {"value_type" : "signal", "value" : "-71"} ]}

			// https://en.wikipedia.org/wiki/Moving_average#Cumulative_moving_average
			sending_time = (3 * sending_time + sum_send_time) / 4;
			if (sum_send_time > 0)
			{
				debug_outln_info(F("Time for Sending (ms): "), String(sending_time));
			}

			// reconnect to WiFi if disconnected
			if (WiFi.status() != WL_CONNECTED)
			{
				debug_outln_info(F("Connection lost, reconnecting "));
				WiFi_error_count++;
				WiFi.reconnect();
				waitForWifiToConnect(20);
			}

			// only do a restart after finishing sending
			if (msSince(time_point_device_start_ms) > DURATION_BEFORE_FORCED_RESTART_MS)
			{
				sensor_restart();
			}

			// Resetting for next sampling
			last_data_string = std::move(data);
			sample_count = 0;
			last_micro = 0;
			min_micro = 1000000000;
			max_micro = 0;
			sum_send_time = 0;
			starttime = millis(); // store the start time
			count_sends++;
		}

		if (cfg::has_lora)
		{
			prepareTxFrame();
			os_runloop_once();

			// POUR VERIF:
			// void os_runloop_once()
			// {
			// 	osjob_t *j = NULL;
			// 	hal_processPendingIRQs();

			// 	hal_disableIRQs();
			// 	// check for runnable jobs
			// 	if (OS.runnablejobs)
			// 	{
			// 		j = OS.runnablejobs;
			// 		OS.runnablejobs = j->next;
			// 	}
			// 	else if (OS.scheduledjobs && hal_checkTimer(OS.scheduledjobs->deadline))
			// 	{ // check for expired timed jobs
			// 		j = OS.scheduledjobs;
			// 		OS.scheduledjobs = j->next;
			// 	}
			// 	else
			// 	{				 // nothing pending
			// 		hal_sleep(); // wake by irq (timer already restarted)
			// 	}
			// 	hal_enableIRQs();
			// 	if (j)
			// 	{ // run job callback
			// 		j->func(j);
			// 	}
			// }
		}
	}

	if (sample_count % 500 == 0)
	{
		//		Serial.println(ESP.getFreeHeap(),DEC);
	}
}