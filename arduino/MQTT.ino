////////////////////////////////////////////////////////////////////////////////
// IDE:
//   Arduino 2.0.0
// Platform:
//   esp32 2.0.5 - https://github.com/espressif/arduino-esp32
// Board:
//   XIAO_ESP32C3
// Libraries:
//   MQTT 2.5.0 - https://github.com/256dpi/arduino-mqtt
//   ArduinoJson 6.19.4 - https://github.com/bblanchon/ArduinoJson
// 	https://github.com/Seeed-Studio/Seeed_Arduino_MultiGas

////////////////////////////////////////////////////////////////////////////////
// Includes

#include <atomic>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <ArduinoJson.h>

#include <Multichannel_Gas_GMXXX.h>
#include <Wire.h>
GAS_GMXXX<TwoWire> gas;

////////////////////////////////////////////////////////////////////////////////
// Constants

#define MUTUAL_AUTHENTICATION	(0)																		// Enable mutual authentication
// #define WIFI_TX_POWER					(WIFI_POWER_2dBm)											// Wi-Fi maximum transmitting power (wifi_power_t)
// #define WIFI_PROTOCOL					(WIFI_PROTOCOL_11N)										// Wi-Fi protocol {WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR}

static constexpr unsigned long INTERVAL = 60;												// [sec.]

static const char WIFI_SSID[] = "wifi";
static const char WIFI_PASSPHRASE[] = "pass";
static constexpr uint32_t WIFI_CONNECT_TIMEOUT = 30000;							// [msec.]

static const char TZ[] = "JST-9";
static const char SNTP_SERVER[] = "pool.ntp.org";
static constexpr unsigned long TIME_SYNC_TIMEOUT = 30000;						// [msec.]

static const char MQTT_SERVER[] = "test.mosquitto.org";
static constexpr uint16_t MQTT_SERVER_PORT = 8883;
static constexpr lwmqtt_qos_t QOS = LWMQTT_QOS0;										// MQTT Quality of Service {LWMQTT_QOS0, LWMQTT_QOS1, LWMQTT_QOS2}

static time_t StartTime = 0;

static WiFiClientSecure TcpClient;
static MQTTClient MqttClient;

static StaticJsonDocument<64> JsonDoc;	

////////////////////////////////////////////////////////////////////////////////
// Certificates

////////////////////////////////////////
// Common Name: mosquitto.org
// Organizational Name: Mosquitto
// Organizational Unit: CA
// From Date: 2020-06-09 20:06:39
// To Date: 2030-06-07 20:06:39
static const char SERVER_CA_CERT[] = \
	"-----BEGIN CERTIFICATE-----\n" \
	"MIIEAzCCAuugAwIBAgIUBY1hlCGvdj4NhBXkZ/uLUZNILAwwDQYJKoZIhvcNAQEL\n" \
	"BQAwgZAxCzAJBgNVBAYTAkdCMRcwFQYDVQQIDA5Vbml0ZWQgS2luZ2RvbTEOMAwG\n" \
	"A1UEBwwFRGVyYnkxEjAQBgNVBAoMCU1vc3F1aXR0bzELMAkGA1UECwwCQ0ExFjAU\n" \
	"BgNVBAMMDW1vc3F1aXR0by5vcmcxHzAdBgkqhkiG9w0BCQEWEHJvZ2VyQGF0Y2hv\n" \
	"by5vcmcwHhcNMjAwNjA5MTEwNjM5WhcNMzAwNjA3MTEwNjM5WjCBkDELMAkGA1UE\n" \
	"BhMCR0IxFzAVBgNVBAgMDlVuaXRlZCBLaW5nZG9tMQ4wDAYDVQQHDAVEZXJieTES\n" \
	"MBAGA1UECgwJTW9zcXVpdHRvMQswCQYDVQQLDAJDQTEWMBQGA1UEAwwNbW9zcXVp\n" \
	"dHRvLm9yZzEfMB0GCSqGSIb3DQEJARYQcm9nZXJAYXRjaG9vLm9yZzCCASIwDQYJ\n" \
	"KoZIhvcNAQEBBQADggEPADCCAQoCggEBAME0HKmIzfTOwkKLT3THHe+ObdizamPg\n" \
	"UZmD64Tf3zJdNeYGYn4CEXbyP6fy3tWc8S2boW6dzrH8SdFf9uo320GJA9B7U1FW\n" \
	"Te3xda/Lm3JFfaHjkWw7jBwcauQZjpGINHapHRlpiCZsquAthOgxW9SgDgYlGzEA\n" \
	"s06pkEFiMw+qDfLo/sxFKB6vQlFekMeCymjLCbNwPJyqyhFmPWwio/PDMruBTzPH\n" \
	"3cioBnrJWKXc3OjXdLGFJOfj7pP0j/dr2LH72eSvv3PQQFl90CZPFhrCUcRHSSxo\n" \
	"E6yjGOdnz7f6PveLIB574kQORwt8ePn0yidrTC1ictikED3nHYhMUOUCAwEAAaNT\n" \
	"MFEwHQYDVR0OBBYEFPVV6xBUFPiGKDyo5V3+Hbh4N9YSMB8GA1UdIwQYMBaAFPVV\n" \
	"6xBUFPiGKDyo5V3+Hbh4N9YSMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEL\n" \
	"BQADggEBAGa9kS21N70ThM6/Hj9D7mbVxKLBjVWe2TPsGfbl3rEDfZ+OKRZ2j6AC\n" \
	"6r7jb4TZO3dzF2p6dgbrlU71Y/4K0TdzIjRj3cQ3KSm41JvUQ0hZ/c04iGDg/xWf\n" \
	"+pp58nfPAYwuerruPNWmlStWAXf0UTqRtg4hQDWBuUFDJTuWuuBvEXudz74eh/wK\n" \
	"sMwfu1HFvjy5Z0iMDU8PUDepjVolOCue9ashlS4EB5IECdSR2TItnAIiIwimx839\n" \
	"LdUdRudafMu5T5Xma182OC0/u/xRlEm+tvKGGmfFcN0piqVl8OrSPBgIlb+1IKJE\n" \
	"m/XriWr/Cq4h/JfB7NTsezVslgkBaoU=\n" \
	"-----END CERTIFICATE-----\n";

