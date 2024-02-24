// AWS IoT 接続
bool connectAWSIoT();

// 現在の点灯状態をpublichする
void publish(int secTime);

// メッセージを受信した際に、点灯状態を切り替える
void callback(char *topic, byte *payload, unsigned int length);

void loopMQTT();