/**
 * \addtogroup rime
 * @{
 */
/**
 * \defgroup rimeroute Rime route table
 * @{
 *
 * The route module handles the route table in Rime.
 */

/*
 * Copyright (c) 2005, Swedish Institute of Computer Science.
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
 *         Header file for the Rime route table
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#ifndef __ROUTE_H__
#define __ROUTE_H__

#include "net/rime/rimeaddr.h"

//Pending entry tuple structure for the Pending Acknowledgement Set.
struct pending_entry {
	struct pending_entry* next;
	rimeaddr_t P_next_hop;
	rimeaddr_t P_originator;
	clock_time_t P_ack_timeout;
	uint16_t P_seq_num;
};

//Blacklist tuple entry structure for the Blacklisted Neighbor Set.
struct blacklist_tuple {
	struct blacklist_tuple* next;
	rimeaddr_t B_neighbor_address;
	clock_time_t B_valid_time;
};

//The distance structure consists of a tuple (route_cost, weak_links), and
//works together with its correspondent metrics.
struct dist_tuple {
	uint8_t route_cost;
	uint8_t weak_links:4;
	uint8_t padding:4;	//not used, initialized to 0;
};

//This structure redefines the routing tuple with another name.
//Just defined to maintain compatibility with mesh.c , uip-over-mesh.h and other libraries that call for routes.
//same as struct routing_tuple
struct route_entry {
	struct route_entry* next;
	//TODO: we need to figure out how to represent the type of an address
	rimeaddr_t R_dest_addr;
	rimeaddr_t R_next_addr;
	struct dist_tuple R_dist;
	uint16_t R_seq_num;
	clock_time_t R_valid_time;
	uint8_t R_metric:4;	//R_metric: type of routing metric. 0, by default, means using hop-count
	uint8_t padding:4;	//not used, initialized to 0;
};

void route_init(void);
struct routing_entry *route_add(const rimeaddr_t *dest,
		const rimeaddr_t *nexthop, struct dist_tuple *dist, uint16_t seqno);
struct route_entry *route_lookup(const rimeaddr_t *dest);
struct pending_entry *route_pending_list_lookup (const rimeaddr_t *from,
		const rimeaddr_t *orig, uint16_t seq_num);
struct pending_entry *route_pending_add(const rimeaddr_t *nexthop,
		const rimeaddr_t *dest, uint16_t RREQ_ID, clock_time_t timeout);
struct blacklist_tuple *route_blacklist_lookup(const rimeaddr_t *addr );
struct blacklist_tuple *route_blacklist_add(const rimeaddr_t *neighbor, clock_time_t timeout );
void route_refresh(struct route_entry *e);
void route_decay(struct route_entry *e);
void route_remove(struct route_entry *e);
void pending_remove(struct pending_entry *e);
void blacklist_remove(struct blacklist_tuple *e);

void route_flush_all(void);
void route_set_lifetime(int seconds);
int route_num(void);
struct route_entry *route_get(int num);

#endif /* __ROUTE_H__ */
/** @} */
/** @} */
