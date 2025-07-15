#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <FS.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ======= DISPLAY CONFIG =======
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 21
#define OLED_SCL 22
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ======= WIFI CONFIG =======
const char *ssid = "MI.6";
const char *password = "miclem1112";
const char *serverUrl = "https://6efd5c0dd390.ngrok-free.app/core/api/predict/";

// ======= AUDIO CONFIG =======
#define I2S_SD 34    // Microphone data pin (GPIO34)
#define BUTTON_PIN 0 // Boot button
#define LED_PIN 2    // Onboard LED

const int sampleRate = 16000;
const int bitsPerSample = 16;
const int recordingSeconds = 3;
const char *wavFilename = "/recorded.wav";

// I2S Pins
#define I2S_WS 25  // Word select
#define I2S_SCK 26 // Bit clock

// Button state tracking
bool buttonPressed = false;
unsigned long buttonPressTime = 0;
const unsigned long DEBOUNCE_TIME = 50; // 50ms debounce time
const unsigned long PRESS_DELAY = 1000; // 1 second press required
const unsigned long COOLDOWN = 10000;   // 10 seconds between recordings

void setup()
{
    Serial.begin(115200);

    // Initialize OLED
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("OLED failed"));
        while (1)
            ;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Initializing...");
    display.display();

    // Initialize GPIO
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Initialize SPIFFS
    initSPIFFS();

    // Connect to WiFi
    connectToWiFi();

    Serial.println("System Ready");
    showDisplayMessage("System Ready", "Press BOOT button");
}

void initSPIFFS()
{
    showDisplayMessage("Mounting", "SPIFFS...");

    if (!SPIFFS.begin(true))
    {
        Serial.println("Formatting SPIFFS...");
        showDisplayMessage("Formatting", "SPIFFS...");

        if (SPIFFS.format())
        {
            if (SPIFFS.begin(true))
            {
                Serial.println("SPIFFS formatted & mounted");
                return;
            }
        }

        // Permanent failure
        Serial.println("SPIFFS FAILED");
        showDisplayError("SPIFFS FAILED");
        while (1)
        {
            digitalWrite(LED_PIN, HIGH);
            delay(250);
            digitalWrite(LED_PIN, LOW);
            delay(250);
        }
    }
    Serial.println("SPIFFS mounted");
}

void loop()
{
    handleButton();
    updateIdleDisplay();
}

void handleButton()
{
    static unsigned long lastActionTime = 0;
    int buttonState = digitalRead(BUTTON_PIN);

    // Button pressed (active LOW)
    if (buttonState == LOW)
    {
        if (!buttonPressed)
        {
            buttonPressed = true;
            buttonPressTime = millis();
        }

        // Check for long press
        if (buttonPressed && (millis() - buttonPressTime > PRESS_DELAY))
        {
            // Check cooldown period
            if (millis() - lastActionTime > COOLDOWN)
            {
                lastActionTime = millis();
                startRecordingProcess();
            }

            // Reset button state after action
            while (digitalRead(BUTTON_PIN) == LOW)
            {
                delay(10); // Wait for button release
            }
            buttonPressed = false;
        }
    }
    else
    {
        buttonPressed = false;
    }
}

void startRecordingProcess()
{
    digitalWrite(LED_PIN, HIGH);

    // Recording
    showDisplayMessage("Recording...", "Hold mic steady");
    if (recordWav())
    {
        showDisplayMessage("Recording", "Complete!");
        delay(1000);

        // Upload
        showDisplayMessage("Uploading...", "To server");
        if (uploadWav())
        {
            showDisplayMessage("Upload", "Success!");
        }
        else
        {
            showDisplayMessage("Upload", "Failed!");
        }
    }
    else
    {
        showDisplayMessage("Recording", "Failed!");
    }

    digitalWrite(LED_PIN, LOW);
    delay(1000); // Brief pause before returning to idle
}

void updateIdleDisplay()
{
    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate > 2000)
    {
        lastUpdate = millis();

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("System Ready");
        display.setCursor(0, 20);
        display.println("Press BOOT button");

        // Add WiFi status indicator
        display.setCursor(0, 40);
        if (WiFi.status() == WL_CONNECTED)
        {
            display.print("WiFi: ");
            display.println(WiFi.SSID());
        }
        else
        {
            display.println("WiFi: Disconnected");
        }

        // Add memory indicator
        display.setCursor(0, 54);
        display.printf("Mem: %dKB", ESP.getFreeHeap() / 1024);

        display.display();
    }
}

