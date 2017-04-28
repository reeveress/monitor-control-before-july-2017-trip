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
//      6              ---- ----       unassigned 
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
String datReq; //String for data

byte mac[6];

unsigned int EEPROM_SIZE = 1024;
unsigned int eeadr = 0; //MACburner.bin writes MAC addres to the first 6 addresses of EEPROM

// Sensor objects
Adafruit_MCP9808 mcp = Adafruit_MCP9808(); 
Adafruit_HTU21DF htu = Adafruit_HTU21DF();


float tempC; // Declare variable for temperature in C
float tempF; // Declare variable for temperature in F

// struct for a UDP packet
struct sensors {
  float mcpTemp;
  float htuTemp;
  float htuHumid;

} sensorArray,sensorArrayDebug;



void setup() {
  Watchdog.disable(); // Disable Watchdog so it doesn't get into infinite reset loop
  
  // Initialize Serial for error message output and debugging
  Serial.begin(9600);
  Serial.println("Program reset..");
  
  // Read MAC address from EEPROM (burned previously with MACburner.bin sketch)
  for (int i = 0; i < 6; i++){
    mac[i] = EEPROM.read(eeadr);
    ++eeadr;
    }
  
  /*for (int i = 0; i < 1024; i++) {
    Serial.println("Printing the contents of EEPROM");
    Serial.print("Address: ");
    Serial.print(i);
    Serial.print("\t");
    Serial.print("Data: ");
    Serial.print(EEPROM.read(i));
    Serial.print("\t");

  }*/

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

  //Checking if HTU21DF temp and humidity sensor
  if (!htu.begin()) {
    Serial.println("Couldn't find HTU21DF");
  }

  // Checking if sensor is connected; if not then stay in perpetual loop of doom
  if (!mcp.begin()) {
    Serial.println("Couldn't find MCP9808!");
    while (1);
  }

  // Enable Watchdog for 10 seconds
  Watchdog.enable(8000);


  
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

    //Watchdog.reset();
   int udp_before = millis();
    // Send UDP packet to the server ip address serverIp that's listening on port localPort
    Udp.beginPacket(serverIp, localPort); // Initialize the packet send
    Udp.write((byte *)&sensorArray, sizeof sensorArray); // Send the temperature
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
        
      // Reset Watchdog before all the delays
      //Watchdog.reset();
      if (datReq =="getTemps") { //Do the following if Temperature is requested
        Serial.println("....Got UDP packet request from sockServer...");
        // Make a temperature reading and save it in a struct
        sensorArrayDebug.mcpTemp = mcp.readTempC();
        delay(2000);
    
        // Read and send humidity and temperature from HTU21DF sensor and send as UDP
        sensorArrayDebug.htuTemp = htu.readTemperature();
        sensorArrayDebug.htuHumid = htu.readHumidity();
        Serial.println(sensorArrayDebug.htuTemp);
    
        // Send UDP packet to the server ip address serverIp that's listening on port localPort
        Udp.beginPacket(serverIp, localPort); // Initialize the packet send
        Udp.write((byte *)&sensorArrayDebug, sizeof sensorArrayDebug); 
        Udp.endPacket(); // End the packet
        delay(2000);
        
      }
    int udpparse_after = millis();
    Serial.println("Time in ms that it took to check for received packet from udpClient.py, getTempDebug readings, etc..: ");
    Serial.println(udpparse_after - udpparse_before);
    
    }
    memset(packetBuffer, 0, UDP_TX_PACKET_MAX_SIZE); //clear out the packetBuffer array
    
    
    
    
    
   }
  
  
  

}


