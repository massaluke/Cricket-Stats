#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Encoder.h>
#include <string>
#include "graphics.h"

#define EEPROM_SIZE 1024
#define MAX_CREDENTIALS 10
#define CREDENTIALS_ADDR 0 // Starting address in EEPROM
#define NUM_CREDS_ADDR (CREDENTIALS_ADDR + (MAX_CREDENTIALS * sizeof(WiFiCredentials)))

// Screen setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Rotary Encoder pins
#define ENCODER_CLK 16 // Clock pin
#define ENCODER_DT 4   // Data pin
#define ENCODER_SW 2   // Push-button pin

// Variables for rotary encoder
volatile int encoderDirection = 0; // Tracks direction of rotation (-1 or +1)
bool encoderButtonPressed = false;

// Match JSON data
String data = "";

ESP32Encoder encoder; // Create an encoder instance

const char *api_url = "https://cricket-9vwkbt678-lukes-projects-4e5a708c.vercel.app/api/index.js";

const int statusLedPin = 2;

// Define game data
String t1 = "";
String t2 = "";
String t1_score_str = "";
String t1_overs = "";
String t2_score_str = "";
String t2_overs = "";

String trails_text_line0 = "";
String trails_text_line1 = "";
String trails_text_line2 = "";

String match_type = "";

String allT1[7];
String allT2[7];

int type_offset = 0;

int refresh = 0;

String match = "";

boolean gameScreen = false;

// Menu variables
String dataList[7]; // Holds the menu items
int dataListSize;   // Actual number of items in the array
int currentSelection = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200;

const char *ESP32_SSID = "BatStat"; // ESP32_SSID for the Access Point
boolean reset = false;
// Function prototypes
void displayMenu();
void handleEncoder();
void handleButtonPress();
void populateDataList(String json);
void displayMatchDetails(int matchIndex);
void extractMatchData();
void printScreen();
int leftOffset(String score);
int rightOffset(String score);
int middleOffset(String score);
String swapScore(String score);
String doWifi();
String nullConverter(const String &nullGuyMaybe, boolean runs);
void doRefresh();
void setupAP();
void eepromify(String ssid, String password);

// Web server instance
WebServer server(80);
boolean connected = false;
void eepromify(String ssid, String password);

struct WiFiCredentials
{
  char ssid[32];     // Maximum length for SSID
  char password[64]; // Maximum length for password
};

void setup()
{
  Serial.begin(115200);
  int numCreds;
  WiFiCredentials wifiCreds;
  int wifiGIFcount = 0;
  int ballGIFcount = 0;

  EEPROM.begin(EEPROM_SIZE);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    while (1)
      ; // Stay in this loop if initialization fails
  }
  display.setTextColor(WHITE);

  // Set up the status LED pin
  pinMode(statusLedPin, OUTPUT);
  Wire.begin(21, 22); // SDA pin 21, SCL pin 22
  // Connect to WiFi
  EEPROM.get(NUM_CREDS_ADDR, numCreds);
  // Ensure numCreds is valid
  if (numCreds < 0 || numCreds > MAX_CREDENTIALS)
  {
    EEPROM.put(NUM_CREDS_ADDR, 0);
    numCreds = 0;
  }

  for (int i = 0; i < numCreds; i++)
  {
    int address = CREDENTIALS_ADDR + (i * sizeof(WiFiCredentials));
    EEPROM.get(address, wifiCreds);
    String ssid = wifiCreds.ssid;
    String password = wifiCreds.password;

    if (ssid.length() == 0 || password.length() == 0)
    {
      break;
    }
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    unsigned long previousMillis = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000)
    {
      unsigned long currentMillis = millis();

      if (currentMillis - previousMillis >= 200)
      {
        // Save the current time to previousMillis
        previousMillis = currentMillis;

        // Clear the display and update it with the next GIF frame
        display.clearDisplay();
        display.drawBitmap(0, 0, wifiGIF[wifiGIFcount % 7], 128, 64, WHITE);
        display.setTextColor(WHITE);

        display.setCursor(0, 45);
        display.println("Connecting to: ");
        display.print(ssid);
        display.print("      ");
        display.setCursor(120, 0);
        display.print(((5000 - (millis() - startAttemptTime)) / 1000) + 1);
        display.display();
        Serial.print(".");

        wifiGIFcount++;
      }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nConnected!");
      connected = true;
      break;
    }
    else
    {
      Serial.println("\nFailed to connect to " + ssid);
    }
  }

  if (!connected)
  {
    setupAP();
    Serial.println("Could not connect to any SSID. Connect to ESP32 Wifi and enter new Wifi credentials");
  }
  unsigned long previousMillis = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    server.handleClient(); // Handle incoming client requests

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= 200)
    {
      // Save the current time to previousMillis
      previousMillis = currentMillis;

      // Clear the display and update it with the next GIF frame
      display.clearDisplay();
      display.drawBitmap(0, 32, ballGIF[ballGIFcount % 7], 128, 32, WHITE);
      display.setTextColor(WHITE);

      display.setCursor(0, 0);
      display.print("1) Connect to ");
      display.println(ESP32_SSID);
      display.println("2) Visit http://192.168.4.1/");
      display.println("3) Enter Credentials");
      display.display();
      Serial.print(".");

      ballGIFcount++;
    }
  }
  Serial.println("\nConnected to WiFi!");

  display.clearDisplay();
  display.drawBitmap(0, 0, dataLoad, 128, 64, WHITE);
  display.display();

  // Initialize encoder
  ESP32Encoder::useInternalWeakPullResistors = puType::down;
  encoder.attachHalfQuad(ENCODER_DT, ENCODER_CLK);
  encoder.clearCount();

  // Set up button pin
  pinMode(ENCODER_SW, INPUT_PULLUP);

  // Attach interrupt to CLK pin
  int theTime = millis();
  data = doWifi();
  Serial.println("time taken = ");
  theTime = millis() - theTime;
  Serial.println(theTime);
  populateDataList(data); // Populate the menu with match data from JSON

  displayMenu();
}

