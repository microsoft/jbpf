# Data input and output architecture

*jbpf* provides shared memory API to collect data from a codelet and send the data to it. 
This API is marked as *Output API* and *Input API* in the figure below.  
Developers needs to provide code that will consume the API in a safe and scalable way. 
This is marked as *Data collection and control* in the figure below. 

![Architectural overview](./jbpf_oss_architecture.png)


## Thread mode

The default mode is a *thread-mode*, which is also easiest to get started. 
An example is given [here](../examples/first_example_standalone).
In this mode, *jbpf* starts a thread that performs the *Data collection and control* functionality.
It attaches to the *output API* of *jbpf* and checks for any data delivered by *jbpf*. 
Every time a message is created by the codelet, the threads calls a callback, previously registered. 
In this example, the callback prints the received message and also sends its sequence number back to the codelet to demonstrate that functionality. 


## IPC mode

A more involved example will require a developer to build their own *Data collection and control* framework. 
This is given in an example [here](../examples/first_example_ipc).
In this example developers writes their own *Data collection and control* [framework](../examples/first_example_ipc/example_collection_control.cpp).
It creates an inter-process shared memory pool with the application and it polls the pool for new messages. 
