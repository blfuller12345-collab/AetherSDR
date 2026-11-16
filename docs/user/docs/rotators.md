---
title: "Rotators"
slug: "/rotators"
description: "The ROT applet shows and turns station rotators on the network: microHAM ARCO and other GS-232A controllers, and Hamlib rotctld (including PstRotator)."
---

The **ROT** applet shows the heading of each rotator in your station and turns
it. It talks to the rotator controller directly over the network, knows
nothing about the radio, and works with any radio connected, or none. A spot
on the panadapter can also turn a rotator to point at the spotted station.

## Requirements

AetherSDR needs to reach each rotator controller over TCP, using one of two
protocols:

- **GS-232A**: the Yaesu GS-232A command set. microHAM ARCO and ERC
  controllers serve it on the network (port **4001** on an ARCO), as do many
  GS-232-compatible interfaces and a ser2net server in front of a GS-232
  controller.
- **rotctld**: Hamlib's network rotator server (port **4533**). PstRotator
  can serve it too, through its Hamlib emulation, so any rotator PstRotator
  drives works this way.

Some controllers accept only one network connection at a time. If another
program (a station controller, PstRotator, a logger) already holds the
connection, AetherSDR may not be able to connect, or may take the connection
from the other program. Check your controller's documentation, or try it with
the other program running.

## Setup

1. Open the **ROT** applet (Antennas & Switching).
2. Click **Setup**.
3. For each rotator, fill in a **Name** (for example "South 2m Yagi"), its
   **Address** (an IP address or host name), the **Port**, and the
   **Protocol**. Choosing a protocol fills in its usual port. **Add** adds
   another row; **✕** removes one.
4. Click **Save**. AetherSDR connects to each rotator and keeps reconnecting
   every few seconds if the connection drops.

The list is kept for the whole station, not per radio.

## Using the applet

Each rotator has its own row:

- Its **name**, and its **heading** on the right, read once a second. While a
  turn is under way it shows where it is and where it is going, for example
  `120° → 240°`. **offline** means AetherSDR can't reach it; hover over it
  for the reason.
- Type a heading in the box and click **Go** (or press **Enter**) to turn to
  it. Typing alone never turns the rotator.
- **◀** and **▶** turn it 10° counter-clockwise or clockwise. Clicking again
  before it arrives adds another 10°.
- **Stop** stops it straight away.

The controls are dimmed while a rotator is offline.

## Turning toward a spot

Right-click a spot on the panadapter and choose **Turn beam to …**, then the
rotator to turn. The heading is the short-path bearing shown in the spot's
tooltip (see [SpotHub](./spothub.md)).

The entry is dimmed when AetherSDR doesn't know the heading: it needs your own
position (from GPS or your own QRZ record) and a position for the spotted
call. It does not appear at all until a rotator is set up.

AetherSDR never turns a rotator on its own: only **Go**, **Enter** in the
heading box, the nudge buttons and the spot menu send a turn.

## Reference

| Protocol | Usual port | Read heading | Turn | Stop |
|---|---|---|---|---|
| GS-232A | 4001 (ARCO) | `C2` | `Mnnn` | `S` |
| rotctld | 4533 | `p` | `P az 0` | `S` |
