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

//This structure stores the <message> field of a RREQ and RREPpacket
//RREQ-Specific and RREP Message
typedef struct general_message{
	//uint8_t addr-length:4;
	uint8_t type;
	uint8_t seqno;
	/*if metric_type set to 0 hop_count is used otherwise route_metric is used*/
	uint8_t metric_type;
	uint32_t route_metric;
	uint8_t hop_limit;
	uint8_t hop_count;
	linkaddr_t orginator;
	linkaddr_t destination;
	//TODO:only used for RREP
	uint8_t ackrequired;
}rreq_message,rrep_message;

//This structure stores the <message> field of a RREP-ACK packet.
typedef struct rrep_ack_message_struture {
	//uint8_t addr-length:4;
	uint8_t seqno;
	linkaddr_t destination;
}rrep_ack_message;

typedef struct rerr_message_struture {
	//TODO:uint8_t addr-length:4;
	uint8_t hop_limit;
	uint8_t errorcode;
	linkaddr_t unreachable;
	linkaddr_t orginator;
	linkaddr_t destination;
}rerr_message;


/*------------------------------------------------------------------------------------------------------------------------*/
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

/*------------------------------------------------------------------------------------------------------------------------*/
/*Parameters and constants*/
#define ROUTE_TIMEOUT 5
#define RREP_ACK_TIMEOUT 0.1
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
#define MAXVALUE 255
#define VERBOSE 1
#define BACKOFF 1
#define HOP_COUNT 0
#define MAXA(A,B) ( ((A>B)&&((A-B)<=(MAXVALUE/2)))||((A<B)&&((B-A)>(MAXVALUE/2))) )

/*------------------------------------------------------------------------------------------------------------------------*/
/*check if rreq or rrep is valid return 0 means valid return -1 means invalid*/
//TODO input is rreq or should be con
int valid_check(struct general_message *input){
	//address check is skipped;
	struct route_entry *rt;
	struct blacklist_tuple * bl;

	if(linkaddr_cmp(input->orginator,&linkaddr_node_addr)){
	      PRINTF("Receive RREQ from itself\n");
	      return -1;
	}

	rt = route_lookup(input->orginator);
	if((rt!=NULL) && MAXA(rt->seqno,input->seqno) ){
	      PRINTF("Receive RREQ originator in already in routing table\n");
	      return -1;
	}
	//TODO: received address is not present as linkaddr_t
	if(input->type == RREQ_TYPE ){
		rt = route_blacklist_lookup((linkaddr_t*)from);
		if(rt!=NULL){
			PRINTF("Receive RREQ previous is in black list\n");
			return -1;
		}
	}
	return 0;
}



/*------------------------------------------------------------------------------------------------------------------------*/
static void
send_rreq(struct route_discovery_conn *c, rreq_message *input)
{
	linkaddr_t dest_copy;
	rreq_message *msg;
	linkaddr_copy(&dest_copy, dest);
	dest = &dest_copy;
	packetbuf_clear();
	msg = packetbuf_dataptr();
	packetbuf_set_datalen(sizeof(struct rreq_message));
	msg->type = RREQ_TYPE;
	msg->metric_type = input->metric_type;
	msg->route_metric = input->route_metric;
	msg->seqno = input->seqno;
	msg->hop_count = input->hop_count;
	msg->hop_limit = input->hop_limit;
	linkaddr_copy(&msg->destination,&input->destination);
	linkaddr_copy(&msg->originator,linkaddr_node_addr);
	netflood_send(&c->rreqconn, c->rreq_id);
	c->rreq_id++;
}

/*------------------------------------------------------------------------------------------------------------------------*/
static void
send_rrep(struct route_discovery_conn *c, const linkaddr_t *dest)
{
	struct route_entry *rt;
	linkaddr_t dest_copy;
	rrep_message *msg;
	linkaddr_copy(&dest_copy, dest);
	dest = &dest_copy;
	packetbuf_clear();
	msg = packetbuf_dataptr();
	packetbuf_set_datalen(sizeof(struct rreq_message));
	msg->type = RREP_TYPE;
	msg->metric_type = 0;
	msg->route_metric = 0;
	msg->seqno = 0;
	msg->hop_count = 0;
	msg->hop_limit = MAX_HOP_LIMIT;
	linkaddr_copy(&msg->destination,dest);
	linkaddr_copy(&msg->originator,linkaddr_node_addr);

	rt = route_lookup(dest);
	if(rt != NULL) {
	    PRINTF("%d.%d: send_rrep to %d.%d via %d.%d\n",
		   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
		   dest->u8[0],dest->u8[1],
		   rt->nexthop.u8[0],rt->nexthop.u8[1]);
	    unicast_send(&c->rrepconn, &rt->nexthop);
	}
	else {
		PRINTF("%d.%d: no route for rrep to %d.%d\n",
		   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
		   dest->u8[0],dest->u8[1]);
	}
}

