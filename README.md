
# Reverse Osmosis Water Filter Controller

## Intro

Project designed to automate home electric RO water filter system that includes intake shutoff valve, pump and optionally automatic flush valve, using microcontroller Arduino Pro Micro(ATmega32U4). Project has low cost, requires minimum components and uses no extra sensors and actuators.

Aims of the project:

- Make operation safer.
- Prolong lifespan of valves and pump by eliminating major flaws of pure electric control.
- Prevent extra wear of filters.
- Make diagnostics easier.
- Make it possible to work from unpressurized water source without rewiring.

Basic electric circuit has two sensors for low and high pressure, if pressure is not high and not low respective contacts are closed and power supplied to the actuators: intake shutoff valve, diaphragm pump and automatic flush valve that bypasses drain restriction for short period of time. The major flaw is that low pressure sensor doesn't have deadband- at certain low pressure, when sensor is not tripped, starting the pump causes pressure drop and sensor trips which turns off actuators and pressure becomes sufficient again and cycle continues. This self-oscilating process has period less than a second causing major wear to valves and pump mechanics, electrical parts work at inrush current producing excessive heat which in turns can damage the insulation of windings. Second problem is frequent turn on and off caused by opening a tap for a short period of time so closing it will cause pressure to trip high pressure sensor again thus de-energizing the actuators. Third problem is there is no protection from sensors or pump malfunction and accidental leaving the tap open.

## Hardware

Schematics made with KiCad, PDF is available in respective folder.

The main controller is Arduino Pro Micro(ATmega32U4) reads 3 signals from two sensors and a switch, output 3 signals to 2 LEDs and one to actuators, that controlled with logic-level mosfet. MC is powered from 24V main supply using L7805 linear voltage regulator. User panel consist of: two LEDs- yellow and red, a switch. Diodes are used to protect from kickback voltages. Cap C3 filtering ripple from DC motor operation.

Note that high pressure sensor breaks the circuit so as detected by the MC. It's made intentionally redundant to protect from dangerous pressure to build-up or continuous uncontrolled operation due to controller or MOSFET fault.

Power consumption of the device is 14mA(max) at 24V- 0.336W, 40mA during boot.

## Software

Software is written in PlatformIO with C++ using arduino framework. Software is compatible with any other Arduino-compatible board.

Software has following functions:

- Monitors state of sensors.
- Prevent operation in undesired conditions: low pressure or too high pressure.
- Prevent indefinite operation due to inability to build up pressure by limiting continuous operation time.
- Prevent frequent turning on/offs caused by low water pressure by adding progressive delay between retries: 10-30-60 minutes
   thus making it self-recoverable condition.
- Prevent frequent operation by requiring high-pressure sensor to stay closed for 30s and making 10m delay between turn-ons.
- Display sensors state and faults.
- Allowing to bypass low pressure sensor, reset the faulty state, bypass waiting between switching on.

Loading program only possible shortly(~12s) after reboot.

In order to save power in program is used power down mode, which uses inaccurate wdt timer with tolerance +-20%, so all time operations will be inaccurate, due to that interval time has to be calibrated to exact used devise. In this project interval increased by 18.3% from default 60 to 71ms. Anyways for that project such time drift isn't critical.

Software has a watchdog to prevent mc to hang during program execution leaving outputs in high state, if program won't respond in 3s mc watchdog will reboot the mc.

## Operation manual

Normal operation doesn't require any user interaction with the controller.

Front panel yellow LED displays state of high pressure sensor, if continuos on means high pressure is reached or sensor's wire is broken. If the LED flash it signals that maximum operation time is reached(1h), that can be reseted by switching switch on-off or turning off/on the power supply.

Front panel red LED displays the state of low pressure sensor, if continuous on means that pressure is low or sensor wire is broken. If the LED flash that means low pressure turned on rapidly and gone quickly, which is faulty state and the controller waits some time to try turn on actuators again. This fault recovers by itself or switching the switch on/off or turning off/on the power supply.

Front panel switch resets faults and turn on the maintenance mode that allows operation without delays and ignores low pressure sensor.

If none of LEDs is on means that program is either waiting in restart delay or actuators work.

Built-in arduino green LED signals that power is on.
