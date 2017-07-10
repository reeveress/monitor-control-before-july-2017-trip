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
Adafruit_MCP9808 mcp = Adafruit_MCP9808(); 
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
  float mcpTemp;
  float htuTemp;
  float htuHumid;
  float windSpeed_MPH;
  float tempCAirflow;

} sensorArray;



void setup() {
  Watchdog.disable(); // Disable Watchdog so it doesn't get into infinite reset loop
  
  // Initialize Serial for error message output and debugging
  Serial.begin(9600);
  Serial.println("Microcontroller reset..");
  
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

  // Start Ethernet connection, automatically tries to get IP using DHCP
  if (Ethernet.begin(mac) == 0) {

    Serial.println("Failed to configure Ethernet using DHCP");
    for (;;)
      ;
  }
  Serial.println(Ethernet.localIP());
  
  // Start UDP
  Udp.begin(localPort);
  delay(1500); // delay to give time for initialization

  
  // Enable Watchdog for 8 seconds
  Watchdog.enable(8000);

  // Checking if HTU21DF temp and humidity sensor
  if (!htu.begin()) {
    Serial.println("Couldn't find HTU21DF!");
    Serial.println("Resetting the Microcontroller until the sensor is back online");
    delay(10000);
  }

  // Checking if sensor is connected; if not keep resetting
  if (!mcp.begin()) {
    Serial.println("Couldn't find MCP9808!");
    Serial.println("Resetting the Microcontroller until the sensor is back online");
    delay(10000);
  }

  
  
  // Set Pin 4 as the reset pin
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  // PSU VAC pin
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  // White Rabbit 5V pin
  pinMode(3, OUTPUT);
  digitalWrite(3,LOW);
  // FEM VAC pin
  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);
  // PAM VAC pin
  pinMode(6, OUTPUT);
  digitalWrite(6, LOW);
   
}


 
void loop() {
  // Ping the Server to check network connectivity
  int icmpe_before = millis();
  
  ICMPEchoReply echoReply = ping(serverIp, 4); //takes about 7295 ms to fail
  int icmpe_after = millis();
  Serial.println("Time in ms to get ping reply from the server: ");
  Serial.println(icmpe_after - icmpe_before);
  if (echoReply.status == SUCCESS){
    Watchdog.reset();
      
    //Serial.println("Wake up MCP9808.... "); // wake up MSP9808 - power consumption ~200 mikro Ampere
    //tempsensor.shutdown_wake(0);   // Don't remove this line! required before reading temp
    // Make a temperature reading and save it in a struct
    int mcp_before = millis();
    sensorArray.mcpTemp = mcp.readTempC();
    int mcp_after = millis();
    Serial.println("Time in ms to get mcpTemp reading: ");
    Serial.println(mcp_after - mcp_before);
    
    //delay(250);
    //Serial.println("Shutdown MCP9808.... ");
    //tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere
    
    delay(2000);
  
    // Read and send humidity and temperature from HTU21DF sensor and send as UDP
    int htu_before = millis();
    sensorArray.htuTemp = htu.readTemperature();
    sensorArray.htuHumid = htu.readHumidity();
    int htu_after = millis();
    Serial.println("Time in ms to get htuTemp reading: ");
    Serial.println(htu_after - htu_before);

 
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
   
    Serial.print("  TMP volts ");
    Serial.print(TMP_Therm_ADunits * 0.0048828125);
    
    Serial.print(" RV volts ");
    Serial.print((float)RV_Wind_Volts);

    Serial.print("TempC");
    Serial.print(sensorArray.tempCAirflow);

    Serial.print("   ZeroWind volts ");
    Serial.print(zeroWind_volts);

    Serial.print("   WindSpeed MPH ");
    Serial.println(sensorArray.windSpeed_MPH);
      
    
      
      
    Serial.println("Sending UDP packet......");
    int udp_before = millis();
    // Send UDP packet to the server ip address serverIp that's listening on port localPort
    
    Udp.beginPacket(serverIp, localPort); // Initialize the packet send
    Udp.write((byte *)&sensorArray, sizeof sensorArray); // Send the struct as UDP packet
    Udp.endPacket(); // End the packet
    int udp_after = millis();
    Serial.println("Time in ms to send a UDP packet: ");
    Serial.println(udp_after - udp_before);

    // Reset watchdog for good measure
    //Watchdog.reset();  
    // Clear UDP packet buffer before sending another packet
    memset(packetBuffer, 0, UDP_TX_PACKET_MAX_SIZE);
  
    int udpparse_before = millis();
    
    // Check if request was sent to Arduino
    packetSize = Udp.parsePacket(); //Reads the packet size
    
    if(packetSize>0) { //if packetSize is >0, that means someone has sent a request
  
      Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE); //Read the data request
      String datReq(packetBuffer); //Convert char array packetBuffer into a string called datReq
      Serial.println("Contents of the packetBuffer: ");
      Serial.println(packetBuffer);
        
      
      if (datReq == "PSU_on") {
        digitalWrite(2, HIGH);
      }     
      
      if (datReq == "PSU_off") {
        digitalWrite(2, LOW);
      }
      
      if (datReq == "WR_on") {
        digitalWrite(3, HIGH);
      }
      
      if (datReq == "WR_off") {
        digitalWrite(3, LOW);
      }
      
      if (datReq == "FEM_on") {
        digitalWrite(5, HIGH);
      }
      
      if (datReq == "FEM_off") {
        digitalWrite(5, LOW);
      }
      
      if (datReq == "PAM_on") {
        digitalWrite(6, HIGH);
      }
      
      if (datReq == "PAM_off") {
        digitalWrite(6, LOW);
      }
      
    
      if (datReq == "reset") {
        Serial.println("Resetting the microcontroller...");
        digitalWrite(4, LOW);
        }
    
    
      int udpparse_after = millis();
      Serial.println("Time in ms that it took to check for received packet from udpClient.py, getTempDebug readings, etc..: ");
      Serial.println(udpparse_after - udpparse_before);
    
    }

    //clear out the packetBuffer array
    memset(packetBuffer, 0, UDP_TX_PACKET_MAX_SIZE); 
    
    // Renew DHCP lease - times out eventually if this is removed
    Ethernet.maintain();

    // stroke the watchdog just in case
    Watchdog.reset();
   
  
    
   }
  
  
  

}