/*------------------------------------------------------------------------------------------------------------------------*/
static void
send_rrep_ack(struct route_discovery_conn *c, rreq_message *input)
{
	struct route_entry *rt;
	linkaddr_t dest_copy;
	rrep_ack_message *msg;
	linkaddr_copy(&dest_copy, &input->orginator);
	dest = &dest_copy;
	packetbuf_clear();
	msg = packetbuf_dataptr();
	packetbuf_set_datalen(sizeof(struct rreq_message));
	msg->seqno = input->seqno;

	linkaddr_copy(&msg->destination,&input->orginator);

	rt = route_lookup(input->orginator);
	if(rt != NULL) {
	    PRINTF("%d.%d: send_rrep to %d.%d via %d.%d\n",
		   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
		   dest->u8[0],dest->u8[1],
		   rt->nexthop.u8[0],rt->nexthop.u8[1]);
	    unicast_send(&c->rrepconn, &rt->nexthop);
	}
	else {
		PRINTF("%d.%d: no route for rrep to %d.%d\n",
		   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
		   dest->u8[0],dest->u8[1]);
	}
}
/*------------------------------------------------------------------------------------------------------------------------*/
static void
send_rerr(struct route_discovery_conn *c, rerr_message *input)
{
	struct route_entry *rt;
	rerr_message *msg;
	packetbuf_clear();
	msg = packetbuf_dataptr();
	packetbuf_set_datalen(sizeof(struct rerr_message));

	msg->errorcode = input->errorcode;
	msg->hop_limit = input->hop_limit;
	linkaddr_copy(&msg->unreachable,&input->unreachable);
	linkaddr_copy(&msg->destination,&input->destination);
	linkaddr_copy(&msg->originator,&input->originator);

	rt = route_lookup(&msg->destination);
	if(rt != NULL) {
	    PRINTF("%d.%d: send_rrep to %d.%d via %d.%d\n",
		   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
		   dest->u8[0],dest->u8[1],
		   rt->nexthop.u8[0],rt->nexthop.u8[1]);
	    unicast_send(&c->rrepconn, &rt->nexthop);
	}
}

