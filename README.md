FirstPico.c is the file that is compiled and run on the Pico 1. Each solution to the Kitronik experiments will be moved over to separate file called ExpOne.c, ExpTwo.c etc.


## ExpTwo Ideas
- Make a button that resets the cut off value to the current adc_read.
- DONE initialize the microcontroller by measuring say 10 different values and save the lowest value and the range that they are in. The cut off could then be the lowest value minus an additional figure just to allow some additional fluctuation without turning it off.

## Debugging

CMakeLists.txt has `pico_enable_stdio_usb(FirstPico 1)` set to 1 to allow us to get logs from `printf` in our terminal. To disable this, set it to `0`.

Then in your main you need the following code:

```
  // Needed for getting logs from `printf` via USB 
    stdio_init_all();
```

To get logs, open a terminal and type `ls /dev/tty.usb*` which should give you a device such as `/dev/tty.usbmodem11401`. Note, that the device does not show up when in BOOTSEL mode. You need to flash the program on the microcontroller first, and then you can get logs. Afterwards, run `screen /dev/tty.usbmodem11401 115200` to receive logs in the terminal. 