# Carbon Monoxide Detection and Alert System for Automotive Safety (with optional Google Sheets logging)

## Introduction
This is a mini project that my team and I did for our undergraduate course. It’s a pretty mediocre project for a UG student, I know. But we couldn’t find any other ideas within the specified timeframe. It’s not much, but it’s honest work. This project uses a modification to the MQ7 sensor to make it compatible with the ESP32. The modification can be found at [this link](https://devices.esphome.io/devices/MQ-7). The mathematical equations we use are also from this website (not entirely; I’ve made some guesses in the calculations, so if anything doesn’t add up in the equations, I’m sorry. Please take a look at and correct any mistakes if you can). Now that we've covered that, let’s discuss more about this project.

This project is titled “Carbon Monoxide Detection and Alert System for Automotive Safety”. Using this project, we can accurately (not really, without proper calibration the values are garbage) measure the amount of Carbon Monoxide in the air and take necessary actions based on that. Why? Because Carbon Monoxide is basically John Cena; we cannot see it. It’s a colorless and odorless gas, unlike its relative Carbon Dioxide. Carbon Monoxide has a higher affinity for hemoglobin compared to Oxygen and binds to it. So, if CO is present in high concentrations in the air and we inhale it, the Oxygen-carrying capacity of our blood reduces, and we will asphyxiate.

Carbon Monoxide poisoning due to vehicles is rare but not impossible. Numerous incidents have occurred. This project aims to combat that by providing a CO detection system that gives early warnings to drivers about rising CO levels and, if something bad happens, sends a text message to the driver’s emergency contacts. This project also provides optional Google Sheets logging.

## Hardware Requirements
1. ESP32
2. MQ7 Sensors (modified)
3. DHT11 Sensors
4. ST7735 TFT Display (128x160)
5. NEO6M GPS Module
6. SIM800L GSM Module
7. 5V Power Supply
8. Buzzer

## Software Requirements (Optional)
1. Visual Studio Code
2. PlatformIO

Personally, I’ve used PlatformIO with VS Code and that’s what I prefer. You can use the Arduino IDE if you want, but make sure that all the necessary libraries are installed. If you are using the Arduino IDE, ensure you have correctly configured the TFT_eSPI library, specifically the `user_Setup.h` file.

## Project Details
We use a pair of MQ7 and DHT11 sensors, one situated on the outside and the other on the inside of the car. We display the CO levels, both outside and inside the car, on the display, so that in case any increase in levels occurs, the driver can decide whether to get outside or stay inside the car and turn on recirculation.

We cannot directly interface the MQ7 sensors to the ESP32. The MQ7 sensor requires heating at 5V for 60 seconds and cooling at 1.5V for 90 seconds to provide accurate readings. That means we could get a reading only once every 2.5 minutes (which is pretty lame, but the MQ7s are very cheap compared to others, so can’t complain). Most of the MQ7 breakout boards do not come with a heater control circuit, so we have to build one ourselves. The modification needed is given on this [website](https://devices.esphome.io/devices/MQ-7).

This modification converts the D0 pin of the module to act as the heater enable pin through some clever black magic trickery discovered by the author of that ESPHome website.

We have another problem: the sensor requires 150mA current, but the ESP32 can only provide a maximum of 40mA. So, we need a power supply that can provide the necessary voltage and current to the sensor.

After making this modification, provide the connections as per the circuit diagram. The circuit diagram, block diagram, and pictures of our project are given in separate appropriately titled folders.

**Note:** The connection of the buzzer isn’t provided in the circuit diagram. Just connect the buzzer to the GPIO19 pin of the ESP32.

## Code
This code uses Google Sheets for logging the calculation data for demonstration purposes. It’s not necessary for the functioning of the project.

**Very Important:** Comment out everything within the 
```c
/* --------------------------------- logging -------------------------------- */
```
and
```c
/* ------------------------------- End Logging ------------------------------ */
```
sections if you aren’t planning on using the logging feature. Failure to do so may result in the code not working. Commenting out those sections saves program memory as well. Note: There is one such section at the very end of the code. Comment that out as well if you want.

But if you are planning on using the logging feature, follow this [tutorial](https://randomnerdtutorials.com/esp32-datalogging-google-sheets/). It’s very detailed. Just replace the Project ID and other relevant information in the code, and it will be up and running in no time. The screenshots of the Google Sheets are provided as well.

Next, you need to provide the ‘clean air compensated resistance’ value for both sensors in the code. This is very important. The value is different for different sensors. To find the value of your sensor, run the program while keeping the sensor in clean air and note the ‘Compensated Resistance’ for that sensor from the Serial Monitor. Copy and paste this value into the code where it says ‘clean air compensated resistance’. Repeat this for the other sensor as well. This is the reference or baseline that we use to calculate further values, so do this. Otherwise, you’ll get absurd values as ppm.

## Footnote
I know this is a very mediocre project from a UG student, but I hope this becomes useful to someone out there. You could further enhance this project by adding an automatic power window lowering feature that lowers the power window of the car if the CO level is high inside but low outside to facilitate rapid ventilation. This idea was part of our project as well, but due to time, budget, and practicality constraints, we couldn’t implement it. We are planning on adding it in the future. I don’t know when or if that’ll be possible, but if I do, I’ll surely update it here.

This is the first time I’m using GitHub. If you found this helpful or if you have any doubts, contact me, and I’ll be glad to help you if I can. If you did anything from this project, please let me know as well. I’ll be happy to see someone benefit from my project. So that’s it. Hope you have a wonderful project.


