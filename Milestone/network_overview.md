## Network protocol
- For this project all nodes will be communicating with each other using Bluetooth low energy and a custom GATT profile and GAP with one device acting as a peripheral and the other as a central
- Due to the fact that very little  bidirectional communication occurs in this scenario
- The maximum theoretical data rate for Bluetooth low energy is around 1Mbit/s. In reality this number is likely closer to 270kbit/s
## Topology
- This network has a logical star topology on a physical mesh topology
- This proposed system has 3 kinds of node
	- A single base node
	- Multiple mobile (mule) nodes
	- And even more patient (stationary) nodes
- The patient node periodically broadcasts its presence and waits for a response
- If a mule node is within rage that node responds to the patient node
- In response the patient node sends all the data is has saved
- If any of the base node's known mule nodes are in range, it sends a request to whichever mule it wants to load data from and that mule node responds by sending all its data. 


## Message Format and JSON Conversion
A python script will run and read all the relevant information from the base node via UART, the message protocol will then appear similar to the following:

- Example data might look like:
P01,HR=75,SpO2=98,Temp=36.7,CO=0.03

- A python script for json conversion then might produce data like:

- **Example JSON conversion output:**
```json
{
  "patient_id": "P01",
  "heart_rate": 75,
  "spo2": 98,
  "temperature": 36.7,
  "co_ppm": 0.03,
  "timestamp": "2025-05-14T12:34:56Z"
}
```
## Integration with Grafana:


The JSON packets are sent to a time-series database such as **InfluxDB**, **Prometheus**, or **SQLite** with a backend.

Grafana reads from the database and presents real-time dashboards with:

- **Heart rate trends**
- **SpO₂ levels**
- **Gas exposure incidents**
- **Alerts when thresholds are breached**
