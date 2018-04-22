# esp8266_RFControl

Use an esp8266 and some cheap 433mhz sender/receiver to send any (ask non-rolling) rf command.

Default pins are:
Pin 16 send date (D0 on a nodeMCU board)
Pin 2 receive data (D4 on a nodeMCU board)

Setup:
* Flash esp8266 with the sketch (prequisite: https://github.com/pimatic/RFControl in your libraries folder)
* Connect to the RFControl wifi
* A browser should open and redirect you to a wifi setup screen (if not open any browser and visit a non https site and you will be redirected)
* Set up your wifi
* Done

To "learn" a command:
* Go to http://your.esp.ip/receive
* Within 5 seconds press the button on the original remote
* A link with the parameters to send the received code will be displayed in the browser
* Klicking the link will send out the signal (can also easily be used with other software as a webhook)

Sending a command:
* open the generated link from learning

Example URL:
http://192.168.0.195/send?buckets=888,1776,5892,36924,0,0,0,0&timings=210000111001000000001021110000111021001100100123&repeats=3
This will send a RF message with high/low lengths defined in *buckets* in the order of *timings* and repeat it *repeats* times.


Many thanks to the creator of the RFControl library:
https://github.com/pimatic/RFControl
