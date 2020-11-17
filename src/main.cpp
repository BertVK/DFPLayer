/*
PINOUT
 5 : internal LED
17 : Serial2 Tx
16 : Serial2 Rx
15 : interrupt line
21 : SDA
22 : SCL
23 : busy signal from DFPlayer

Upload files in data folter to flash memeory using platformio run --target uploadfs
*/

#include <Arduino.h>
#include <MCP23017.h>
#include <CircularBuffer.h> //https://github.com/rlogiacco/CircularBuffer
#include <DFRobotDFPlayerMini.h>
#include <WiFi.h>
#include <WebServer.h>
#include <AutoConnect.h>
#include <FS.h>
#include <SPIFFS.h>

//Comment out line below for production.
#define DEBUG true;

//Define pins
#define INT_PIN GPIO_NUM_15
#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22
#define BUSY_PIN GPIO_NUM_23

//Create the filesystem
fs::SPIFFSFS &FlashFile = SPIFFS;

MCP23017 mcp = MCP23017(0x20);
bool interrupted = false;
uint8_t a, b;
CircularBuffer<byte, 100> playlist;
uint8_t lastButtonPressed;
DFRobotDFPlayerMini dfPlayer;
int volume = 20;
#define KEY "dUhzSZ0OiI4YBAEPamqC3t"
#define EVENT_NAME "doggybutton"
String buttonsNames[] = {
    "outside"};
HTTPClient http;
Preferences preferences;

//WiFi objects
WebServer server;
AutoConnect portal(server);

//supported URIs
#define URI_ROOT "/"

void getSettings()
{
  File settingsFile = FlashFile.open("/settings.txt", "r");
  if (!settingsFile)
  {
    // File not found
#ifdef DEBUG
    Serial.println("Failed to open settings file");
#endif
    return;
  }
  else
  {
    String inputLine = "";
    String valueString = "";
    while (settingsFile.available())
    {
      inputLine = settingsFile.readString();
      //Get volume
      if (inputLine.startsWith("volume"))
      {
        valueString = inputLine.substring(inputLine.indexOf('=') + 1);
        volume = valueString.toInt();
      }
    }
    settingsFile.close();
  }
}

void setSettings()
{
  File settingsFile = FlashFile.open("/settings.txt", "w");
  if (!settingsFile)
  {
    // File not found
#ifdef DEBUG
    Serial.println("Failed to open settings file");
#endif
    return;
  }
  else
  {
    settingsFile.print("volume=");
    settingsFile.println(volume);
    settingsFile.flush();
    settingsFile.close();
  }
}

String volumeSelect(PageArgument &args)
{
  if (args.size() > 0)
  {
    volume = args.arg("volume").toInt();
#ifdef DEBUG
    Serial.printf("volume from args = %d\n", volume);
#endif
    setSettings();
  }
  String returnString;
  returnString = "<div><label for=\"volume\" class=\"label\">Volume : </label>";
  returnString += "<select id=\"volume\" name=\"volume\"/>";
  for (int i = 0; i <= 30; i++)
  {
    returnString += "<option value=\"";
    returnString += i;
    returnString += "\"";
    if (i == volume)
    {
      returnString += " selected ";
    }
    returnString += ">";
    returnString += i;
    returnString += "</option>";
  }
  returnString += "</select></div>";
  return returnString;
}

String rootContent(PageArgument &args)
{
  return "<div>Content provided by function</div>";
}

PageElement ROOT_CONTENT("file:/index.htm",
                         {{"TITLE", [](PageArgument &args) { return "DoggyTalk home"; }},
                          {"VOLUME_INPUT", volumeSelect},
                          {"CONTENT", rootContent}});

PageBuilder ROOT_PAGE(URI_ROOT, {ROOT_CONTENT});

void userInput()
{
  interrupted = true;
}

void initMcp(MCP23017 thisMcp)
{
  thisMcp.init();
  //Set both ports as input
  thisMcp.portMode(MCP23017Port::A, 0b11111111);
  thisMcp.portMode(MCP23017Port::B, 0b11111111);
  //set interrupt to trigger on either bank
  thisMcp.interruptMode(MCP23017InterruptMode::Or);
  //Set interrupt on falling edge (start of pressing button)
  thisMcp.interrupt(MCP23017Port::A, FALLING);
  thisMcp.interrupt(MCP23017Port::B, FALLING);
  //Set polarity
  thisMcp.writeRegister(MCP23017Register::IPOL_A, 0x00);
  thisMcp.writeRegister(MCP23017Register::IPOL_B, 0x00);
  //Init bank reghisters to 0
  thisMcp.writeRegister(MCP23017Register::GPIO_A, 0x00);
  thisMcp.writeRegister(MCP23017Register::GPIO_B, 0x00);
  //Clear interrupt, ready to rumble
  thisMcp.clearInterrupts();
}

#ifdef DEBUG
void printBuffer()
{
  if (playlist.size() > 0)
  {
    Serial.printf("playlist = ");
    for (int i = 0; i < playlist.size(); i++)
    {
      Serial.print(playlist[i]);
      if (i < playlist.size() - 1)
        Serial.print(",");
    }
    Serial.printf("\n");
  }
}
#endif

