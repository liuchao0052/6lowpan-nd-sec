/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
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
 */

/**
 * \addtogroup uip6
 * @{
 */

/**
 * \file
 *    IPv6 address registration list manuiplation
 * \author Mohamed Seliem <mseliem11@gmail.com>
 * \author Khaled Elsayed <khaled@ieee.org> 
 * \author Ahmed Khattab <ahmed.khattab@gmail.com> 
 *
 */
//#if UIP_CONF_IPV6_LOWPAN_ND
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "lib/random.h"
#include "net/ipv6/uip-6lowpan-nd6.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include "net/ip/uip-packetqueue.h"
#include "net/ipv6/uip-ds6-reg.h"

//#define DEBUG DEBUG_NONE
#define DEBUG DEBUG_PRINT
#define PRINTADDR(addr) PRINTF("%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7])
#include "net/ip/uip-debug.h"
uip_ds6_reg_t uip_ds6_reg_list[UIP_DS6_REG_LIST_SIZE];      /**< Registrations list */
//uip_ip6addr_t ipaddr_reg_list[UIP_DS6_REG_LIST_SIZE];
//uip_lladdr_t eui64_reg_list[UIP_DS6_REG_LIST_SIZE];

static uip_ds6_reg_t *locreg;
static uip_ds6_defrt_t *min_defrt; /* default router with minimum lifetime */ 
static unsigned long min_lifetime; /* minimum lifetime */ 

/*---------------------------------------------------------------------------*/
//uip_ds6_reg_t*
//uip_ds6_reg_add(uip_ds6_addr_t* addr, uip_ds6_defrt_t* defrt, uint8_t state, uint16_t lifetime, uip_lladdr_t* mac) {
//
//	uip_ds6_reg_t* candidate = NULL;
//
//	for (locreg = uip_ds6_reg_list;
//			locreg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; locreg++) {
//		if (!locreg->isused) {
//			candidate = locreg;
//			break;
//		} else if (locreg->state == REG_GARBAGE_COLLECTIBLE) {
//			candidate = locreg;
//		}
//	}
//
//	/* If there was an entry not in use, use it; otherwise overwrite
//	 * our canditate entry in Garbage-collectible state*/
//	if (candidate != NULL) {
//		candidate->isused = 1;
//		candidate->addr = addr;
//		candidate->defrt = defrt;
//		candidate->state = state;
//		timer_set(&candidate->registration_timer, 0);
//		candidate->reg_count = 0;
//		candidate->mac = mac;
//		if(candidate->state == REG_GARBAGE_COLLECTIBLE) {
//			stimer_set(&candidate->reg_lifetime, UIP_DS6_GARBAGE_COLLECTIBLE_REG_LIFETIME);
//		} else if (candidate->state == REG_TENTATIVE) {
//			stimer_set(&candidate->reg_lifetime, UIP_DS6_TENTATIVE_REG_LIFETIME);
//		} else {
//			stimer_set(&candidate->reg_lifetime, lifetime);
//		}
//		defrt->registrations++;
//
////                PRINTF("# Register ip: ");
////                PRINT6ADDR(candidate->addr);
////                PRINTF(", State: %d, Register Count: %d, Register lifetime: %d, Registration timer: %d \n",
////                		candidate->state, candidate->reg_count, uip_htons(lifetime), candidate->registration_timer.interval);
//
//		return candidate;
//	}
//	return NULL;
//}

