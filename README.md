# CS 462 - Sliding Window Protocols

## Introduction
This project is an implementation of three network protocols for the reliable 
transfer of packets between a server and a client. The project was split into several 
phases which included the following:

* Establishing reliable communication between two processes, e.g., A and B, with error-handling, timeouts, 
and file writing using multithreading.
* Implementation of the following sliding window protocols, Stop and Wait(SaW), Go-Back-N(GBN), 
and Selective Repeat(SR).
* Creating a command line interface to allow users the ability to manipulate different aspects of the 
transfer such as the timeout used and error scenarios.
* Creating code to inject various errors during transmission of packets for the purposes of testing the reliability 
of the transmission such as the ability to deal with the loss of ACKs, the loss of packets, 
and the failure of a checksums.
* Verifying the reliability of the code by running various tests using the aforementioned error 
scenarios and various sized files ranging from several megabytes to several gigabytes.

## Getting Started

These instructions will guide you on how to get a copy of the project up and running on the thing servers ran 
by the Computer Science department at the University of Wisconsin - Eau Claire (UWEC). 

## Prerequisites

A copy of the project must be uploaded to the thing servers. This can be achieved through FTPing into one
of the thing servers via your UWEC user account or by accessing the drive directly via a University computer or 
the virtual lab.

## Configuration, Compiling, and Execution

The following is step by step instructions on the configuration, compilation, and execution of the program. 

1. Before compiling or executing the code the configuration of how it will run can be tweaked to
   adjust various parameters such as which thing server will act as the server, the protocol used, etc.
   
**NOTE: As it stands currently the application is configured such that thing1 acts as the server. Running
the server code on any of the other thing servers will not work and the program will not run. Instructions
on how to adjust the thing that will act as the server can be found below.**
		   
### Adjusting the thing that acts as the server.
	
In order to adjust the thing that will act as the server the following lines of code within the "Utilities.h" file
must be altered. In order to adjust the server used the #define for the desired server must be uncommented and the previously defined server must be commented out. For example the code below is currently configured to use Thing 1 as the server:
	
```
//#define SERVER "10.35.195.46"   // THING 0
#define SERVER "10.35.195.47"   // THING 1
//#define SERVER "10.35.195.22"   // THING 2
//#define SERVER "10.35.195.49"   // THING 3
```
	
For example, if you instead wish to use Thing 0 as the server the code in utilities.h must be adjusted to the following:
	
```
#define SERVER "10.35.195.46"   // THING 0
//#define SERVER "10.35.195.47"   // THING 1
//#define SERVER "10.35.195.22"   // THING 2
//#define SERVER "10.35.195.49"   // THING 3
```
	
### Adjusting the protocol used to perform transmissions.
	
In order to adjust the protocol that will be used during network transmissions the following lines of code 
within the "Utilities.h" file must be altered. In order to adjust the protocol used the #define for 
the desired protocol must be uncommented and the previously defined protocol must be commented out. For example 
the below code is currently configured to use the protocol Selective Repeat (SR):
	
```
//#define SaW
//#define GBN
#define SR
```
	
If you instead wish to use the protocol Go-Back-N (GBN) instead the code in utilities.h must be adjusted to the following:
	
```
//#define SaW
#define GBN
//#define SR
```
	
Various other parameters that can be tweaked can be found in the following section of the utilities.h file:
	
```
// Misc. Tweaks
#define NUM_THREADS 5
#define ATTEMPTS 10
#define MAXTIMEOUT 1000
```
	
* The num_threads parameter adjusts the number of threads used during program execution.
* The attempts paramter affects the number of attempts the client makes to initially connect to the server.
* The maxtimeout parameter affects both the amount of time the automatic timeout calculation will wait before ignoring
the roundtrip time of a packet sent to the client, as well as the max allowable user input timeout. 
The restriction on the RTT of the packets used during the automatic timeout calculation ensures 
that if a packet is dropped, or takes an excessive amount of time to return it will not adversely 
affect the automatic timeout calculation.
	
2. After the parameters of the application have been set the program then needs to be compiled. 
This can be achieved by running the following command while in the same directory 
as the programs files. After running the command all the necessary files will be compiled and linked.
   
```
make all
```
   
3. To then execute the program you **MUST** run the following command on the thing server previously designated during
configuration as the server (**NOTE:** Thing 1 is the default server set in the Utilities.h file):
   
```
./NODE_A
```
   
Failure to run NODE_A on the designated server will result in the following error:
   
```
bind: Cannot assign requested address
```
   
After the server application is running you will be greeted with a similar screen to the following:   

![alt tag](https://i.imgur.com/nMQIVa3.png)
   
This indicates that the server has been successfully run and that it is now waiting for the client to connect.
   
4. Next you must run the following command on any of the other things server besides the one you are currently 
running the server on:
   
```
./NODE_B
```
   
Successfully connecting to the server from the client will result in a readout similar to the following to display:

![alt tag](https://i.imgur.com/G1IG8Ws.png)
   
If the client fails to connect to the server a readout similar to the following will be displayed:

![alt tag](https://i.imgur.com/vwQcaxb.png)
   
If this occurs, terminate the server application and go back to step 3 and repeat.
   
5. From here you will be prompted to set further parameters via a command line menu on the server. These parameters 
will include things such as the length of the timeout, if you wish to introduce situational errors, and the name of 
file you wish to transfer from the server to the client.


## Example Run

The following .gif is a sample run of the program sending a small file using the Go-Back-N protocol from thing1 to thing0:
<br>
**NOTE: The below .gif can be clicked to increase its size**

![alt tag](https://i.imgur.com/qLqP2vM.gif)

## Authors

* **Tyler Herrmann**   - [Tylerherrmann](https://github.com/Tylerherrmann)
* **Brandon Woodford** - [IveGotNorto](https://github.com/IveGotNorto)

## Acknowledgments

* Dr. Jack Tan for his continued support and guidance on this project.