void loop()
{
  static unsigned long lastRefreshTime = 0;
  handleButtonPress();

  int64_t position = encoder.getCount();
  static int64_t lastPosition = 0;

  if (!gameScreen)
  {
    if (position != lastPosition && position % 2 == 0)
    {
      if (position > lastPosition)
      {
        currentSelection--;
      }
      else
      {
        currentSelection++;
      }
      if (currentSelection >= dataListSize)
      {
        currentSelection = 0;
      }
      else if (currentSelection < 0)
      {
        currentSelection = dataListSize - 1;
      }

      lastPosition = position;
      displayMenu(); // Update the menu display
    }

    refresh = 0;
  }
  else
  {
    if (millis() - lastRefreshTime >= 400)
    {
      lastRefreshTime = millis(); // Update the last refresh time
      doRefresh();                // Perform the refresh
    }
  }
}

// Parse JSON and populate the data list with matches
void populateDataList(String json)
{
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error)
  {
    Serial.println(F("JSON parsing failed!"));
    dataListSize = 0;
    return;
  }

  JsonArray matches = doc.as<JsonArray>();
  dataListSize = 7;

  for (int i = 0; i < dataListSize; i++)
  {
    t1 = matches[i]["t1"].as<String>();
    t2 = matches[i]["t2"].as<String>();

    String matchType = matches[i]["matchType"].as<String>();
    // Extract 3-letter team codes
    int start1 = t1.indexOf('[') + 1;
    int end1 = t1.indexOf(']');
    if (start1 > 0 && end1 > 0 && (end1 - start1 > 2))
    {
      t1 = t1.substring(start1, end1);
    }
    else
    {
      t1 = t1.substring(0, 3);
      t1.toUpperCase();
    }

    int start2 = t2.indexOf('[') + 1;
    int end2 = t2.indexOf(']');
    if (start2 > 0 && end2 > start2 && (end2 - start2 > 2))
    {
      t2 = t2.substring(start2, end2);
    }
    else
    {
      t2 = t2.substring(0, 3);
      t2.toUpperCase();
    }

    dataList[6 - i] = t1.substring(0, 4) + " vs " + t2.substring(0, 4) + " (" + matchType + ")";
    allT1[i] = t1;
    allT2[i] = t2;
  }
}

