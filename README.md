# computer-network-project---rdt_protocols
# computer-networks-projects---chatting-app
* The project is to implement three reliable transfer protocol : Alternating Bit, Go-Back-N, and Selective Repeat.
* The performance of these three protols are tested and analyzed after the implementation.


### To build:
1. git clone
2. cd senpan
3. cmake ..
4. make

### To Run the protocols:
 * run ./abt -s 200 -m 50 -t 30 -c 0.2 -l 0.1 -w 20 -v 3
 * run ./gbn -s 200 -m 50 -t 30 -c 0.2 -l 0.1 -w 20 -v 3
 * run ./sr -s 200 -m 50 -t 30 -c 0.2 -l 0.1 -w 20 -v 3

### parameters:
| parameter       | Parameters    | Comments |
| ------------- | ------------- | ------------- |
| -s            | 200           |The seed value is set to 200. Simulator runs with the same seed value should produce the same output |
| -m           | 50           |50 messages are delivered to the transport layer on the sender side|
| -t            | 30            |An average of 30 seconds between messages arriving from the sender’s application layer |
| -c            | 0.2            |An average of 20% of sent packets are corrupted |
| -l            | 0.1            |An average of 10% of sent packets are lost |
| -w            | 20            |The window size is set to 20 packets |
| -v            | 3            |Number of debugging statements from the simulator are printed to the screen|

### Implementing the multiple software timer in selective repeat:
Implemented a virtual timer queue to acheive multiple software timers with one physical timer. 
When a packet is sent, we push a record of the packet’s sequence number and its interrupt time into the back of the virtual timer queue. the interrupt time for the paket is:
**paket_interrupt_time = current_system_time + timeout_interval;**

When the a timer interrupt happens, we popped the timeout paket from the head of the queue and set the physical timer to start at next_interrupt_time:
 **next interrupt time = first element in the queue’s interrupt time - current system time**
 

### References:
1. "Beej's Guide to Network Programming"
2. "computer network a top down approach"
