## Network protocol
- For this project all nodes will be communicating with each other using Bluetooth low energy
- Due to the fact that very little  bidirectional communication occurs in this scenario
## Topology
- This network has a logical line topology on a physical star? (or maybe mesh?) topology
- This proposed system has 3 kinds of node
	- A single base node
	- Multiple mobile (mule) nodes
	- And even more patient (stationary) nodes
- The patient node periodically broadcasts its presence and waits for a response
- If a mule node is within rage that node responds to the patient node
- In response the patient node sends all the data is has saved
- If any of the base node's known mule nodes are in range, it sends a request to whichever mule it wants to load data from and that mule node responds by sending all its data. 
