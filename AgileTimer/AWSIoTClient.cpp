#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "ConnectWifi.h"

// AWS Definition
#define MQTT_PORT 8883
#define PUBLISH_TOPIC DEVICE_NAME "/publish"
#define SUBSCRIBE_TOPIC DEVICE_NAME "/subscribe"

// Device Definition
#define DEVICE_NAME "m5dial"
#define QOS 0

// config.cpp
extern const char *awsEndpoint;

// certs.cpp
extern const char *rootCA;
extern const char *certificate;
extern const char *privateKey;

void callback(char *, byte *, unsigned int);

WiFiClientSecure httpsClient;
PubSubClient mqttClient(httpsClient);

int mqtt_retry_cnt = 20; // 0.5秒×20=最大10秒で接続タイムアウト

// AWS IoT 接続
bool connectAWSIoT()
{
    httpsClient.setCACert(rootCA);
    httpsClient.setCertificate(certificate);
    httpsClient.setPrivateKey(privateKey);
    mqttClient.setServer(awsEndpoint, MQTT_PORT);
    mqttClient.setCallback(callback);

    Serial.print("MQTT connection.");
    while (!mqttClient.connected())
    {
        Serial.print(".");
        delay(500);
        if (mqttClient.connect(DEVICE_NAME))
        {
            Serial.println("");
            Serial.println("Connected AWS IoT Core.");
            return true;
        }
        if (--mqtt_retry_cnt == 0)
        {
            Serial.println("");
            Serial.println("Failed to connect AWS IoT Core.");
            return false;
        }
    }
}

void loopMQTT()
{
    mqttClient.loop();
}

// 現在の点灯状態をpublishする
void publish(int secTime)
{
    StaticJsonDocument<200> jsonDocument;
    char jsonString[100];
    jsonDocument["time"] = secTime;
    serializeJson(jsonDocument, jsonString);
    mqttClient.publish(PUBLISH_TOPIC, jsonString);
    Serial.println("Published.");
}

// メッセージを受信した際に、点灯状態を切り替える
void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.println("メッセージを受信");
}