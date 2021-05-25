#include <Arduino.h>
#include <ESP32QRCodeReader.h>

#define BAUD (9600)
#define OPEN_DOOR_MSG ("OPEN")

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);

void open_door_flow(const char *qr_data)
{
  bool authorized = true; // TODO call webserver and VALIDATE qr code data

  if (authorized) {
    // "Open door" message format is always 2 messages, the first is "OPEN" and second is the locker number as a string
    Serial.println(OPEN_DOOR_MSG);
    Serial.println("1");  // TODO decide on locker number according to qr code data
    delay(2000);  // prevent race in the webserver
  } else {
    // TODO
  }
}

void onQrCodeTask(void *pvParameters)
{
  struct QRCodeData qrCodeData;

  while (true)
  {
    // Check if Arduino wants to validate an NFC against the webserver
    // TODO

    // Check for QR in ESP32CAM's camera
    if (reader.receiveQrCode(&qrCodeData, 100))
    {
      // Serial.println("Found QRCode");
      // if (qrCodeData.valid)
      // {
      //   Serial.print("Payload: ");
      //   Serial.println((const char *)qrCodeData.payload);
      // }
      // else
      // {
      //   Serial.print("Invalid: ");
      //   Serial.println((const char *)qrCodeData.payload);
      // }
      open_door_flow((const char *)qrCodeData.payload);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(BAUD);
  Serial.println();

  reader.setup();

  Serial.println("Setup QRCode Reader");

  reader.beginOnCore(1);

  Serial.println("Begin on Core 1");

  xTaskCreate(onQrCodeTask, "onQrCode", 4 * 1024, NULL, 4, NULL);
}

void loop()
{
  delay(100);
}