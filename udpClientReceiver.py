"""
This class is used for receiving the UDP packets from the Arduino.

It goes into an infinite while loop so has only the packet receiving functionality. 
"""



import time
import datetime
import struct
import redis
import socket
import sys
import smtplib

# Define IP address of the Redis server host machine
serverAddress = '10.28.1.207'

# Define PORT for socket creation
PORT = 8888


class UdpClient():


    def __init__(self):

        # define socket address for binding; necessary for receiving data from Arduino 
        self.localSocket = (serverAddress, PORT)


        # Instantiate redis object connected to redis server running on serverAddress
        self.r = redis.StrictRedis(serverAddress)

        # Create a UDP socket
        try:
                self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self.client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                print('Socket created')
        except socket.error, msg:
                print('Failed to create socket. Error Code : ' + str(msg[0]) + ' Message ' + str(msg[1]))
                sys.exit()


        # Bind socket to local host and port
        try:
                self.client_socket.bind(self.localSocket)
                print('Bound socket')
        except socket.error , msg:
                print('Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1])
                sys.exit()

        # Make a server object to send alerts by email
        #server = smtplib.SMTP('smtp.gmail.com', 587)
        #server.login('heranodemc@gmail.com','monitorcontrol')
        #server.ehlo()
        #server.starttls()

    def receiveUDP(self):
        """
        Goes into an infinite while loop to grab UDP packets.
        """

        # Loop to grap UDP packets from Arduino and push to Redis
        while 1:

            # Receive data continuously from the server (Arduino in this case)
            data, addr =  self.client_socket.recvfrom(1024)

            # Arduino sends a Struct via UDP so unpacking is needed 
            # struct.unpack returns a tuple with one element
            # Each struct element is 4 Bytes (c floats are packed as 4 byte strings)

            unpacked_nodeID = struct.unpack('=f',data[0:4])
            unpacked_mcptemp_top = struct.unpack('=f',data[4:8])
            unpacked_mcptemp_mid = struct.unpack('=f',data[8:12])
            unpacked_htutemp = struct.unpack('=f', data[12:16])
            unpacked_htuhumid = struct.unpack('=f', data[16:20])
            unpacked_windspeed_mph = struct.unpack('=f', data[20:24])
            unpacked_tempCairflow = struct.unpack('=f', data[24:28])
            unpacked_serial = struct.unpack('=B',data[28])
            node = int(unpacked_nodeID[0])

            # if (unpacked_mcptemp_top > 27 && unpacked_mcptemp_mid > 27 && unpacked_htutemp > 27):
               #server.send('heranodemc@gmail.com','recipientemail@gmail.com','The temperature values are approaching critical levels, shutdown sequence initiated') 
            # Set hashes in Redis composed of sensor temperature values
            self.r.hmset('status:node:%d'%node, {'tempTop':unpacked_mcptemp_top[0]})
            self.r.hmset('status:node:%d'%node, {'tempMid':unpacked_mcptemp_mid[0]})
            self.r.hmset('status:node:%d'%node, {'humidTemp':unpacked_htutemp[0]})
            self.r.hmset('status:node:%d'%node, {'humid':unpacked_htuhumid[0]})
            self.r.hmset('status:node:%d'%node, {'windSpeed_mph':unpacked_windspeed_mph[0]})
            self.r.hmset('status:node:%d'%node, {'tempCairflow':unpacked_tempCairflow[0]})
            
            # Set timestamp 
            self.r.hmset('status:node:%d'%node, {'timestamp':str(datetime.datetime.now())})
            self.r.hmset('status:node:%d'%node, {'serial': unpacked_serial[0]})
            print('status:node:%d'%node,self.r.hgetall('status:node:%d'%node))