/*------------------------------------------------------------------------------------------------------------------------*/
static int
rreq_msg_reveived(struct netflood_conn *nf, const linkaddr_t *from,
		     const linkaddr_t *originator, uint8_t seqno, uint8_t hops)
{
	int ret_val = 0;
	rreq_message *msg = packetbuf_dataptr();
	rreq_message new_msg;
	struct route_entry *rt;
	struct route_discovery_conn *c = (struct route_discovery_conn *)
    ((char *)nf - offsetof(struct route_discovery_conn, rreqconn));

	PRINTF("%d.%d: rreq_packet_received from %d.%d hops %d  last %d.%d/%d\n",
	 linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
	 from->u8[0], from->u8[1],
	 hops,
     c->last_rreq_originator.u8[0],
     c->last_rreq_originator.u8[1],
	 c->last_rreq_id);

	ret_val = valid_check(msg);
	if(ret_val!=0){
		return ret_val;
	}

	new_msg->hop_count = msg->hop_count + 1;
	new_msg->hop_limit = msg->hop_limit - 1;

	//msg->type = 0 means use hop otherwise use other metrics
	//TODO consider other metrics document 11.2.4 11.2.5
	new_msg->type = msg_type;

	rt = route_lookup(msg->orginator);
	if(rt==NULL){
		//TODO:check if route_add done 11.2.7 and check input parameter
		route_add(msg->orginator, from, dist, msg->seqno);
	}
	else{
		//TODO:
		//route check to see if it need to be update
		//route_remove(struct route_entry *e);
		//route_add();
	}

    if(linkaddr_cmp(&msg->dest, &linkaddr_node_addr)) {
      PRINTF("%d.%d: route_packet_received: route request for our address\n",
	     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
      PRINTF("from %d.%d hops %d rssi %d lqi %d\n",
	     from->u8[0], from->u8[1],
	     hops,
	     packetbuf_attr(PACKETBUF_ATTR_RSSI),
	     packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
      send_rrep(c, msg->orginator);
      return 0; /* Don't continue to flood the rreq packet. */
    }
    else {
      /*      PRINTF("route request for %d\n", msg->dest_id);*/
      PRINTF("from %d.%d hops %d rssi %d lqi %d\n",
	     from->u8[0], from->u8[1],
	     hops,
	     packetbuf_attr(PACKETBUF_ATTR_RSSI),
	     packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
      if(msg->hop_count < MAX_HOP_COUNT && msg->hop_limits >0){
      		send_rreq(c,&new_msg);
      }

    return 1;
  }
}
/*------------------------------------------------------------------------------------------------------------------------*/
static int
rrep_msg_reveived(struct unicast_conn *uc, const linkaddr_t *from)
{
	int ret_val = 0;
	rrep_message *msg = packetbuf_dataptr();
	rrep_message new_msg;
	struct route_entry *rt;
	struct route_discovery_conn *c = (struct route_discovery_conn *)
    ((char *)nf - offsetof(struct route_discovery_conn, rreqconn));

	ret_val = valid_check(msg);
	if(ret_val!=0){
		return ret_val;
	}

	new_msg->hop_count = msg->hop_count + 1;
	new_msg->hop_limit = msg->hop_limit - 1;

	//msg->type = 0 means use hop otherwise use other metrics
	//TODO consider other metrics document 11.2.4 11.2.5
	new_msg->type = msg_type;

	rt = route_lookup(msg->orginator);
	if(rt==NULL){
		//TODO:check if route_add done 11.2.7 and check input parameter
		route_add(msg->orginator, from, dist, msg->seqno);
	}
	else{
		//TODO:
		//route check to see if it need to be update
		//route_remove(struct route_entry *e);
		//route_add();
	}
	if(msg->ackrequired){
		send_rrep_ack(c,new_msg);
	}
	return 0;
}

/*------------------------------------------------------------------------------------------------------------------------*/
static int
rerr_msg_process(struct unicast_conn *uc, const linkaddr_t *from)
{
	rerr_message *msg = packetbuf_dataptr();
	rerr_message new_msg;
	struct route_entry *rt;
	struct route_discovery_conn *c = (struct route_discovery_conn *)
    ((char *)nf - offsetof(struct route_discovery_conn, rreqconn));

	new_msg->hop_limit = msg->hop_limit - 1;
	new_msg->type = msg_type;

	rt = route_lookup(msg->unreachable);
	if(rt!==NULL &&linkaddr_cmp(rt->nexthop,from)){
			route_remove(rt);
			route_blacklist_add(rt);
			if(msg->hop_limit>0){
				send_rerr(c,new_msg);
			}
			return 1;
	}
	send_rerr(c,new_msg);
	return 0;
}
/*------------------------------------------------------------------------------------------------------------------------*/
static const struct unicast_callbacks rrep_callbacks = {rrep_packet_received};
static const struct netflood_callbacks rreq_callbacks = {rreq_packet_received, NULL, NULL};
/*------------------------------------------------------------------------------------------------------------------------*/
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

/*------------------------------------------------------------------------------------------------------------------------*/
void
route_discovery_close(struct route_discovery_conn *c)
{
  unicast_close(&c->rrepconn);
  netflood_close(&c->rreqconn);
  ctimer_stop(&c->t);
}

/*------------------------------------------------------------------------------------------------------------------------*/
static void
timeout_handler(void *ptr)
{
  struct route_discovery_conn *c = ptr;
  PRINTF("route_discovery: timeout, timed out\n");
  rrep_pending = 0;
  if(c->cb->timedout) {
    c->cb->timedout(c);
  }
}

/*------------------------------------------------------------------------------------------------------------------------*/
int
route_discovery_discover(struct route_discovery_conn *c, const linkaddr_t *addr,
			 clock_time_t timeout)
{
  if(rrep_pending) {
    PRINTF("route_discovery_send: ignoring request because of pending response\n");
    return 0;
  }
	rreq_message new_msg;
	rreq_initial(&new_msg,addr);
	PRINTF("route_discovery_send: sending route request\n");
	ctimer_set(&c->t, timeout, timeout_handler, c);
	rrep_pending = 1;
	send_rreq(c, &new_msg);
	return 1;
}
/*------------------------------------------------------------------------------------------------------------------------*/
int
route_discovery_repairs(struct route_discovery_conn *c, const linkaddr_t *addr,
			 clock_time_t timeout)
{

}
/*------------------------------------------------------------------------------------------------------------------------*/
//generate the err msg
int
route_discovery_rerr(struct route_discovery_conn *c, uint8_t Error_Code,const linkaddr_t *broken_source_addr,
		const linkaddr_t *broken_dest_addr)
{
	//A packet with an RERR message is generated by the LOADng Router,detecting the link breakage
	struct route_entry *rt;
	rerr_message msg;
	packetbuf_clear();
	msg = packetbuf_dataptr();
	packetbuf_set_datalen(sizeof(struct rerr_message));
	msg->errorcode = Error_Code;
	msg->hop_limit = MAX_HOP_LIMIT;
	linkaddr_copy(&msg->unreachable,broken_dest_addr);
	linkaddr_copy(&msg->destination,broken_source_addr);
	linkaddr_copy(&msg->originator,&linkaddr_node_addr);

	rt = route_lookup(&msg->destination);
	if(rt != NULL) {
	    unicast_send(&c->rrepconn, &rt->nexthop);
	}

}
