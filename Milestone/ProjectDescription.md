# Project and Senario Description

## Senario
Rural locations often have limited or unreliable internet connectivity, making data transfer difficult. In critical applications like healthcare, poor connectivity can delay the transmission of patient data, making it harder to detect emergencies or provide timely medical interventions. 

Rural village with limited internet connectivity, elderly patients wear small, low-power health monitoring devices that track vital signs such as heart rate, blood oxygen levels, and body temperature throughout the day. A mobile data mule, visits the stationary with a portable device that collects the stored data from each patient's wearable. Once the data mule returns to the base node, the data is uploaded to a central server where medical staff can review it, identify any health risks, and make informed care decisions. This system enables continuous, low-cost health monitoring without relying on constant internet access.

## Project Description 
This project implements data muling to reduce the ocmplication of data transmission. Patients are equiped with health monitoring devices that track vital signs such as heart rate, blood oxygen levels, and body temperature throughout the day. A health professional acts as a mobile data mule, visits the homes daily with a portable device that wirelessly collects the stored data from each patient. Once the healthcare worker returns to a connected health center (base node), the data is uploaded where it can be analysed to make informed healthcare decisions. This system enables continuous, low-cost health monitoring without relying on constant internet access.


### Mobile Node
The healthcare professional will act as the mobile node, retriving the health data from the stationary node(s) (the patient) and then relay this information for analysis (to the base node).
Within the mobile node, there will be an immediate response indicator that will assess the data and trigger a buzzer and led (actuator) in situatons where immediate care is required (ie. irregular heart rate or low oxygen levels).

### Stationary Node
The patient will be the stationary node and will be equiped with a variety of sensors that track and store data for the period of thime between data collection. These sensors include: 
- Temperature sensor: to track heat exposure
- Gas sensor: to detect exposure to potentially harmful gasses 
- Arduino MAX30102 sensor: to gather heart rate and sp0_2 levels

### Base Node
The base node processes the data communicated by the mobile node/and provides respective data visualisation so that the information can be used to make informed medical decisions.
