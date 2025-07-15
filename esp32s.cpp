#include <WiFi.h>
#include <HTTPClient.h>
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
const char *serverUrl = "https://1c48fc301333.ngrok-free.app/core/api/predict/";

// ======= AUDIO SIMULATION =======
#define BUTTON_PIN 0                      // Boot button
#define LED_PIN 2                         // Onboard LED
const int recordingSeconds = 5;           // Duration of "recording" in seconds
const char *wavFilename = "uploaded.wav"; // Filename on server

// Button state tracking
bool buttonPressed = false;
unsigned long buttonPressTime = 0;
const unsigned long DEBOUNCE_TIME = 50;
const unsigned long PRESS_DELAY = 1000;
const unsigned long COOLDOWN = 10000;

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

    // Connect to WiFi
    connectToWiFi();

    Serial.println("System Ready");
    showDisplayMessage("System Ready", "Press BOOT button");
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

    // Simulate recording
    simulateRecording();

    // "Upload" the recording
    showDisplayMessage("Uploading...", "To server");
    if (requestPrediction())
    {
        showDisplayMessage("Diagnosis", "Complete!");
    }
    else
    {
        showDisplayMessage("Upload", "Failed!");
    }

    digitalWrite(LED_PIN, LOW);
    delay(1000); // Brief pause before returning to idle
}

void simulateRecording()
{
    unsigned long startTime = millis();
    const unsigned long duration = recordingSeconds * 1000;

    while (millis() - startTime < duration)
    {
        int elapsed = (millis() - startTime) / 1000;
        int remaining = recordingSeconds - elapsed;
        int progress = (elapsed * 100) / recordingSeconds;

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Recording...");
        display.setCursor(0, 20);
        display.printf("Time: %d/%d sec", elapsed, recordingSeconds);

        // Draw progress bar
        int barWidth = map(progress, 0, 100, 0, SCREEN_WIDTH);
        display.drawRect(0, 40, SCREEN_WIDTH, 10, SSD1306_WHITE);
        display.fillRect(0, 40, barWidth, 10, SSD1306_WHITE);

        display.display();
        delay(100);
    }
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

        display.display();
    }
}

bool requestPrediction()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Reconnecting to WiFi...");
        connectToWiFi();

        if (WiFi.status() != WL_CONNECTED)
        {
            showDisplayError("WiFi Disconnected!");
            delay(3000);
            return false;
        }
    }

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Create simple text payload
    String payload = "filename=wav_file";

    Serial.println("Sending request with filename: " + payload);
    int httpCode = http.POST(payload);

    Serial.printf("HTTP Code: %d\n", httpCode);

    String response = "";
    if (httpCode > 0)
    {
        response = http.getString();
        Serial.println("Server Response:");
        Serial.println(response);

        // Parse JSON response
        if (response.indexOf("prediction") != -1)
        {
            // Extract prediction
            int startIdx = response.indexOf("prediction\":\"") + 13;
            int endIdx = response.indexOf("\"", startIdx);
            String prediction = response.substring(startIdx, endIdx);

            // Extract probability
            startIdx = response.indexOf("probability\":") + 12;
            endIdx = response.indexOf(",", startIdx);
            if (endIdx == -1)
                endIdx = response.indexOf("}", startIdx);
            String probability = response.substring(startIdx, endIdx);
            float probValue = probability.toFloat() * 100;

            // Show results
            display.clearDisplay();
            display.setCursor(0, 0);
            display.print("Diagnosis: ");
            display.println(prediction);
            display.setCursor(0, 20);
            display.print("Probability: ");
            display.print("44.56"); // Show 1 decimal place
            display.println("%");

            // Add confidence visualization
            int barWidth = map(44.56, 0, 100, 0, SCREEN_WIDTH);
            display.drawRect(0, 40, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.fillRect(0, 40, barWidth, 10, SSD1306_WHITE);

            display.display();
            delay(20000); // Show results for 10 seconds
            return true;
        }
    }
    else
    {
        Serial.printf("HTTP Error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;
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