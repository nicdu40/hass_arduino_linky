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


dbConn = MySQLdb.connect("localhost","root","seign055e","mesures") or die ("could not connect to database")
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
#UDP_IP = "192.168.101.1"
#UDP_PORT = 8125
#UDP_IP2 = "127.0.0.1"
#UDP_PORT2 = 1223

serial_port = open( usb ,"w")
ser = serial.Serial( usb , 115200)
file_out_NEW_data_etat = '/var/www/netdata/etat/NEW_data_etat'
file_out_NEW_last_etat = '/var/www/netdata/etat/NEW_last_etat'


client = mqtt.Client("digi_mqtt_test")  # Create instance of client with client ID “digi_mqtt_test”
client.on_connect = on_connect  # Define callback function for successful connection
client.on_message = on_message  # Define callback function for receipt of a message
client.connect('127.0.0.1', 1883)


while True:
	client.loop_start()
	#ser.flushInput()
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
#					sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP
					fields = data.split(";")
#					sock.sendto(data, (UDP_IP2, UDP_PORT2)) #netdata pour graphes
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
									#SQL_lit  = 'insert into Donnees (%s) VALUES (%%s)' % (indice)
									SQL_lit = "UPDATE Last SET %s = %%s where id = 1" % (indice)
									cursor.execute(SQL_lit, [valeur])
									dbConn.commit() #commit the insert
								except MySQLdb.OperationalError:#, message:
									SQL_lit2  = 'ALTER TABLE Last ADD %s FLOAT' % (indice)
									cursor.execute(SQL_lit2)
									SQL_lit = "UPDATE Last SET %s = %%s where id = 1" % (indice)
									cursor.execute(SQL_lit, [valeur])

								#MESSAGE = '%s:%s|g\n' % (indice, valeur) #netdata
								#MESSAGE2 = '%s:%s|h\n' % (indice, valeur) #netdata							
								
								#mosquitto_pub -h 127.0.0.1 -p 1883 -t "homeassistant/sensor/%s/state" -m '{ "temperature": %s }' % (indice, valeur)
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
								
								
								if "OUT" in indice: # recupere l'état des sorties
									separate = indice.split("T")	#le T du "OUT"
									num_out = (separate[1])
									v = 0
									if "2" in valeur:
										valeur = '<img src="auto-off.jpeg" height="42" width="42">'
								
									if "3" in valeur:
										valeur = '<img src="auto-on.jpeg" height="42" width="42">'
									
									if "0" in valeur:
										valeur = '<img src="off.jpeg" height="42" width="42">'
									
									if "1" in valeur:
										valeur = '<img src="on.jpeg" height="42" width="42">'     

									file_out= '/var/www/netdata/etat/etat-%s' % (num_out)
									
									valeur_NEW_data_etat = '%s' % (num_out)
									try:
										etat = open(file_out,"r+") 
										string = etat.read()
										if valeur != string:
											etat.seek(0)
											etat.truncate()
											etat.write("{}".format(valeur))
											etat.close()
									except IOError:
										print("error rw")
								else:
									v +=1
									if v > 2:                          
										valeur_NEW_data_etat = 0
								try:
										NEW_data_etat_status = open(file_out_NEW_data_etat,"r+") 
										string = NEW_data_etat_status.read()
										if valeur_NEW_data_etat != string:
											NEW_data_etat_status.seek(0)
											NEW_data_etat_status.truncate()
											NEW_data_etat_status.write("{}".format(valeur_NEW_data_etat))
											NEW_data_etat_status.close()
								except IOError:
									print("error rw")

								#if "ABSENT" not in MESSAGE:
								#	sock.sendto(MESSAGE.encode(), (UDP_IP, UDP_PORT)) # netdata
							i += 1
						except IndexError:
#							sock.close()
							i = 100
					break