// Function to start the Access Point (AP)
void setupAP()
{
  // Set up an open WiFi network
  bool result = WiFi.softAP(ESP32_SSID); // Open network without a password

  if (result)
  {
    Serial.println("Access Point started successfully!");
    Serial.print("ssid: ");
    Serial.println(ESP32_SSID);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Set up web server routes
    server.on("/", HTTP_GET, []()
              { server.send(200, "text/html", R"rawliteral(
        <h1>Welcome to BatStat</h1>
        <p>Enter Wifi credentials to set up BatStat internet connectivity.</p>
        <form action="/test" method="GET">
          <input type="text" name="ssid" placeholder="Enter SSID"><br>
          <input type="text" name="password" placeholder="Enter Password"><br>
          <button type="submit">Test WiFi</button>
        </form>
        <form action="/reset" method="POST">
          <button type="submit">Reset Internal Credential Storage</button>
        </form>
      )rawliteral"); });

    server.on("/test", HTTP_GET, []()
              {
      String ssid = server.arg("ssid");
      String password = server.arg("password");

      if (ssid.length() == 0 || password.length() == 0)
      {
        server.send(400, "text/html", "<h1>Error: SSID or Password is missing!</h1>");
        return;
      }

      WiFi.begin(ssid.c_str(), password.c_str());
      server.send(200, "text/html", "<h1>Check ESP32</h1>");

      Serial.print("Connecting to WiFi: ");
      Serial.println(ssid);

      unsigned long startAttemptTime = millis();
      int wifiGIFcount = 0;
      unsigned long previousMillis = 0;

      while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000)
      {
      unsigned long currentMillis = millis();

      if (currentMillis - previousMillis >= 200)
      {
        // Save the current time to previousMillis
        previousMillis = currentMillis;

        // Clear the display and update it with the next GIF frame
        display.clearDisplay();
        display.drawBitmap(0, 0, wifiGIF[wifiGIFcount % 7], 128, 64, WHITE);
        display.setTextColor(WHITE);

        display.setCursor(0, 45);
        display.println("Connecting to: ");
        display.print(ssid);
        display.print("      ");
        display.setCursor(120, 0);
        display.print(((10000 - (millis() - startAttemptTime)) / 1000) + 1);
        display.display();
        Serial.print(".");

        wifiGIFcount++;
      }
      }

      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("\nConnected!");
        connected = true;

        int numCreds = EEPROM.get(NUM_CREDS_ADDR, numCreds);

        eepromify(ssid, password);
        numCreds = EEPROM.get(NUM_CREDS_ADDR, numCreds);
      }
      else
      {
        Serial.println("\nFailed to connect!");
      }

      // Revert back to AP mode
      if (!connected) {
        WiFi.disconnect();
        setupAP();
      } });

    server.on("/reset", HTTP_POST, []()
              {
    EEPROM.put(NUM_CREDS_ADDR, 0);
    EEPROM.commit();

    server.send(200, "text/plain", "System reset triggered.");
    Serial.println("Reset button clicked; reset = true."); 
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Storage Reset.");
    display.println("Credentials will now be saved to device");
    display.display(); 
    delay(10000); });

    if (!connected)
    {
      server.begin();
      Serial.println("Web server started.");
    }
  }
  else
  {
    Serial.println("Failed to start Access Point.");
  }
}

void eepromify(String ssid, String password)
{
  int numCreds;
  EEPROM.get(NUM_CREDS_ADDR, numCreds);

  if (numCreds < 0 || numCreds > MAX_CREDENTIALS)
  {
    numCreds = 0;
  }

  // Check if there's space for more credentials
  if (numCreds >= MAX_CREDENTIALS)
  {
    Serial.println("Error: Maximum number of credentials reached.");
    display.clearDisplay();
    display.drawBitmap(0, 32, noSpace, 128, 32, WHITE);
    display.setTextColor(WHITE);

    display.setCursor(0, 0);
    display.println("No space to store credentials");
    display.println("Connect to BatStat and reset storage space");
    display.display();
    delay(10000);
    return;
  }

  // Ensure the input fits within the struct's bounds
  if (ssid.length() >= 32 || password.length() >= 64)
  {
    Serial.println("Error: SSID or password exceeds maximum allowed length.");
    return;
  }

  // Create a new credential
  WiFiCredentials newCredential;
  strncpy(newCredential.ssid, ssid.c_str(), sizeof(newCredential.ssid) - 1);
  newCredential.ssid[sizeof(newCredential.ssid) - 1] = '\0'; // Null-terminate
  strncpy(newCredential.password, password.c_str(), sizeof(newCredential.password) - 1);
  newCredential.password[sizeof(newCredential.password) - 1] = '\0'; // Null-terminate

  // Write the new credential to the EEPROM
  int address = CREDENTIALS_ADDR + (numCreds * sizeof(WiFiCredentials));
  EEPROM.put(address, newCredential);

  // Update numCreds
  numCreds++;
  EEPROM.put(NUM_CREDS_ADDR, numCreds);
  EEPROM.commit();
}
void handleEncoder()
{
  if (digitalRead(ENCODER_DT) == HIGH)
  {
    encoderDirection = 1; // Clockwise
  }
  else
  {
    encoderDirection = -1; // Counterclockwise
  }
}

