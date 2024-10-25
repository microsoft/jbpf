# Reverse Proxy

This example showcases how to map network requests to *jbpf* LCM unix socket api. **It is not meant for production environments.**

This example is a companion to [first_example_ipc](../first_example_ipc/). This proxy app can be started alongside the *jbpf* agent to load and unload codelets with a REST request.



The example is a [boost.beast](https://github.com/boostorg/beast) simple REST server with two methods implemented. `POST` to load a codelet set and `DELETE` to unload. The request body for the load request matches the configuration file used in the [lcm_cli](../../tools/lcm_cli/).

Prerequisites (for Ubuntu 22.04, modify for your development environment):
* cmake
* build-essential
* libboost-dev
* libboost-program-options-dev

## Usage

Firstly, run the set up for [first_example_ipc](../first_example_ipc/) without loading or unloading a codelet.

You have `./run_collect_control.sh` and `./run_app.sh` running in two terminals from the `first_example_ipc` instructions.
In a third terminal start the proxy:
```sh
$ source ../../setup_jbpf_env.sh
$ ./run_proxy.sh
```

If successful you will see the following lines:
```
Forwarding messages to: /var/run/jbpf/jbpf_lcm_ipc
Listening on: 0.0.0.0:8080
```

Now you can load and unload the codelet using a REST request:
```sh
# Load the codelet:
./load.sh
```

You should get the following output:
```
Loading codelet set: example_codeletset
```
And you can now observe in the primary and agent terminals that data is being produced.

Finally. 
```sh
# Unload the codelet
./unload.sh
```
