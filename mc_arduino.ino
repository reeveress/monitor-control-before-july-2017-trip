#include <ICMPPing.h>
#include <util.h>
#include <Adafruit_SleepyDog.h>
#include <EEPROM.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <Adafruit_MCP9808.h>
#include <Adafruit_HTU21DF.h>


//================ EEPROM Memory Map ================= 
// Address Byte       Data(8 bits)        Type                
//      0              ---- ----       MAC byte 0       
//      1              ---- ----       MAC byte 1       
//      2              ---- ----       MAC byte 2
//      3              ---- ----       MAC byte 3
//      4              ---- ----       MAC byte 4
//      5              ---- ----       MAC byte 5
//      6              ---- ----        Node ID 
//      7              ---- ----       unassigned
//      8              ---- ----       unassigned
//      9              ---- ----       unassigned
//      10             ---- ----       unassigned
//      .              ---- ----       unassigned
//      ..             ---- ----       unassigned
//      ...            ---- ----       unassigned
//      1024           ---- ----       unassigned




// I2C addresses for the two MCP9808 temperature sensors
#define TEMP_TOP 0x1A
#define TEMP_MID 0x1B


IPAddress serverIp(10, 0, 1, 224); // Server ip address
EthernetClient client; // client object
EthernetUDP Udp; // UDP object

SOCKET pingSocket = 1; //Socket for pinging the server to monitor network connectivity; Socket 0 works for pinging but breaks the EthernetUDP
ICMPPing ping(pingSocket, (uint16_t)random(0, 255));

unsigned int localPort = 8888; // Assign a port to talk over
int packetSize;

// For future use; initializing buffer and data variables for receiving packets from the server
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];
String datReq; // String for data

byte mac[6];

unsigned int EEPROM_SIZE = 1024;
unsigned int eeadr = 0; // MACburner.bin writes MAC addres to the first 6 addresses of EEPROM
unsigned int eeNodeAdr = 6; // EEPROM node ID address


// Sensor objects
Adafruit_MCP9808 mcpTop = Adafruit_MCP9808(); 
Adafruit_MCP9808 mcpMid = Adafruit_MCP9808(); 
Adafruit_HTU21DF htu = Adafruit_HTU21DF();



// Wind Sensor
#define analogPinForRV    1   // change to pins you the analog pins are using
#define analogPinForTMP   0

// to calibrate your sensor, put a glass over it, but the sensor should not be
// touching the desktop surface however.
// adjust the zeroWindAdjustment until your sensor reads about zero with the glass over it. 

const float zeroWindAdjustment =  -.4; // negative numbers yield smaller wind speeds and vice versa.

int TMP_Therm_ADunits;  //temp termistor value from wind sensor
float RV_Wind_ADunits;    //RV output from wind sensor 
float RV_Wind_Volts;
int TempCtimes100;
float zeroWind_ADunits;
float zeroWind_volts;



// struct for a UDP packet
struct sensors {
  float nodeID;
  float mcpTempTop;
  float mcpTempMid;
  float htuTemp;
  float htuHumid;
  float windSpeed_MPH;
  float tempCAirflow;

} sensorArray;



