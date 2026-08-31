# Aerial Platform Hardware Integration Resources

This folder contains the design files needed to integrate the ECU, PCU,
RS232-to-CAN interface, and enclosure. It is intended for the PCB and
mechanical-design collaborators.

## Contents

- `Final_Relay_Board/`: final Altium project for the relay-equipped board.
- `Final_NoRelay_Board/`: final Altium project for the board without the relay.
- `RS232_CAN_Adapter/`: Altium project for the RS232-to-CAN adapter.
- `Module_References/`: UWB footprint and datasheet, LoRa datasheet, RC522
  documentation, project overview, and the ECU/PCU pin assignment.

## File selection

The Altium project, schematic, PCB, library, output-job, and reference PDF
files are included where available. History folders, project logs, preview
files, compiled firmware, temporary files, and personal authentication data
are intentionally excluded.

## Mechanical-design notes

Use the `.PcbDoc` files for the board outline, mounting holes, connector
locations, and keep-out regions. Use the UWB `.DXF` and `.PcbLib` files for
the UWB module footprint. Before the enclosure is frozen, confirm the actual
battery, cable bend radius, external connectors, relay clearance, and NFC
card-reading window against the physical samples.

## Electrical-design notes

The two LoRa modules use the same radio parameters and a 9600-baud local UART.
The MCU pin assignment and current interface roles are documented in the
included pin-assignment file. Treat the final relay-equipped board as the ECU
reference and the final no-relay board as the PCU reference unless the
hardware team identifies a newer revision.
