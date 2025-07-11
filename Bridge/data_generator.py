from typing import Generator
from collections.abc import Callable
import time
from functools import partial
import random
import json
import requests

period: float =60
days: int =1
hours: int = 0
minutes: int = 0
starting_time: float = 
http_url : str="http://DESKTOP-RQ61EI4.local:8080/api/v1/OenEZRx55rYiYpRmuI7y/telemetry"
data: list[tuple[str, Callable[[], float]]] = [
    ("temperature", partial(random.gauss, 100, 10)),
    ("humidity", partial(random.uniform, 30, 60)),
    ("pressure", partial(random.gauss, 1013, 5)),
    ("wind_speed", partial(random.expovariate, 1/5)),
    ("noise", partial(random.uniform, 20, 80)),
]



def send(url: str, payload: dict):
    try:
        print(json.dumps(payload))
        response = requests.post(url, json=payload, timeout=5)
        if response.status_code != 200:
            print(f"HTTP POST failed: {response.status_code} {response.text}")
    except requests.RequestException as e:
        print("HTTP request error:", e)

def generate():
    duration= ((days*24+hours)*60+minutes)*60
    curr_time=starting_time
    while curr_time<starting_time+duration:
        payload: dict={'ts':(int(curr_time*1000))}
        pay=payload["values"]
        for key, generator in data:
           pay[key] = generator()
        send(http_url,payload)
        curr_time+=period


