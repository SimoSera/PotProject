from typing import Generator
from collections.abc import Callable
import time
from functools import partial
import random
import json
import requests

period: float
days: int
hours: int
minutes: int
starting_time: float
http_url : str="http://desktop-...:8080/api/v1/telemetry"
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
        payload={'ts':curr_time}
        for key, generator in data:
            payload[key] = generator()
        send(http_url,payload)
        curr_time+=period


