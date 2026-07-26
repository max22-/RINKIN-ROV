package main

import (
	"fmt"
	"log"
	"net"
	"os"
	"simulation/rov"
)

func main() {
	addr, err := net.ResolveUDPAddr("udp", "0.0.0.0:1234")
	if err != nil {
		log.Fatal("failed to resolve address: ", err)
	}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		log.Fatal("failed to start listening: ", err)
	}
	defer conn.Close()

	var clientAddr *net.UDPAddr = nil

	udpChan, addrChan := udpRecv(conn)

	rov := rov.New()
	rov.DisplayMatrix()

	for {
		select {
		case addr := <-addrChan:
			clientAddr = addr
			rov.Start()
		case udpPacket := <-udpChan:
			fmt.Println("received command: ", udpPacket)
			rov.Cmd <- udpPacket
		case info := <-rov.Info:
			fmt.Println("sending ", info)
			conn.WriteToUDP([]byte(info), clientAddr)
		}
	}
}

func udpRecv(conn *net.UDPConn) (<-chan string, chan *net.UDPAddr) {
	msgChan := make(chan string, 10)
	addrChan := make(chan *net.UDPAddr)
	go func() {
		buffer := make([]byte, 1500)
		for {
			n, clientAddr, err := conn.ReadFromUDP(buffer)
			if err != nil {
				fmt.Fprintln(os.Stderr, err)
				continue
			}
			msg := string(buffer[:n])
			addrChan <- clientAddr
			msgChan <- string(msg)
		}
	}()
	return msgChan, addrChan
}
