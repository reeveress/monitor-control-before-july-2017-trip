from socket import *
import time

address = ('10.1.1.155', 5000) #arduino ip address and port number
client_socket = socket(AF_INET, SOCK_DGRAM) #Set up the socket
client_socket.settimeout(1)

while(1): #main loop
	data = "Blue" #Set Data to Blue command
	client_socket.sendto(data, address)
	try:
		rec_data, addr  = client_socket.recvfrom(2048) #reading response from arduino
		print rec_data #print the response from arduino 
	except: 
		pass
	time.sleep(2) 

	
	data = "Red" #Set Data to Red command
	client_socket.sendto(data, address)
	try:
		rec_data, addr  = client_socket.recvfrom(2048) #reading response from arduino
		print rec_data #print the response from arduino 
	except: 
		pass
	time.sleep(2) 
		


	data = "Green" #Set Data to Green command
	client_socket.sendto(data, address)
	try:
		rec_data, addr  = client_socket.recvfrom(2048) #reading response from arduino
		print rec_data #print the response from arduino 
	except: 
		pass
	time.sleep(2) 
