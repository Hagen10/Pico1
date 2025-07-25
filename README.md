FirstPico.c is the file that is compiled and run on the Pico 1. Each solution to the Kitronik experiments will be moved over to separate files called ExpOne.c, ExpTwo.c etc. In order to test them at a later point, copy everything from the desired Exp*.c file and paste it in the FirstPico.c file. Finally, compile and run it on the connected pico.

## ExpTwo Ideas
- Make a button that resets the cut off value to the current adc_read.
- DONE initialize the microcontroller by measuring say 10 different values and save the lowest value and the range that they are in. The cut off could then be the lowest value minus an additional figure just to allow some additional fluctuation without turning it off.

## ExpThree
- DONE Figure out why the switch button doesn't always work. Are we doing something wrong here? It seems as if one switch press can trigger many interrupts. Maybe the buttons don't work properly?

## ExpFive
- DONE Experiment more with the gpio_level range. It seems like the servo only operates in the range 500 to around 3200, not 1638 to 8192 as the booklet says.
- It seems as if it requires a long sleep before updating the gpio_level otherwise it won't turn.
- The highest values (3000-3200) seems to cause it to shake, not sure why?

## ExpEight
- Occasionally, I can get the adc read to increase when I blow on the fan, but other times the adc read doesn't change at all based on the movement of the motor. I'm not sure what the problem is here. But I proved that it worked sometimes.

## ExpNine
- Like with many other experiments, the booklet claims the range is 0-65535 (16 bit) but the range ends up being 0-4096 only. Not sure why that is the case.

## ExpTen
- Check and see if having the same interrupt for each of the buttons is a bad idea! NOT A PROBLEM
- The fix was to remove "opts pindirs" from the .side_set 1 and possibly also adding a longer sleep instead of just 80 µs. Now it's 50 ms
- What if I want to set different colours for the differen LEDs?

## Communication between Picos using UART
Files are called `Pico1UART.c` which is the one waiting for messaging and lights up the zip leds as a result. `Pico2UART.c` is for pico 2 which will only send out messages to request turning out LEDs on the pico 1 and wait for responses to ensure that everything was well-received. Both Picos have a watchdog configured but it's not really used for much and is only updated once per loop.

### Wiring diagram

![UART](assets/UART.png)

#### Pico 2
GP4 (UART TX) -> GP5 (UART RX) on the Pico 1
GP5 (UART RX) -> GP4 (UART TX) on the Pico 1

#### ZIP stick LED
GND -> Minus line on breadboard -> any GND on Pico 1
5VDC -> VSYS on Pico 1
DIN -> GP16 on Pico 1


## Sending data with SPI to E-ink Paper display (Waveshare)

the epd directory contains files imported directly from [WaveShare's Github](https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_7in5b_V2.c). The files have been modified slightly to ignore the `Debug` calls and to make it align with Raspberry PI Pico instead of a regular Pi running Linux.

### Wiring diagram

![Display](assets/Eink_display.png)

## 33 Minute timer

A timer that runs for 33 minutes and then lights up a red led. Green led lights up when the timer is ready to be started.

### Wiring diagram

![Timer](assets/33_Minutes.png)

## Debugging

CMakeLists.txt has `pico_enable_stdio_usb(FirstPico 1)` set to 1 to allow us to get logs from `printf` in our terminal. To disable this, set it to `0`.

Then in your main you need the following code:

```
  // Needed for getting logs from `printf` via USB 
    stdio_init_all();
```

To get logs, open a terminal and type `ls /dev/tty.usb*` which should give you a device such as `/dev/tty.usbmodem11401`. Note, that the device does not show up when in BOOTSEL mode. You need to flash the program on the microcontroller first, and then you can get logs. Afterwards, run `screen /dev/tty.usbmodem11401 115200` to receive logs in the terminal. 

### Using Pico 2

When using Pico 2, CmakeLists.txt must be updated with board type set to pico2 in `set(PICO_BOARD pico2 CACHE STRING "Board type")`

- maybe set up interrupts for all the different reads/writes across the SPI, UART and I2C?

- Interrupts seem to work for UART now, but there is some timing issue. We need to maybe move more of the comms back to main(), so the interrupts only set flags for what needs to be done in main.

- How should I toy around with SPI and I2C?