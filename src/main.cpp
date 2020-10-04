/*
PINOUT
 5 : internal LED
17 : Serial2 Tx
16 : Serial2 Rx
15 : interrupt line
21 : SDA
22 : SCL
23 : busy signal from DFPlayer
*/

#include <Arduino.h>
#include <MCP23017.h>
#include <CircularBuffer.h> //https://github.com/rlogiacco/CircularBuffer
#include <DFRobotDFPlayerMini.h>
#include <WiFi.h>
#include <WebServer.h>
#include <AutoConnect.h>

//Comment out line below for production.
#define DEBUG true;

//Define pins
#define INT_PIN GPIO_NUM_15
#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22
#define BUSY_PIN GPIO_NUM_23

MCP23017 mcp = MCP23017(0x20);
bool interrupted = false;
uint8_t a, b;
CircularBuffer<byte, 100> playlist;
uint8_t lastButtonPressed;
DFRobotDFPlayerMini dfPlayer;
RTC_DATA_ATTR uint8_t volume;
#define KEY "dUhzSZ0OiI4YBAEPamqC3t"
#define EVENT_NAME "doggybutton"
String buttonsNames[] = {
    "outside"};
HTTPClient http;

//WiFi objects
WebServer server;
AutoConnect portal(server);

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

void setup()
{
  //init variables
  interrupted = false;
  volume = 30;

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
  Serial.printf("Volume set to %d\n", volume);
#endif

  AutoConnectConfig config;
  config.autoReconnect = true;
  portal.config(config);
  if (portal.begin())
  {
#ifdef DEBUG
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
#endif
  }

#ifdef DEBUG
  Serial.printf("Setup done");
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
    //process the portal
  }
}