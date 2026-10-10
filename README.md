Microcontroller-Based Voltage and Over-Current Protector

An Arduino-based protection device that disconnects an appliance when the mains voltage surges or sags, or when the load draws too much current. Settings can be changed from the device (3 buttons and an LCD) or from a computer over a serial connection.
Designed to be simulated in [https://wokwi.com](https://wokwi.com/projects/477511752993724417 .

 Features

1. Over-voltage, under-voltage and over-current protection
2. Relay disconnects the load after a 50 ms confirmed fault 
3.Automatic reconnection after the supply has been normal for a configurable delay 
4.Buzzer alarm and green/red status LEDs
5.16x2 I2C LCD showing live voltage, current and relay state
6.Local settings via MENU / UP / DOWN buttons
7.PC settings and monitoring over USB serial (9600 baud)
8.Settings stored in EEPROM, so they survive power loss
9.Optional data streaming (`DATA,...` lines) for logging or plotting



 Pin map

A0:Voltage sensor 
A1:Current sensor 
A4 and A5 : LCD SDA / SCL 
D2, D3 and D4: MENU / UP / DOWN 
D6: Buzzer 
D7:  Relay 
D8:  D9  Green / Red LED 

How to run in Wokwi

1. Create a new Arduino Uno project at wokwi.com.
2. Replace the contents of `diagram.json` with the provided file.
3. Add `LiquidCrystal I2C` to `libraries.txt`.
4. Paste the sketch into `sketch.ino`.
5. Press Start and open the Serial Monitor (9600 baud, newline line ending).

Protection logic

1. Voltage and current are read 10 times each and averaged.
2. If the relay is on and any limit is violated for 50 ms, the relay opens and the cause is logged to the serial port.
3. While tripped, the buzzer beeps and the LCD shows the fault type.
4. Once readings stay normal for the reconnect delay, the relay closes again.
5. At power-up the relay starts OFF and only closes after the supply has been normal for the reconnect delay.

Default:
Over-voltage (OV)  250 V 
Under-voltage (UV)  180 V 
Over-current (OC) 10 A 
Reconnect delay (DLY)  5 s 



 Press MENU to cycle: Monitor, Over-Voltage, Under-Voltage, Over-Current, Reconnect Delay.
Press UP or DOWN to change the value (5 V, 0.5 A or 1 s steps).
Return to the Monitor screen to save to EEPROM.

From a computer (serial)

Send these commands, one per line:

| Command | Description |
|---|---|
| `HELP` | List commands |
| `GET` | Show current settings |
| `STATUS` | Show voltage, current, relay and fault state |
| `SET OV <volts>` | Set over-voltage limit |
| `SET UV <volts>` | Set under-voltage limit |
| `SET OC <amps>` | Set over-current limit |
| `SET DLY <seconds>` | Set reconnect delay |
| `STREAM ON` / `STREAM OFF` | Start or stop 1 Hz data lines |
| `RESET` | Reconnect the load if the fault has cleared |

Stream format: `DATA,<volts>,<amps>,<relay 0/1>,<fault name>`

Example:

```
> SET OV 240
OK OV=240 V, UV=180 V, OC=10.0 A, DLY=5 s
> STREAM ON
OK stream on
DATA,221.4,3.20,1,NONE
```

The test begins by setting the voltage potentiometer to approximately 75% (around 220 V) and keeping the current potentiometer at a low setting. After the reconnect delay, the relay turns on and the green LED illuminates, indicating normal operation. For the over-voltage test, the voltage is increased above 250 V, causing the relay to trip and the LCD to display TRIP:OVER-VOLT. For the under-voltage test, the voltage is reduced below 180 V, while the over-current test is performed by increasing the current above 10 A. To verify the auto-reconnect function, the voltage and current are returned to normal values, and the load reconnects after the configured delay. Local protection settings can be modified using the MENU, UP, and DOWN buttons, after which the system returns to the Monitor screen. Finally, PC-based configuration and monitoring are tested through the Serial Monitor by sending commands

