#define BLYNK_TEMPLATE_ID "TMPL68osl4toF"
#define BLYNK_TEMPLATE_NAME "AirQ"
#define BLYNK_AUTH_TOKEN "v4QYznp813tJRKudu8eSL3qET95v2xRq"
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define input pins
#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ2PIN 34
#define DSM501A_PIN1 17
#define DSM501A_PIN2 18
#define BUTTON_PIN1 13

// Define output and LED pins
#define COOLER_LED 14
#define HEATER_LED 27
#define EXHAUSTF_LED 26
#define HUMIDIFIER_LED 25
#define BUZZER_PIN 32
#define wifi_led 5

char ssid[] = "Sanu's Galaxy M32"; // WiFi SSID (WiFi Name)
char pass[] = "buyw4642";          // WiFi Password
String Message1 = "Harmful Gas Detected";
String Message2 = "High Dust Concentration Detected";
char auth[] = BLYNK_AUTH_TOKEN;
unsigned long old = 0;
unsigned long current = 0;
int interval = 10;
int interval2 = 0;

const int buttonPin = 15;
int currentPage = 0;
unsigned long lastButtonPress = 0;
unsigned long debounceDelay = 500;

Servo exhaustfanServo;
int servoPin = 19; // Changed from 18 to avoid conflicts
int pos = 0;

unsigned long duration1;
unsigned long duration2;
float ratio1;
float ratio2;
float lowConcentration;
float highConcentration;

DHT dht(DHTPIN, DHTTYPE);

bool buzzerActive = false;
unsigned long starttime; // Initialize start time for dust readings

void wifi_testing()
{
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    Serial.print(".");
    digitalWrite(wifi_led, LOW);
    delay(250);
    digitalWrite(wifi_led, HIGH);
    current = millis();
    if ((current - old) / 1000 > interval)
      ESP.restart();
  }
}

