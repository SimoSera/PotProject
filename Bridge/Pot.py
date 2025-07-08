from __future__ import annotations
from dataclasses import dataclass, field
import json
import numpy
from enum import Enum



class Effect(Enum):   # used this numeration and not 0-2 because this way it's compatible with the MC with the leds 
    STATIC = 2
    BREATHING = 3
    CIRCLING = 4
    

# check if order of message is correct (wrgb) and the range of speed
class ColorEffect:
    red: int            # Colors are in range 0-255
    green : int
    blue : int
    white : int
    speed: int          # range ?????
    effect : Effect

    def __init__(self,white:int=0,red : int = 0,green: int =0, blue :int =0, effect: Effect = Effect.STATIC,speed: int=1):
        self.white=white
        self.red=red
        self.green = green
        self.blue = blue
        self.effect = effect
        self.speed=1

    @property
    def toString(self) -> str:
        string : str =""+str(self.effect.value) +" "+ str(self.white)+" "+ str(self.red)+" "+ str(self.green)+" "+ str(self.blue) 
        if self.effect == Effect.BREATHING or self.effect== Effect.CIRCLING:
               string+=" "+str(self.speed)
        return string

    
def EffectFromString(s: str) -> ColorEffect:
    values= s.split()
    if len(values) < 5 or len(values)> 6:
        return ColorEffect()
    elif len(values)==6 :
        return ColorEffect(effect=Effect(int(values[0])),white=int(values[1]), red=int(values[2]),green=int(values[3]),blue=int(values[4]),speed=int(values[5]))
    else:
        return ColorEffect(effect=Effect(int(values[0])),white=int(values[1]), red=int(values[2]),green=int(values[3]),blue=int(values[4]))
    




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




class Pot():


    AQI_threshold: int =  4 # NOT SETTABLE BUT STATIC DEFINED BY STANDARDS
                        # USED IN SENSORSBRIDGE WHEN CHECK TO SET THE COLORS
                        # IF THE CURRENT AQI>= THIS THRESHOLD -> BAD AIR QUALITY

    ID_sensors: str
    ID_cam :str
    TB_device_name: str
    TB_HTTP_Token: str
    LED_on: bool = False
    moisture_threshold: int
    light_threshold: int
    base_color: ColorEffect
    water_effect: ColorEffect
    AQI_effect: ColorEffect
    ill_effect: ColorEffect
    auto_brightness_on: bool = True
    auto_brightness_gamma: float = 0.1
    data: SensorData
    connected: bool =False
    last_effect: ColorEffect
    brightness: int
    healthy_leaves: int
    ill_leaves: int
    healthy: bool
    

    def __init__(self,
                ID_sensors: str,
                ID_cam :str,
                TB_device_name: str ,
                TB_HTTP_Token: str,
                LED_on: bool = False,
                moisture_threshold: int = 30,
                light_threshold: int = 400,
                base_color: ColorEffect= ColorEffect(255,0,0,0,Effect.STATIC),
                water_effect: ColorEffect = ColorEffect(100,0,0,170,Effect.BREATHING),
                AQI_effect: ColorEffect= ColorEffect(0,170,0,0,Effect.CIRCLING),
                ill_effect: ColorEffect = ColorEffect(100,255,50,0,Effect.BREATHING),
                auto_brightness_on: bool =True,
                auto_brightness_gamma: float =0.1,
                brightness: int=100,
                healthy_leaves: int = 0,
                ill_leaves: int = 0,
                healthy: bool = False,
                ):
        self.ID_cam=ID_cam
        self.ID_sensors=ID_sensors
        self.data=SensorData()
        self.TB_HTTP_Token=TB_HTTP_Token
        self.LED_on=LED_on
        self.moisture_threshold=moisture_threshold
        self.light_threshold=light_threshold
        self.base_color=base_color
        self.water_effect=water_effect
        self.AQI_effect=AQI_effect
        self.auto_brightness_on=auto_brightness_on
        self.auto_brightness_gamma=auto_brightness_gamma
        self.TB_device_name=TB_device_name
        self.last_effect=self.base_color
        self.brightness=brightness
        self.ill_effect=ill_effect
        self.healthy=healthy


    @classmethod
    def from_dict(cls,d: dict):
        settings = d["Settings"]
        return cls(ID_cam=d["MAC_cam"],
        ID_sensors=d["MAC_sensors"],
        TB_device_name=d["TB_device_name"],
        TB_HTTP_Token=d["TB_HTTP_token"],
        LED_on=settings["LED_on"],
        moisture_threshold=settings["moisture_threshold"],
        light_threshold=settings["light_threshold"],
        base_color=EffectFromString(settings["base_color"]),
        water_effect=EffectFromString(settings["water_effect"]),
        AQI_effect=EffectFromString(settings["AQI_effect"]),
        auto_brightness_on=settings["auto_brightness_on"],
        auto_brightness_gamma=settings["auto_brightness_gamma"],
        brightness=settings["brightness"], 
        ill_effect=settings["ill_effect"],
        healthy=settings["healthy"])

    def to_dict(self) -> dict:
        return {
            "MAC_cam": self.ID_cam,
            "MAC_sensors": self.ID_sensors,
            "TB_HTTP_token": self.TB_HTTP_Token,
            "TB_device_name":self.TB_device_name,
            "Settings": {
                "LED_on": self.LED_on,
                "moisture_threshold": self.moisture_threshold,
                "light_threshold": self.light_threshold,
                "base_color": self.base_color.toString,
                "water_effect": self.water_effect.toString,
                "AQI_effect": self.AQI_effect.toString,
                "auto_brightness_on": self.auto_brightness_on,
                "auto_brightness_gamma": self.auto_brightness_gamma,
                "brightness": self.brightness,
                "ill_effect" : self.ill_effect,
                "healthy" : self.healthy
            }
        }

    def set_connected(self):
        self.connected=True
    
    def set_disconnected(self):
        self.connected=False

    

