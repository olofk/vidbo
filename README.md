# Virtual Development Board

A protocol for communicating with simulated development boards or chips over websockets.

The protocol can be used for many different things but the initial purpose is to expose simple I/O of an FPGA development board so that it can be interacted with through a web browser as seen below.

![Vidbo in a browser](vidbo_browser.png)

This repository contains the following components:
* Specification: Describes the format of the messages sent over the Websocket
* Backend helper: C code implementing an API that can be used in applications that implement the Vidbo backend side (e.g. simulators)
* Example project: Verilog and C++ code that implements a Vidbo backend + HTML and SVG that implements a simulated Nexys A7 development board

## Quick start example

This will run a simulation that exposes I/O over the vidbo interface. We will then start a web server that present the frontend as HTML and finally connect a browser to interact with the simulation through the frontend.

Make sure you have libwebsockets and verilator installed

1. Install FuseSoC `pip3 install fusesoc`
2. Create and enter an empty workspace directory where we will do our work `mkdir vidbo-example && cd vidbo-example`
3. Add the vidbo library to the workspace `fusesoc library add vidbo https://github.com/olofk/vidbo`
4. Run the example simulation `fusesoc run --target=example vidbo`
5. Open a new terminal in the workspace and start a webserver `python3 -m http.server --directory fusesoc_libraries/vidbo/example`
6. Connect a browser to `localhost:8000`
7. Have fun!

## Background

All chip designs inevitably have some kind of inputs and outputs to the world outside of the device under test. This I/O can be modeled in different ways for different purposes. For regression tests this often takes the form of predefined stimuli sent into the device and outputs being monitored to make sure the device did the expected thing. In other cases however, there is a need to communicate interactively with the simulated design. As an example this can be a simulation of a SoC running a user program, where the user wants to send commands over a simulated UART connection and see that the program behaves as expected by observing changes in the device's output. This adds more requirements to the simulated design, especially when there are multiple types of inputs and outputs. Many solutions exist already for various cases, such as outputting the UART output on the screen, creating a window to show graphics and communicating over sockets and FIFOs. One common problem with these solutions however is that they decrease the portability by depending on external libraries which can be hard to compile on some platforms. They are also not standardized which makes reuse harder

Vidbo is a protocol to be used over websockets to handle input, output and run control of chip designs running in simulations. Websockets was chosen since it's a a light-weight extension over standard sockets and allows the client side to be implemented in a web browser or any program that implements websockets.

Some use cases for the protocol are

* VCD on/off (to just capture the interesting parts)
* Software loading
* 2-way UART
* Data loading (e.g. SPI Flash emulation)
* Video output
* Ethernet
* Run and pause
* Emulated sensors

## Specification

Messages are JSON-encoded. All messages from simulator carries a timestamp of current simulator time. This time can not be decreasing. Simulator can send messages only containing the timestamp to inform the client of the current simulation time. Several messages with the same timestamp can be sent. Several classes of I/O are defined with different characteristics such as *gpio* for single-bit binary I/O signals (e.g. LEDs, switches buttons) and *serial* for serial byte streams such as UARTs

```json
/* Turn on LED 0 */
{
"time" : 12345,
"gpio" : {"LED0" : true}
}
```

Messages from client to simulator does not contain timestamps.

```json
/* Turn off switch 3. Turn on VCD capturing */
{
"gpio" : {"SW3" : false},
"vcd" : true
}
```

If several commands of the same type is sent in the same message, the result is undefined since we can't guarantee ordering in the json struct. Example

```json
/* VCD on and off at the same time. Undefined outcome */
{
"time" : 12345,
"vcd" : false,
"vcd" : true
}
```

TODO: Main websocket channel is text. Binary high-bandwidth data (e.g. ethernet and video) are sent over separate sockets as binary data. Client can ask simulator which additional sockets are used.

TODO : This covers several classes of devices. Would be good to mimic a strcuture here (like the Linux kernel) to avoid having to come up with a device classification from scratch.

TODO: Figure out corner cases and error handling

