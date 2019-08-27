import paho.mqtt.client as mqtt
import os
from urllib.parse import urlparse
import time

mqtt_server = 'soldier-01.cloudmqtt.com'
mqtt_port = 18488
mqtt_user = 'ehvtsbga'
mqtt_password = '5n7anyeMnoLR'
qos = 0 # quality of service we want

# subscribe to those
temp_path = 'system/hardware/temp'
level_path = 'system/hardware/level'
schedule_path = 'system/app/timing'
amount_path = 'system/app/amount'
# publish here
report_path = 'system/bd/report'

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print('Connected to broker')

        global Connected
        Connected = True

    else:
        print('Connection failed')


    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(temp_path, qos)
    client.subscribe(level_path, qos)
    client.subscribe(schedule_path, qos)
    client.subscribe(amount_path, qos)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    print(msg.topic+" "+str(msg.payload))


def on_publish(client, obj, mid):
    print("mid: " + str(mid))
    pass


def on_subscribe(client, obj, mid, granted_qos):
    print("Subscribed: " + str(mid) + " " + str(granted_qos))

def on_log(client, obj, level, string):
    print(string)

Connected= False # global flag for connection status
client = mqtt.Client('Backend')
client.on_connect = on_connect
client.on_message = on_message
mqtt.on_publish = on_publish
mqtt.on_subscribe = on_subscribe
# Uncomment to enable debug messages
# mqttc.on_log = on_log

# Parse CLOUDMQTT_URL (or fallback to localhost)
#url_str = os.environ.get('CLOUDMQTT_URL', 'mqtt://localhost:8080')
#url = urlparse(url_str)

# connect
#client.username_pw_set(url.username, url.password)
client.username_pw_set(mqtt_user, mqtt_password)
client.connect(mqtt_server, mqtt_port)

client.loop_start()
# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
while Connected != True:    #Wait for connection
    time.sleep(0.1)

try:
    while True:

        value = input("input message: ")
        client.publish("system/ping",value)

except KeyboardInterrupt:

    client.disconnect()
    client.loop_stop()

# develop some functionality for creating reports
