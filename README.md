# Mercedes EQS BMS research

Doing my own research from scratch.
Since there was no information available from third party sources
at the moment of creation of this repository.

## Discovered internals

### Current sensor
![IMG_20260909_141050045_HDR.jpg](media/IMG_20260909_141050045_HDR.jpg)
_(Part number)_

![IMG_20260909_141157127.jpg](media/IMG_20260909_141157127.jpg)
_(Internals: Front)_

![IMG_20260909_145634460.jpg](media/IMG_20260909_145634460.jpg)
_(Internals: Rear)_

- Current sensor, based on good ol' shunt
- CAN + LIN interface (Longest chip: NXP: UJA1075A)
- Unknown OEM chip 10104 TJ10173 2247 4NC (middle one). Looks like some NXP based MCU for can controlling
- NXP: MC33772BTC0

Connector socket pinout 0...5 (facing from outside, **LEFT** to **RIGHT** order →):

- 1 - GND
- 2 - CANH
- 3 - CANL
- 4 - VCC (12V) -> V2 (CAN 5v voltage regulator)
- 5 - (5V) Unknown purpose pin traced to unknown OEM chip 48 pin (last).
- 6 - ??? Goes through capacitor to the ground (not connected to the bms via cable and just floating)

Initial assumption was: 2CAN, 1LIN, 12V, gnd.
But with V2 regulator linked directly to socket pin.
LIN might not be used after all. - Weird solution...

I also assume it should just broadcast generic CAN messages with current and other status values.

5th pin goes from leg to unmarked black smd component in parallel (grounded), probably inductor, but strangely have 1.5kOhm resisistance. I guess ferrite bead? It also goes to the 1100ohm resistor which terminates protective SOD-23 diode. Later it goes via some small inductor, then to the capacitor (parallel with ground) and to the MCU 48pin

Quartz at 21/22 pin on unknown OEM mcu. 

> 11.09.2026

We powered BMS and got can messages on 1000kbps rate:
Looks like: 0x080, 0x420, 0x430 are not related to current sensor.
(exclude 1db, since it's what we send via our can analyzer...)

We've just quickly short circuited 3.7v 18650 battery to test shunt capabilities with about 3A current.
0x10, 0x40, 0x50, 0x60 - Are definitely current sensing values. Looks like tripple reduntancy is present.

I have made our local logs compatible with SavvyCAN software (converted to CSV)

Reverse engineering. First byte MSB bit is considered 0, LSB is considered 7.
- 0x10:
	byte 0, bit 0 len:8 - checksum (unknown)
	byte 1, bit 0 len:8 - counter
	byte 2, bit 0, len 24 - signed current adc0 0.0001x ?
	byte 5, bit 0, len 24 - signed current adc1 0.0001x ?

- 0x40:
	byte 0, bit 0 len:8 - checksum (unknown)
	byte 1, bit 0 len:8 - counter

- 0x50:
	byte 0, bit 0 len:8 - checksum (unknown)
	byte 1, bit 0 len:8 - counter

- 0x60:
	- bits 0-24 (len:25) - signed current? (fluctuates between neg/pos values)

So after a bit of research i have found that 0x10 is most likely main current sensing message,
Other are not meaningful or unknown