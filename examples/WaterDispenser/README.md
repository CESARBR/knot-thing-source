[<img src="http://knot.cesar.org.br/images/KNoT_logo_topo1.png" height="80">] (http://knot.cesar.org.br/) KNoT Network of Things
=============================================================================

#### _Scale Application Documentation_

This tutorial explains how to hack a weight scale, model EB9322 of Camry Scales
® using a converter (ADC) for weight scales HX711.

Weight Scale Specifications
----------------

* Product Name: HEAVY DUTY GLASS SCALES
* Producer: Camry Scales
* Model: EB9322

<img src="http://www.camryscale.com/showroom/photo/eb9322-b.jpg" height="200">

Link for more details: [EB9322](http://www.camryscale.com/showroom/eb9322.html)

Similar model used in the application: [EB9321]
(http://www.camryscale.com/showroom/eb9321.html)

24-Bit Analog-to-Digital Converter (ADC) for Weigh Scales - HX711
----------------

HX711 is a precision 24-bit analog-to-digital converter (ADC) designed for
weigh scales and industrial control applications to interface directly with a
bridge sensor.

Link to the datasheet with more details about the board used:
[Datasheet](docs/Datasheet_HX711.pdf)

HX711 Library
----------------

An Arduino library to interface the Avia Semiconductor HX711 24-Bit
Analog-to-Digital Converter (ADC) for Weight Scales.

To use the HX711 lib correctly with this application, follow the instruction:

1 - Download and save the library in the default directory of Arduino
```
$wget --output-document="$HOME/Arduino/libraries/HX711.zip" https://github.com/bogde/HX711/archive/master.zip && cd $HOME/Arduino/libraries/ && unzip HX711.zip
```

OBS: If the Arduino folder isn't in the default directory you must change the
command to point to the correct directory.

Link to lib: [Git](https://github.com/bogde/HX711)

How to hack the Weight Scale
----------------

Opening the back of the balance, you can find the controller board.
In order to capture the data read by the scale using the pins of the HX711 chip,
you need to solder the pins of the HX711 with those of the balance controller
board as follows:

HX711

<img src="http://iotdk.intel.com/docs/master/upm/hx711.jpeg" height="">


| Pins in HX711 |             Reference             |Pins in PCB Weight Scale|
|---------------|-----------------------------------|------------------------|
|        E-     |                GND                |    Middle left down    |
|        E+     | AVDD Analog Power Supply Pin (5V) |    Middle right up     |
|        A-     |      Input Analogic Negative      |    Middle right down   |
|        A+     |      Input Analogic Positive      |    Middle left up      |


PCB EB9360R11

<img src="http://i.imgur.com/pzT8akx.jpg" height="">


| Pins in HX711 |           Wire Color          |Pins in PCB Weight Scale|
|---------------|-------------------------------|------------------------|
|        E-     |             Blue              |    Middle left down    |
|        E+     |             Purple            |    Middle right up     |
|        A-     |             Green             |    Middle right down   |
|        A+     |             Yellow            |    Middle left up      |


Doing this now you are able to read the values and use them in your application.

Water Dispenser Application
----------------


Constants defined for use in the application
```
READ_INTERVAL      	1000
BOUNCE_RANGE 		200	// Constant to filter the highest and lowest values read
```

1 - Set the analogic pins of microcontroller to connect with HX711 chip.
```c++
HX711 scale(A3, A2);
```

2 - Set the microcontroller digital pin number 3 as input of the button of tare.
```c++
pinMode(BUTTON_PIN, INPUT);
```

3 - Wake up the HX711 chip.
```c++
scale.power_up();
```

4 - This part of the function `get_weight(times)` converts the average of values
read from the HX711 chip to a readable value.
```c++
mes = scale.get_value(times);
a = ref_w/(k2 - k1);
b = (-1)*ref_w*k1/(k2-k1);
raw_kg = a*mes + b;
```

5 - Verify if the button to tare is pressed.
```c++
if ((button_value_current == HIGH) && (button_value_previous == LOW)) {
    offset = get_weight(TIMES_READING);
}
```

6 - Save the offset on EEPROM.
```c++
EEPROM.put(OFFSET_ADDR, offset);
```

7 - Calculate the average of the last values reads from HX711 chip and subtract
this with the offset (value read of tare).
```c++
kg = get_weight(TIMES_READING) - offset;
```

8 - The function `remove_noise(value)` uses logic to filter the read data
avoiding abrupt data variation.
```c++
 if(value > (previous_value + BOUNCE_RANGE) || value < (previous_value - BOUNCE_RANGE)) {
        previous_value = value;
}
```

9 - Read the values only during the set interval.
```c++
currentMillis = millis();
if(currentMillis - previousMillis >= READ_INTERVAL){
	previousMillis = currentMillis;
	kg = get_weight(TIMES_READING) - offset;
}
```

10 - Convert from kilogram to gram and call the function `remove_noise(value)`.
```c++
 *val_int = remove_noise(kg*1000);
```
