## PiStackMon, the ultimate SBC stack monitor!
![PiStackMon](https://user-images.githubusercontent.com/44300715/109698341-99742e80-7b8f-11eb-9cdc-f40a1635fd28.png)
![PiStackMon_back](https://user-images.githubusercontent.com/44300715/109701973-ef4ad580-7b93-11eb-8436-c49a15260953.png)
![PiStackMon in action!](https://user-images.githubusercontent.com/44300715/109698544-ce808100-7b8f-11eb-84a5-de4d42992064.jpg)
Hi and welcome!

I have built this little thing to help me manage my stack of Raspberry Pis, mostly because I failed to find anything comparable on the market.

Plugged into the side of Raspberry Pi, it doesn't get in the way of your stacking or thermal management.
It also lets you monitor the basic stats (CPU, RAM, Temperature), turn the power on or off, and much more!

- Compatible with all Raspberry Pis with 40-pin GPIO headers,
- Supplying Raspberry Pis via screw terminals rather than USB port
- Dual power inputs-outputs allow clean and easy daisy-chaining
- Independent 2.5A self-healing polyfuse protection (normally unavailable via GPIO header)
- Bulk capacitor helps preventing transient undervoltage events and eases strain on PSU,
- 2.54mm header for power supply cut-off (using a jumper)
- +5V LED indicator (behind the fuse)

- 3x 5-LED indicators of real-time CPU usage, RAM usage and CPU temperature
- LEDs are powered by constant-current drivers, from PSU rather than GPIOs
- Open source daemon (pistackmond) controls LEDs
	- Easy to compile and install on any Debian or Ubuntu-based distro
	- One file C++ source code
	- Smaller CPU and memory footprint than htop
	- No external libraries used (RIP WiringPi)
	- Runs as systemd service

- Auxiliary IOs for your projects:
	- SPI0, I2C1 and 1Wire easily accesible through 2.54mm headers
	- 1-Wire pullup resistor
	- 3-pin header with RC-filtered 3V3 supply for IR sensors
	- 2-pin header with 5V and BJT-reinforced low side switch, for PWM fan control
	- Two optically isolated outputs, for pulling external logic up or down (>2.5mA)
	- Two high-power, isolated low side switches, up to 26V and 6A
	- Alternatively, MCP1416s can be used as line drivers directly
	- Optical isolation lets you drive separate circuits without making ground loops
	- One isolated output and one high-power switch are connected to hardware PWM GPIO pins

- Low profile PCBs allow stacking with spacers as short as 20mm,
- No risk of contact with conductive spacers
- 2-layer PCB and strict design rules make it very cheap to produce (etching is not recommended)
- Components selected for low cost and relatively easy assembly (One TSSOP24 chip might be tricky)
- All components are broadly available on the market, alternative components are listed as well
- Modular design lets you build even cheaper boards without features you don't need

# Disclaimer
This hardware and software is provided as-is.
All hardware and software presented in this repository is not guaranteed to work at all, may pose a hazard to humans, animals, plants, fungi and hardware.
DO NOT ATTEMPT TO MAKE ANY USE OF FILES PROVIDED IN THIS REPOSITORY.

Users agree to take full responsibility for their actions involving files in this repository.
Ask an expert before you mess with stuff you don't understand.

# Installing Daemon

Log in to your SBC and type in a few commands:

```
# Install a few prerequisites (On Debian, Ubuntu etc.)
sudo apt install git g++ make
# (on Arch, Manjaro etc.)
sudo pacman -S git gcc make

# clone this repository
git clone https://github.com/tomek-szczesny/pistackmon.git

cd pistackmon/sw
# check out building options
make
# build an executable (example for Raspberry Pi 3)
make rpi3
# install and run a service
sudo make install
```

That's it!

# Building hardware

This chapter will definitely need some love, but for now just be sure to follow a few guidelines off the top of my head:
- Decide which features you need or not. As a rule of thumb, you may ignore any boxed part of a schematic independently.
- Check the datasheet of your LEDs or experiment to determine what is the optimal driving current for them. Some LEDs need 1mA, some 10mA. There are no recommended LEDs for PiStackMon.
- If different colors of your LEDs of choice have different intensities, don't worry, there are const values in the code to compensate for that.
- "NM" resistors are simply Not Mounted - leave these places empty, unlesss you want to hack PiStackMon for other purposes.
- Before you do anything, just remember that 40-pin connector should be mounted as the very last component, after you make sure everything else works. Consider it a permanent component, because removing it is indeed a pain in the rectum.
- Mount everything you plan to include, except 40-pin connector and C1. Power up your SBC using its original power input. You may test PiStackMon by carefully connecting it to SBC's GPIO header directly, and applying some tension to ensure proper connection. It is a good idea to check if all LEDs work correctly and that you picked the right LED currents.
- After soldering, make sure you clean your board _very carefully_ from any traces of flux, especially under and around the LED driver chip. Dip it in pure alcohol for an hour if you have to. Failing to do so may make some LEDs glow even though these are supposed to be off.
- Place a tiny heatsink on a low side high-power switch MOSFET if you plan to thrash the living hell out of it.


