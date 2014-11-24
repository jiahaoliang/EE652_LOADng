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
//This structure stores a TLV vector of the TLV block.
struct tlv {
	uint8_t count:4;
	uint8_t type:4;
	uint8_t flags:4;
	/* TODO:tlv-value should be implement when using,append after the struct*/
	uint8_t *value;		
};

//This structure stores the <message> field of a RERR packet.
struct rerr_message {
	uint8_t error-code:4;
	uint8_t addr-length:4;
	/*TODO:both destination address and originator address length are not an instant*/
	linkaddr_t destination;
	linkaddr_t orginator;
};

//This structure points to the information of a RERR packet.
struct rerr_packet {
	uint8_t type;
	struct tlv tlv-block;
	struct rerr_message;
};

//This structure stores the <message> field of a RREP-ACK packet.
struct rrep_ack_message {
	uint8_t addr-length:4;
	uint16_t rreq-id;
	/*TODO:both destination address and originator address length are not an instant*/
	const rimeaddr_t orginator;
};

//This structure points to the information of a RREQ-ACK packet.
struct rrep_ack_packet {
	uint8_t type;
	struct tlv tlv-block;
	struct rrep_ack_message;
};

//This structure stores the <message> field of a RREQ or RREP packet
struct route_message {
	uint8_t flags:4;
	uint8_t addr-length:4;
	uint8_t metric:4;
	uint8_t weak-links:4;
	uint16_t rreq-id;
	uint8_t route-cost;
	/*TODO:both destination address and originator address length are not an instant*/
	linkaddr_t destination;
	linkaddr_t orginator;
};

//This structure points to the information of a RREQ or RREP packet.
struct route_discovery_packet {
	uint8_t type;
	struct tlv tlv-block;
	struct route_message;
};

//A tuple in the Request List.
struct request_tuple {
	uint8_t routing_family;
	uint8_t routing_dest_len;
	uint8_t routing_src_len; 
	uint8_t routing_type;
	
	linkaddr_t nexthop;
	linkaddr_t prevhop;
	
	uint8_t time;
};


//route discovery connection structure
struct route_discovery_conn {
  /*TODO: address structure*/
  linkaddr_t t;
  uint8_t last_rreq_id;
  uint8_t rreq_id;
  struct netflood_conn rreqconn;
  struct unicast_conn rrepconn;
};

//route discovery callbacks structure
struct route_discovery_callbacks {
  struct route_discovery_conn *c;
  }

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
	netflood_open(&c->rreqconn, time, channels + 0, &rreq_callbacks);
	unicast_open(&c->rrepconn, channels + 1, &rrep_callbacks);
	c->cb = callbacks;
}
/*---------------------------------------------------------------------------*/
//Closes a route discovery connection.
void
route_discovery_close(struct route_discovery_conn *c)
{
	unicast_close(&c->rrepconn);
	netflood_close(&c->rreqconn);
	ctimer_stop(&c->t);
}

/*---------------------------------------------------------------------------*/
//Starts a LOADng route discovery protocol.
int
route_discovery_discover(struct route_discovery_conn *c, const rimeaddr_t *addr,
			 clock_time_t timeout){
	ctimer_set(&c->t, timeout, timeout_handler, c);
	rrep_pending = 1;
	/*generate broadcast message*/
	linkaddr_t dest_copy;
	char buf[MAXBUFSIZE];
	struct route_msg *msg;
	linkaddr_copy(&dest_copy, dest);
	dest = &dest_copy;
	msg->rreq_id = c->rreq_id;
	linkaddr_copy(&msg->dest, dest);
	strcpy(buf,route_msg);
	netflood_send();
	c->rreq_id++;

	return 1;
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
	linkaddr_t dest_copy;
	char buf[MAXBUFSIZE];
	struct rerr_packet *packet;
	linkaddr_copy(&dest_copy, dest);
	strcpy(buf,rerr_packet);
	unicast_send();
	c->rreq_id++;
}

/*---------------------------------------------------------------------------*/
/** @} */
