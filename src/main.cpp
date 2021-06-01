#include <WiFi.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <ESP32QRCodeReader.h>

#define BAUD (9600)
#define OPEN_DOOR_MSG ("OPEN")

// TODO update according to current working environment
#define WIFI_SSID ("Gelbermann")     
#define WIFI_PASSWORD ("metallica") 
#define WEBHOOK_URL ("http://192.168.43.174:8000/lockers/api/")

#define QR_ENDPOINT ("collect/qr/")
#define NFC_ENDPOINT ("collect/nfc/")

bool is_connected = false;

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);

void open_door_flow(const char *qr_data)
{
  bool authorized = true; // TODO call webserver and VALIDATE qr code data

  if (authorized)
  {
    // "Open door" message format is always 2 messages, the first is "OPEN" and second is the locker number as a string
    Serial.println(OPEN_DOOR_MSG);
    Serial.println("1"); // TODO decide on locker number according to qr code data
    delay(2000);         // prevent race in the webserver
  }
  else
  {
    // TODO
  }
}

bool connect_to_wifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int maxRetries = 10;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    // Serial.print(".");
    maxRetries--;
    if (maxRetries <= 0)
    {
      return false;
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  return true;
}

void call_webhook(String endpoint, String payload)
{
  HTTPClient http;
  // uuid=<str:uuid>&<int:recipient_id>&<int:locker_id>
  String url = WEBHOOK_URL + endpoint + payload;
  Serial.print("url: ");
  Serial.println(url);
  int httpBegin = http.begin(url);
  Serial.print("httpBegin: ");
  Serial.println(httpBegin);

  int httpCode = http.GET();
  Serial.print("httpCode: ");
  Serial.println(httpCode);
  if (httpCode == HTTP_CODE_OK)
  {
    // Serial.println("Open door");      

    // open_door_flow((const char *)qr_code_data.payload);
    Serial.println("TEMP - here we will try to open door");
    delay(2000);
  }

  http.end();
}

void check_for_qr()
{
  struct QRCodeData qr_code_data;
  if (reader.receiveQrCode(&qr_code_data, 100))
  {
    Serial.println("Found QRCode");
    if (qr_code_data.valid)
    {
      uint8_t* qr_uuid = qr_code_data.payload + 10;
      qr_uuid[32] = 0;
      
      Serial.print("Payload: ");
      Serial.println((const char *) qr_uuid);
      call_webhook(QR_ENDPOINT, String((const char *) qr_uuid));
    }
    else
    {
      Serial.print("Invalid: ");
      Serial.println((const char *) qr_code_data.payload);
    }
  }
}

void setup()
{
  Serial.begin(BAUD);

  reader.setup();
  //reader.setDebug(true);
  Serial.println("Setup QRCode Reader");

  reader.begin();
  Serial.println("Begin QR Code reader");

  delay(1000);
}

void loop()
{
  bool connected = connect_to_wifi();
  if (is_connected != connected)
  {
    is_connected = connected;
  }

  check_for_qr();

  delay(300);
}