// Check button press and debounce
void handleButtonPress()
{
  if (digitalRead(ENCODER_SW) == HIGH && (millis() - lastDebounceTime > debounceDelay))
  {
    lastDebounceTime = millis();
    if (!gameScreen)
    {
      displayMatchDetails(6 - currentSelection);
      gameScreen = true;
    }
    else
    {
      displayMenu();
      gameScreen = false;
    }
  }
}

// Display the match details on the next screen
void displayMatchDetails(int matchIndex)
{
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, data);
  JsonArray matches = doc.as<JsonArray>();

  if (error)
  {
    Serial.println(F("JSON parsing failed!"));
    dataListSize = 0;
    return;
  }
  match = matches[matchIndex].as<String>();
  extractMatchData();
  printScreen();
}

// Display the menu
void displayMenu()
{
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Select Match:");

  // Display the menu items
  for (int i = 0; i < dataListSize; i++)
  {
    if (i == currentSelection)
    {
      display.print("> ");
    }
    else
    {
      display.print("  ");
    }
    display.println(dataList[i]);
  }

  display.display();
  delay(100);
}

void printScreen()
{
  display.clearDisplay();
  boolean t1_logo = true;
  boolean t2_logo = true;
  // Display team logos
  if (t1 == "ADS")
  {
    display.drawBitmap(0, 5, ADS, 32, 32, WHITE);
  }
  else if (t1 == "BRH")
  {
    display.drawBitmap(0, 5, BRH, 32, 32, WHITE);
  }
  else if (t1 == "HBH")
  {
    display.drawBitmap(0, 5, HBH, 32, 32, WHITE);
  }
  else if (t1 == "MLR")
  {
    display.drawBitmap(0, 5, MLR, 32, 32, WHITE);
  }
  else if (t1 == "MLS")
  {
    display.drawBitmap(0, 5, MLS, 32, 32, WHITE);
  }
  else if (t1 == "PRS")
  {
    display.drawBitmap(0, 5, PRS, 32, 32, WHITE);
  }
  else if (t1 == "SYS")
  {
    display.drawBitmap(0, 5, SYS, 32, 32, WHITE);
  }
  else if (t1 == "SYT")
  {
    display.drawBitmap(0, 5, SYT, 32, 32, WHITE);
  }
  else
  {
    t1_logo = false;
  }

  // Display team logos for t2
  if (t2 == "ADS")
  {
    display.drawBitmap(SCREEN_WIDTH - 37, 5, ADS, 32, 32, WHITE);
  }
  else if (t2 == "BRH")
  {
    display.drawBitmap(SCREEN_WIDTH - 37, 5, BRH, 32, 32, WHITE);
  }
  else if (t2 == "HBH")
  {
    display.drawBitmap(SCREEN_WIDTH - 37, 5, HBH, 32, 32, WHITE);
  }
  else if (t2 == "MLR")
  {
    display.drawBitmap(SCREEN_WIDTH - 37, 5, MLR, 32, 32, WHITE);
  }
  else if (t2 == "MLS")
  {
    display.drawBitmap(SCREEN_WIDTH - 37, 5, MLS, 32, 32, WHITE);
  }
  else if (t2 == "PRS")
  {
    display.drawBitmap(SCREEN_WIDTH - 37, 5, PRS, 32, 32, WHITE);
  }
  else if (t2 == "SYS")
  {
    display.drawBitmap(SCREEN_WIDTH - 37, 5, SYS, 32, 32, WHITE);
  }
  else if (t2 == "SYT")
  {
    display.drawBitmap(SCREEN_WIDTH - 37, 5, SYT, 32, 32, WHITE);
  }
  else
  {
    t2_logo = false;
  }

  // Display team names
  if (t1_logo)
  {
    display.setCursor(leftOffset(t1), 0);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print(t1);
  }
  else
  {
    display.setCursor(0, 16);
    display.setTextSize(2);
    display.print(t1);
  }

  if (t2_logo)
  {
    display.setCursor(rightOffset(t1), 0);
    display.setTextSize(1);
    display.print(t2);
  }
  else
  {
    display.setCursor(SCREEN_WIDTH - 36, 16);
    display.setTextSize(2);
    display.print(t2);
  }

  // Display status
  display.setCursor(middleOffset(trails_text_line0), 6); // Adjust to center
  display.setTextSize(1);

  display.setTextColor(WHITE);
  display.print(trails_text_line0);

  display.setCursor(middleOffset(trails_text_line1), 16); // Adjust to center
  display.print(trails_text_line1);

  display.setCursor(middleOffset(trails_text_line2), 26); // Adjust to center
  display.print(trails_text_line2);

  display.setCursor(type_offset, 50); // Adjust to center
  display.print(match_type);

  display.setCursor(leftOffset(t1_score_str), 45);
  display.print(swapScore(t1_score_str));
  display.setCursor(leftOffset(t1_overs), 55);
  display.print(t1_overs);

  display.setCursor(rightOffset(t2_score_str), 45);
  display.print(swapScore(t2_score_str));
  display.setCursor(rightOffset(t2_overs), 55);
  display.print(t2_overs);

  display.display();
}