bool recordWav()
{
    // I2S Configuration
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = sampleRate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD};

    // Install and start I2S driver
    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK)
    {
        showDisplayError("I2S Driver Fail");
        return false;
    }
    if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK)
    {
        showDisplayError("I2S Pin Fail");
        return false;
    }

    // Create WAV file
    File file = SPIFFS.open(wavFilename, FILE_WRITE);
    if (!file)
    {
        showDisplayError("File Create Fail");
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    // Write empty header (will update later)
    uint8_t header[44] = {0};
    file.write(header, sizeof(header));

    // Record audio
    size_t bytesRead;
    int16_t buffer[512];
    uint32_t totalBytes = 0;
    const uint32_t recordDuration = sampleRate * recordingSeconds * sizeof(int16_t);

    Serial.println("Recording started...");
    unsigned long startTime = millis();

    while (totalBytes < recordDuration)
    {
        // Read data from I2S
        if (i2s_read(I2S_NUM_0, (char *)buffer, sizeof(buffer), &bytesRead, portMAX_DELAY) != ESP_OK)
        {
            Serial.println("I2S read error");
            break;
        }

        // Write to file
        if (bytesRead > 0)
        {
            file.write((uint8_t *)buffer, bytesRead);
            totalBytes += bytesRead;
        }

        // Show progress on display
        int progress = (totalBytes * 100) / recordDuration;
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print("Recording: ");
        display.print(progress);
        display.println("%");
        display.setCursor(0, 20);
        display.printf("%d/%d KB", totalBytes / 1024, recordDuration / 1024);
        display.display();
    }

    // Finalize recording
    file.close();
    i2s_driver_uninstall(I2S_NUM_0);

    // Update WAV header with actual size
    updateWavHeader(totalBytes);

    Serial.printf("Recording complete: %d bytes\n", totalBytes);
    return totalBytes > (recordDuration * 0.9); // 90% of expected size
}

void updateWavHeader(uint32_t dataSize)
{
    uint32_t fileSize = dataSize + 36;
    uint32_t byteRate = sampleRate * 2; // 16-bit = 2 bytes

    // WAV header
    uint8_t header[44] = {
        'R', 'I', 'F', 'F',         // RIFF
        (uint8_t)(fileSize & 0xFF), // File size
        (uint8_t)((fileSize >> 8) & 0xFF),
        (uint8_t)((fileSize >> 16) & 0xFF),
        (uint8_t)((fileSize >> 24) & 0xFF),
        'W', 'A', 'V', 'E',           // WAVE
        'f', 'm', 't', ' ',           // fmt
        0x10, 0x00, 0x00, 0x00,       // PCM header size
        0x01, 0x00,                   // PCM format
        0x01, 0x00,                   // Mono
        (uint8_t)(sampleRate & 0xFF), // Sample rate
        (uint8_t)((sampleRate >> 8) & 0xFF),
        (uint8_t)((sampleRate >> 16) & 0xFF),
        (uint8_t)((sampleRate >> 24) & 0xFF),
        (uint8_t)(byteRate & 0xFF), // Byte rate
        (uint8_t)((byteRate >> 8) & 0xFF),
        (uint8_t)((byteRate >> 16) & 0xFF),
        (uint8_t)((byteRate >> 24) & 0xFF),
        0x02, 0x00,                 // Block align (2 bytes/sample)
        0x10, 0x00,                 // Bits per sample (16)
        'd', 'a', 't', 'a',         // data
        (uint8_t)(dataSize & 0xFF), // Data size
        (uint8_t)((dataSize >> 8) & 0xFF),
        (uint8_t)((dataSize >> 16) & 0xFF),
        (uint8_t)((dataSize >> 24) & 0xFF)};

    // Update header at beginning of file
    File headerFile = SPIFFS.open(wavFilename, "r+");
    if (headerFile)
    {
        headerFile.seek(0);
        headerFile.write(header, 44);
        headerFile.close();
        Serial.println("WAV header updated");
    }
    else
    {
        Serial.println("Failed to update WAV header");
    }
}

// bool uploadWav()
// {
//     if (WiFi.status() != WL_CONNECTED)
//     {
//         Serial.println("Reconnecting to WiFi...");
//         connectToWiFi();

//         if (WiFi.status() != WL_CONNECTED)
//         {
//             showDisplayError("WiFi Disconnected!");
//             return false;
//         }
//     }

//     File file = SPIFFS.open(wavFilename, FILE_READ);
//     if (!file)
//     {
//         showDisplayError("File Read Fail");
//         return false;
//     }

//     const char *boundary = "----ESP32Boundary12345";
//     String head = "--" + String(boundary) + "\r\n";
//     head += "Content-Disposition: form-data; name=\"wav_file\"; filename=\"recorded.wav\"\r\n";
//     head += "Content-Type: audio/wav\r\n\r\n";

//     String tail = "\r\n--" + String(boundary) + "--\r\n";

//     int contentLength = head.length() + file.size() + tail.length();

//     WiFiClient *stream = new WiFiClient();
//     HTTPClient http;
//     http.begin(*stream, serverUrl);

//     http.addHeader("Content-Type", "multipart/form-data; boundary=" + String(boundary));
//     http.setTimeout(20000);

//     Serial.println("Uploading...");

//     int httpCode = http.sendRequest("POST", (uint8_t *)head.c_str(), head.length());
//     if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY && httpCode != HTTP_CODE_TEMPORARY_REDIRECT && httpCode != -1)
//     {
//         Serial.printf("HTTP initial send failed: %d\n", httpCode);
//         file.close();
//         http.end();
//         delete stream;
//         return false;
//     }

