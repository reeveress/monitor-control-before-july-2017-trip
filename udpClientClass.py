import time
import datetime
import struct
import redis
import socket
import sys

# Define IP address of the Redis server host machine
serverAddress = '10.0.1.224'


# Make this a passable argument => instance-level attribute
#arduinoAddress = '10.1.1.247'

PORT = 8888


class UdpClient():
    

    def __init__(arduinoAddress,self):
     
        # define socket address for binding 
        localSocket = (serverAddress, PORT)
        
        # define arduino socket to send requests
        arduinoSocket = (arduinoAddress, PORT)

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
      



        # Loop to grap UDP packets from Arduino and push to Redis
        while 1:


            print("Collecting Data and pushing to Redis...")
            # Receive data continuously from the server (Arduino in this case)
            data, addr =  client_socket.recvfrom(1024)

            # Arduino sends a Struct via UDP so unpacking is needed 
            # struct.unpack returns a tuple with one element
            # Each struct element is 4 Bytes (c floats are packed as 4 byte strings)

            unpacked_nodeID = struct.unpack('=f',data[0:4])
            unpacked_mcptemp = struct.unpack('=f',data[4:8])
            unpacked_htutemp = struct.unpack('=f', data[8:12])
            unpacked_htuhumid = struct.unpack('=f', data[12:16])
            print('Unpacked_nodeID: ',unpacked_nodeID) 
            node = int(unpacked_nodeID[0])
            print('Node ID: ', node)
    #       print('Unpack type: ', type(unpacked_mcptemp))
    #       print('MCP9808 Temperature: ' ,unpacked_mcptemp[0])
    #       print('HTU21DF Temperature: ', unpacked_htutemp[0])
    #       print('HTU21DF Humidity: ', unpacked_htuhumid[0])

            # Set hashes in Redis composed of sensor temperature values
            r.hmset('status:node:%d'%node, {'tempBot':unpacked_htutemp[0]})
            r.hmset('status:node:%d'%node, {'tempMid':unpacked_mcptemp[0]})
            r.hmset('status:node:%d'%node, {'humidBot':unpacked_htuhumid[0]})

            # Set timestamp 
            r.hmset('status:node:%d'%node, {'timestamp':str(datetime.datetime.now())})

            print('status:node:%d'%node,r.hgetall('status:node:%d'%node))



            # Check if getTemps flag is set by a mcNode object
            if (r.hmget('status:node:%d'%node, 'getTemps') == "True"):
                print("Inside the if statement")
                try:
                    print('getTemps is...',bool(r.hmget('status:node:%d'%node, 'getTemps')))
                    print('getTemps is...',type(bool(r.hmget('status:node:%d'%node, 'getTemps'))))

                    client_socket.sendto('getTemps', arduinoSocket)

                    time.sleep(2)
                    # Arduino checks if it received any udp packets and sends a respons back based on request
                    debug_data, addr = client_socket.recvfrom(1024)

                    unpacked_mcptemp_debug = struct.unpack('=f',debug_data[0:4])
                    unpacked_htutemp_debug = struct.unpack('=f', debug_data[4:8])

        #               print("Unpacked Response from Arduino:  ", unpacked_mcptemp_debug[0])

                    r.hmset('status:node:%d'%node, {'tempBotDebug':unpacked_htutemp_debug[0]})
                    r.hmset('status:node:%d'%node, {'tempMidDebug':unpacked_mcptemp_debug[0]})
        #               print(r.hmget('status:node:0','tempBotDebug'))

                    # Set timestamp
                    r.hmset('status:node:%d'%node, {'timestampDebug':str(datetime.datetime.now())})

                    # Reset the flag for next request 
                    r.hmset('status:node:%d'%node, {'getTemps':False})
                    # print('getTemps after resetting it is...',r.hget('status:node:0','getTemps'))

                except:

                    pass




            # Reset the microcontroller when reset is sent through the mcNode class
            # might remove this functionality in the future; too much power for the mcNode class
            if (r.hmget('status:node:%d'%node, 'reset')[0] == "True"):
    #            print("Inside the reset if block")
                client_socket.sendto('reset', arduinoSocket)

                time.sleep(2)

                # Reset the flag to False so it's not resetting microcontroller to infinity
                r.hmset('status:node:%d'%d, {'reset': False})


            # Set delay before receiving more data
            time.sleep(2)



