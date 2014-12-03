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
	rimeaddr_t originator;
	rimeaddr_t destination;
	//TODO:only used for RREP
	uint8_t ackrequired;
}rreq_message,rrep_message;

//This structure stores the <message> field of a RREP-ACK packet.
typedef struct rrep_ack_message_struture {
	//uint8_t addr-length:4;
	uint8_t type;
	uint8_t seqno;
	rimeaddr_t destination;
}rrep_ack_message;

typedef struct rerr_message_struture {
	//TODO:uint8_t addr-length:4;
	uint8_t type;
	uint8_t hop_limit;
	uint8_t errorcode;
	rimeaddr_t unreachable;
	rimeaddr_t originator;
	rimeaddr_t destination;
}rerr_message;


/*------------------------------------------------------------------------------------------------------------------------*/
#if CONTIKI_TARGET_NETSIM
#include "ether.h"
#endif

#define DEBUG 1
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
#define MAX_HOP_COUNT 255
#define MAX_HOP_LIMIT 255

/*Defines*/
#define MAXVALUE 255
#define VERBOSE 1
#define BACKOFF 1
#define HOP_COUNT 0
#define FALSE -1
#define TRUE 0
#define SENDREP 1
#define FORWARD 2
#define DROP 3
#define MAXA(A,B) ( ((A>B)&&((A-B)<=(MAXVALUE/2)))||((A<B)&&((B-A)>(MAXVALUE/2))) )

static uint8_t rreq_seqno = 0;
static uint8_t rrep_seqno = 0;
static char rrep_pending = 0;

/*------------------------------------------------------------------------------------------------------------------------*/
/*check if rreq or rrep is valid return 0 means valid return -1 means invalid*/
//TODO input is rreq or should be con
int valid_check(struct general_message *input, const rimeaddr_t *from){
	//address check is skipped;
	struct route_entry *rt;
	struct blacklist_tuple *bl;

	if(rimeaddr_cmp(&input->originator,&rimeaddr_node_addr)){
	      PRINTF("valid_check: Receive RREQ from itself\n");
	      return FALSE;
	}

	rt = route_lookup(&input->originator);
	if((rt!=NULL) && MAXA(rt->R_seq_num,input->seqno) ){
	      PRINTF("valid_check: Receive RREQ originator in already in routing table\n");
	      return FALSE;
	}
	//TODO: received address is not present as rimeaddr_t
	if(input->type == RREQ_TYPE ){
		bl = route_blacklist_lookup(from);
		if(bl!=NULL){
			PRINTF("valid_check:Receive RREQ previous is in black list\n");
			return FALSE;
		}
	}
	return TRUE;
}
/*------------------------------------------------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------------------------------------------------*/
static void
send_rreq(struct route_discovery_conn *c, rreq_message *input)
{
	rreq_message *msg;
	packetbuf_clear();
	msg = packetbuf_dataptr();
	packetbuf_set_datalen(sizeof(rreq_message));
	msg->type = RREQ_TYPE;
	msg->metric_type = input->metric_type;
	msg->route_metric = input->route_metric;
	msg->seqno = input->seqno;
	msg->hop_count = input->hop_count;
	msg->hop_limit = input->hop_limit;
	rimeaddr_copy(&msg->destination,&input->destination);
	rimeaddr_copy(&msg->originator,&input->originator);
	//netflood_send(&c->rreqconn, c->rreq_id);
	netflood_send(&c->rreqconn, msg->seqno);
	//c->rreq_id++;
    PRINTF("send_rreq: %d.%d: send_rreq orig: %d.%d dest: %d.%d "
    		"route_metric: %d hop_count: %d seqno: %d\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	   msg->originator.u8[0], msg->originator.u8[1],
	   msg->destination.u8[0], msg->destination.u8[1],
	   msg->route_metric, msg->hop_count, msg->seqno);
}

