from dataclasses import dataclass, field
from collections import deque
import time

MAX_SENSORS_VALUES=10

@dataclass
class SensorData:
    temperature: list= field(default_factory=list)
    humidity: list= field(default_factory=list)
    light: list= field(default_factory=list)
    moisture: list= field(default_factory=list)
    aqi: list= field(default_factory=list)
    timestamp : list= field(default_factory=list)
    count: int = 0 
data_dict = {}



HTTP_SERVER_URL="thingsboard" # TO FIX



import requests
import json
from paho.mqtt import client as mqtt_client
import logging
from collections import defaultdict


# TOPICS DEFINITION
topic = "pot/+" 
 



client_id= 'bridge-subscribe'


# BROKER SETTINGS

broker = '0.0.0.0'
port = 1883



# RECONNECT PROPERTIES
FIRST_RECONNECT_DELAY = 1
RECONNECT_RATE = 1.5
MAX_RECONNECT_DELAY = 10

def reconnection(client, userdata, rc):
    logging.info(f'{client.client_id} : Disconnected with result code: {rc}')
    reconnect_delay =FIRST_RECONNECT_DELAY
    while True:
        logging.info(f'{client.client_id} : Reconnecting in  {reconnect_delay} seconds...')
        time.sleep(reconnect_delay)

        try:
            client.reconnect()
            logging.info(f'{client.client_id} : Reconnected successfully!')
            return
        except Exception as err:
            logging.error(f'{client.client_id} : {err}. Reconnect failed. Retrying...')
        
        reconnect_delay *= RECONNECT_RATE
        reconnect_delay = min(reconnect_delay, MAX_RECONNECT_DELAY)
        


def on_message(client,userdata,msg):
    message = msg.payload.decode("utf-8")  # Convert bytes to string
    parts = message.split(" ")          # Split at the first space only
    topic_parts = msg.topic.split("/")
    if len(parts) != 5 or len(topic_parts)!=2:
        print("Invalid message or topic:  message: ", message, "  topic:",msg.topic)
        return
    
    id=topic_parts[1]
    d = data_dict.get(id)
    if d is None:
        d = SensorData()
        data_dict[id] = d  # Necessary only on first creation
    d.temperature.append(parts[0])
    d.humidity.append(parts[1])
    d.light.append(parts[2])
    d.moisture.append(parts[3])
    d.aqi.append(parts[4])
    timestamp=time.time()
    d.timestamp.append(time.time())
    d.count+=1

    try:
        payload = {
            "ts": timestamp,
            "values":{
            "id": id,
            "temperature": parts[0],
            "humidity": parts[1],
            "light": parts[2],
            "moisture": parts[3],
            "aqi": parts[4]}
        }

        response = requests.post(HTTP_SERVER_URL, json=payload, timeout=5)
        if response.status_code != 200:
            print(f"HTTP POST failed: {response.status_code} {response.text}")
    except requests.RequestException as e:
        print("HTTP request error:", e)


    if(d.count>=MAX_SENSORS_VALUES):
        predict_sensors(id)
    

def connect_mqtt():
    def on_connect(client, userdata, flags, rc, properties):
        if rc == 0:
            print("Connected to MQTT Broker!")
        else:
            print("Failed to connect, return code %d\n", rc)
    # Set Connecting Client ID
    clients=[]

    client= mqtt_client.Client(client_id=client_id, callback_api_version=mqtt_client.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.connect(broker, port)
    client.on_disconnect=reconnection
    return client



def subscribe(client: mqtt_client):
    client.subscribe(topic)
    client.on_message = on_message





if __name__ == '__main__':
    client=connect_mqtt()
    subscribe(client)
