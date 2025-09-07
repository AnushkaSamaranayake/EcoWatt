# EcoWatt for Solar inverters

Ecowatt is an embedded system that is connected to solar inverters to read parameters from them and send the data to the cloud. This demo uses an online InverterSIM as the solar inverter.

The following is the architecture of the project.

![architecture](./assets/architecture.png)

There are three components of this project,
1. Online inverterSIM - which is configured using API
2. EcoWatt device - NodeMCU
3. EcoWatt cloud - cloud configuration for the device

This project consists of 4 main milestones.

## Milestone 1

In Milestone 1, we built a PetriNet diagram to identify the architecture of the EcoWatt device and how it works. Then based on that diagram, a scaffold code is created to simulate the functionality of the device. Following are the Petrinet diagram and the demo video of the scaffold code.

[PeteriNet diagram](https://drive.google.com/file/d/15ALogLgaHMUhQ6W8kmQFQu1SFCyAevvc/view?usp=drive_link)
[Demo video](https://drive.google.com/file/d/1hROSwhe20sWfY8Vbdq4S5FbaViUpt6ox/view) 

To see how it works, follow the following commands

```bash
# clone the repository
git clone 
