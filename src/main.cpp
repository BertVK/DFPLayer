/* 
PINOUT
 5 : internal LED
16 : Software Serial Rx
17 : Software Serial Tx
*/

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>
#include <MCP23017.h>

// define pins

DFRobotDFPlayerMini dfPlayer;
MCP23017 mcp = MCP23017(0x20);
uint8_t volume;

void setup()
{
  //Init variables
  volume = 1;

  Serial.begin(115200);
  while (!Serial)
  {
  }
  Serial.println("USB Serial ready");

  Serial2.begin(9600);
  while (!Serial2)
  {
  }
  Serial.println("Serial2 ready");

  Wire.begin();
  mcp.init();
  uint8_t conf = mcp.readRegister(MCP23017Register::IODIR_A);
  Serial.print("IODIR_A : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::IODIR_B);
  Serial.print("IODIR_B : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::IPOL_A);
  Serial.print("IPOL_A : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::IPOL_B);
  Serial.print("IPOL_B : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::GPINTEN_A);
  Serial.print("GPINTEN_A : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::GPINTEN_B);
  Serial.print("GPINTEN_B : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::DEFVAL_A);
  Serial.print("DEFVAL_A : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::DEFVAL_B);
  Serial.print("DEFVAL_B : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::INTCON_A);
  Serial.print("INTCON_A : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::INTCON_B);
  Serial.print("INTCON_B : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::IOCON);
  Serial.print("IOCON : ");
  Serial.print(conf, BIN);
  Serial.println();

  //conf = mcp.readRegister(IOCONB);
  //Serial.print("IOCONB : ");
  //Serial.print(conf, BIN);
  //Serial.println();

  conf = mcp.readRegister(MCP23017Register::GPPU_A);
  Serial.print("GPPU_A : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::GPPU_B);
  Serial.print("GPPU_B : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::INTF_A);
  Serial.print("INTF_A : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::INTF_B);
  Serial.print("INTF_B : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::INTCAP_A);
  Serial.print("INTCAP_A : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::INTCAP_B);
  Serial.print("INTCAP_B : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::GPIO_A);
  Serial.print("GPIO_A : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::GPIO_B);
  Serial.print("GPIO_B : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::OLAT_A);
  Serial.print("OLAT_A : ");
  Serial.print(conf, BIN);
  Serial.println();

  conf = mcp.readRegister(MCP23017Register::OLAT_B);
  Serial.print("OLAT_B : ");
  Serial.print(conf, BIN);
  Serial.println();


  

  delay(1000);
  if (!dfPlayer.begin(Serial2))
  {
    Serial.println(F("Unable to begin"));
    Serial.println(F("1. Please rechek the connection"));
    Serial.println(F("2. Please insert the SD card"));
  }
  Serial.println(F("DFPlayer Mini is Online"));
  dfPlayer.volume(volume);
}

void loop()
{
}