uip_ds6_reg_t*
//uip_ds6_reg_add(uip_ip6addr_t addr, uip_ds6_defrt_t* defrt, uint8_t state, uint16_t lifetime, uip_lladdr_t mac) {
uip_ds6_reg_add(uip_ip6addr_t addr, uip_ds6_defrt_t* defrt, uint8_t state, uint16_t lifetime, uip_802154_longaddr mac) {

	uip_ds6_reg_t* candidate = NULL;

	for (locreg = uip_ds6_reg_list;
			locreg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; locreg++) {
		if (!locreg->isused) {
			candidate = locreg;
			break;
		} else if (locreg->state == REG_GARBAGE_COLLECTIBLE) {
			candidate = locreg;
		}
	}

	/* If there was an entry not in use, use it; otherwise overwrite
	 * our canditate entry in Garbage-collectible state*/
	if (candidate != NULL) {
		candidate->isused = 1;
//		candidate->addr = addr;
		memcpy(&candidate->addr, &addr, sizeof(uip_ip6addr_t));
		candidate->defrt = defrt;
		candidate->state = state;
		timer_set(&candidate->registration_timer, 0);
		candidate->reg_count = 0;
//		candidate->mac = mac;
//		memcpy(&candidate->mac, &mac, sizeof(uip_lladdr_t));
		memcpy(&candidate->mac, &mac, sizeof(uip_802154_longaddr));
//		memcpy(&candidate->mac, mac, 8);
		if(candidate->state == REG_GARBAGE_COLLECTIBLE) {
			stimer_set(&candidate->reg_lifetime, UIP_DS6_GARBAGE_COLLECTIBLE_REG_LIFETIME);
		} else if (candidate->state == REG_TENTATIVE) {
			stimer_set(&candidate->reg_lifetime, UIP_DS6_TENTATIVE_REG_LIFETIME);
		} else {
			stimer_set(&candidate->reg_lifetime, lifetime);
		}
		defrt->registrations++;

//		PRINTF("# Register ip: ");
//		PRINT6ADDR(candidate->addr);
//		PRINTF(", State: %d, Register Count: %d, Register lifetime: %d, Registration timer: %d \n",
//				candidate->state, candidate->reg_count, uip_htons(lifetime), candidate->registration_timer.interval);

		return candidate;
	}
	return NULL;
}
/*---------------------------------------------------------------------------*/
void
uip_ds6_reg_rm(uip_ds6_reg_t* reg){

        reg->defrt->registrations--;
        reg->isused = 0;

}

/*---------------------------------------------------------------------------*/
//uip_ds6_reg_t*
//uip_ds6_reg_lookup(uip_ds6_addr_t* addr, uip_ds6_defrt_t* defrt){
//
//        uip_ds6_reg_t* reg;
//
//        for (reg = uip_ds6_reg_list;
//                        reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
//                if ((reg->isused) && (reg->addr == addr) && (reg->defrt == defrt)) {
////                		PRINTF("@@query ");PRINT6ADDR(reg->addr);
//                        return reg;
//                }
//        }
//        return NULL;
//}
uip_ds6_reg_t*
uip_ds6_reg_lookup(uip_ipaddr_t addr, uip_ds6_defrt_t* defrt){

        uip_ds6_reg_t* reg;

        for (reg = uip_ds6_reg_list;
                        reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
                if ((reg->isused) && !memcmp(&reg->addr, &addr, sizeof(uip_ipaddr_t)) && (reg->defrt == defrt)) {
//                		PRINTF("@@query ");PRINT6ADDR(&reg->addr);
                        return reg;
                }
        }
        return NULL;
}
/*---------------------------------------------------------------------------*/
void
uip_ds6_reg_cleanup_defrt(uip_ds6_defrt_t* defrt) {

        uip_ds6_reg_t* reg;

        for (reg = uip_ds6_reg_list;
                        reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
                if ((reg->isused) && (reg->defrt == defrt)) {
                        uip_ds6_reg_rm(reg);
                }
        }
}

/*---------------------------------------------------------------------------*/
//void
//uip_ds6_reg_cleanup_addr(uip_ds6_addr_t* addr) {
//
//        uip_ds6_reg_t* reg;
//
//        for (reg = uip_ds6_reg_list;
//                        reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
//                if ((reg->isused) && (reg->addr == addr)) {
//                        if (reg->state != REG_REGISTERED) {
//                                uip_ds6_reg_rm(reg);
//                        } else {
//                                /* Mark it as TO_BE_UNREGISTERED */
//                                reg->state = REG_TO_BE_UNREGISTERED;
//                        }
//                }
//        }
//}