String doWifi()
{
  String response = "";
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;

    // Start the HTTP request
    http.begin(api_url);
    http.setTimeout(20000);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
      Serial.printf("HTTP Response Code: %d\n", httpResponseCode);

      // Get the response payload
      response = http.getString();
    }
    else
    {
      Serial.printf("Error in HTTP request: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    if (httpResponseCode != 200)
    {
      Serial.println("Retrying");
      return doWifi();
    }
    // Close connection
    http.end();
  }
  else
  {
    Serial.println("WiFi Disconnected. Reconnecting...");
    WiFi.reconnect();
  }
  return response;
}

void extractMatchData()
{
  // Create a DynamicJsonDocument to parse the JSON
  StaticJsonDocument<1024> doc;

  // Deserialize the JSON response
  DeserializationError error = deserializeJson(doc, match);

  if (error)
  {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.f_str());
    return;
  }

  String t1_fulls = doc["t1s"].as<String>();
  String t2_fulls = doc["t2s"].as<String>();

  t1 = allT1[6 - currentSelection].substring(0, 3);
  t2 = allT2[6 - currentSelection].substring(0, 3);

  t1_score_str = nullConverter(t1_fulls.substring(0, t1_fulls.indexOf(" ")), true);
  t2_score_str = nullConverter(t2_fulls.substring(0, t2_fulls.indexOf(" ")), true);

  match_type = doc["matchType"].as<String>();
  if (match_type == "test")
  {
    type_offset = SCREEN_WIDTH / 2 - 12;
  }
  else
  {
    type_offset = SCREEN_WIDTH / 2 - 9;
  }

  t1_overs = nullConverter(t1_fulls.substring(t1_fulls.indexOf("("), t1_fulls.indexOf(")") + 1), false);
  t2_overs = nullConverter(t2_fulls.substring(t2_fulls.indexOf("("), t2_fulls.indexOf(")") + 1), false);

  int t1_score = t1_score_str.substring(0, t1_score_str.indexOf("/")).toInt();
  int t2_score = t2_score_str.substring(0, t2_score_str.indexOf("/")).toInt();
  trails_text_line1 = "trail by";

  if (t1_score >= t2_score)
  {
    trails_text_line0 = t2;
    trails_text_line2 = String(t1_score - t2_score);
  }
  else
  {
    trails_text_line0 = t1;
    trails_text_line2 = String(t2_score - t1_score);
  }
}

String nullConverter(const String &nullGuyMaybe, boolean runs)
{
  if (nullGuyMaybe == "")
  {
    if (runs)
    {
      return "0/0";
    }
    else
    {
      return "(0)";
    }
  }
  else
  {
    return nullGuyMaybe;
  }
}

int middleOffset(String strings)
{
  int length = strings.length();
  int dude = (SCREEN_WIDTH / 2) - 3 * (length);
  return dude;
}

int rightOffset(String strings)
{
  int length = strings.length();
  int dude = (SCREEN_WIDTH - 18 - 3 * (length));
  return dude;
}

int leftOffset(String strings)
{
  int length = strings.length();
  int dude = (18 - 3 * (length));
  return dude;
}

String swapScore(String score)
{
  int separatorIndex = score.indexOf('/');
  String firstPart = score.substring(0, separatorIndex);
  String secondPart = score.substring(separatorIndex + 1);
  return secondPart + "/" + firstPart;
}

void doRefresh()
{
  if (refresh >= 127)
  {
    display.drawLine(0, 63, 127, 63, SSD1306_BLACK);
    data = doWifi();
    displayMatchDetails(6 - currentSelection);
    refresh = 0;
  }
  else
  {
    refresh++;
    display.drawPixel(refresh, 63, SSD1306_WHITE);
    display.display();
  }
}
