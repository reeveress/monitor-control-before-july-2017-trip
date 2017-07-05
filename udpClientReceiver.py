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

# Define IP address of the Redis server host machine
serverAddress = '10.0.1.224'

# Define PORT for socket creation
PORT = 8888


class UdpClient():


    def __init__(self):

        # define socket address for binding; necessary for receiving data from Arduino 
        localSocket = (serverAddress, PORT)


        # Instantiate redis object connected to redis server running on serverAddress
        r = redis.StrictRedis(serverAddress)

        # Create a UDP socket
        try:
                client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                print('Socket created')
        except socket.error, msg:
                print('Failed to create socket. Error Code : ' + str(msg[0]) + ' Message ' + str(msg[1]))
                sys.exit()


        # Bind socket to local host and port
        try:
                client_socket.bind(localSocket)
                print('Bound socket')
        except socket.error , msg:
                print('Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1])
                sys.exit()


    def receiveUDP(self):
        """
        Goes into an infinite while loop to grab UDP packets.
        """

        # Loop to grap UDP packets from Arduino and push to Redis
        while 1:

            # Receive data continuously from the server (Arduino in this case)
            data, addr =  client_socket.recvfrom(1024)

            # Arduino sends a Struct via UDP so unpacking is needed 
            # struct.unpack returns a tuple with one element
            # Each struct element is 4 Bytes (c floats are packed as 4 byte strings)

            unpacked_nodeID = struct.unpack('=f',data[0:4])
            unpacked_mcptemp = struct.unpack('=f',data[4:8])
            unpacked_htutemp = struct.unpack('=f', data[8:12])
            unpacked_htuhumid = struct.unpack('=f', data[12:16])
            node = int(unpacked_nodeID[0])
            #print('Node ID: ', node)
            #print('MCP9808 Temperature: ' ,unpacked_mcptemp[0])
            #print('HTU21DF Temperature: ', unpacked_htutemp[0])
            #print('HTU21DF Humidity: ', unpacked_htuhumid[0])

            # Set hashes in Redis composed of sensor temperature values
            r.hmset('status:node:%d'%node, {'tempBot':unpacked_htutemp[0]})
            r.hmset('status:node:%d'%node, {'tempMid':unpacked_mcptemp[0]})
            r.hmset('status:node:%d'%node, {'humidBot':unpacked_htuhumid[0]})

            # Set timestamp 
            r.hmset('status:node:%d'%node, {'timestamp':str(datetime.datetime.now())})

            print('status:node:%d'%node,r.hgetall('status:node:%d'%node))

