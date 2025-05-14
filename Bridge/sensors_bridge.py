from dataclasses import dataclass, field
from collections import deque
import time
from sklearn.ensemble import RandomForestRegressor
import joblib
import os
import requests
import json
from paho.mqtt import client as mqtt_client
import logging
from collections import defaultdict
import configparser

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




class Bridge():


    sensors_regress_model : RandomForestRegressor   # model to be used for regressoin on sensors values
    model_path : str                                # path of the model weights 
    MAX_SENSORS_VALUES: int =10                     # number of values to keep stored and to use on regressor
    http_post_sensors_url: str                            # HTTP server of the Thingsboard server  
    http_get_config_url : str
    topic : str = "pot/+" 
    client_id : str = 'bridge-subscribe'
    # BROKER SETTINGS
    broker = '127.0.0.1'
    port = 1883
    
    # RECONNECT PROPERTIES
    FIRST_RECONNECT_DELAY = 1
    RECONNECT_RATE = 1.5
    MAX_RECONNECT_DELAY = 10
    config_path: str
    config : configparser.ConfigParser


    def __init__(
            self,
            model_path: str,
            http_post_sensors_url: str,
            http_get_config_url : str,
            config_path: str,
            topic : str = "pot/+",
            client_id: str = '',
            broker:str ='127.0.0.1',
            port:int =1883):
        self.model_path=model_path
        self.http_post_sensors_url=http_post_sensors_url
        self.topic=topic
        self.client_id=client_id
        self.broker=broker
        self.port=port
        self.config=configparser.ConfigParser()
        self.config_path=config_path
        self.http_get_config_url=http_get_config_url


    def load_model(self):
        if os.path.exists(self.model_path):
            try:
                self.sensors_regress_model = joblib.load(self.model_path)
                print("Model loaded successfully.")
            except Exception as e:
                print(f"Error loading model: {e}")
                self.sensors_regress_model = None
        else:
            print("Model file not found.")
            self.sensors_regress_model = None


    



    # model = RandomForestRegressor()
    # model.fit(X, y)

    # # Save model to file
    # joblib.dump(model, 'regressor_model.pkl')

    def sensors_regress(self,data:SensorData):
        reg_data=[data.temperature,data.humidity,data.light,data.moisture,data.aqi]
        prediction=self.sensors_regress_model.predict(reg_data)
        del data.temperature[0]
        del data.humidity[0]
        del data.light[0]
        del data.moisture[0]
        del data.aqi[0]
        data.count-=1
        return prediction


  

    def reconnection(self,client, userdata, rc):
        logging.info(f'{client.client_id} : Disconnected with result code: {rc}')
        reconnect_delay = self.FIRST_RECONNECT_DELAY
        while True:
            logging.info(f'{client.client_id} : Reconnecting in  {reconnect_delay} seconds...')
            time.sleep(reconnect_delay)

            try:
                client.reconnect()
                logging.info(f'{client.client_id} : Reconnected successfully!')
                return
            except Exception as err:
                logging.error(f'{client.client_id} : {err}. Reconnect failed. Retrying...')
            
            reconnect_delay *= self.RECONNECT_RATE
            reconnect_delay = min(reconnect_delay, self.MAX_RECONNECT_DELAY)
            


    # Variables that are set from




    def on_message(self,client,userdata,msg):
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


        if(d.count>=self.MAX_SENSORS_VALUES and self.sensors_regress_model!=None):
            pred=self.sensors_regress(id)
            payload["values"]["pred"]=pred    

        try:
            response = requests.post(self.http_post_sensors_url, json=payload, timeout=5)
            if response.status_code != 200:
                print(f"HTTP POST failed: {response.status_code} {response.text}")
        except requests.RequestException as e:
            print("HTTP request error:", e)



    def connect_mqtt(self):
        def on_connect(client, userdata, flags, rc, properties):
            if rc == 0:
                print("Connected to MQTT Broker!")
            else:
                print("Failed to connect, return code %d\n", rc)
        # Set Connecting Client ID
        clients=[]

        client= mqtt_client.Client(client_id=self.client_id, callback_api_version=mqtt_client.CallbackAPIVersion.VERSION2)
        client.on_connect = on_connect
        client.connect(self.broker, self.port)
        client.on_disconnect=self.reconnection
        return client



    def subscribe(self,client: mqtt_client):
        client.subscribe(self.topic)
        client.on_message = self.on_message



    def read_config(self,):
        self.config.read(self.config_path)

    

if __name__ == '__main__':
    br = Bridge(http_post_sensors_url="http://DESKTOP-RQ61EI4:8080/api/v1/PrbP2jlNNpjp8hJ5FcOV/telemetry",
                http_get_config_url="http://DESKTOP-RQ61EI4:8080/api/v1/???",
                model_path="regressor_model.pkl",config_path="config.ini")
    br.read_config()

    client=br.connect_mqtt()
    br.subscribe(client)
    client.loop_forever()