//Check the buffer and queue the number of the button in the playlist
void queueButtons(uint8_t buffer, bool isBufferA, uint8_t chipNumber)
{
  uint8_t buttonNumber = 0;
  //detect the first bit
  for (int position = 0; position < 8; position++)
  {
    if (1 == ((buffer >> position) & 1))
    {
      //Calculate the button number
      buttonNumber = (isBufferA) ? (chipNumber * 16) + (position + 1) : (chipNumber * 16) + 8 + (position + 1);
#ifdef DEBUG
      Serial.print("Detected button nr ");
      Serial.print(buttonNumber);
      Serial.print(" - Last button nr ");
      Serial.println(lastButtonPressed);
#endif
      //We do not want twice the same button in the queue. It has to finish playing before we can play it again.
      if (lastButtonPressed != buttonNumber)
      {
        //Add the button to the queue
#ifdef DEBUG
        Serial.print("Added button nr ");
        Serial.println(buttonNumber);
#endif
        playlist.push(buttonNumber);
      }
      lastButtonPressed = buttonNumber;
    }
  }
}

bool isPlayerBusy()
{
  if (digitalRead(BUSY_PIN))
    return false;
  return true;
}

String urlencode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (c == ' ')
    {
      encodedString += '+';
    }
    else if (isalnum(c))
    {
      encodedString += c;
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
      {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
    yield();
  }
  return encodedString;
}

void playAudio()
{
  if (!playlist.isEmpty())
  {
    //There's a request on the queue, play it when the player is not playing
    if (!isPlayerBusy())
    {
      uint8_t songNr = playlist.shift();
      dfPlayer.volume(volume);
      dfPlayer.play(songNr);
#ifdef DEBUG
      Serial.print("Button name: ");
      Serial.println(buttonsNames[songNr - 1]);
#endif
      http.begin("https://maker.ifttt.com/trigger/" + String(EVENT_NAME) + "/with/key/" + KEY + "?value1=" + urlencode(buttonsNames[songNr - 1]));
      int httpCode = http.GET();
#ifdef DEBUG
      if (httpCode > 0)
      {
        //Check for the returning code
        String payload = http.getString();
        Serial.println(httpCode);
        Serial.println(payload);
      }
      else
      {
        Serial.println("Error on HTTP request");
      }
#endif
      http.end();
#ifdef DEBUG
      Serial.print("Playing song Nr ");
      Serial.println(songNr);
#endif
    }
  }
  else
  {
    //When last audio is played, we can play the same one again
    lastButtonPressed = 0;
  }
}

String getContentType(String filename)
{
  if (server.hasArg("download"))
    return "application/octet-stream";
  else if (filename.endsWith(".htm"))
    return "text/html";
  else if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".xml"))
    return "text/xml";
  else if (filename.endsWith(".pdf"))
    return "application/x-pdf";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path)
{
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/"))
    path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    Serial.printf("%d bytes sent to client\n", sent);
    file.close();
    return true;
  }
  return false;
}

void setup()
{
  //init variables
  interrupted = false;

//Set up serial port
#ifdef DEBUG
  Serial.begin(115200);
#endif

  //Set up pin modes
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUSY_PIN, INPUT);
  //Switch off built in LED
  digitalWrite(LED_BUILTIN, HIGH);

  //Configure  I2C connection
  Wire.begin(SDA_PIN, SCL_PIN, 100000L);
  //Init MCP device
  initMcp(mcp);
  attachInterrupt(digitalPinToInterrupt(INT_PIN), userInput, RISING);

  //Get the portal running
  AutoConnectConfig config;
  config.autoReconnect = true;
  config.hostName = "doggytalk";
  server.on("/favicon.ico", HTTP_GET, []() {
    if (!handleFileRead("/favicon.ico"))
      server.send(404, "text/plain", "FileNotFound");
  });
  server.on("/style.css", HTTP_GET, []() {
    if (handleFileRead("/style.css"))
      server.send(404, "text/plain", "FileNotFound");
  });
  portal.config(config);
  ROOT_PAGE.insert(server);
  //Mount the ISPFF file system
  FlashFile.begin(true);

  //read settings from file. This can only be run after the ISPFF file system has started
  getSettings();

  if (portal.begin())
  {
#ifdef DEBUG
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    Serial.println("Host name: " + String(WiFi.getHostname()));
#endif
  }

  //Set up the DFPlayer
  Serial2.begin(9600);
  //Give the DFPlayer a little time to initialize. Absolutely necessary!
  delay(1000);
  if (!dfPlayer.begin(Serial2))
  {
#ifdef DEBUG
    Serial.println(F("Unable to begin"));
    Serial.println(F("1. Please rechek the connection"));
    Serial.println(F("2. Please insert the SD card"));
#endif
    while (true)
    {
      //Could not connect to the DFPlayer, show an error code.
      digitalWrite(BUILTIN_LED, LOW);
      delay(1000);
      digitalWrite(BUILTIN_LED, HIGH);
      delay(1000);
    }
  }
#ifdef DEBUG
  Serial.println("DFPlayer Mini is Online");
#endif
  dfPlayer.volume(volume);

#ifdef DEBUG
  Serial.printf("Volume = %d\n", volume);
  Serial.printf("Setup done\n");
#endif
}

void loop()
{
  if (interrupted)
  {
    //Process what needs to be bone when a button is pressed.

    //Debounce the button. ToDo : can we do this reliably in hardware?
    delay(100);

    //Capture the button that triggered the interrupt
    mcp.interruptedBy(a, b);

    if (a != 0 && a != 255)
      queueButtons(a, true, 0);
    if (b != 0 && b != 255)
      queueButtons(b, false, 0);

#ifdef DEBUG
    printBuffer();
#endif
    //Reset the interrupt
    mcp.clearInterrupts();
    interrupted = false;
  }
  else
  {
    //No button is pressed.
    if (!playlist.isEmpty())
    {
      //Play the sound and Send the notification
      playAudio();
    }
    else
    {
      lastButtonPressed = 0;
    }
  }
  //process the portal
  portal.handleClient();
  portal.handleRequest();
  server.handleClient();
}