/* -------------------------------------------------------------------------- */
/*                                    setup                                   */
/* -------------------------------------------------------------------------- */

#include <WiFi.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <ESP32QRCodeReader.h>

/* --------------------------------- globals -------------------------------- */

#define BAUD (9600)
#define OPEN_DOOR_MSG ("OPEN")
#define NFC_START_MSG ("NFC_UUID")
#define WEBSERVER_MSG_PREFIX ("prefix_")
#define WEBSERVER_MSG_PREFIX_LEN (7)

// TODO update according to current working environment
#define WIFI_SSID ("S21")     
#define WIFI_PASSWORD ("ejfj9838") 
#define ESP_ACCESS_PORT (80)
#define WEBHOOK_URL ("http://192.168.115.174:8000/lockers/api/")

#define QR_ENDPOINT ("collect/qr/")
#define NFC_ENDPOINT ("collect/nfc/")

#define INVALID_LOCKER_ID (-1)

bool is_connected = false;
WiFiServer server(ESP_ACCESS_PORT);

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);

/* ----------------------------- setup functions ---------------------------- */

bool connect_to_wifi()
{
  // //dubug_print Serial.println("Connecting to wifi");
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int maxRetries = 10;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    // //dubug_print Serial.print(".");
    maxRetries--;
    if (maxRetries <= 0)
    {
      return false;
    }
  }
  //dubug_print Serial.println("");
  //dubug_print Serial.println("WiFi connected");
  //dubug_print Serial.println(WiFi.localIP());
  return true;
}

void setup()
{
  //dubug_print Serial.begin(BAUD);

  reader.setup();
  //reader.setDebug(true);
  //dubug_print Serial.println("Setup QRCode Reader");

  reader.begin();
  //dubug_print Serial.println("Begin QR Code reader");

  is_connected = connect_to_wifi();
  if (is_connected) {
    server.begin();
  }

  delay(1000);
}

/* -------------------------------------------------------------------------- */
/*                               business logic                               */
/* -------------------------------------------------------------------------- */

/* ---------------------------------- utils --------------------------------- */

void send_open_door_message(int locker_id)
{
  // "Open door" message format is always 2 messages, the first is "OPEN" and second is the locker number as a string
  Serial.println(OPEN_DOOR_MSG);
  Serial.println(locker_id);   // TODO decide on locker number according to qr code data
  delay(2000);         // prevent race in the webserver
}

/*void open_door_flow(const char *qr_data)
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
}*/

void call_webhook(String endpoint, String payload)
{
  HTTPClient http;
  // uuid=<str:uuid>&<int:recipient_id>&<int:locker_id>
  String url = WEBHOOK_URL + endpoint + payload;
  //dubug_print Serial.print("url: ");
  //dubug_print Serial.println(url);
  int httpBegin = http.begin(url);
  //dubug_print Serial.print("httpBegin: ");
  //dubug_print Serial.println(httpBegin);

  int httpCode = http.GET();
  //dubug_print Serial.print("httpCode: ");
  //dubug_print Serial.println(httpCode);
  if (httpCode == HTTP_CODE_OK)
  {
    // //dubug_print Serial.println("Open door");      

    // open_door_flow((const char *)qr_code_data.payload); - 
    // TODO REPLACE WITH: "validate qr code" function -> send_open_door_message(locker_id)
    // TODO Then remove the open_door_flow function
    //dubug_print Serial.println("TEMP - here we will try to open door");
    delay(2000);
  }

  http.end();
}

/* ------------------------- check-for-... functions ------------------------ */

void check_for_qr()
{
  struct QRCodeData qr_code_data;
  if (reader.receiveQrCode(&qr_code_data, 100))
  {
    //dubug_print Serial.println("Found QRCode");
    if (qr_code_data.valid)
    {
      uint8_t* qr_uuid = qr_code_data.payload + 10;
      qr_uuid[32] = 0;
      
      //dubug_print Serial.print("Payload: ");
      //dubug_print Serial.println((const char *) qr_uuid);
      call_webhook(QR_ENDPOINT, String((const char *) qr_uuid));
    }
    else
    {
      //dubug_print Serial.print("Invalid: ");
      //dubug_print Serial.println((const char *) qr_code_data.payload);
    }
  }
}

void check_for_nfc() {

  if(Serial.available())
  {
    String msg = Serial.readStringUntil('\n');
    if(msg.startsWith(NFC_START_MSG))
    {
      String uuid = Serial.readStringUntil('\n');
      sendUuidToServer(uuid);
    }
  }
}

void sendUuidToServer(String uuid)
{
  call_webhook(NFC_ENDPOINT, uuid));
}

void check_for_remote_input() {
  WiFiClient client = server.available(); 
  int locker_id = INVALID_LOCKER_ID;

  if (client) { // new client connects
    //dubug_print Serial.print("Found client: ");
    //dubug_print Serial.println(client.remoteIP());
    
    if (client.connected() && client.available()) 
    {
      //dubug_print Serial.println("Read client data...");
      String http_request = client.readString();
      // //dubug_print Serial.println(http_request);

      int message_start_idx = http_request.indexOf(WEBSERVER_MSG_PREFIX) + WEBSERVER_MSG_PREFIX_LEN;
      // //dubug_print Serial.print("Message start index: ");
      // //dubug_print Serial.println(message_start_idx);
      String message = http_request.substring(message_start_idx);
      locker_id = message.toInt();

      //dubug_print Serial.print("Incoming message: ");
      //dubug_print Serial.println(message);

      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/plain");
      client.println("Connection: close");
      client.println();
      delay(300);
    }

    // Close the connection
    client.stop();
    // //dubug_print Serial.println("Client disconnected.");
    // //dubug_print Serial.println("");
  } 
  else 
  {
    // //dubug_print Serial.println("Client not available");
  }

  if (locker_id != INVALID_LOCKER_ID)  // Received locker id to open from webserver
  {
    send_open_door_message(locker_id);
  }
} 

/* -------------------------------------------------------------------------- */
/*                                 boilerplate                                */
/* -------------------------------------------------------------------------- */

void loop()
{
  bool connected = connect_to_wifi();
  if (is_connected != connected)
  {
    is_connected = connected;
  }

  check_for_qr();
  check_for_nfc();
  check_for_remote_input();

  delay(300);
}