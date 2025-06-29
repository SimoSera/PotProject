from paho.mqtt import client as mqtt_client
from paho.mqtt.enums import CallbackAPIVersion
import logging
import time
from typing import Callable, Any

void_function = lambda *args, **kwargs: None


class MQTTClient:
    broker: str             # either IP address or Domain Name
    broker_port: int
    client_id :str
    client: mqtt_client.Client = None
    MQTT_username: str
    MQTT_pass: str



    on_connect_additional :  Callable[[Any, Any, Any, int, Any], None]
    on_message: Callable[[ Any, Any, Any], None]
    # RECONNECT PROPERTIES
    FIRST_RECONNECT_DELAY = 1
    RECONNECT_RATE = 1.5
    MAX_RECONNECT_DELAY = 10
#    remote_sub_topic: 

# add on connect additional
    def __init__(self,
                broker: str ='127.0.0.1',
                broker_port : int = 1883,
                client_id: str = 'client',
                on_connect_additional:Callable[[Any, Any, Any, int, Any], None] = void_function,
                on_message: Callable[[Any, Any, Any], None] = void_function,
                MQTT_username: str = '',
                MQTT_pass: str = '',
                FIRST_RECONNECT_DELAY : float = 1,
                RECONNECT_RATE: float = 1.5,
                MAX_RECONNECT_DELAY: float = 10) :
            self.broker=broker
            self.broker_port=broker_port
            self.client_id=client_id
            self.MQTT_username=MQTT_username
            self.MQTT_pass=MQTT_pass
            self.FIRST_RECONNECT_DELAY=FIRST_RECONNECT_DELAY
            self.RECONNECT_RATE=RECONNECT_RATE
            self.MAX_RECONNECT_DELAY=MAX_RECONNECT_DELAY
            self.on_connect_additional=on_connect_additional
            self.on_message=on_message

    def reconnection(self,client,disconnect_flags, reason_code, properties,rc):
        logging.info(f'{self.client_id} : Disconnected with result code: {rc}')
        reconnect_delay = self.FIRST_RECONNECT_DELAY
        while True:
            logging.info(f'{self.client_id} : Reconnecting in  {reconnect_delay} seconds...')
            time.sleep(reconnect_delay)

            try:
                client.reconnect()
                logging.info(f'{self.client_id} : Reconnected successfully!')
                return
            except Exception as err:
                logging.error(f'{self.client_id} : {err}. Reconnect failed. Retrying...')

            reconnect_delay *= self.RECONNECT_RATE
            reconnect_delay = min(reconnect_delay, self.MAX_RECONNECT_DELAY)



    def connect(self):
        
        def on_connect(client: mqtt_client.Client, userdata, flags, rc, properties):
            if rc.is_failure:
               print(f"Failed to connect, return code {rc}\n")
            else:
                print(f'{client._client_id} Connected to MQTT Broker!')
                self.on_connect_additional(client, userdata, flags, rc, properties)
                

        self.client= mqtt_client.Client(client_id=self.client_id, callback_api_version=CallbackAPIVersion.VERSION2)

        if  self.MQTT_username!='' and self.MQTT_pass!='':
            self.client.username_pw_set(self.MQTT_username,self.MQTT_pass)
        print(f"Username {self.MQTT_username}  pass: {self.MQTT_pass}")
        self.client.on_connect = on_connect
        print(self.broker+":"+str(self.broker_port))
        self.client.connect(self.broker, self.broker_port)
        self.client.on_disconnect=self.reconnection
        self.client.loop_start()  


        
    def subscribe(self, topic: str, on_message = None):
        if not self.client or not self.client.is_connected():
            raise RuntimeError("Client not connected yet. Call connect() first.")
        self.client.subscribe(topic,qos=1)
        if on_message!=None:
            self.on_message=on_message
        self.set_on_message(self.on_message)
        

    def set_on_message(self,on_mess):
        if not self.client:
            raise RuntimeError("Client not connected yet. Call connect() first.")
        self.client.on_message=on_mess

    def publish_mess(self, topic:str ,message:str, qos=1):
        if not self.client or not self.client.is_connected():
            raise RuntimeError("Client not connected yet. Call connect() first.")
        self.client.publish(topic,message,qos)