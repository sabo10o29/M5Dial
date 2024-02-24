#include <WiFiClient.h>
#include <WiFiClientSecure.h>

extern const char *wifiSSID;
extern const char *wifiPassword;

int wifi_retry_cnt = 20; // 0.5秒×20=最大10秒で接続タイムアウト

// WiFi接続
bool connectWifi()
{
    Serial.print("Wi-Fi connection.");
    WiFi.begin(wifiSSID, wifiPassword);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
        if (--wifi_retry_cnt == 0)
        {
            Serial.println("");
            Serial.println("Failed to connect Wi-Fi.");
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            return false;
        }
    }
    Serial.println("");
    Serial.println("Connected Wi-Fi.");
    return (true);
}