//     // Send the file content manually
//     uint8_t buffer[512];
//     while (file.available())
//     {
//         size_t bytes = file.read(buffer, sizeof(buffer));
//         stream->write(buffer, bytes);
//     }

//     // Send the tail
//     stream->print(tail);

//     // Get the response
//     httpCode = http.GET(); // Actually parse the server response

//     Serial.printf("HTTP Code: %d\n", httpCode);
//     String response = http.getString();
//     Serial.println("Server Response:");
//     Serial.println(response);

//     file.close();
//     http.end();
//     delete stream;

//     bool success = (httpCode >= 200 && httpCode < 300);

//     if (success)
//     {
//         int startIdx = response.indexOf("\"prediction\":");
//         if (startIdx != -1)
//         {
//             int endIdx = response.indexOf(',', startIdx);
//             String prediction = response.substring(startIdx + 14, endIdx - 1);
//             showDisplayMessage("Diagnosis:", prediction);
//         }
//     }

//     return success;
// }

bool uploadWav()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Reconnecting to WiFi...");
        connectToWiFi();

        if (WiFi.status() != WL_CONNECTED)
        {
            showDisplayError("WiFi Disconnected!");
            return false;
        }
    }

    // === FIND WAV FILE IN /data ===
    String wavPath = "";
    File root = SPIFFS.open("/data");
    if (!root || !root.isDirectory())
    {
        showDisplayError("/data DIR not found!");
        return false;
    }

    File file = root.openNextFile();
    while (file)
    {
        String name = file.name();
        if (name.endsWith(".wav"))
        {
            wavPath = name;
            break;
        }
        file = root.openNextFile();
    }

    if (wavPath == "")
    {
        showDisplayError("No .wav file found!");
        return false;
    }

    Serial.println("Found file: " + wavPath);

    File wavFile = SPIFFS.open(wavPath, FILE_READ);
    if (!wavFile)
    {
        showDisplayError("File Open Fail");
        return false;
    }

    // === PREPARE MULTIPART ===
    const char *boundary = "----ESP32Boundary12345";
    String head = "--" + String(boundary) + "\r\n";
    head += "Content-Disposition: form-data; name=\"wav_file\"; filename=\"uploaded.wav\"\r\n";
    head += "Content-Type: audio/wav\r\n\r\n";

    String tail = "\r\n--" + String(boundary) + "--\r\n";

    int contentLength = head.length() + wavFile.size() + tail.length();

    WiFiClient *stream = new WiFiClient();
    HTTPClient http;
    http.begin(*stream, serverUrl);
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + String(boundary));
    http.setTimeout(20000);

    Serial.println("Uploading...");

    int httpCode = http.sendRequest("POST", (uint8_t *)head.c_str(), head.length());
    if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY && httpCode != HTTP_CODE_TEMPORARY_REDIRECT && httpCode != -1)
    {
        Serial.printf("HTTP initial send failed: %d\n", httpCode);
        wavFile.close();
        http.end();
        delete stream;
        return false;
    }

    uint8_t buffer[512];
    while (wavFile.available())
    {
        size_t bytes = wavFile.read(buffer, sizeof(buffer));
        stream->write(buffer, bytes);
    }

    stream->print(tail);

    httpCode = http.GET(); // Read final response

    Serial.printf("HTTP Code: %d\n", httpCode);
    String response = http.getString();
    Serial.println("Server Response:");
    Serial.println(response);

    wavFile.close();
    http.end();
    delete stream;

    bool success = (httpCode >= 200 && httpCode < 300);

    if (success)
    {
        int startIdx = response.indexOf("\"prediction\":");
        if (startIdx != -1)
        {
            int endIdx = response.indexOf(',', startIdx);
            String prediction = response.substring(startIdx + 14, endIdx - 1);
            showDisplayMessage("Diagnosis:", prediction);
        }
    }

    return success;
}

void connectToWiFi()
{
    showDisplayMessage("Connecting WiFi", ssid);

    WiFi.disconnect(true);
    WiFi.begin(ssid, password);

    unsigned long startTime = millis();
    bool connected = false;

    while (millis() - startTime < 30000)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            connected = true;
            break;
        }
        Serial.print(".");
        display.print(".");
        display.display();
        delay(500);
    }

    if (connected)
    {
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        showDisplayMessage("WiFi Connected!", WiFi.localIP().toString());
    }
    else
    {
        Serial.println("\nConnection failed!");
        showDisplayMessage("WiFi Failed!", "Check Credentials");
    }
    delay(1000);
}

void showDisplayMessage(String line1, String line2)
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(line1);
    display.setCursor(0, 20);
    display.println(line2);
    display.display();
}

void showDisplayError(String message)
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("ERROR:");
    display.setCursor(0, 20);
    display.println(message);
    display.display();
    Serial.println("ERROR: " + message);
}