#!/bin/bash
set -euxo pipefail

# Activer l'I2C
sudo raspi-config nonint do_i2c 0
# Activer le port série
sudo raspi-config nonint do_serial_hw 0
# Pas de console sur le port série
sudo raspi-config nonint do_serial_cons 1

sudo apt update
sudo apt install python3-pip -y

# Installation de Mediamtx (caméra)
cd /home/rinkin/setup/mediamtx
sudo cp -u mediamtx /usr/bin/
sudo cp -u mediamtx.yml /etc/
sudo cp -u mediamtx.service /etc/systemd/system
sudo systemctl daemon-reload
sudo systemctl enable mediamtx.service
sudo systemctl start mediamtx.service

# Installation du programme en go
cd ~/rinkin
sudo cp -u rinkin.service /etc/systemd/system
sudo systemctl daemon-reload
sudo systemctl enable rinkin.service
sudo systemctl start rinkin.service

