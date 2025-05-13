# Deluverables and KPI's

## 1. Integrated stationary node and sensor system 
**KPI's**
- **Sensor accuracy** 
    - **What:** achieve X % level of accuracy of readings
    - **How to measure:** Compare readings to calibrated results. For example, heart rate to apple watch reading, temperature to thermometer
    - **Why:** accurate data is important
- **Sampling rate**
    - **What:** Sampling rate of each element is appropriate for the data type and all sampling can be achieved simultaneously. 
    - **How to measure:** Test sampling rate of sensor system and assess data collection
    - **Why:** If data is not sampled at the appropriate rate for each variable being measured, the results will not reflect the data accurately. 
- **Alert Triggering**
    - **What:** Alert is triggered immediatly when measurement is above threshold values
    - **How:** Implement controlled variables (ie. known value) and nsure alert is triggered

## 2. Mobile node data collection
**KPIs**
- **Low latency**
    - **What:** Entire data transfer between the mobile and stationary node occurs within a short amount of time
    - **How to measure:** Time the complete data transfer
    - **Why:** Low latency means that more smaples can be achieved in the same duration. More efficient system. 
- **Long battery life**
    - **What:** 
    - **How to measure:** Measure the current draw and overall battery life
    - **Why:** Allows for the system to run for longer without needing maintenence, which is important in rural application where resources are limited and maintenece is inconvinient. 
- **Large Data Storage**
    - **What:** The mobile node cna hold the data for multiple patients at any given time
    - **How to measure:** Data is able to be transfered from multiple stationary nodes to the mobile node and then reproduced accurately
    - **Why:** Being able to hold the data for multiple nodes increases the number of patient that can be seen before returning to the base node. 

## 3. Base Node Data Aggregation
**KPIs**
- **System Throughput**
    - **What:** Can hold and display the information for various patients
    - **How to measure:** 
    - **Why:** Allows for multiple patients to be monitored on the same system, reducing resources. 
- **Visualisation of Data**
    - **What:** The transferred data is displayed in a way that is most appropriate to draw conclusions for that data type
    - **How to measure:** Graphs are accurately produced from the data with the implementation of trends 
    - **Why:** Raw data is harder to draw conclusions from, thus visualisation is important to efficiently and effectively identify trends. 
- **Storage** 
    - **What:** The base node is able to store patient information for an extended duration 
    - **How to measure:** 
    - **Why:** More data allows for more accurate trends to be identified and therefore provides a better resource for identifying medical issues


