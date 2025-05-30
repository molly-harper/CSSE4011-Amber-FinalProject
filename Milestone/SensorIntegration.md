# Sensor Integration 

## What sensors are used?
- Temperature: on board sensor (for room temp) or MAX30205 (for body temp)
- Gas: on board sensor (need to determine what baord will be used as the stationary node)
- Heart Rate: MAX30102
- sp02: MAX30102

## What type of data is required?
The following table describes the kind of data each sensor outputs, including units, format, and the required accuracy:

| Sensor    | Data Measured          | Units/Format     |
|-----------|------------------------|------------------|
| MAX30102  | Heart rate, SpO₂       | bpm, %           |
| BME680    | Temp, Gas              | °C, ppm          |
| RTC       | Time                   | ISO 8601 (UTC)   | 

## How are the sensors integrated?
- Completed flow chart on the google drive in the milestone plan pdf as of v2

#### Technical Integration

#### Connection Type:
- Most sensors use **I²C** or **SPI** interfaces.
- **UART** may be used for communication with the base node

#### Board Integration:
- Sensors are connected to the **Thingy52** or **nRF52840**, which collects data
- Data is periodically sampled every **5 seconds** for the patient sensors
- Data is buffered and transmitted over BLE

#### Software Stack:
- The system uses Zephyr libraries for task scheduling and sensor drivers.
- Queues, threads and ring buffers will also be utilisied alongside sensor integration
- Sensor readings are passed through a Kalman Filter for noise reduction.
- Filtered values are formatted intoa buffer and sent via BLE.

