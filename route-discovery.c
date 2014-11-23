/**
 * \addtogroup routediscovery
 * @{
 */

/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
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
 *         Route discovery protocol(Using LOADng)
 * \author
 *         {Jiahao Liang, Zhikun Liu, Haimo Bai} @ USC
 */

#include "contiki.h"
#include "net/rime.h"
#include "net/rime/route.h"
#include "net/rime/route-discovery.h"

#include <stddef.h> /* For offsetof */
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/*Structures we need to implement*/
struct tlv {};
//This structure stores a TLV vector of the TLV block.
struct rerr_message {};
//This structure stores the <message> field of a RERR packet.
struct rrep_ack_message {};
//This structure stores the <message> field of a RREP-ACK packet.
struct route_message {};
//This structure stores the <message> field of a RREQ or RREP packet.72
struct route_discovery_packet {};
//This structure points to the information of a RREQ or RREP packet.
struct rrep_ack_packet {};
//This structure points to the information of a RREQ-ACK packet.
struct rerr_packet {};
//This structure points to the information of a RERR packet.
struct request_tuple {};
//A tuple in the Request List.
struct route_discovery_callbacks {};
//route discovery callbacks structure
struct route_discovery_conn {};
//route discovery connection structure
/*---------------------------------------------------------------------------*/

#if CONTIKI_TARGET_NETSIM
#include "ether.h"
#endif


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
/*Parameters and constants*/
#define ROUTE_TIMEOUT 5 ∗ CLOCK_SECOND
#define RREP_ACK_TIMEOUT 0.1 ∗ CLOCK_SECOND
#define ROUTE_DISCOVERY_ENTRIES 8
#define MAX_RETRIES 3
#define MAX_REPAIR_RETRIES max_retries
#define NUM_REQ_ENTRIES 8
#define MEAN_BACKOFF 50
#define UNIFORM 1
#define EXPONENTIAL 1-uniform
#define ACK_REQUIRED 1
#define METRICS 0
/*Defines*/
#define RREQ_TYPE 0x0
#define RREP_TYPE 0x1
#define RERR_TYPE 0x2
#define RREP_ACK_TYPE 0x3 • #define MAXVALUE 255
#define VERBOSE 1
#define BACKOFF 1
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
//Routing Set
//Stores the Request Tuples responsible to resend RREQ packets in case of not receiving a RREP.
LIST (request_set);
MEMB (request_mem, struct request_tuple, NUM_REQ_ENTRIES);

/*---------------------------------------------------------------------------*/
//Opens a route discovery connection.
void
route_discovery_open(struct route_discovery_conn *c,
		     clock_time_t time,
		     uint16_t channels,
		     const struct route_discovery_callbacks *callbacks)
{

}
/*---------------------------------------------------------------------------*/
//Closes a route discovery connection.
void
route_discovery_close(struct route_discovery_conn *c)
{

}

/*---------------------------------------------------------------------------*/
//Starts a LOADng route discovery protocol.
int
route_discovery_discover(struct route_discovery_conn *c, const rimeaddr_t *addr,
			 clock_time_t timeout)
{

}
/*---------------------------------------------------------------------------*/
//Opens a route discovery repair procedure.
int
route_discovery_repair(struct route_discovery_conn *c,const rimeaddr_t *addr,
		clock_time_t timeout)
{

}

/*---------------------------------------------------------------------------*/
//Initiates the route error communication procedure.
int
route_discovery_rerr (struct route_discovery_conn *c, uint8_t Error_Code,
		const rimeaddr_t *broken_source_addr, const rimeaddr_t *broken_dest_addr)
{

}

/*---------------------------------------------------------------------------*/
/** @} */