void
uip_ds6_reg_cleanup_addr(uip_ds6_addr_t* addr) {

        uip_ds6_reg_t* reg;

        for (reg = uip_ds6_reg_list;
                        reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
                if ((reg->isused) && !memcmp(&reg->addr, &addr, sizeof(uip_ipaddr_t))) {
                        if (reg->state != REG_REGISTERED) {
                                uip_ds6_reg_rm(reg);
                        } else {
                                /* Mark it as TO_BE_UNREGISTERED */
                                reg->state = REG_TO_BE_UNREGISTERED;
                        }
                }
        }
}

/*---------------------------------------------------------------------------*/

uint8_t uip_ds6_get_registrations(uip_ds6_defrt_t *defrt) {

//        if ((defrt == NULL) || (!defrt->isused))
        if ((defrt == NULL)){
                return 0;
        }

        return defrt->registrations;
}
/*---------------------------------------------------------------------------*/
//void
//uip_ds6_print_reg_list() {
//	uip_ds6_reg_t* reg;
//	uint8_t is_null = 1;
//	PRINTF("# Register list: \n");
//	for (reg = uip_ds6_reg_list;
//					reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
//		if(reg->addr != NULL){
//			PRINTF("[ MAC:");
//			PRINTADDR(reg->mac);
//			PRINTF(", Register ip: ");
//			PRINT6ADDR(reg->addr);
//			PRINTF(", Is used:%d, State: %d, Register Count: %d, Register lifetime: %u, Registration timer: %d, ",
//					reg->isused, reg->state, reg->reg_count, uip_htons(reg->reg_lifetime.interval), reg->registration_timer.interval);
//			PRINTF("Default router:(ip:");
//			PRINT6ADDR(reg->defrt);
//			PRINTF(", lifetime: %u) ]\n", reg->defrt->lifetime.interval);
//			is_null  = 0;
//		}
//	}
//	if(is_null){
//		PRINTF("[ NULL ]\n");
//	}
//}

void
uip_ds6_print_reg_list() {
	uip_ds6_reg_t* reg;
	uint8_t is_null = 1;
	PRINTF("# Register list: \n");
	for (reg = uip_ds6_reg_list;
					reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
		if(!uip_is_addr_unspecified(&reg->addr)){
			PRINTF("[ MAC:");
			PRINTADDR(&reg->mac);
			PRINTF(", Register ip: ");
			PRINT6ADDR(&reg->addr); //stimer_elapsed
			PRINTF(", Is used: %d, State: %d, Register Count: %d, Register lifetime: %u, Registration timer: %d, ",
					reg->isused, reg->state, reg->reg_count, /*uip_htons(*/stimer_remaining(&reg->reg_lifetime), reg->registration_timer.interval);
			PRINTF("Default router:(ip:");
			PRINT6ADDR(reg->defrt);
			PRINTF(", lifetime: %u) ]\n", reg->defrt->lifetime.interval);
			is_null  = 0;
		}
	}
	if(is_null){
		PRINTF("[ NULL ]\n");
	}
}

void
uip_ds6_reg_update(uip_ds6_addr_t* addr, uip_ds6_defrt_t* defrt, uint16_t lifetime){}

/*---------------------------------------------------------------------------*/
//uip_ds6_defrt_t*
//uip_ds6_defrt_choose_min_reg(uip_ds6_addr_t* addr)
//{
//        uint8_t min = 0;
//        uip_ds6_defrt_t* min_defrt = NULL;
//        uip_ds6_defrt_t* locdefrt;
//  for(locdefrt = uip_ds6_defrt_list;
//      locdefrt < uip_ds6_defrt_list + UIP_DS6_DEFRT_NB; locdefrt++) {
//    if (locdefrt->isused) {
//        if (NULL == uip_ds6_reg_lookup(addr, locdefrt)) {
//              if ((min_defrt == NULL) ||
//                        ((min_defrt != NULL) && (uip_ds6_get_registrations(locdefrt) < min))) {
//                min_defrt = locdefrt;
//                min = uip_ds6_get_registrations(locdefrt);
//                if (min == 0) {
//                        /* We are not going to find a better candidate */
//                        return min_defrt;
//                }
//              }
//        }
//    }
//  }
//  return min_defrt;
//}
//#endif

/** @}*/
