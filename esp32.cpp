#include <WiFi.h>       // ✅ Connect ESP32 to WiFi network
#include <HTTPClient.h> // ✅ Make HTTP requests (GET, POST)
#include <SD.h>         // ✅ Access SD card storage
#include <SPI.h>        // ✅ SPI bus for SD card

// WiFi credentials for your router
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// Your Django server endpoint
const char *serverName = "http://192.168.x.x:8000/core/api/predict/";
// ⚠️ localhost won’t work — use PC's LAN IP!

void setup()
{
    Serial.begin(115200);

    // 1️⃣ Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");

    // 2️⃣ Initialize SD card
    if (!SD.begin())
    {
        Serial.println("Card Mount Failed");
        return;
    }

    // 3️⃣ Open your .wav file on the SD card
    File file = SD.open("/101_1b1_Al_sc_Meditron.wav");
    if (!file)
    {
        Serial.println("Failed to open file for reading");
        return;
    }

    // 4️⃣ Create HTTP POST request
    HTTPClient http;

    // Create multipart boundary
    String boundary = "----ESP32Boundary";

    http.begin(serverName);
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

    // 5️⃣ Build the multipart body parts
    String bodyStart = "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"wav_file\"; filename=\"101_1b1_Al_sc_Meditron.wav\"\r\n";
    bodyStart += "Content-Type: audio/wav\r\n\r\n";

    String bodyEnd = "\r\n--" + boundary + "--\r\n";

    // Calculate total content length for the header
    int contentLength = bodyStart.length() + file.size() + bodyEnd.length();

    // 6️⃣ Send request manually using stream
    WiFiClient *stream = http.getStreamPtr();

    int httpResponseCode = http.sendRequest("POST");

    if (httpResponseCode > 0)
    {
        // 7️⃣ Send body start
        stream->print(bodyStart);

        // 8️⃣ Send file binary chunk by chunk
        while (file.available())
        {
            stream->write(file.read());
        }

        // 9️⃣ Send closing boundary
        stream->print(bodyEnd);

        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    }
    else
    {
        Serial.printf("Request failed, code: %d\n", httpResponseCode);
    }

    String response = http.getString();
    Serial.println("Server response:");
    Serial.println(response);

    http.end();
    file.close();
}

void loop()
{
    // Nothing here — only run once
}
