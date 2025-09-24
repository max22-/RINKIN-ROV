from smbus2 import SMBus, i2c_msg

class CMPS12:
    def __init__(self, bus, addr = 0x60):
        self.bus = bus
        self.addr = addr
        self.data = []

    def update(self):
        msg1 = i2c_msg.write(self.addr, [1])
        msg2 = i2c_msg.read(self.addr, 6)
        self.bus.i2c_rdwr(msg1, msg2)
        self.data = [1] + list(msg2)

    def heading(self):
        return ((self.data[2] << 8) + self.data[3]) / 10

    def pitch(self):
        return float(self.i8(self.data[4]))

    def roll(self):
        return float(self.i8(self.data[5]))

    def i8(self, x):
        if x > 127:
            return x - 256
        else:
            return x



if __name__ == "__main__":
    import time
    from pythonosc import udp_client
    client = udp_client.SimpleUDPClient("192.168.1.15", 7770) 
    imu = CMPS12(SMBus(1))
    while True:
        imu.update()
        print(f"heading = {imu.heading()}\tpich={imu.pitch()}\troll={imu.roll()}")
        client.send_message("/imu/heading", imu.heading())
        client.send_message("/imu/pitch", imu.pitch())
        client.send_message("/imu/roll", imu.roll())
        time.sleep(0.1)
