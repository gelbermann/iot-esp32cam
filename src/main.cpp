#include <WiFi.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <ESP32QRCodeReader.h>

#define BAUD (9600)
#define OPEN_DOOR_MSG ("OPEN")
#define WEBSERVER_MSG_PREFIX ("prefix_")
#define WEBSERVER_MSG_PREFIX_LEN (7)

// TODO update according to current working environment
#define WIFI_SSID ("22102630")     
#define WIFI_PASSWORD ("0506298446") 
#define ESP_ACCESS_PORT (80)
#define WEBHOOK_URL ("http://192.168.1.128:8000/lockers/api/")

#define QR_ENDPOINT ("collect/qr/")
#define NFC_ENDPOINT ("collect/nfc/")

#define INVALID_LOCKER_ID (-1)

bool is_connected = false;
WiFiServer server(ESP_ACCESS_PORT);

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);

void send_open_door_message(int locker_id)
{
  // "Open door" message format is always 2 messages, the first is "OPEN" and second is the locker number as a string
  Serial.println(OPEN_DOOR_MSG);
  Serial.println(locker_id);   // TODO decide on locker number according to qr code data
  delay(2000);         // prevent race in the webserver
}

void open_door_flow(const char *qr_data)
{
  bool authorized = true; // TODO call webserver and VALIDATE qr_data

  if (authorized)
  {
    send_open_door_message(1);
  }
  else
  {
    // TODO
  }
}

bool connect_to_wifi()
{
  // Serial.println("Connecting to wifi");
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
  Serial.println(WiFi.localIP());
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

void check_for_nfc() {
  // TODO
}

void check_for_remote_input() {
  WiFiClient client = server.available(); 
  int locker_id = INVALID_LOCKER_ID;

  if (client) { // new client connects
    Serial.print("Found client: ");
    Serial.println(client.remoteIP());
    
    if (client.connected() && client.available()) 
    {
      Serial.println("Read client data...");
      String http_request = client.readString();
      // Serial.println(http_request);

      int message_start_idx = http_request.indexOf(WEBSERVER_MSG_PREFIX) + WEBSERVER_MSG_PREFIX_LEN;
      // Serial.print("Message start index: ");
      // Serial.println(message_start_idx);
      String message = http_request.substring(message_start_idx);
      locker_id = message.toInt();

      Serial.print("Incoming message: ");
      Serial.println(message);

      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/plain");
      client.println("Connection: close");
      client.println();
      delay(300);
    }

    // Close the connection
    client.stop();
    // Serial.println("Client disconnected.");
    // Serial.println("");
  } 
  else 
  {
    // Serial.println("Client not available");
  }

  if (locker_id != INVALID_LOCKER_ID)  // Received locker id to open from webserver
  {
    send_open_door_message(locker_id);
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

  is_connected = connect_to_wifi();
  if (is_connected) {
    server.begin();
  }

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
  check_for_nfc();  // TODO
  check_for_remote_input();

  delay(300);
}