/*
 * Copyright (c) 2014, University of Southern California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Testcase for LOADng route.c. Please put it in the folder examples/LOADng/
 * \author
 *         Jiahao Liang <jiahaoli@usc.edu>
 */
#include "contiki.h"
#include "net/rime/route.h"

#include <stdio.h>
#include <string.h>


#define ADDR_NUM 10
#define ADD_NUM 3

/*---------------------------------------------------------------------------*/
PROCESS(route_test, "test case for route.c");
AUTOSTART_PROCESSES(&route_test);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(route_test, ev, data)
{

  PROCESS_BEGIN();
  rimeaddr_t addr[ADDR_NUM];
  struct dist_tuple dist;
  memset(&dist, 0, sizeof(dist));
  int i;
  //initialize address set
  for (i = 0; i < ADDR_NUM; i++) {
	  addr[i].u8[0] = i;
	  addr[i].u8[1] = i;
  }
  route_init();

  for (i = 0; i < ADD_NUM; i++) {
	  dist.route_cost = i;
	  dist.weak_links = 0;
	  printf("test: route_add dest = %d.%d\n", addr[i].u8[0], addr[i].u8[1]);
	  route_add(&addr[i], &addr[i+1], &dist, i);
	  printf("test: route_lookup dest = %d.%d\n", addr[i].u8[0], addr[i].u8[1]);
	  route_lookup(&addr[i]);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
