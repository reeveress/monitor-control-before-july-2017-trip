#include <Ethernet2.h>
#include <EthernetUdp2.h>
#include <SPI.h>
#include <Adafruit_MCP9808.h>

Adafruit_MCP9808 tempsensor = Adafruit_MCP9808(); //Create the sensor object


float tempC; // Declare variable for temperature in C
float tempF; // Declare variable for temperature in F


byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE}; //Assign a mac address
//IPAddress ip(10, 1, 1, 155); // Arduino ip address
IPAddress serverIp(10, 0, 1, 224); // Server ip address
EthernetClient client;

unsigned int localPort = 8888; // Assign a port to talk over




// For future use; initializing buffer and data variables for receiving packets from the server
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];
String datReq; //String for data
int packetSize;


EthernetUDP Udp; // UDP object


void setup() {
  
  Serial.begin(9600); 
  
  // Start Ethernet connection, should automatically try to get IP using DHCP
  if (Ethernet.begin(mac)==0){
    Serial.println("Failed to configure Ethernet using DHCP");
    for(;;)
      ;
    }
  Serial.println(Ethernet.localIP());
  
  Udp.begin(localPort); // Initialize UDP
  delay(1500); // delay to give time for initialization
  // Checking if sensor is connected; if not then stay in perpetual loop of doom
  if (!tempsensor.begin()) {
    Serial.println("Couldn't find MCP9808!");
    while (1);
  }
}



void loop() {

  
  Serial.println("Wake up MCP9808.... "); // wake up MSP9808 - power consumption ~200 mikro Ampere
  tempsensor.shutdown_wake(0);   // Don't remove this line! required before reading temp

  // Read and print out the temperature, then convert to *F
  float c = tempsensor.readTempC();
  float f = c * 1.8 + 32;
  Serial.println(c);
  delay(250);
  

  // Send UDP packet to the server ip address serverIp that's listening on port localPort
  Udp.beginPacket(serverIp, localPort); // Initialize the packet send
  Udp.print(c); // Send the temperature
  Udp.endPacket(); // End the packet
 
  Serial.println("Shutdown MCP9808.... ");
  tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere
  delay(2000);

  memset(packetBuffer, 0, UDP_TX_PACKET_MAX_SIZE);
}


