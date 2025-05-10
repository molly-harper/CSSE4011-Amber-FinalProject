# Project and Senario Description

## Senario
Firefighters often operate in hazardous environments where rel-time response and health stats and environmental constitions monitoring is cruicial. However the conditions that firefighters operate often have unreliable connectivity making communication difficult. 
To resolve this..

## Project Description 
This projects implements data muling between the firefighter, stationary enviromental sensors (for example smoke alarms), and a data collation and monitoring point. 
### Mobile Node
The firefighter will act as the mobile node, retriving data from proximate stationary sensors and communicating this and current health stats with the base node. 
The firefighter node itself will be comprised of a range of sensors that collate important information. This includes: 
- Gyroscope/accelerometer: to detect abromal movement activity such as a sudden fall or lack of movement. 
- Temperature sensor: to track heat exposure
- Gas sensor: to detect exposure to potentially harmful gasses 
- Arduino MAX30102 sensor: to gather heart rate and sp0_2 levels
### Stationary Node

### Base Node
The base node processes the data communicated by the mobile node/firefighter and provides respective data visualisation so that the information can be used to assess the environemtal situation aswell as the current condition of the monitored individual.

