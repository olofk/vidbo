Virtual Development Board
=========================

Components for exposing I/O of a device running in a simulator.

The project consists of several parts.

Specification
-------------

The specification describes the format of the messages sent over the websocket connection. This allows frontends and backends to be developed separately.

Backend helper
--------------

A C API for integrating vidbo e.g. with Verilator is supplied as well as an example to show how to connect this to the simulation loop

Frontend example
----------------

JavaScript, HTML and an SVG to emulate the interactive parts of a Digilent Nexys A7 board is supplied.

Quick start example
-------------------

This will run a simulation that exposes I/O over the vidbo interface. We will then start a web server that present the frontend as HTML and finally connect a browser to interact with the simulation through the frontend.

Make sure you have libwebsockets and verilator installed

1. Install FuseSoC `pip3 install fusesoc`
2. Create and enter an empty workspace directory where we will do our work `mkdir vidbo-example && cd vidbo-example`
3. Add the vidbo library to the workspace `fusesoc library add vidbo https://github.com/olofk/vidbo`
4. Run the example simulation `fusesoc run --target=example vidbo`
5. Open a new terminal in the workspace and start a webserver `python3 -m http.server --directory fusesoc_libraries/vidbo/example`
6. Connect a browser to `localhost:8000`
7. Have fun!

References
----------

- [[olofk/verilatio] A protocol for communicating with HDL simulations over websockets](https://github.com/olofk/verilatio)
- [[dbhi/vboard] Virtual development board for HDL design > References](https://github.com/dbhi/vboard#references)