/*------------------------------------------------------------------------------------------------------------------------*/
static void
send_rrep(struct route_discovery_conn *c, rrep_message *input)
{
	struct route_entry *rt;
	rrep_message *msg;
	packetbuf_clear();
	msg = packetbuf_dataptr();
	packetbuf_set_datalen(sizeof( rrep_message));
	msg->type = RREP_TYPE;
	msg->metric_type = input->metric_type;
	msg->route_metric = input->route_metric;
	msg->seqno = input->seqno;
	msg->hop_count = 0;
	msg->hop_limit = MAX_HOP_LIMIT;
	rimeaddr_copy(&msg->destination,&input->destination);
	rimeaddr_copy(&msg->originator,&input->originator);

	rt = route_lookup(&msg->destination);
	if(rt != NULL) {
	    PRINTF("send_rrep: %d.%d: send_rrep to %d.%d via %d.%d "
	    		"orig: %d.%d dest: %d.%d route_metric: %d hop_count: %d seqno: %d\n",
		   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
		   msg->destination.u8[0],msg->destination.u8[1],
		   rt->R_next_addr.u8[0],rt->R_next_addr.u8[1],
		   msg->originator.u8[0], msg->originator.u8[1],
		   msg->destination.u8[0], msg->destination.u8[1],
		   msg->route_metric, msg->hop_count, msg->seqno);
	    unicast_send(&c->rrepconn, &rt->R_next_addr);
	} else {
		PRINTF("send_rrep: no route entry from %d.%d to %d.%d\n",
			rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
			msg->destination.u8[0],msg->destination.u8[1]);
	}
}

