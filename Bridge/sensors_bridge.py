import requests
import json
import configparser
import time
from Pot import Pot
from Pot import ColorEffect
from MQTTClient import MQTTClient
from AIManager import AIManager
# MQTT conn: 2 -> local (post(leds) + sub(+))
#              -> server(QOS settings) (sub(bridge topic))
# HTTP Server(on the other file): cam data
# HTTP Client: post (sensor data)
#       get (bridge/pots_conn_data)
#       post (cam data) (on the other file)
#       


# TODO:
# new connection of new pot
# message translation
# all mqtt stuff for subscribe to server: subscribe, on_message, etc...
# new_conn function (maybe inside pot?)
# merge this file with camera stuff (maybe other things inside pot)
# Camera AI inference and embedding/data send to server

class Bridge():
    local_MQTT: MQTTClient 
    TB_MQTT: MQTTClient

    # BROKERS SETTINGS
    local_sub_topic : str
    remote_sub_topic: str  
    local_pub_topic_base : str    # add to config 
    server_url: str   # remote server for post of pots
    HTTP_access_token: str
    bridge_device_name: str
    

    AIStuff: AIManager
    MAX_SENSORS_VALUES: int                     # number of values to keep stored and to use on regressor

    # RECONNECT PROPERTIES
    FIRST_RECONNECT_DELAY = 1
    RECONNECT_RATE = 1.5
    MAX_RECONNECT_DELAY = 10
    path_config_ini: str
    path_json_pots: str

    pots: list[Pot]


    def __init__(
            self,
            config_path: str,
            regr_model_path: str ="",
            path_json_pots: str ="",
            local_sub_topic : str = "pot/+",
            client_id_local: str = ''):
        self.regr_model_path=regr_model_path
        self.local_sub_topic=local_sub_topic
        self.client_id_local=client_id_local
        self.path_config_ini=config_path
        self.path_json_pots=path_json_pots




    def get_pot(self, pot_MAC: str) :
        for x in self.pots:
            if x.ID_cam==pot_MAC or x.ID_sensors==pot_MAC:
                return x
        return None

    def get_pot_from_name(self, dev_name : str):
        for p in self.pots:
            if p.TB_device_name==dev_name:
                return p
        return None
    
     


    # still to do if you want ?? (maybe first conenction of knwon pots)
    def newPotConn(self,topic: str):
        # send all pot settings
        # the pot should send the secretkey 
        # Then the bridge should use the mqtt gateway api to claim the device and add it to the connection
        return
    