TODO: Is it necessary to have different sockets for high-bandwidth data. No clue how this works in practice

TODO: PWM for LEDs. Do we send each I/O change (lots of messages) or calculate a PWM value as a float in the sim (how do we decide when to send a new value? First case does not need special protocol handling so lets start with that

TODO: How do we handle 7-segment displays? Just a series of gpio to begin with to make it easy

TODO: Several use cases slot in somewhat with VirtIO. Maybe develop VirtIO<->Vidbo bridges for e.g. ethernet, block devices, console

TODO: How to handle non-buffered data streams like UART? One messages for each character? Do some clever queueing? Treat as binary data or text?

TODO: Transfering large chunks of data such as ihex file for programming or maybe a frame from a graphics output might be split across several messages (frames?) Need to investigate if this is a problem

## Protocol

### gpio

Turn GPIOs on and off. GPIO can be switches, LEDs (RGB LEDs would have three pins associated with them), buttons, 7-segment displays (each segment gets its own gpio pin)

Argument : dict of name/value (string/bool) pairs

dir: both

Example:
```json
/* To simulator */
{
   "gpio":{
      "SW0":true,
      "BTN4":false
   }
}

/* From simulator */
{
   "gpio":{
      "LED2":true,
   }
}
```

### serial

Send a byte on a byte-wide named serial channel, e.g. a UART. Each byte is sent as an integer. Values outside of 0-255 are undefined

Argument : dict of name/value (string/int) pairs

dir: both

Example:
```json
/* To simulator */
{
   "serial":{
      "UART0": 0x48 /* ASCII character 'H' */
   }
}

/* From simulator */
{
   "serial":{
      "stdout": 0x38 /* ASCII character '8' */
   }
}
```

## Backend helper

A C API for integrating vidbo e.g. with Verilator is supplied as well as an example to show how to connect this to the simulation loop.

The API consists of the functions defined in [vidbo.h](src/vidbo.h). An example simulation loop can look like this

```c
#include <time.h>

#include "vidbo.h"

int main(void) {
  vidbo_context_t vidbo_context;

  /* Define backend inputs */
  const char * const inputs[] = {
    "gpio.SW0",
  };
  int num_inputs = sizeof(inputs) / sizeof(inputs[0]);
  int *input_vals = (int *)calloc(num_inputs, sizeof(int));

  /* Initialize vidbo context */
  vidbo_init(&vidbo_context, 8081);

  /* Register inputs */
  vidbo_register_inputs(&vidbo_context, inputs, num_inputs);

  const char *serstr = "Hello world\n";
  int led0 = 0;
  time_t last_time = time(0);
  unsigned long sim_time = 0;
  int check_vidbo = 0;
  int done = 0;
  int sidx = 0;
  while (!(done)) {
    /* Invert led0 every second and send the output value */
    if (time(0) != last_time) {
      led0 = 1-led0;
      vidbo_send(&vidbo_context, sim_time, "gpio", "LD0", led0);
      last_time = time(0);
    }

    /* Check for input updates. If vidbo_recv returns 1, we have inputs to update */
    if (vidbo_recv(&vidbo_context, input_vals)) {

      /* Exit the loop if SW0 is true */
      if (input_vals[0])
	done = 1;
    }

    /* Write character to UART every 1000000 cycles */
    if (!(check_vidbo++ % 1000000)) {
      vidbo_send(&vidbo_context, sim_time, "serial", "uart", serstr[sidx++]);
      if (serstr[sidx] == 0)
	sidx = 0;
    }

    /* timestamp must be monotonically increasing */
    sim_time+=10;
  }

  vidbo_destroy(&vidbo_context);

  exit(0);
}
```

## Frontend example

JavaScript, HTML and an SVG to emulate the interactive parts of a Digilent Nexys A7 board is supplied in the [example directory](example/)

## References

- [[olofk/verilatio] A protocol for communicating with HDL simulations over websockets](https://github.com/olofk/verilatio)
- [[dbhi/vboard] Virtual development board for HDL design > References](https://github.com/dbhi/vboard#references)
