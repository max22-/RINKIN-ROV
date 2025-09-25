#!/usr/bin/python3
import asyncio
from smbus2 import SMBus
from paho.mqtt import client as mqtt_client
from paho.mqtt.enums import CallbackAPIVersion
#import serial
from cmps12 import CMPS12

async def imu_loop(client):
    imu = CMPS12(SMBus(1))
    while True:
        imu.update()
        print(f"heading = {imu.heading()}\tpich={imu.pitch()}\troll={imu.roll()}")
        client.publish("/imu/heading", imu.heading())
        client.publish("/imu/pitch", imu.pitch())
        client.publish("/imu/roll", imu.roll())
        
        await asyncio.sleep(0.1)

def on_mqtt_connect(client, userdata, flags, rc, properties):
    print(f"on_mqtt_connect, rc={rc}")
    client.subscribe("/motor")

def on_message(client, userdata, msg):
    print(f"topic: {msg.topic}, payload: {msg.payload.decode()}")
    

client = mqtt_client.Client(CallbackAPIVersion(2), client_id="raspberry-pi")
client.on_connect = on_mqtt_connect
client.on_message = on_message
client.connect("localhost", 1883)

client.loop_start()

async def main():
    imu_task = asyncio.create_task(imu_loop(client))
    await imu_task

asyncio.run(main())

"""

def motor(address, *args):
    print(f"{address:}: {args}")
    ser.write(int.to_bytes(args[0]))

ser = serial.Serial("/dev/ttyS0", 115200)

ser.close()

"""