# add check on moisture level if it is over the threshold
# chenge to handle correctly prediction and to check if the model was loaded
    # maybe need to fix the type of value (is it string or int )
    def changePotAttr(self, pot: Pot,key,val):
        message =""
        print("TB:" +str(key)+" : "+str(val))
        if key == "LED_on":
            if val == False:
                pot.LED_on=False
                message="0"
            else:
                message="1"
                pot.LED_on=True
            self.send_pot_attribute(pot=pot,message=message) # change with update at the end
            return
        elif key == "moisture_threshold" :
            pot.moisture_threshold=int(val)
        elif key == "light_threshold" :
            pot.light_threshold=int(val)
            message= "6 "+str(int(val))    
            self.send_pot_attribute(pot=pot,message=message) 
        elif key == "base_color" :
            pass
             ############ TODO # change to use the new class type
        elif key == "water_effect" :
            pass
             ############ TODO # change to use the new class type
        elif key == "AQI_effect" :
            pass
             ############ TODO # change to use the new class type (set the pot.... attribute and it will auto update)
        elif key == "auto_brightness_on" :
            if val=="true":
                pot.auto_brightness_on=True
                self.send_pot_attribute(pot,"7 1")
            else:
                pot.auto_brightness_on=False
                self.send_pot_attribute(pot,"7 0")
            # you need to send message
        elif key == "atuo_brightness_gamma" :
            pot.auto_brightness_gamma=float(val)
            self.send_pot_attribute(pot,"8 "+str(float(val)))
            #you need to send message
    
        self.UpdateLeds(pot)


    def UpdateLeds(self, pot: Pot):
        final_effect=pot.base_color
        
        if pot.data.moisture[pot.data.count-1] < pot.moisture_threshold: # if the measured moisture is lower than threshold
            final_effect=pot.water_effect
        if pot.data.aqi[pot.data.count-1] < Pot.AQI_threshold:
            final_effect=pot.AQI_effect
        if final_effect.toString!=pot.last_effect.toString:
            self.send_pot_attribute(pot,final_effect.toString)
            pot.last_effect=final_effect

    def send_pot_attribute(self, pot:Pot, message: str):
        self.local_MQTT.publish_mess("pot/"+pot.ID_sensors+"/leds",message)
        print("pot/"+pot.ID_sensors+"/leds :  "+message)

    def on_message_local(self,client,userdata,msg):
        message = msg.payload.decode("utf-8")  # Convert bytes to string
        parts = message.split(" ")          # Split at the first space only
        topic_parts = msg.topic.split("/")
        if len(topic_parts)==3 and topic_parts[2]=="/new_conn":
            self.newPotConn(topic_parts[0]+topic_parts[1])
            return
        
        if len(topic_parts)==3 and topic_parts[2]=="/disconnect":   # LWT message received
            id=topic_parts[1]
            p = self.get_pot(id)
            if p is None:
                return
            self.TB_MQTT.publish_mess("v1/gateway/disconnect",f'{{"device": "{p.TB_device_name}"}}')
            p.connected=False
            return
        
        if len(parts) != 5 or len(topic_parts)!=2:
            print("Invalid message or topic:  message: ", message, "  topic:",msg.topic)
            return

        id=topic_parts[1]
        p = self.get_pot(id)
        if p is None:
            # New Unknown Pot Connected
            print("Error: Unknown/Unexpected Pot")
            return
        if p.connected==False:
            self.TB_MQTT.publish_mess("v1/gateway/connect",f'{{"device": "{p.TB_device_name}", "type": "Pot"}}')
            p.connected=True
        d=p.data
        d.temperature.append(int(parts[0]))
        d.humidity.append(int(parts[1]))
        d.light.append(int(parts[2]))
        d.moisture.append(int(parts[3]))
        d.aqi.append(int(parts[4]))
        timestamp=int(time.time()*1000)
        d.timestamp.append(timestamp)
        d.count+=1

        payload = {
            "ts": timestamp,
            "values":{
            "temperature": int(parts[0]),
            "humidity": int(parts[1]),
            "light": int(parts[2]),
            "moisture": int(parts[3]),
            "aqi": int(parts[4])}
        }


        if(d.count>=self.MAX_SENSORS_VALUES and self.AIStuff.is_regr_loaded()):
            pred=self.AIStuff.sensors_regress(p) # remember to edit the pred type
            del p.data.temperature[0]
            del p.data.humidity[0]
            del p.data.light[0]
            del p.data.moisture[0]
            del p.data.aqi[0]
            p.data.count-=1
            p.prediction=pred
            payload["values"]["pred"]=pred 

        try:
            print(json.dumps(payload))
            response = requests.post(self.server_url+p.TB_HTTP_Token+"/telemetry", json=payload, timeout=5)
            if response.status_code != 200:
                print(f"HTTP POST failed: {response.status_code} {response.text}")
        except requests.RequestException as e:
            print("HTTP request error:", e)
        
        self.UpdateLeds(p)

    def on_connect_local(self,client, userdata, flags, rc, properties):
        self.local_MQTT.subscribe(topic=self.local_sub_topic)
                



    # still to modidfy for the remote connection  everything
    def on_message_remote(self,client,userdata,msg):

        # Message: {"device": "Device A", "data": {"attribute1": "value1", "attribute2": 42}}

        message = msg.payload.decode("utf-8")  # Convert bytes to string
        parsed=json.loads(message)
        pot=self.get_pot_from_name(parsed["device"])  # write get from device name
        if pot == None:
            print("Received message from Thingsboard for unknown device")
            return
        for key,val in parsed["data"].items():
            self.changePotAttr(pot,key,val)
       
        
    # to add ?? (maybe done)
    def  on_connect_remote(self,client, userdata, flags, rc, properties):
        self.TB_MQTT.subscribe(self.local_sub_topic)
        for dev in self.pots:
            if dev.connected:
                self.TB_MQTT.publish_mess("v1/gateway/connect",f'{{"device": "{dev.TB_device_name}","type": "Pot"}}')

    
    def mqtt_remote_start(self):
        if self.TB_MQTT != None:
            self.TB_MQTT.connect()

    def mqtt_local_start(self):
        if self.local_MQTT != None:
            self.local_MQTT.connect()

    #define function for new connectionn of a known pot



    def read_config(self):
        config1 =configparser.ConfigParser()

        if  len(config1.read(self.path_config_ini))<1:
            print("ERROR: The specified config.ini file couldn't be opened")
            return                          # Possibly change
        
        self.path_json_pots =               config1.get("POTS","JSONSettPath",fallback="./pots.json")
        regr_model_path =              config1.get("MODELS","Regression_model_path",fallback="regr.pkl")
        yolo_model_path =              config1.get("MODELS","YOLO_model_path",fallback="yolov8m.pt")
        self.MAX_SENSORS_VALUES =           config1.getint("MODELS","MAX_SENSORS_VALUES",fallback=10)
        broker_local =                      config1.get("LOCAL_BROKER","Local_borker_address",fallback="127.0.0.1")
        port_local =                        config1.getint("LOCAL_BROKER","Local_broker_port",fallback=1883)
        
        self.local_sub_topic =              config1.get("LOCAL_BROKER","Local_MQTT_topic",fallback="pot/+")
        port_remote=                        config1.getint("THINGSBOARD","MQTT_port",fallback=1883)
        remote_broker_name =                config1.get("THINGSBOARD","Server_name",fallback="DESKTOP-RQ61EI4")
        remote_MQTT_username =              config1.get("THINGSBOARD","MQTT_username",fallback='')
        remote_MQTT_pass =                  config1.get("THINGSBOARD","MQTT_password",fallback='')
        self.server_url =                   config1.get("THINGSBOARD","Server_URL",fallback="http://DESKTOP-RQ61EI4.local:8080/devices/api/v1/")
        self.HTTP_access_token =            config1.get("THINGSBOARD","HTTP_access_token")
        self.bridge_device_name=            config1.get("THINGSBOARD","device_name")
        self.remote_sub_topic=              config1.get("THINGSBOARD","MQTT_sub_topic") 

        self.local_MQTT=MQTTClient(broker_local,port_local,'Local bridge',self.on_connect_local, self.on_message_local)
        self.TB_MQTT=MQTTClient(remote_broker_name,port_remote,'Thingsboard bridge',self.on_connect_remote, self.on_message_remote,remote_MQTT_username,remote_MQTT_pass)
        self.AIStuff=AIManager(regr_model_path,yolo_model_path)   

    def read_pots(self):
        with open(self.path_json_pots, "r") as f:
            data = json.load(f)
            self.pots = [Pot.from_dict(d) for d in data]

    def write_pots(self):
        with open(self.path_json_pots, "w") as f:
            json.dump([s.to_dict() for s in self.pots], f)



if __name__ == '__main__':
    br = Bridge(config_path="config.ini")

    br.read_config()
    br.read_pots()
    br.mqtt_local_start()
    br.mqtt_remote_start()
    # add mqtt subscribe to remote
    print("Started successfully")
    while True:
        pass