/*------------------------------------------------------------------------------------------------------------------------*/
static void
send_rrep_ack(struct route_discovery_conn *c, rrep_ack_message *input)
{
	struct route_entry *rt;
	rrep_ack_message *msg;
	packetbuf_clear();
	msg = packetbuf_dataptr();
	packetbuf_set_datalen(sizeof(rrep_ack_message));
	msg->seqno = input->seqno;
	rimeaddr_copy(&msg->destination,&input->destination);

	rt = route_lookup(&msg->destination);
	if(rt != NULL) {
	    PRINTF("send_rrep_ack: %d.%d: send_rrep to %d.%d via %d.%d\n",
		   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
		   msg->destination.u8[0],msg->destination.u8[1],
		   rt->R_next_addr.u8[0],rt->R_next_addr.u8[1]);
	    unicast_send(&c->rrepconn, &rt->R_next_addr);
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
	packetbuf_set_datalen(sizeof( rerr_message));

	msg->errorcode = input->errorcode;
	msg->hop_limit = input->hop_limit;
	rimeaddr_copy(&msg->unreachable,&input->unreachable);
	rimeaddr_copy(&msg->destination,&input->destination);
	rimeaddr_copy(&msg->originator,&input->originator);

	rt = route_lookup(&msg->destination);
	if(rt != NULL) {
	    PRINTF("send_rerr: %d.%d: send_rrep to %d.%d via %d.%d\n",
		   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
		   msg->destination.u8[0],msg->destination.u8[1],
		   rt->R_next_addr.u8[0],rt->R_next_addr.u8[1]);
	    unicast_send(&c->rrepconn, &rt->R_next_addr);
	}
}

/*------------------------------------------------------------------------------------------------------------------------*/
static int
rreq_msg_received(struct netflood_conn *nf, const rimeaddr_t *from)
{
	int ret_val = 0;
	rreq_message *msg = packetbuf_dataptr();
	struct general_message new_msg;	//the new msg, can be either rreq pr rrep
	struct route_entry *rt;
	struct route_discovery_conn *c = (struct route_discovery_conn *)
    ((char *)nf - offsetof(struct route_discovery_conn, rreqconn));

	PRINTF("rreq_msg_received: %d.%d: rreq_packet_received from %d.%d "
			"orig: %d.%d dest: %d.%d route_metric: %d hop_count: %d seqno: %d\n",
	 rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	 from->u8[0], from->u8[1],
	 msg->originator.u8[0], msg->originator.u8[1],
	 msg->destination.u8[0], msg->destination.u8[1],
	 msg->route_metric, msg->hop_count, msg->seqno);

	ret_val = valid_check(msg, from);
	if(ret_val!=0){
		return ret_val;
	}
	rimeaddr_copy(&new_msg.destination,&msg->destination);
	rimeaddr_copy(&new_msg.originator,&msg->originator);
	rt = route_lookup(&msg->originator);
	if(rt==NULL){
		//TODO:check if route_add done 11.2.7 and check input parameter
		//route_add the third parameter should be metric?
		struct dist_tuple new_tup;
		new_tup.weak_links = 0;
		new_tup.route_cost = msg->hop_count + 1;
		route_add(&msg->originator, from, &new_tup, msg->seqno);
	}
	else{
		//TODO:
		//route check to see if it need to be update
		//route_remove(struct route_entry *e);
		//route_add();
	}

    if(rimeaddr_cmp(&msg->destination, &rimeaddr_node_addr)) {
      PRINTF("rreq_msg_received: %d.%d: route_packet_received: route request for our address\n",
	     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
      PRINTF("rreq_msg_received: from %d.%d rssi %d lqi %d\n",
	     from->u8[0], from->u8[1],
	     packetbuf_attr(PACKETBUF_ATTR_RSSI),
	     packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
	  //generate new rrep
		new_msg.type = RREP_TYPE;
		new_msg.metric_type = 0;
		new_msg.route_metric = 0;
		new_msg.seqno = rrep_seqno;
		rrep_seqno++;
		new_msg.hop_count = 0;
		new_msg.hop_limit = MAX_HOP_LIMIT;
		rimeaddr_t temp_dest;
		rimeaddr_copy(&temp_dest,&new_msg.destination);
		rimeaddr_copy(&new_msg.destination,&new_msg.originator);
		rimeaddr_copy(&new_msg.originator,&temp_dest);
      send_rrep(c, &new_msg);
      return SENDREP; /* Don't continue to flood the rreq packet. */
    }
    else {
      /*      PRINTF("route request for %d\n", msg->dest_id);*/
      PRINTF("rreq_msg_received: from %d.%d rssi %d lqi %d\n",
	     from->u8[0], from->u8[1],
	     packetbuf_attr(PACKETBUF_ATTR_RSSI),
	     packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
      if(msg->hop_count < MAX_HOP_COUNT && msg->hop_limit >0){
    		//TODO:weak link ?
    		new_msg.hop_count = msg->hop_count + 1;
    		new_msg.hop_limit = msg->hop_limit - 1;
    		new_msg.route_metric = msg->hop_count + 1;
    		new_msg.seqno = msg->seqno;
    		//msg->metric_type = 0 means use hop otherwise use other metrics
    		//TODO consider other metrics document 11.2.4 11.2.5
    		new_msg.metric_type = msg->metric_type;
    		new_msg.type = RREQ_TYPE;
      		send_rreq(c,&new_msg);
      		return FORWARD;
      }
    }
    return DROP;
}
/*------------------------------------------------------------------------------------------------------------------------*/
static int
rrep_msg_received(struct unicast_conn *uc, const rimeaddr_t *from)
{
	int ret_val = 0;
	rreq_message *msg = packetbuf_dataptr();
	rreq_message new_msg;
	struct route_entry *rt;
	struct route_discovery_conn *c = (struct route_discovery_conn *)
	    ((char *)uc - offsetof(struct route_discovery_conn, rrepconn));

	PRINTF("rrep_msg_received: %d.%d: rrep_msg_received from %d.%d "
			"orig: %d.%d dest: %d.%d route_metric: %d hop_count: %d seqno: %d\n",
	 rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	 from->u8[0], from->u8[1],
	 msg->originator.u8[0], msg->originator.u8[1],
	 msg->destination.u8[0], msg->destination.u8[1],
	 msg->route_metric, msg->hop_count, msg->seqno);
	ret_val = valid_check(msg, from);
	if(ret_val!=0){
		return ret_val;
	}

	rt = route_lookup(&msg->originator);
	if(rt==NULL){
		//TODO:check if route_add done 11.2.7 and check input parameter
		//route_add the third parameter should be metric?
		struct dist_tuple new_tup;
		new_tup.weak_links = 0;
		new_tup.route_cost = msg->hop_count + 1;
		route_add(&msg->originator, from, &new_tup, msg->seqno);
	}
	else{
		//TODO:
		//route check to see if it need to be update
		//route_remove(struct route_entry *e);
		//route_add();
	}

	/*if(msg->ackrequired){
		send_rrep_ack(c,new_msg);
	}*/
	if(!rimeaddr_cmp(&msg->destination, &rimeaddr_node_addr)) {
		//TODO:weak link ?
		new_msg.hop_count = msg->hop_count + 1;
		new_msg.hop_limit = msg->hop_limit - 1;
		new_msg.seqno = msg->seqno;
		new_msg.route_metric = msg->hop_count + 1;
		new_msg.type = msg->type;
		//msg->metric_type = 0 means use hop otherwise use other metrics
		//TODO consider other metrics document 11.2.4 11.2.5
		new_msg.metric_type = msg->metric_type;
		rimeaddr_copy(&new_msg.destination,&msg->destination);
		rimeaddr_copy(&new_msg.originator,&msg->originator);
		  send_rrep(c, &new_msg);
	      return SENDREP; /* Don't continue to flood the rreq packet. */
	}else {
	    PRINTF("rrep_msg_received: rrep for us!\n");
	    rrep_pending = 0;
	    ctimer_stop(&c->t);
	    if(c->cb->new_route) {
	      rimeaddr_t originator;

	      /* If the callback modifies the packet, the originator address
	         will be lost. Therefore, we need to copy it into a local
	         variable before calling the callback. */
	      rimeaddr_copy(&originator, &msg->originator);
	      c->cb->new_route(c, &originator);
	    }
	}
	return TRUE;
}

/*------------------------------------------------------------------------------------------------------------------------*/
static int
rerr_msg_process(struct unicast_conn *uc, const rimeaddr_t *from)
{
	rerr_message *msg = packetbuf_dataptr();
	rerr_message new_msg;
	struct route_entry *rt;
	struct route_discovery_conn *c = (struct route_discovery_conn *)
    ((char *)uc - offsetof(struct route_discovery_conn, rreqconn));

	new_msg.hop_limit = msg->hop_limit - 1;
	new_msg.type = msg->type;

	rt = route_lookup(&msg->unreachable);
	if(rt != NULL && rimeaddr_cmp(&rt->R_next_addr,from)){
			route_remove(rt);
			uint8_t time = 255;
			route_blacklist_add(&rt->R_dest_addr,time);
			if(msg->hop_limit>0){
				send_rerr(c,&new_msg);
			}
			return 1;
	}
	send_rerr(c,&new_msg);
	return 0;
}
/*------------------------------------------------------------------------------------------------------------------------*/
static const struct unicast_callbacks rrep_callbacks = {rrep_msg_received};
static const struct netflood_callbacks rreq_callbacks = {rreq_msg_received, NULL, NULL};
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
  PRINTF("route_discovery_open: \n");
}

/*------------------------------------------------------------------------------------------------------------------------*/
void
route_discovery_close(struct route_discovery_conn *c)
{
  unicast_close(&c->rrepconn);
  netflood_close(&c->rreqconn);
  ctimer_stop(&c->t);
  PRINTF("route_discovery_close: \n");
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
void rreq_initial(rreq_message *msg,const rimeaddr_t *addr){

	msg->type = RREQ_TYPE;
	msg->metric_type = 0;
	msg->route_metric = 0;
	msg->seqno = rreq_seqno;
	msg->hop_count = 0;
	msg->hop_limit = MAX_HOP_LIMIT;
	rimeaddr_copy(&msg->destination,addr);
	rimeaddr_copy(&msg->originator,&rimeaddr_node_addr);
	rreq_seqno++;
}

/*------------------------------------------------------------------------------------------------------------------------*/
int
route_discovery_discover(struct route_discovery_conn *c, const rimeaddr_t *addr,
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
route_discovery_repairs(struct route_discovery_conn *c, const rimeaddr_t *addr,
			 clock_time_t timeout)
{

}
/*------------------------------------------------------------------------------------------------------------------------*/
//generate the err msg
int
route_discovery_rerr(struct route_discovery_conn *c, uint8_t Error_Code,const rimeaddr_t *broken_source_addr,
		const rimeaddr_t *broken_dest_addr)
{
	//A packet with an RERR message is generated by the LOADng Router,detecting the link breakage
	struct route_entry *rt;
	rerr_message *msg;
	packetbuf_clear();
	msg = packetbuf_dataptr();
	packetbuf_set_datalen(sizeof( rerr_message));
	msg->errorcode = Error_Code;
	msg->hop_limit = MAX_HOP_LIMIT;
	rimeaddr_copy(&msg->unreachable,broken_dest_addr);
	rimeaddr_copy(&msg->destination,broken_source_addr);
	rimeaddr_copy(&msg->originator,&rimeaddr_node_addr);

	rt = route_lookup(&msg->destination);
	if(rt != NULL) {
	    unicast_send(&c->rrepconn, &rt->R_next_addr);
	}

}

