#include <SPI.h>
#include <MFRC522.h>
#include <WiFiClientSecure.h>

#define UID "(変更)00 00 00 00 00 00 00" // NFCタグのUID　スマホアプリなどで確認できる
#define SLEEP 2 // 鍵をリーダから取ってから何秒後にサーバに送信するか(2以上)
#define STOPTIME 30 // 鍵の変化がなかった場合、サーバへの送信を何回分スキップするか

const char* ssid     = "(変更)SSID";
const char* password = "(変更)パスワード";

#define RST_PIN 26
#define SS_PIN 5

#define OPEN 1
#define CLOSE 0

const char* host = "(変更)送信先のサーバ";

//HTTPSを用いる場合の認証(証明書は定期的に更新)
//詳しくは　https://www.mgo-tec.com/blog-entry-arduino-esp32-ssl-stable-root-ca.html
const char* root_ca = \
    "-----BEGIN CERTIFICATE-----\n"\
    (略)
    "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"\
    (略)
    "-----END CERTIFICATE-----\n";


int led_blue = 16;
int led_red = 4;

int timer = SLEEP;
int counter = 0;
int flag = CLOSE;

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

WiFiClientSecure client;

void setup() {
    Serial.begin(115200);
    pinMode(led_blue, OUTPUT);
    pinMode(led_red, OUTPUT);
    digitalWrite(led_red, HIGH); // 赤いLEDを光らせる
    while (!Serial);
    
    WiFi.begin(ssid, password);
    Serial.print("try");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    
    SPI.begin();
    mfrc522.PCD_Init();
    mfrc522.PCD_DumpVersionToSerial();

    client.setCACert(root_ca);
}

void loop() {
    delay(1000); // 1秒待つ
    
    if ( timer >= SLEEP ){
        // 部屋をクローズ扱いに
        timer = SLEEP;
        digitalWrite(led_blue, LOW); // 青いLEDを消す
        digitalWrite(led_red, HIGH); // 赤いLEDを光らせる
        if ( flag == OPEN){
            // flagの入れ替え
            counter = STOPTIME;
            flag = CLOSE;
        }
    }else{
        digitalWrite(led_red, LOW); // 赤いLEDを消す
        digitalWrite(led_blue, HIGH); // 青いLEDを光らせる
        if ( flag == CLOSE){
            counter = STOPTIME;
            flag = OPEN;
        }
    }

    counter++; //STOPTIME秒に一回サーバーへ送信
    if ( counter > STOPTIME){
        if (!client.connect(host, 443)) {
            Serial.println("connection failed");
            return;
        }

        // 今回は/api/room/state/openで鍵があることをサーバに通知し、/api/room/state/closeでないことを通知する
        String url = "/api/room/state/";
        if ( flag == OPEN){
            url += "open";
        }else{
            url += "close";
        }

        client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "accept: application/json\r\n" +
                     "Authorization: (変更:サーバ側に認証がない場合いらない行)" +
                     "Connection: close\r\n\r\n");
    
        // 応答がない場合はあきらめる
        unsigned long timeout = millis();
        while (client.available() == 0) {
            if (millis() - timeout > 1000) {
                Serial.println(">>> Client Timeout !");
                client.stop();
                return;
            }
        }
    
        // 結果をシリアルポートに通知
        while(client.available()) {
            String line = client.readStringUntil('\r');
            Serial.println(line);
        }
        counter = 0;
    }
    
    if ( ! mfrc522.PICC_ReadCardSerial()) {//カードが読み取れなかった場合に実行
        Serial.println(timer);
        timer++;
        return;
    }

    // 読み込んだNFCのUIDを取得
    String strBuf[mfrc522.uid.size];
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        strBuf[i] =  String(mfrc522.uid.uidByte[i], HEX);
        if(strBuf[i].length() == 1){
          strBuf[i] = "0" + strBuf[i];
        }
    }

    String strUID = strBuf[0] + " " + strBuf[1] + " " + strBuf[2] + " " + strBuf[3] + " " + strBuf[4] + " " + strBuf[5] + " " + strBuf[6];
    // 登録してあるUIDと等しければタイマーをリセット、そうでなければerror
    if ( strUID.equalsIgnoreCase(UID) ){
        //Serial.println("access");
        timer = 0;
    } else {
        //Serial.println("error!");
        digitalWrite(led_blue, LOW); // 青いLEDを消す
        digitalWrite(led_red, HIGH); // 赤いLEDを光らせる
    }
    
}
