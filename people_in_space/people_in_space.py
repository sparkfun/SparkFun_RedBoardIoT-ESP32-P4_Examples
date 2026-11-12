################################################################################
# Configure the example using the settings below before running it!
################################################################################

# Select 'LAN' for Ethernet or 'WLAN' for Wi-Fi
network_if = 'LAN'
# network_if = 'WLAN'

# If you're using WLAN (Wi-Fi), set your SSID and password below
wlan_ssid = 'your-ssid'
wlan_password = 'your-password'

# Select whether to use the serial LCD display. To install with mip, run:
# mpremote mip install github:sparkfun/qwiic_serlcd_py
# use_serlcd = False
use_serlcd = True

################################################################################
# Configure the example using the settings above before running it!
################################################################################

# Import necessary modules
import network
import urequests
import time
import neopixel
import machine

# Set up the NeoPixel LED and LCD (if used)
led = neopixel.NeoPixel(machine.Pin.board.NEOPIXEL, 1)
if use_serlcd:
    import qwiic_serlcd
    myLCD = qwiic_serlcd.QwiicSerlcd()

# Set the LED to red while connecting
led[0] = (255, 0, 0)
led.write()
if use_serlcd:
    myLCD.setBacklight(255, 0, 0)

print("Waiting for connection...")
if use_serlcd:
    myLCD.clearScreen()
    myLCD.print("Waiting for")
    myLCD.setCursor(0,1)
    myLCD.print("connection...")

# Connect to the network
if network_if == 'LAN':
    net = network.LAN(
        phy_addr = 0,
        phy_type=network.PHY_GENERIC,
        mdc=22,
        mdio=21,
        crs_dv=28,
        rxd0=29,
        rxd1=30,
        tx_en=33,
        txd0=34,
        txd1=35,
        clk_out=23,
        clk_in=32,
    )
    net.active(True)
elif network_if == 'WLAN':
    net = network.WLAN(network.STA_IF)
    net.active(True)
    net.connect(wlan_ssid, wlan_password)

# Wait for the connection to complete
while not net.isconnected():
    time.sleep(1)
    print('.', sep='', end='', )

# Print IP address of the device
print('Connected! IP address:', net.ipconfig('addr4')[0])
if use_serlcd:
    myLCD.clearScreen()
    myLCD.print(net.ipconfig('addr4')[0])

# Set the LED to blue while downloading data
led[0] = (0, 0, 255)
led.write()
if use_serlcd:
    myLCD.setBacklight(0, 0, 255)

# Download the list of people currently in space
downloaded = False
while not downloaded:
    try:
        people = urequests.get("https://corquaid.github.io/international-space-station-APIs/JSON/people-in-space.json").json()
        downloaded = True
    except:
        print('Failed to download, retrying...')
        time.sleep(1)

# Set the LED to green to indicate success
led[0] = (0, 255, 0)
led.write()
if use_serlcd:
    myLCD.setBacklight(0, 255, 0)

# Print the number of people and their names
number = people['number']
print('There are', number, 'people in space.')
for i in range(number):
    print(i+1, people['people'][i]['name'])

# Display the number of people in space on the LCD if used
if use_serlcd:
    # Get the button to use for cycling through people in space
    btn = machine.Pin.board.BUTTON
    btn.init(btn.IN)

    # Show the initial number of people in space on the LCD
    myLCD.clearScreen()
    myLCD.print(net.ipconfig('addr4')[0])
    myLCD.setCursor(0,1)
    myLCD.print(str(number) + " ppl in space!")

    # Cycle through the people in space when the button is pressed
    idx = 0
    while True:
        # Wait for the button to be pressed and released
        pressed = False
        while btn() == 0:
            pressed = True

        # Update the display with the next person in space
        if pressed:
            # Increment index, wrapping around if necessary
            idx += 1
            if idx > number:
                idx = 1

            # Display the next person in space on the LCD
            myLCD.clearScreen()
            myLCD.setCursor(0,0)
            myLCD.print("Person #" + str(idx) + "/" + str(number))
            myLCD.setCursor(0,1)
            myLCD.print(people['people'][idx-1]['name'])

