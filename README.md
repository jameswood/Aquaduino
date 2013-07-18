Aquaduino
=========

## Arduino-based fishtank control system

###Requires
* Arduino Uno
* Ethernet Shield
* LCD shield (optional)

###Features
* Controls fishtank lighting
* Connects to NTP server to sync Arduino's unreliable clock
* Connects to Earthtools to determine sunrise and sunset times
 
###Plans
* Determine lat/long from public IP
* Add control for servo-operated food dispenser
* Switch to I2C LCD to save pins! (currently there is no PWM-enabled pin available for servo)