void setup()
{
  Serial.begin(115200);

  // Initialize DHT sensor
  dht.begin();

  // Initialize LED pins as outputs
  pinMode(COOLER_LED, OUTPUT);
  pinMode(HEATER_LED, OUTPUT);
  pinMode(EXHAUSTF_LED, OUTPUT);
  pinMode(HUMIDIFIER_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Set button pin as input with pullup resistor
  pinMode(BUTTON_PIN1, INPUT_PULLUP);

  // DSM501A pins as input
  pinMode(DSM501A_PIN1, INPUT);
  pinMode(DSM501A_PIN2, INPUT);

  // Set initial output states
  digitalWrite(COOLER_LED, LOW);
  digitalWrite(HEATER_LED, LOW);
  digitalWrite(EXHAUSTF_LED, LOW);
  digitalWrite(HUMIDIFIER_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  exhaustfanServo.attach(servoPin);

  pinMode(buttonPin, INPUT_PULLUP);

  Serial.println("\nPlease wait for Blynk Server connection");
  pinMode(wifi_led, OUTPUT);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  wifi_testing();
  Blynk.begin(auth, ssid, pass);

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("AirQ"));

  display.display();
  delay(5000);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(F("Starting..."));

  display.display();
  delay(5000);

  starttime = millis(); // Initialize start time for dust sensor readings
}

void loop()
{
  pagesetup(); // Call the function to update the display pages
  Blynk.run();
  wifi_testing();
  current = millis();
  interval2 = (current - old) / 1000;

  // Read temperature and humidity
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, humidity);

  Serial.print("Temperature: ");
  Serial.println(temperature);
  Serial.print("Humidity: ");
  Serial.println(humidity);

  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Temperature Control
  if (temperature > 28)
  {
    digitalWrite(COOLER_LED, HIGH);
    Blynk.virtualWrite(V5, HIGH);
  }
  else if (temperature <= 24)
  {
    digitalWrite(COOLER_LED, LOW);
    Blynk.virtualWrite(V5, LOW);
  }

  if (temperature < 18)
  {
    digitalWrite(HEATER_LED, HIGH);
    Blynk.virtualWrite(V6, HIGH);
  }
  else if (temperature >= 24)
  {
    digitalWrite(HEATER_LED, LOW);
    Blynk.virtualWrite(V6, LOW);
  }

  // Humidity Control

  static bool humidifierOn = false;
  static unsigned long humidifierLastOffPulse = 0;
  static int humidifierOffPulseCount = 0;

  if (humidity < 35 && !humidifierOn)
  {

    digitalWrite(HUMIDIFIER_LED, HIGH); // Send one pulse to turn ON the humidifier
    delay(100);                         // Pulse duration
    digitalWrite(HUMIDIFIER_LED, LOW);

    Blynk.virtualWrite(V8, HIGH);
    humidifierOn = true;

    Serial.println("Humidifier turned ON via single pulse.");
  }
  else if (humidity >= 45 && humidifierOn)
  {
    // Send two pulses to turn OFF the humidifier
    if (humidifierOffPulseCount < 2 && (millis() - humidifierLastOffPulse > 1000))
    {
      digitalWrite(HUMIDIFIER_LED, HIGH);
      delay(100); // Pulse duration
      digitalWrite(HUMIDIFIER_LED, LOW);

      humidifierLastOffPulse = millis();
      humidifierOffPulseCount++;
      Serial.println("Sent pulse to turn OFF humidifier.");
    }
    else if (humidifierOffPulseCount == 2)
    {
      // Humidifier is fully turned off
      Blynk.virtualWrite(V8, LOW);
      humidifierOn = false;
      humidifierOffPulseCount = 0;

      Serial.println("Humidifier turned OFF after two pulses.");
    }
  }

  // Gas Detection
  int gasValue = analogRead(MQ2PIN);
  Blynk.virtualWrite(V3, gasValue);
  Serial.print("MQ2 Sensor Value: ");
  Serial.println(gasValue);

  if (gasValue > 450)
  {
    // Turn ON the exhaust fan LED and move the servo to open position
    digitalWrite(EXHAUSTF_LED, HIGH);

    // Move servo to the maximum angle
    exhaustfanServo.write(60);

    Serial.println("Blynk notification Sent to Mobile");
    Blynk.logEvent("notification", Message1);
    current = millis();
    old = millis();
    Blynk.virtualWrite(V8, HIGH);
    interval2 = 0;

    if (!buzzerActive)
    {
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerActive = true;
    }
  }
  else
  {
    // Only reset the servo if gas level is safe
    digitalWrite(EXHAUSTF_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    Blynk.virtualWrite(V8, LOW);
    buzzerActive = false;

    // Move servo back to initial position (120 degrees)
    exhaustfanServo.write(120);
  }

  // Dust Sensor Readings
  if ((millis() - starttime) > 30000)
  {
    starttime = millis();

    duration1 = pulseIn(DSM501A_PIN1, LOW);
    duration2 = pulseIn(DSM501A_PIN2, LOW);

    ratio1 = duration1 / (30000.0 * 10.0);
    lowConcentration = 1.1 * pow(ratio1, 3) - 3.8 * pow(ratio1, 2) + 520 * ratio1 + 0.62;

    ratio2 = duration2 / (30000.0 * 10.0);
    highConcentration = 1.1 * pow(ratio2, 3) - 3.8 * pow(ratio2, 2) + 520 * ratio2 + 0.62;
    Blynk.virtualWrite(V4, highConcentration);

    Serial.print("Dust Level: ");
    Serial.println(highConcentration);

    if (highConcentration > 100.0)
    {
      Serial.println("Dust Level: HEAVY");
      digitalWrite(BUZZER_PIN, HIGH);
      Blynk.virtualWrite(V8, HIGH);
      buzzerActive = true;
      Serial.println("Blynk notification Sent to Mobile");
      Blynk.logEvent("notification", Message2);
      current = millis();
      old = millis();
      interval2 = 0;
    }
    else
    {
      digitalWrite(BUZZER_PIN, LOW);
      Blynk.virtualWrite(V8, LOW);
      buzzerActive = false;
    }
  }

  if (digitalRead(BUTTON_PIN1) == LOW)
  {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
    Serial.println("Buzzer Stopped by User");
  }

  static unsigned long lastLoopTime = 0;
  if (millis() - lastLoopTime > 1000)
  {
    lastLoopTime = millis();
    Serial.println("Loop running...");
  }
}

void pagesetup()
{
  int buttonState = digitalRead(buttonPin);
  if (buttonState == LOW && millis() - lastButtonPress > debounceDelay)
  {
    lastButtonPress = millis();
    currentPage = (currentPage + 1) % 5;
  }
  drawPage(currentPage);
}

void drawPage(int page)
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  switch (page)
  {
  case 0:
    display.setCursor(20, 20);
    display.println("HOME");
    break;
  case 1:
    display.setCursor(25, 5);
    display.println("Temp.");
    display.setCursor(15, 40);
    display.print(dht.readTemperature());
    display.println(" C");
    break;
  case 2:
    display.setCursor(15, 5);
    display.println("Humidity");
    display.setCursor(20, 40);
    display.print(dht.readHumidity());
    display.println(" %");
    break;
  case 3:
    display.setCursor(15, 5);
    display.println("H. Gases");
    display.setCursor(0, 40);
    display.print(analogRead(MQ2PIN));
    display.println(" ppm");
    break;
  case 4:
    display.setCursor(5, 5);
    display.println("Dust Level");

    display.setCursor(0, 40);
    display.print(highConcentration);
    display.println("ug/m3");
    break;
  }

  display.display();
}