#if MUTUAL_AUTHENTICATION != 0

static const char CLIENT_CERT[] = \
	"-----BEGIN CERTIFICATE-----\n" \
	"-----END CERTIFICATE-----\n";

static const char CLIENT_KEY[] = \
	"-----BEGIN RSA PRIVATE KEY-----\n" \
	"-----END RSA PRIVATE KEY-----\n";

#endif // MUTUAL_AUTHENTICATION

////////////////////////////////////////////////////////////////////////////////
// TimeSyncNotificationCallback

static std::atomic_bool TimeSyncCompleted;

static void TimeSyncNotificationCallback(struct timeval* tv)
{
	if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
	{
		TimeSyncCompleted = true;
	}
}

void setup()
{
	Serial.begin(115200);
	Serial.println();
	Serial.println();

	setenv("TZ", TZ, 1);
	tzset();

	Serial.println("TCP: Configure.");
	TcpClient.setCACert(SERVER_CA_CERT);
#if MUTUAL_AUTHENTICATION != 0
	TcpClient.setCertificate(CLIENT_CERT);
	TcpClient.setPrivateKey(CLIENT_KEY);
#endif // MUTUAL_AUTHENTICATION

	Serial.println("MQTT: Configure.");
	MqttClient.begin(MQTT_SERVER, MQTT_SERVER_PORT, TcpClient);
  
	Serial.println("WIFI: Start.");
	WiFi.enableSTA(true);
#if defined(WIFI_PROTOCOL)
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL);
#endif // WIFI_PROTOCOL
#if defined(WIFI_TX_POWER)
  WiFi.setTxPower(WIFI_TX_POWER);
#endif // WIFI_TX_POWER
	if (WIFI_SSID[0] != '\0')
	{
		WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE);
	}
	else
	{
		WiFi.begin();
	}

	////////////////////////////////////////
	// Wait to connect to Wi-Fi

	Serial.print("WIFI: Wait to connect...");
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print('.');
		delay(200);
	}
	Serial.println("Connected.");

	////////////////////////////////////////
	// Sync clock with SNTP server

	Serial.print("SNTP: Syncing clock...");
	sntp_set_time_sync_notification_cb(TimeSyncNotificationCallback);
	TimeSyncCompleted = false;
	configTzTime(getenv("TZ"), SNTP_SERVER);

	while (!TimeSyncCompleted)
	{
		Serial.print('.');
		delay(200);
	}
	Serial.println("Synced.");

	if (StartTime == 0)
	{
		StartTime = time(nullptr);
	}

  gas.begin(Wire, 0x08); // use the hardware I2C

}

void loop()
{
	const time_t now = time(nullptr);
	struct tm nowTm;
	localtime_r(&now, &nowTm);
	char nowStr[20];
	strftime(nowStr, sizeof(nowStr), "%Y/%m/%d %H:%M:%S", &nowTm);
	Serial.println(nowStr);

	Serial.print("WIFI: Connected to Wi-Fi? ");
	if (WiFi.status() != WL_CONNECTED)
	{
		Serial.println("No.");
	}
	else
	{
		Serial.println("Yes.");

		const time_t uptime = time(nullptr) - StartTime;
		const int8_t rssi = WiFi.RSSI();

		////////////////////////////////////////
		// Publish to MQTT server

		Serial.print("MQTT: Connect...");
		if (!MqttClient.connect(DEVICE_NAME))
		{
			Serial.println("ERROR.");
		}
		else
		{
			Serial.println("Connected.");

			Serial.println("MQTT: Publish.");

			String topic = "/xiao_data";

			JsonDoc.clear();
      // int val = gas.getGM102B();
      JsonDoc["NO2"] = gas.getGM102B();
      int val = gas.getGM302B();
      JsonDoc["C2H5CH"] = gas.getGM302B();
      val = gas.getGM502B();
      JsonDoc["VOC"] =  gas.getGM502B();
      val = gas.getGM702B();
			JsonDoc["CO"] = gas.getGM702B();
			
			String payload;
			serializeJson(JsonDoc, payload);
			Serial.print("Payload=");
			Serial.println(payload);

			MqttClient.publish(topic, payload, false, QOS);

			Serial.println("MQTT: Disconnect.");
			MqttClient.disconnect();
		}
	}

	Serial.println("Wait for time.");
	const int sleepSec = StartTime == 0 ? 0 : INTERVAL - (time(nullptr) - StartTime) % INTERVAL;
	delay(sleepSec * 10);
}