void setup() {

  Watchdog.disable(); // Disable Watchdog so it doesn't get into infinite reset loop
  
  // Initialize Serial for error message output and debugging
  Serial.begin(57600);
  Serial.println("Running Setup...");
  
  
  // Read MAC address from EEPROM (burned previously with MACburner.bin sketch)
  for (int i = 0; i < 6; i++){
    mac[i] = EEPROM.read(eeadr);
    ++eeadr;
    }
   
  // Read node ID from EEPROM (burned with MACburner.bin sketch) and assign it to struct nodeID member
   sensorArray.nodeID = EEPROM.read(eeNodeAdr);
  
  
  
  for (int i = 0; i < 8; i++) {
    Serial.println("Printing the contents of EEPROM");
    Serial.print("Address: ");
    Serial.print(i);
    Serial.print("\t");
    Serial.print("Data: ");
    Serial.print(EEPROM.read(i));
    Serial.print("\t");
  }
 
  
  // Setting pins appropriately. Very important to first write LOW to digital pins 
  // because setting the pin as OUTPUT changes it's state and has caused problems with the reset pin 4 before
  
  // PSU pin
  digitalWrite(2, LOW);
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  // White Rabbit 5V pin
  digitalWrite(3, LOW);
  pinMode(3, OUTPUT);
  digitalWrite(3,LOW);
  // FEM VAC pin
  digitalWrite(5, LOW);
  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);
  // PAM VAC pin
  digitalWrite(6, LOW);
  pinMode(6, OUTPUT);
  digitalWrite(6, LOW);
  // reset pin
  digitalWrite(4, HIGH);
  pinMode(4, OUTPUT); 
  digitalWrite(4, HIGH);

  

  // Start Ethernet connection, automatically tries to get IP using DHCP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP, restarting sketch...");
    delay(10000);
  }
  Serial.println("Configured IP:");
  Serial.println(Ethernet.localIP());
  
  
  // Start UDP
  Udp.begin(localPort);
  Serial.println("UDP initialized!");
  delay(1500); // delay to give time for initialization

  
  // Enable Watchdog for 8 seconds
  Watchdog.enable(8000);
  Serial.println("Watchdog enabled");

  // Checking if HTU21DF temp and humidity sensor
  if (!htu.begin()) {
    Serial.println("Couldn't find HTU21DF!");
    //Serial.println("Resetting the Microcontroller until the sensor is back online");
    delay(10000);
  }
  Watchdog.reset();
   
   
  if (!mcpTop.begin(TEMP_TOP)) {
    Serial.println("Couldn't find MCP9808 TOP! Restarting sketch...");
    delay(10000);
  }
  Watchdog.reset();
  

  if (!mcpMid.begin(TEMP_MID)) {
    Serial.println("Couldn't find MCP9808 MID! Restarting sketch...");
    delay(10000);
  }
  Watchdog.reset();
  
}


 
void loop() {
  
  ICMPEchoReply echoReply = ping(serverIp, 4); //takes about 7295 ms to fail
//  char buffer [256];
//  Serial.println(echoReply.status);
//  if (echoReply.status == SUCCESS)
//  {
//    
//    sprintf(buffer,
//            "Reply[%d] from: %d.%d.%d.%d: bytes=%d time=%ldms TTL=%d",
//            echoReply.data.seq,
//            echoReply.addr[0],
//            echoReply.addr[1],
//            echoReply.addr[2],
//            echoReply.addr[3],
//            REQ_DATASIZE,
//            millis() - echoReply.data.time,
//            echoReply.ttl);
//  }
//  else
//  {
//    sprintf(buffer, "Echo request failed; %d", echoReply.status);
//  }
//  Serial.println(buffer);
//  delay(500);

  
  if (echoReply.status == SUCCESS){
    Serial.println("Server ping successful!");
    Watchdog.reset();
    sensorArray.mcpTempTop = mcpTop.readTempC();
    delay(2000);
    Watchdog.reset();
    sensorArray.mcpTempMid = mcpMid.readTempC();
    delay(2000);
    Watchdog.reset();
    
    
    // Read and send humidity and temperature from HTU21DF sensor and send as UDP
    sensorArray.htuTemp = htu.readTemperature();
    sensorArray.htuHumid = htu.readHumidity();
   
 
    // Wind Sensor
    TMP_Therm_ADunits = analogRead(analogPinForTMP);
    RV_Wind_ADunits = analogRead(analogPinForRV);
    RV_Wind_Volts = (RV_Wind_ADunits *  0.0048828125);

    // these are all derived from regressions from raw data as such they depend on a lot of experimental factors
    // such as accuracy of temp sensors, and voltage at the actual wind sensor, (wire losses) which were unaccouted for.
    TempCtimes100 = (0.005 *((float)TMP_Therm_ADunits * (float)TMP_Therm_ADunits)) - (16.862 * (float)TMP_Therm_ADunits) + 9075.4;  
    sensorArray.tempCAirflow = TempCtimes100/100.0;
    zeroWind_ADunits = -0.0006*((float)TMP_Therm_ADunits * (float)TMP_Therm_ADunits) + 1.0727 * (float)TMP_Therm_ADunits + 47.172;  //  13.0C  553  482.39

    zeroWind_volts = (zeroWind_ADunits * 0.0048828125) - zeroWindAdjustment;  

    // This from a regression from data in the form of 
    // Vraw = V0 + b * WindSpeed ^ c
    // V0 is zero wind at a particular temperature
    // The constants b and c were determined by some Excel wrangling with the solver.
    
    sensorArray.windSpeed_MPH =  pow(((RV_Wind_Volts - zeroWind_volts) /.2300) , 2.7265);   
   
    //Serial.print("  TMP volts ");
    //Serial.print(TMP_Therm_ADunits * 0.0048828125);
    
    //Serial.print(" RV volts ");
    //Serial.print((float)RV_Wind_Volts);

    //Serial.print("TempC");
    //Serial.print(sensorArray.tempCAirflow);

    //Serial.print("   ZeroWind volts ");
    //Serial.print(zeroWind_volts);

    // Serial.print("   WindSpeed MPH ");
    // Serial.println(sensorArray.windSpeed_MPH);
      
    
      
      
    
    // Send UDP packet to the server ip address serverIp that's listening on port localPort
    Udp.beginPacket(serverIp, localPort); // Initialize the packet send
    Udp.write((byte *)&sensorArray, sizeof sensorArray); // Send the struct as UDP packet
    Udp.endPacket(); // End the packet
    Serial.println("UDP packet sent...");
    Watchdog.reset();  
    
    // Clear UDP packet buffer before sending another packet
    memset(packetBuffer, 0, UDP_TX_PACKET_MAX_SIZE);
  ;
    
    
    // Check if request was sent to Arduino
    packetSize = Udp.parsePacket(); //Reads the packet size
    
    if(packetSize>0) { //if packetSize is >0, that means someone has sent a request
  
      Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE); //Read the data request
      String datReq(packetBuffer); //Convert char array packetBuffer into a string called datReq
      Serial.println("Contents of the packetBuffer: ");
      Serial.println(packetBuffer);
      Serial.println("datReq:");
      Serial.println(datReq);
      
      if (datReq == "PSU_on") {
        digitalWrite(2, HIGH);
      }     
      
      else if (datReq == "PSU_off") {
        digitalWrite(2, LOW);
      }
      
      else if (datReq == "WR_on") {
        digitalWrite(3, HIGH);
      }
      
      else if (datReq == "WR_off") {
        digitalWrite(3, LOW);
      }
      
      else if (datReq == "FEM_on") {
        digitalWrite(5, HIGH);
      }
      
      else if (datReq == "FEM_off") {
        digitalWrite(5, LOW);
      }
      
      else if (datReq == "PAM_on") {
        Serial.println("Setting pin 6 HIGH");
        delay(100);
        digitalWrite(6, HIGH);
      }
      
      else if (datReq == "PAM_off") {
        digitalWrite(6, LOW);
      }
      
    
      else if (datReq == "reset") {
        Serial.println("Resetting the microcontroller...");
        digitalWrite(4, LOW);
        }
    
    }

    //clear out the packetBuffer array
    memset(packetBuffer, 0, UDP_TX_PACKET_MAX_SIZE); 
    
    // Renew DHCP lease - times out eventually if this is removed
    Ethernet.maintain();

    // stroke the watchdog just in case
    Watchdog.reset();
   
  
    
   }
   else {
     Serial.println("Server ping unsuccessful, restarting sketch...");
     delay(10000);
   }
  
}

