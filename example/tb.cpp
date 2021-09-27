// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Olof Kindgren
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdint.h>
#include <signal.h>

#include "vidbo.h"

#include "Vtop.h"

using namespace std;

static bool done;

vluint64_t main_time = 0;

double sc_time_stamp () {
  return main_time;
}

void INThandler(int signal) {
  printf("\nCaught ctrl-c\n");
  done = true;
}

int main(int argc, char **argv, char **env) {
  vidbo_context_t vidbo_context;

  const char * const inputs[] = {
    "gpio.SW0",
    "gpio.SW1",
    "gpio.SW2",
    "gpio.SW3",
    "gpio.SW4",
    "gpio.SW5",
    "gpio.SW6",
    "gpio.SW7",
    "gpio.SW8",
    "gpio.SW9",
    "gpio.SW10",
    "gpio.SW11",
    "gpio.SW12",
    "gpio.SW13",
    "gpio.SW14",
    "gpio.SW15",
  };
  int num_inputs = 16;

  vidbo_register_inputs((void *)inputs, num_inputs);

  int *input_vals = (int *)calloc(16, sizeof(int));

  vidbo_init(&vidbo_context, 8081);

  vidbo_register_inputs((void *)inputs, num_inputs);
	
  Verilated::commandArgs(argc, argv);

  Vtop* top = new Vtop;

  signal(SIGINT, INThandler);

  int check_vidbo = 0;

  top->i_clk = 1;
  int last_leds = top->o_led;
  int sidx = 0;
  const char *serstr = "UART my lucky start\n";
  while (!(done || Verilated::gotFinish())) {

    top->eval();

    /* To improve performance, only poll websockets connection every 10000 sim cycles */
    check_vidbo++;
    if (!(check_vidbo % 10000)) {

      /* Send out all GPIO status
       TODO: Only send changed pins.
      */
      char item[5] = {0}; //Space for LD??\0
      if (last_leds != top->o_led) {
	for (int i=0;i<16;i++) {
	  snprintf(item, 5, "LD%d", i);
	  vidbo_send(&vidbo_context, main_time, "gpio", item, (top->o_led>>i) & 0x1);
	}
	last_leds = top->o_led;
      }

      /* Check for input updates. If vidbo_recv returns 1, we have inputs to update */
      if (vidbo_recv(&vidbo_context, input_vals)) {

	/* Update the GPIO inputs from the received frame */
	top->i_sw = 0;
	for (int i=0;i<16;i++)
	  if (input_vals[i])
	    top->i_sw |= (1 << i);
      }
    }

    /* Write character to UART */
    if (!(check_vidbo % 1000000)) {
      vidbo_send(&vidbo_context, main_time, "serial", "uart", serstr[sidx++]);
      if (serstr[sidx] == 0)
	sidx = 0;
    }

    top->i_clk = !top->i_clk;
    main_time+=10;
  }


  exit(0);
}
