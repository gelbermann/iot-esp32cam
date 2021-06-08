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
#define WIFI_SSID ("Gelbermann")     
#define WIFI_PASSWORD ("metallica") 
#define ESP_ACCESS_PORT (80)
#define WEBHOOK_URL ("http://192.168.43.174:8000/lockers/api/")

#define QR_ENDPOINT ("collect/qr/")
#define NFC_ENDPOINT ("collect/nfc/")

#define INVALID_LOCKER_ID (-1)

bool is_connected = false;
WiFiServer server(ESP_ACCESS_PORT);

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);

/* ----------------------------- setup functions ---------------------------- */

bool connect_to_wifi()
{
  // /* debug_print */ Serial.println("Connecting to wifi");
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int maxRetries = 10;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    // /* debug_print */ Serial.print(".");
    maxRetries--;
    if (maxRetries <= 0)
    {
      return false;
    }
  }
  /* debug_print */ Serial.println("");
  /* debug_print */ Serial.println("WiFi connected");
  /* debug_print */ Serial.println(WiFi.localIP());
  return true;
}

void setup()
{
  /* debug_print */ Serial.begin(BAUD);

  reader.setup();
  //reader.setDebug(true);
  /* debug_print */ Serial.println("Setup QRCode Reader");

  reader.begin();
  /* debug_print */ Serial.println("Begin QR Code reader");

  is_connected = connect_to_wifi();
  if (is_connected) {
    server.begin();
  }

  /* debug_print */ Serial.println(WiFi.localIP());

  delay(1000);
}

/* -------------------------------------------------------------------------- */
/*                               business logic                               */
/* -------------------------------------------------------------------------- */

/* ---------------------------------- utils --------------------------------- */

void send_open_door_message(int locker_id)
{
  // "Open door" message format is always 2 messages, the first is "OPEN" and second is the locker number as a string
  /* debug_print */ Serial.println("asking server to open door");
  Serial.println(OPEN_DOOR_MSG);
  Serial.println(locker_id);   // TODO decide on locker number according to qr code data
  delay(2000);         // prevent race in the webserver
}

int call_webhook(String endpoint, String payload)
{
  HTTPClient http;
  int locker_id = INVALID_LOCKER_ID;
  // uuid=<str:uuid>&<int:recipient_id>&<int:locker_id>
  String url = WEBHOOK_URL + endpoint + payload;
  /* debug_print */ Serial.print("url: ");
  /* debug_print */ Serial.println(url);
  int httpBegin = http.begin(url);
  /* debug_print */ Serial.print("httpBegin: ");
  /* debug_print */ Serial.println(httpBegin);

  int httpCode = http.GET();
  /* debug_print */ Serial.print("httpCode: ");
  /* debug_print */ Serial.println(httpCode);
  if (httpCode >= HTTP_CODE_OK && httpCode < 300)
  {
    // /* debug_print */ Serial.println("Open door");      

    // open_door_flow((const char *)qr_code_data.payload); - 
    // TODO REPLACE WITH: "validate qr code" function -> send_open_door_message(locker_id)
    // TODO Then remove the open_door_flow function
    /* debug_print */ Serial.println("TEMP - here we will try to open door");
    delay(2000);
    locker_id = httpCode - HTTP_CODE_OK;
  }

  http.end();
  return locker_id;
}

/* ------------------------- check-for-... functions ------------------------ */

void check_for_qr()
{
  int locker_id;
  struct QRCodeData qr_code_data;

  if (reader.receiveQrCode(&qr_code_data, 100))
  {
    /* debug_print */ Serial.println("Found QRCode");
    if (qr_code_data.valid)
    {
      uint8_t* qr_uuid = qr_code_data.payload + 10;
      qr_uuid[32] = 0;

      char* locker_id_ptr = (char*)(qr_code_data.payload + 77);
      locker_id_ptr[1] = 0;
      locker_id = atoi(locker_id_ptr);
      /* debug_print */ Serial.print("Locker id parsed from qr: ");
      /* debug_print */ Serial.println(locker_id);
      
      /* debug_print */ Serial.print("Payload: ");
      /* debug_print */ Serial.println((const char *) qr_uuid);
      int locker_id_for_qr_by_server = call_webhook(QR_ENDPOINT, String((const char *) qr_uuid));
      if (locker_id_for_qr_by_server == locker_id) 
      {
        send_open_door_message(locker_id);
      }
    }
    else
    {
      /* debug_print */ Serial.print("Invalid: ");
      /* debug_print */ Serial.println((const char *) qr_code_data.payload);
    }
  }
}

void sendUuidToServer(String uuid)
{
  bool uuid_is_validated = call_webhook(NFC_ENDPOINT, uuid);
  if (uuid_is_validated) 
  {

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

void check_for_remote_input() {
  WiFiClient client = server.available(); 
  int locker_id = INVALID_LOCKER_ID;

  if (client) { // new client connects
    /* debug_print */ Serial.print("Found client: ");
    /* debug_print */ Serial.println(client.remoteIP());
    
    if (client.connected() && client.available()) 
    {
      /* debug_print */ Serial.println("Read client data...");
      String http_request = client.readString();
      // /* debug_print */ Serial.println(http_request);

      int message_start_idx = http_request.indexOf(WEBSERVER_MSG_PREFIX) + WEBSERVER_MSG_PREFIX_LEN;
      // /* debug_print */ Serial.print("Message start index: ");
      // /* debug_print */ Serial.println(message_start_idx);
      String message = http_request.substring(message_start_idx);
      locker_id = message.toInt();

      /* debug_print */ Serial.print("Incoming message: ");
      /* debug_print */ Serial.println(message);

      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/plain");
      client.println("Connection: close");
      client.println();
      delay(300);
    }

    // Close the connection
    client.stop();
    // /* debug_print */ Serial.println("Client disconnected.");
    // /* debug_print */ Serial.println("");
  } 
  else 
  {
    // /* debug_print */ Serial.println("Client not available");
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