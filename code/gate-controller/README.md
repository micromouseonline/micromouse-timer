# Timing gate Controller

All of the actual timing decisions are made by the gate controller. This device is an Arduino nano with a number of attached peripheral devices incuding a radio receiver that listens for event essages from the individual gates. The controller can be configured for a number of different rule sets and contest types. For the 'standard' micromouse event, the controller keeps track of total maze time, current run time and best run time for that entry. There is no information stored in the controller about which mouse is running.

## Front Panel

On the front panel are a 20x4 LCD character display and an encoder/switch for altering the current mode and operating any other UI on the controller. Four buttons allow the controller to be operated manually if the gates are not functioning or not present. As well as a power indicator, four LEDs indicate the current state of the controller and the events driving it.

## Connectivity

Although it can operate in a completely stand-alone mode, the controller would normally be connected to a host computer that will keep a record of the run times and look after any dislay devices for the conestants and spectators. Communication with the host is performed over a serial link (at 57600 baud) using the Arduino's built-in serial bridge. This has two side-effects. One is that the device can be programmed over the same link, the other is that connecting the cable will reset the controller. Since this connection is also how it gets its power, that is not a great problem.

Optionally, the internal BlueTooth transeiver can be used for serial communications if a plain USB power supply is used. The communications speed is still 57600 baud and the two serial connections share the same USART of the Arduino.

## Storage

Also present are a micro SD card reader/writer and a Real Time CLock. The SD card interface makes it possible for al messages and times to be saved to a suitable SD Card for subsequent analysis and reporting or in case the host connection fails or is not present. With the time data from the RTC, files and events can be properly time stamped. Note that the RTC is battery backed but does not incorporate a charger. The battery life of the clock is given as 1 year. Without the battery, all times will be relative to the last application of pwer to the device.

