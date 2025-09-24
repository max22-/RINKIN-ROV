#!/usr/bin/python3
import asyncio
from smbus2 import SMBus
from pythonosc import udp_client
from pythonosc.osc_server import AsyncIOOSCUDPServer
from pythonosc.dispatcher import Dispatcher
import serial
from cmps12 import CMPS12

async def imu_loop():
    imu = CMPS12(SMBus(1))
    client = udp_client.SimpleUDPClient("192.168.4.1", 7770)
    while True:
        imu.update()
        print(f"heading = {imu.heading()}\tpich={imu.pitch()}\troll={imu.roll()}")
        client.send_message("/imu/heading", imu.heading())
        client.send_message("/imu/pitch", imu.pitch())
        client.send_message("/imu/roll", imu.roll())
        await asyncio.sleep(0.1)


ser = serial.Serial("/dev/ttyS0", 115200)

def motor(address, *args):
    print(f"{address:}: {args}")
    ser.write(int.to_bytes(args[0]))


dispatcher = Dispatcher()
dispatcher.map("/motor", motor)


async def main():
    imu_task = asyncio.create_task(imu_loop())
    server = AsyncIOOSCUDPServer(("0.0.0.0", 7770), dispatcher, asyncio.get_event_loop())
    transport, protocol = await server.create_serve_endpoint()
    await imu_task
    transport.close()

asyncio.run(main())

ser.close()
