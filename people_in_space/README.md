# People in Space

A MicroPython script that connects to the internet over Ethernet or Wi-Fi, downloads the list of people currently in space from the [International Space Station APIs](https://github.com/corquaid/international-space-station-APIs) project, and prints their names. If a Qwiic SerLCD is connected, it also shows the count on the display, and you can press the board's button to step through the names.

## Hardware

- SparkFun IoT RedBoard - ESP32-P4
- USB-C cable
- One of:
  - An Ethernet cable connected to a network with internet access
  - A Wi-Fi network with internet access
- Optional: a [SparkFun Qwiic SerLCD](https://www.sparkfun.com/catalogsearch/result/?q=serlcd) (16x2) and a Qwiic cable
  - Also install the Qwiic Serial LCD Python driver with [mpremote](https://docs.micropython.org/en/latest/reference/mpremote.html):
  - `mpremote connect <port> mip install github:sparkfun/qwiic_serlcd_py`

If you're using the SerLCD, plug it into the board's Qwiic connector.

## Configure the example

Open [people_in_space.py](people_in_space.py) and edit the settings at the top:

| Setting | Description |
|---|---|
| `network_if` | `'LAN'` for Ethernet (the default) or `'WLAN'` for Wi-Fi. |
| `wlan_ssid`, `wlan_password` | Your Wi-Fi network's name and password. Only used with `'WLAN'`. |
| `use_serlcd` | `True` (the default) to use the Qwiic SerLCD, or `False` if you don't have one connected. |

## Run the example

Run the script on the board. You can do this with [Thonny](https://thonny.org/) by opening the file and clicking **Run**, or with mpremote:

```bash
mpremote connect <port> run people_in_space.py
```

The RGB LED (and the SerLCD backlight, if used) shows what the script is doing:

| Color | Meaning |
|---|---|
| Red | Connecting to the network |
| Blue | Downloading the list of people in space |
| Green | Done |

The serial output should look something like this. The names will change as crews come and go.

```
Waiting for connection...
...Connected! IP address: 192.168.1.42
There are 10 people in space.
1 Jane Doe
2 John Smith
...
```

If the download fails, the script retries every second until it succeeds.

With the SerLCD, the display shows the board's IP address and the number of people in space. Press the button to show the next person's name. After the last person, it goes back to the first.
