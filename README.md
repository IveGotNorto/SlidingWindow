# KeyExchange
## CS.452 Operating Systems Lab #1: Key Exchange Application

The Objective of this project is to practice the concepts of computer security, end-to-end protocol design and implementation.

[Link to pdf specifications.](https://cs.uwec.edu/~tan/priv/www-docs/cs452/Labs/Lab1/lab1.pdf)

Instructions on how to run this application:
0. Depending on which mode you wish to use (File transmission or message sending) a debug flag must be flipped in the file utilities.h. Setting this flag to 0 will perform encrypted file transmission, and setting it to 1 will perform encrypted message sends.
1. Compile all files using the supplied makefile. Do so using the following command: make all
2. Run KDC on cs.thing0.uwec.edu (10.35.195.46). Do so using the following command: ./kdc
3. Run ResponderB on cs.thing2.uwec.edu (10.35.195.22). Do so using the following command: ./responderB
4. Run InitiatorA on cs.this1.uwec.edu (10.35.195.47). Do so using the following command: ./initiatorA
5. Input all necessary data into InitiatorA, KDC, and ResponderB. InitiatorA will request a Key as well as a nonce. ResponderB will also requst a key and a nonce. KDC will also request both A and B's keys, ensure that the entry of these keys into the KDC matches the keys input to both InitatorA and ResponderB.
6. Now depending on the debug mode set InitiatorA will now request either a file name or a message to send to responderB. 

Contributors:
@Tylerherrmann - Tyler Herrmann
@IveGotNorto - Brandon Woodford
@Mknappskirata - Matt Knapp
@Jknappskirata - John Knapp
