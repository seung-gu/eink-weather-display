[English](battery-cutoff.md) | [한국어](battery-cutoff.ko.md)

# Cutting the battery off at low voltage

Deep sleep is not off. The chip keeps drawing, and once the battery is empty that draw is the
*only* thing left consuming it — there are no wakes to dwarf it. A battery left to drain past ~3.0 V
loses capacity permanently.

This note describes a circuit that disconnects the battery outright, and comes back on its own when
a charger is plugged in. It is not in the firmware; §7 says why.

---

## 1. What the parts do

The threshold lives in software. The MOSFET is only a switch.

| | |
|---|---|
| Divider + ADC | **measures** the battery (already built, `battery.cpp`) |
| Firmware | **decides** — one comparison against a `#define` |
| P-MOSFET | **disconnects** the battery |

Changing the cutoff from 3.4 V to 3.3 V is one number and a reflash. No part changes.

---

## 2. The circuit

![The switch in place: the battery feeds a P-MOSFET whose gate is held up to the battery + rail by a 1 MΩ pull-up and pulled down through a small N-MOSFET from one GPIO; past the switch sit the 3.3 V regulator and the MCU, with USB 5 V entering at the regulator. The sense divider is tapped on the battery side of the switch, ahead of it.](cutoff-circuit.png)

The divider taps **ahead of the switch**. Behind it, a disconnected battery reads 0 V and the
firmware cannot tell a flat battery from a charged one after waking on USB.

---

## 3. Why the gate has to be pulled up

A P-MOSFET conducts when its gate sits below its source, by more than a threshold of roughly 1 V.
The source is tied to the battery, so:

| Gate | V_GS | |
|---|---|---|
| left floating, pulled to battery + | 0 V | **off** |
| held at 0 V | −3.7 V | **on** |

![A MOSFET in cross-section, twice. With the gate at the same voltage as the source, V_GS is 0 V, nothing is drawn under the gate, the gap between source and drain stays empty and the two are disconnected. With the gate pulled to 0 V, V_GS is −3.7 V, charge is dragged under the gate to bridge the gap, and current flows from source to drain. In both, an insulator separates the gate plate from the channel.](mosfet-inside.png)

The gate holds no charge of its own — it is a plate over an insulator, so nothing flows into it and
nothing holds it anywhere. Without the pull-up it floats, and a floating gate switches on stray
noise. The pull-up makes **off** the state the circuit falls into whenever nothing is driving it,
which is exactly the state wanted once the MCU is gone.

**1 MΩ is bounded on both sides.** Too low wastes current continuously while the switch is on
(10 kΩ would burn 370 µA, more than the sleep floor). Too high and leakage wins: the off-state
leakage of the N-MOSFET and the gate leakage of the P-MOSFET, a few hundred nA together, develop a
voltage across the pull-up. At 10 MΩ that is ~2 V, past the threshold, and the switch never fully
turns off.

---

## 4. Why there are two transistors

A full battery is 4.2 V and the GPIO is rated to 3.3 V. Wiring the gate straight to the pin would push
4.2 V back through the pin's clamp diode whenever the switch is off.

The small N-MOSFET keeps the two apart: its gate only ever sees the GPIO, its drain only ever sees
the P-MOSFET gate. The two voltage domains never touch.

---

## 5. Shutting down in order

Releasing the gate kills the rail. Bulk capacitance buys about a millisecond — enough that an
unfinished flash write is corrupted, not enough to do anything useful. Everything that has to
survive goes first:

1. Draw the low-battery screen — e-Paper holds its last image with no power at all
2. `prefs.putBool("parked", true)`, then `prefs.end()`
3. `Serial.flush()`
4. `digitalWrite(PWR_HOLD, LOW)` — the rail drops here

---

## 6. Why it stays off, and how it comes back

![Three states. Running: the battery reaches the board through a closed switch and the GPIO holds it closed. Parked: the gate is released, the switch is open, every line from the battery is dead, and holding it open draws nothing — it cannot restart itself. Charger plugged in: USB 5 V reaches the regulator while the battery is still cut, the MCU boots on it and recloses the switch.](cutoff-states.png)

Gate and source both sit on the battery, so **V_GS is 0 V however the battery moves**. A battery that
recovers to 3.5 V once the load is gone moves both terminals together and changes nothing. The MCU
is unpowered and cannot pull the gate down. There is no path back on.

Except one. The board's regulator runs from USB 5 V as well as from the battery, so:

```
USB plugged in  →  regulator runs on 5 V  →  MCU boots (battery still disconnected)
                →  setup() drives the GPIO  →  switch closes  →  charging starts
```

The charger is the power button, and nothing else is needed.

Holding the off state is nearly free but not quite. The pull-up draws nothing against an unpowered
gate, but the sense divider sits **ahead** of the switch — that is what lets the battery still be
measured — so it stays across the battery forever, drawing 3.4 V / 2 MΩ = **1.7 µA**. That is a
month per 1.2 mAh, and at 3.4 V there are only 10-15 mAh left, so a parked board empties itself in
roughly a year. It is the one leak this circuit cannot remove; moving the divider behind the switch
would remove it and take the measurement with it.

---

## 7. Why this is not in the firmware

**No pin is free for it.** What remains is D6 (GPIO21), which is UART0 TX: the ROM bootloader
drives it on every boot. On battery, a reset would let the ROM wiggle the gate mid-boot and the
board could cut its own power before it finishes starting.

The polarity already helps: the circuit is active-high, so an idle UART line (which rests high)
holds the switch closed. What the ROM actually sends is data, not a steady level, and the 1 MΩ
pull-up against the gate capacitance is far too slow to follow bit-rate pulses — but that is an
argument, not a measurement, and it wants testing. A capacitor on the gate would widen the margin.
The other free pins (GPIO2, 8, 9) are strapping pins, which is worse.

A board layout settles it: with the wiring fixed, the pin assignment can be chosen rather than
scavenged.

---

## 8. Parts

| | |
|---|---|
| P-channel MOSFET | logic level, e.g. AO3401 |
| N-channel MOSFET | small signal, e.g. 2N7002 |
| Resistor | 1 MΩ — pull-up, sets the default to off |
| Resistor | 10 kΩ — gate series, protects the GPIO |
| GPIO | one |

A dedicated load switch (TPS22860 and similar) replaces all four parts, with a nanoamp quiescent
current and an enable input that tolerates a much larger pull-up.
