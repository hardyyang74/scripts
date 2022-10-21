#!/bin/sh

startedwifi=1
sleep 5
while [ true ]; do
  # neigh=`grep "192.168.88." /proc/net/arp`
  hasneigh=`/oem/bin/testwifi -c /var/run/hostapd/p2p0 | awk -F: '{print $2}'`
  # echo "[hardy] hasneigh:$hasneigh"

  if [ "x0" = "x$hasneigh" -a "x0" = "x$startedwifi" ]; then
    echo "[hardy] connect"
#    wifi_start.sh "rk360" "12345678"
    wpa_cli -i wlan0 reconnect
    startedwifi=1
  elif [ "x0" != "x$hasneigh" -a "x1" = "x$startedwifi" ]; then
    echo "[hardy] disconnect"
    wpa_cli -i wlan0 disconnect
#    killall wpa_supplicant
#    ifconfig wlan0 down
#    route del -net 169.254.0.0 netmask 255.255.0.0

    startedwifi=0
  fi

  sleep 3
done

