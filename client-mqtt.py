import paho.mqtt.client as mqtt
import os
from urllib.parse import urlparse
import time
import time_functions as tm

mqtt_server = 'soldier-01.cloudmqtt.com'
mqtt_port = 18488
mqtt_user = 'ehvtsbga'
mqtt_password = '5n7anyeMnoLR'
qos = 0 # quality of service we want

# subscribe to those
temp_path = 'system/hardware/temp'
level_path = 'system/hardware/level'
schedule_path = 'system/app/timing'
days_path = 'system/app/days'
amount_path = 'system/app/amount'
next_schedule = 'system/hardware/nextWSchedule'
# publish here
report_path = 'system/report'
timing_remaining_path='system/api/timing'

current_program = {'timing':'', 'days':'', 'amount':''}
new_program = {}

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
    client.subscribe(days_path, qos)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload
    print(topic+" "+str(payload))

    if topic == amount_path:
        new_program['amount']=payload.decode('utf-8')
    if topic == schedule_path:
        new_program['timing']=payload.decode('utf-8')
        #tm.ETANextWateringTime(current_program)
    elif topic == days_path: # we know this path is the last received
        new_program['days']=payload.decode('utf-8')
        #tm.ETANextWateringDay(current_program)

        # if the new information coming is has not already been
        # registered, replace the current program with the new one
        # and notify the listening parties.
        if(not tm.isSameProgram(current_program, new_program)):
            print("new program detected, sending data to broker")
            current_program['timing'] = new_program['timing']
            current_program['days'] = new_program['days']
            current_program['amount'] = new_program['amount']

            h,m = tm.TimeToNextWatering(current_program)
            timing = str(h) + ':' + str(m)
            print('absolute time until next watering: ',timing)
            client.publish(timing_remaining_path, timing)

    # hardware requesting next schedule 
    if topic == next_schedule:
        h,m = tm.TimeToNextWatering(current_program)
        timing = str(h) + ':' + str(m)
        print('absolute time until next watering: ',timing)
        client.publish(timing_remaining_path, timing)





def on_publish(client, obj, mid):
    print("mid: " + str(mid))



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
mqtt.on_log = on_log

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
'''
'''
try:
    while True:
        pass
        #value = input("input message: ")
        #client.publish("system/ping",value)

except KeyboardInterrupt:

    client.disconnect()
    client.loop_stop()

# develop some functionality for creating reports
