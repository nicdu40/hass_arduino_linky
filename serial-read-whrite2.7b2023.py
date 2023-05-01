#!/usr/bin/python3
# -- coding: utf-8 --
import serial, re,  time, socket, os, MySQLdb
import paho.mqtt.publish as publish
import paho.mqtt.client as mqtt
usb = '/dev/ttyUSB1'

def on_connect(client, userdata, flags, rc):  # The callback for when the client connects to the broker print("Connected with result code {0}".format(str(rc)))  
    # Print result of connection attempt 
    client.subscribe("homeassistant/switch/#")  
    # Subscribe to the topic “digitest/test1”, receive any messages     published on it


def on_message(client, userdata, msg):  # The callback for when a PUBLISH message is received from the server. 
#	print("Message received-> " + msg.topic + " " + str(msg.payload))  # Print a received msg
	serial_port = open( usb ,"w")
	data_to_whrite= ""
	status=str(msg.payload).split("'")[1]
	OUT= msg.topic.split("/")[2].split("T")[1]
	if len(OUT) < 2:
		OUT=("0"+OUT)
	if re.match('ON', (status)):
		data_to_whrite = ("O" + OUT + "1")
		#print(data_to_whrite)
	if re.match('OFF', (status)):
		data_to_whrite = ("O" + OUT + "0")
		#print(data_to_whrite)
	serial_port.write(data_to_whrite + "{}\n")
	serial_port.close()


dbConn = MySQLdb.connect("localhost","root","****","mesures") or die ("could not connect to database")
#open a cursor to the database
cursor = dbConn.cursor()

#RESET ALL DB
SQL_litb  = 'DROP TABLE `Last`'
cursor.execute(SQL_litb)
SQL_litb  = 'CREATE TABLE `Last` ( `id` INT NOT NULL ) ENGINE = MEMORY'
cursor.execute(SQL_litb)
#SQL_litb  = 'ALTER TABLE Last ADD ID INT UNSIGNED NOT NULL PRIMARY KEY DEFAULT 1' 
#cursor.execute(SQL_litb)
SQL_lit = """DELETE FROM Last WHERE id=1"""
cursor.execute(SQL_lit)

#CHECK if ID=1 OK
cursor.execute("SELECT * FROM Last WHERE ID=1")
row = cursor.fetchone()
if row == None:
	print("CREATE FIRST LIGNE  (ID=1)")
	SQL_lit = """INSERT INTO Last(id) VALUES ('1')"""
	try:
		cursor.execute(SQL_lit)
		dbConn.commit() #commit the insert
	except:
		dbConn.rollback()


mysqlduplicatecolumn = 1060 # MySQL error code when INSERT finds a duplicate

serial_port = open( usb ,"w")
ser = serial.Serial( usb , 115200)
serial_port.write("R{}\n") #reset
file_out_NEW_data_etat = '/var/www/netdata/etat/NEW_data_etat'
file_out_NEW_last_etat = '/var/www/netdata/etat/NEW_last_etat'


client = mqtt.Client("digi_mqtt_test")  # Create instance of client with client ID “digi_mqtt_test”
client.on_connect = on_connect  # Define callback function for successful connection
client.on_message = on_message  # Define callback function for receipt of a message
client.connect('127.0.0.1', 1883)


while True:
	client.loop_start()
	data = ser.readline().decode()
	
	if re.match('^!(.)', (data)): # remet à l'heure arduino
		serial_port = open( usb ,"w")
		epoch_time = int(time.time())
		epoch_time += 7200
#		epoch_time += 3600
		print(epoch_time)
		serial_port.write("~{}\n".format(epoch_time))
		print(data)
		serial_port.close()
	else:
		if ";" in data:
			longueur = len(data)
			if longueur >= 2:
				for line in data:
					print(data)
					fields = data.split(";")
					i = 1
					while i < 100:
						try:
							field = (fields[i])	
							if "=" in field:
								#print (field)
								fields2 = field.split("=")	
								indice = (fields2[0])
								valeur = (fields2[1])
		
								"""
								try:	#pour ajouter les colones en cas de nouveux capteurs ou pour la premiere fois <---
									SQL_lit2  = 'ALTER TABLE Last ADD %s FLOAT' % (indice)
									#print SQL_lit2
									cursor.execute(SQL_lit2)
								except MySQLdb.OperationalError:#, message:
									#errorcode = message[0] # get MySQL error code
									#if errorcode == mysqlduplicatecolumn : # if duplicate
									print ("duplicate")
									#else:
										#raise # unexpected error, reraise
								#fin ajoute colone <---
								#***
								"""
							
								try:								
									SQL_lit = "UPDATE Last SET %s = %%s where id = 1" % (indice)
									cursor.execute(SQL_lit, [valeur])
									dbConn.commit() #commit the insert
								except MySQLdb.OperationalError:#, message:
									SQL_lit2  = 'ALTER TABLE Last ADD %s FLOAT' % (indice)
									cursor.execute(SQL_lit2)
									SQL_lit = "UPDATE Last SET %s = %%s where id = 1" % (indice)
									cursor.execute(SQL_lit, [valeur])
					
								if "OUT" in indice:
									topic = "homeassistant/switch/%s/state" % (indice)
									if "1" in valeur:
										jsonMsg = "ON"
									else:
										jsonMsg = "OFF"
																	
								else:	
									topic = "homeassistant/sensor/%s/state" % (indice)
									jsonMsg = "{\"temperature\":%s}" % (valeur)
								
								publish.single(topic, jsonMsg, hostname="127.0.0.1" )
								
							i += 1
						except IndexError:
							i = 100
					break
