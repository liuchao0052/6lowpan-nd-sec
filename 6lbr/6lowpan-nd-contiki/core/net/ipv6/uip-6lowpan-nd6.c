/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
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
 * \addtogroup uip6
 * @{
 */

/**
 * \file
 *    Source file for the 6LoWPAN-ND secure addressing implementation (Sec scheme)
 * \author Chao Liu <Lcahnu@ahnu.edu.cn>
 * \author Fulong Chen <Long005@ahnu.edu.cn>
 */
#include <string.h>

#include <stdlib.h>
#include "net/ipv6/opt8/api.h"
#include "net/ipv6/opt8/crypto_hash.h"

#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/uip-6lowpan-nd6.h"
#include "net/ipv6/uip-ds6-reg.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-nameserver.h"
#include "lib/random.h"
#include "dev/ds2411/ds2411.h"
/*------------------------------------------------------------------*/
#define DEBUG 1
#include "net/ip/uip-debug.h"
#define PRINTADDR(addr) PRINTF("%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7])
#if UIP_LOGGING
#include <stdio.h>
void uip_log(char *msg);

#define UIP_LOG(m) uip_log(m)
#else
#define UIP_LOG(m)
#endif /* UIP_LOGGING == 1 */

void __print__(unsigned char c, unsigned char* x, unsigned long long xlen) {
  unsigned long long i;
  printf("%c[%d]=", c, (int)xlen);
  for (i = 0; i < xlen; ++i) printf("%02x", x[i]);
  printf("\n");
}

//#if UIP_CONF_IPV6_LOWPAN_ND
/*------------------------------------------------------------------*/
/** @{ */
/** \name Pointers to the header structures.
 *  All pointers except UIP_IP_BUF depend on uip_ext_len, which at
 *  packet reception, is the total length of the extension headers.
 *
 *  The pointer to ND6 options header also depends on nd6_opt_offset,
 *  which we set in each function.
 *
 *  Care should be taken when manipulating these buffers about the
 *  value of these length variables
 */
#define UIP_IP_BUF                ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])  /**< Pointer to IP header */
#define UIP_ICMP_BUF            ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])  /**< Pointer to ICMP header*/
/**@{  Pointers to messages just after icmp header */
#define UIP_ND6_RS_BUF            ((uip_nd6_rs *)&uip_buf[uip_l2_l3_icmp_hdr_len])
#define UIP_ND6_RA_BUF            ((uip_nd6_ra *)&uip_buf[uip_l2_l3_icmp_hdr_len])
#define UIP_ND6_NS_BUF            ((uip_nd6_ns *)&uip_buf[uip_l2_l3_icmp_hdr_len])
#define UIP_ND6_NA_BUF            ((uip_nd6_na *)&uip_buf[uip_l2_l3_icmp_hdr_len])
/** @} */
/** Pointer to ND option */
#define UIP_ND6_OPT_HDR_BUF  ((uip_nd6_opt_hdr *)&uip_buf[uip_l2_l3_icmp_hdr_len + nd6_opt_offset])
#define UIP_ND6_OPT_PREFIX_BUF ((uip_nd6_opt_prefix_info *)&uip_buf[uip_l2_l3_icmp_hdr_len + nd6_opt_offset])
#define UIP_ND6_OPT_MTU_BUF ((uip_nd6_opt_mtu *)&uip_buf[uip_l2_l3_icmp_hdr_len + nd6_opt_offset])
#define UIP_ND6_OPT_RDNSS_BUF ((uip_nd6_opt_dns *)&uip_buf[uip_l2_l3_icmp_hdr_len + nd6_opt_offset])
#define UIP_ND6_OPT_ARO_BUF  ((uip_nd6_opt_aro *)&uip_buf[uip_l2_l3_icmp_hdr_len + nd6_opt_offset])
#define UIP_ND6_OPT_6CO_BUF  ((uip_nd6_opt_6co *)&uip_buf[uip_l2_l3_icmp_hdr_len + nd6_opt_offset])
#define UIP_ND6_OPT_ABRO_BUF  ((uip_nd6_opt_abro *)&uip_buf[uip_l2_l3_icmp_hdr_len + nd6_opt_offset])
#define UIP_ND6_OPT_NONCE_BUF  ((uip_nd6_opt_nonce *)&uip_buf[uip_l2_l3_icmp_hdr_len + nd6_opt_offset])
#define UIP_ND6_OPT_AUTH_BUF  ((uip_nd6_opt_auth *)&uip_buf[uip_l2_l3_icmp_hdr_len + nd6_opt_offset])
/** @} */

#if UIP_ND6_SEND_NA || UIP_ND6_SEND_RA || !UIP_CONF_ROUTER
static uint8_t nd6_opt_offset;                     /** Offset from the end of the icmpv6 header to the option in uip_buf*/
static uint8_t *nd6_opt_llao;   /**  Pointer to llao option in uip_buf */
static uip_nd6_opt_aro *nd6_opt_aro;   /**  Pointer to aro option in uip_buf */
static uip_ds6_nbr_t *nbr; /**  Pointer to a nbr cache entry*/
static uip_ds6_defrt_t *defrt; /**  Pointer to a router list entry */
static uip_ds6_addr_t *addr; /**  Pointer to an interface address */
static uip_ds6_reg_t *reg; /**  Pointer to an address registration list entry */
#endif /* UIP_ND6_SEND_NA || UIP_ND6_SEND_RA || !UIP_CONF_ROUTER */

#if !UIP_CONF_ROUTER            // TBD see if we move it to ra_input
static uip_nd6_opt_prefix_info *nd6_opt_prefix_info; /**  Pointer to prefix information option in uip_buf */
static uip_ipaddr_t ipaddr;
#endif
#if (!UIP_CONF_ROUTER || UIP_ND6_SEND_RA)
static uip_ds6_prefix_t *prefix; /**  Pointer to a prefix list entry */
#if UIP_CONF_ND6_RA_6CO
static uip_ds6_addr_context_t *context; /**  Pointer to a context list entry */
static uip_nd6_opt_6co *nd6_opt_6co; /**  Pointer to a 6lowpan context option in uip_buf */
#endif /* UIP_CONF_ND6_RA_6CO */
#endif /* (!UIP_CONF_ROUTER || UIP_ND6_SEND_RA) */

#if UIP_ND6_SEND_NA || UIP_ND6_SEND_RA || !UIP_CONF_ROUTER
/*------------------------------------------------------------------*/
/* Copy link-layer address from LLAO option to a word-aligned uip_lladdr_t */
static int
extract_lladdr_from_llao_aligned(uip_lladdr_t *dest) {
  if(dest != NULL && nd6_opt_llao != NULL) {
    memcpy(dest, &nd6_opt_llao[UIP_ND6_OPT_DATA_OFFSET], UIP_LLADDR_LEN);
    return 1;
  }
  return 0;
}
#endif /* UIP_ND6_SEND_NA || UIP_ND6_SEND_RA || !UIP_CONF_ROUTER */
/*------------------------------------------------------------------*/

#if UIP_ND6_NS_NONCE
static uint8_t nonce = 0;
static uint8_t nonce_arr[6]={0};
static uip_nd6_opt_nonce *nd6_opt_nonce;
#endif

#if UIP_ND6_NS_AUTH
static uip_nd6_opt_auth *nd6_opt_auth;
#endif

int compareArr(uint8_t arr1[], uint8_t arr2[], uint8_t size) {
	uint8_t i;
    for (i = 0; i < size; i++) {
        if (arr1[i] < arr2[i]) {
            return -1;
        } else if (arr1[i] > arr2[i]) {
            return 1;
        }
    }
    return 0;
}

/* create a llao */
static void
create_llao(uint8_t *llao, uint8_t type) {
  llao[UIP_ND6_OPT_TYPE_OFFSET] = type;
  llao[UIP_ND6_OPT_LEN_OFFSET] = UIP_ND6_OPT_LLAO_LEN >> 3;
  memcpy(&llao[UIP_ND6_OPT_DATA_OFFSET], &uip_lladdr, UIP_LLADDR_LEN);
  /* padding on some */
  memset(&llao[UIP_ND6_OPT_DATA_OFFSET + UIP_LLADDR_LEN], 0,
         UIP_ND6_OPT_LLAO_LEN - 2 - UIP_LLADDR_LEN);
}

/*------------------------------------------------------------------*/
static void
create_aro(uip_nd6_opt_aro * aro, uint16_t lifetime, uint8_t status, uip_802154_longaddr *eui64) {
  	aro->type = (uint8_t)UIP_ND6_OPT_ARO;
  	aro->len = (uint8_t)UIP_ND6_OPT_ARO_LEN;
  	aro->reserved1 = (uint8_t)0;
  	aro->reserved2 = (uint16_t)0;
  	aro->status = status; /* Status: must be set to 0 in NS */
   	aro->lifetime = uip_htons(lifetime);
	memcpy(&(((uip_nd6_opt_aro*)aro)->eui64), eui64, 8);
}
/*------------------------------------------------------------------*/

/*------------------------------------------------------------------*/
static void
create_abro(uip_nd6_opt_abro * abro, uint16_t lifetime, uint16_t v_low, uint16_t v_high) {
  	abro->type = (uint8_t)UIP_ND6_OPT_ABRO;
  	abro->len = (uint8_t) UIP_ND6_OPT_ABRO_LEN;
  	abro->v_low = v_low;
  	abro->v_high = v_high;
   	abro->valid_lifetime = uip_htons(lifetime);
  	memcpy(&(((uip_nd6_opt_abro*)abro)->ipaddr), &global_fipaddr, 16);
}
/*------------------------------------------------------------------*/
static void
create_nonce(uip_nd6_opt_nonce * nonce, uint8_t  counter[]) {
	nonce->type = (uint8_t)UIP_ND6_OPT_NONCE;
	nonce->len = (uint8_t) UIP_ND6_OPT_NONCE_LEN;
  	memcpy(&(((uip_nd6_opt_nonce*)nonce)->counter), counter, 6);
}
/*------------------------------------------------------------------*/

#if UIP_ND6_SEND_NA
static void
ns_input(void)
{
	uint8_t flags;
	uint8_t reg_status;
	uip_ip6addr_t srcipaddr;
	uip_802154_longaddr eui64;
	uip_ds6_reg_t *reg_query;
	uint8_t counter[6];

	PRINTF("Received NS from ");
	PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
	PRINTF(" to ");
	PRINT6ADDR(&UIP_IP_BUF->destipaddr);
	PRINTF(" with target address ");
	PRINT6ADDR((uip_ipaddr_t *) (&UIP_ND6_NS_BUF->tgtipaddr));
	PRINTF("\n");
	UIP_STAT(++uip_stat.nd6.recv);

#if UIP_CONF_IPV6_CHECKS
	if((UIP_IP_BUF->ttl != UIP_ND6_HOP_LIMIT) ||
		(uip_is_addr_mcast(&UIP_ND6_NS_BUF->tgtipaddr)) ||
		(UIP_ICMP_BUF->icode != 0)) {
		PRINTF("NS received is bad\n");
		goto discard;
	}
#endif /* UIP_CONF_IPV6_CHECKS */

  /*
   * Options processing
   */
	nd6_opt_llao = NULL;
	nd6_opt_aro = NULL;
	nd6_opt_nonce = NULL;
	nd6_opt_auth = NULL;
	nd6_opt_offset = UIP_ND6_NS_LEN;

  /* Verification of security registration related aspects  */
	while(uip_l3_icmp_hdr_len + nd6_opt_offset < uip_len) {
#if UIP_CONF_IPV6_CHECKS
		if(UIP_ND6_OPT_HDR_BUF->len == 0) {
			PRINTF("NS received is bad\n");
			goto discard;
		}
#endif /* UIP_CONF_IPV6_CHECKS */
		switch (UIP_ND6_OPT_HDR_BUF->type) {
		case UIP_ND6_OPT_ARO:
			PRINTF("Processing ARO option in NS\n");
			addr = uip_ds6_addr_lookup(&UIP_ND6_NS_BUF->tgtipaddr);
			if(addr != NULL){  //only handle ARO in the NS message which send to me
				nd6_opt_aro = UIP_ND6_OPT_ARO_BUF;
				/*step 1. Verify MAC address*/
				memcpy(&eui64, &nd6_opt_aro->eui64, sizeof(uip_802154_longaddr));
				if( (reg_query = uip_ds6_reg_lookup_mac(eui64)) == NULL){
					PRINTF("Unauthorized node MAC address in NS, discard ...\n");
					goto discard;
				}else{
					PRINTF("Authorized node MAC address in NS, continue verification ...\n");
				}
			}
			break;
		}
		nd6_opt_offset += (UIP_ND6_OPT_HDR_BUF->len << 3);
	}
	nd6_opt_offset = UIP_ND6_NS_LEN;

	while(uip_l3_icmp_hdr_len + nd6_opt_offset < uip_len) {
		switch (UIP_ND6_OPT_HDR_BUF->type) {
		case UIP_ND6_OPT_NONCE:
			PRINTF("Processing NONCE option in NS\n");
			nd6_opt_nonce = UIP_ND6_OPT_NONCE_BUF;
			/*step 2. Verify Nonce option*/
			if(compareArr(reg_query->counter, nd6_opt_nonce->counter, 6) < 0)
				PRINTF("Nonce is valid in NS, continue verification ...\n");
			else{
				PRINTF("Nonce is invalid in NS, discard ...\n");
				goto discard;
			}
			break;

		case UIP_ND6_OPT_AUTH:
			PRINTF("Processing AUTH option in NS\n");
			nd6_opt_auth = UIP_ND6_OPT_AUTH_BUF;
			/*step 3. Verify Auth option*/
			uip_ip6addr_t gp16;
			memcpy(&gp16, &UIP_IP_BUF->srcipaddr, sizeof(uip_ip6addr_t));

			uint16_t LT[1]={nd6_opt_aro->lifetime};

			lbr_info lbrinfo;

			for(prefix = uip_ds6_prefix_list;
				prefix < uip_ds6_prefix_list + UIP_DS6_PREFIX_NB; prefix++) {
			  if((prefix->isused) && (prefix->advertise)) {
				  lbrinfo.pio.type = UIP_ND6_OPT_PREFIX_INFO;
				  lbrinfo.pio.len = UIP_ND6_OPT_PREFIX_INFO_LEN / 8;
				  lbrinfo.pio.preflen = prefix->length;
				  lbrinfo.pio.flagsreserved1 = prefix->l_a_reserved;
				  lbrinfo.pio.validlt = uip_htonl(prefix->vlifetime);
				  lbrinfo.pio.preferredlt = uip_htonl(prefix->plifetime);
				  lbrinfo.pio.reserved2 = 0;
				uip_ipaddr_copy(&(lbrinfo.pio.prefix), &(prefix->ipaddr));
			  }
			}

			for(context = uip_ds6_context_list;
				context < uip_ds6_context_list + UIP_DS6_6CO_NB; context++) {
			  if(context->state==2) { //IN_USE_COMPRESS
				  lbrinfo.co.type = UIP_ND6_OPT_6CO;
				  lbrinfo.co.len = UIP_ND6_OPT_6CO_LEN ;
				  lbrinfo.co.context_len = context->length;
				  lbrinfo.co.res1_c_cid = 16 + context->context_id; // Flag C is set to 1
				  lbrinfo.co.valid_lifetime = uip_htons(context->vlifetime);
				  lbrinfo.co.reserved = 0;
				uip_ipaddr_copy(&(lbrinfo.co.prefix), &(context->prefix));
			  }
			}

			lbrinfo.abro.type = (uint8_t)UIP_ND6_OPT_ABRO;
			lbrinfo.abro.len = (uint8_t) UIP_ND6_OPT_ABRO_LEN;
			lbrinfo.abro.v_low = 0xABCD; /*v_low*/
			lbrinfo.abro.v_high = 0x1234; /*v_high*/
			lbrinfo.abro.valid_lifetime = 0xFFFF; /*uip_htons(lifetime)*/
			memcpy(&(lbrinfo.abro.ipaddr), &global_fipaddr, 16);

			int len = sizeof(gp16) + sizeof(eui64) + sizeof(LT) + sizeof(lbrinfo) +
					sizeof(nd6_opt_nonce->counter) + sizeof(reg_query->key);

			uint8_t m[128];

			memcpy(m, &gp16, sizeof(gp16));
			memcpy(m + sizeof(gp16), &eui64, sizeof(eui64));
			memcpy(m + sizeof(gp16) + sizeof(eui64), LT, sizeof(LT));
			memcpy(m + sizeof(gp16) + sizeof(eui64) + sizeof(LT), &lbrinfo, sizeof(lbrinfo));
			memcpy(m + sizeof(gp16) + sizeof(eui64) + sizeof(LT) + sizeof(lbrinfo),
					nd6_opt_nonce->counter, sizeof(nd6_opt_nonce->counter));
			memcpy(m + sizeof(gp16) + sizeof(eui64) + sizeof(LT) + sizeof(lbrinfo) + sizeof(nonce_arr),
					reg_query->key, sizeof(reg_query->key));

			unsigned char h[32];
			unsigned long long mlen = len;
			int result = 0;

			__print__('m', m, mlen);
			printf(" -> ");
			result |= crypto_hash(h, m, mlen);
			__print__('h', h, CRYPTO_BYTES);

			int k;
			PRINTF("# received auth:");
			for(k=0;k<32;k++)
				PRINTF("%02x",nd6_opt_auth->auth[k]);
			PRINTF("\n");

			PRINTF("# calculate auth:");
			for(k=0;k<32;k++)
				PRINTF("%02x",h[k]);
			PRINTF("\n");


			if(!memcmp(nd6_opt_auth->auth, h, sizeof(h))){
				PRINTF("Authentication passed, execute DAD next ...\n");
			}
			else{
				PRINTF("Authentication failed, discard ...\n");
				goto discard;
			}
		break;

	case UIP_ND6_OPT_SLLAO:
		PRINTF("Processing SLLAO option in NS\n");
		nd6_opt_llao = &uip_buf[uip_l2_l3_icmp_hdr_len + nd6_opt_offset];
#if UIP_CONF_IPV6_CHECKS
      /* There must be NO option in a DAD NS */
		if(uip_is_addr_unspecified(&UIP_IP_BUF->srcipaddr)) {
			PRINTF("NS received is bad\n");
			goto discard;
		}
#endif
		uip_lladdr_t lladdr_aligned;
		extract_lladdr_from_llao_aligned(&lladdr_aligned);
		nbr = uip_ds6_nbr_lookup(&UIP_IP_BUF->srcipaddr);
#if UIP_CONF_ROUTER
		if(nbr == NULL) {
		  uip_ds6_nbr_add(&UIP_IP_BUF->srcipaddr, &lladdr_aligned,
			  0, NBR_STALE, NBR_TABLE_REASON_IPV6_ND, NULL);
		} else {
			const uip_lladdr_t *lladdr = uip_ds6_nbr_get_ll(nbr);
			if(lladdr == NULL) {
				goto discard;
			}
			if(memcmp(&nd6_opt_llao[UIP_ND6_OPT_DATA_OFFSET],lladdr, UIP_LLADDR_LEN) != 0) {
				if(nbr_table_update_lladdr((const linkaddr_t *)lladdr,
						(const linkaddr_t *)&lladdr_aligned, 1) == 0){
				  /* failed to update the lladdr */
				  goto discard;
				}
				nbr->state = NBR_STALE;
			} else {
				if(nbr->state == NBR_INCOMPLETE) {
				  nbr->state = NBR_STALE;
				}
			}
		}
#endif
      break;
	/***********************************************************************
	* A router handles NS messages as specified in [RFC4861], with added  *
	* logic described in RfC 6775 section 6.5. for handling the ARO.      *
	***********************************************************************/
    case UIP_ND6_OPT_ARO:
    	PRINTF("Processing ARO option in NS\n");
    	addr = uip_ds6_addr_lookup(&UIP_ND6_NS_BUF->tgtipaddr);
    	if(addr != NULL){  //only handle ARO in the NS message which send to me
			nd6_opt_aro = UIP_ND6_OPT_ARO_BUF;
#if UIP_CONF_IPV6_CHECKS
		if((nd6_opt_aro->len != UIP_ND6_OPT_ARO_LEN)
		  ||(nd6_opt_aro->status != 0)||(nd6_opt_llao == NULL)) {
			/* If the Length field is not two, or if the Status field is not zero,
			 * then the NS is silently ignored. */

			/* If the source address of the NS is the unspecified address, or if no
			 * SLLAO is included, then any included ARO is ignored, that is, the NS
			 * is processed as if it did not contain an ARO.ignore this option */
			nd6_opt_aro = NULL;
			PRINTF("ND ARO option is not supported in received NS\n");
		}else{
			PRINTF("ND ARO option is supported in received NS\n");
		}
#endif /* UIP_CONF_IPV6_CHECKS */

//	 /*******************************************************
//	  * Optimization process logic of the register list     *
//	  *******************************************************/
		  reg = uip_ds6_reg_lookup(UIP_IP_BUF->srcipaddr, NULL);
		  if(reg == NULL || !memcmp(&reg->mac, &eui64, UIP_802154_LONGADDR_LEN)) {
			  PRINTF("DAD passed, update registration entries.\n");
			  if (nd6_opt_aro->lifetime > 0) {
				  uip_ds6_reg_update(eui64,
						  UIP_IP_BUF->srcipaddr,
						  NULL,
						  REG_REGISTERED,
						  uip_ntohs(nd6_opt_aro->lifetime),
						  counter);
//				  uip_ds6_print_reg_list();
				  uip_ds6_print_reg_list_keyinfo();
			  }else{
				  uip_ds6_reg_update(eui64,
						  UIP_IP_BUF->srcipaddr,
						  NULL,
						  REG_TO_BE_UNREGISTERED,
						  0,
						  counter);
				  uip_ds6_print_reg_list_keyinfo();
			  }
			  reg_status = UIP_ND6_ARO_SUCCESS;
			  goto create_lowpan_na;
		 }
		 else if(reg != NULL && memcmp(&reg->mac, &eui64, UIP_802154_LONGADDR_LEN)){
			 reg_status = UIP_ND6_ARO_DUPLICATE_ADDRESS;
			 goto create_lowpan_na;
		 }
    	}
    	else{ // if(addr != NULL)
    		goto discard;
    	}
      break;

    default:
      PRINTF("ND option not supported in NS\n");
      break;
    }
    nd6_opt_offset += (UIP_ND6_OPT_HDR_BUF->len << 3);
  }

/**********************************************************
 * Create NA message as describe in RFC 6775              *
 **********************************************************/
create_lowpan_na:
    /*
     * As described in section 6.7.5 of RFC 6775, Address registration errors
     * are not sent back to the source address of the NS due to a possible risk of
     * L2 address collision.
     * Instead, the NA is sent to the link-local IPv6 address with the Interface ID
     * part derived from the EUI-64 field of the ARO as per [RFC4944].
     */
#if UIP_CONF_ROUTER
	/**in case of ARO included */
	if(reg_status == UIP_ND6_ARO_DUPLICATE_ADDRESS){
		uip_create_linklocal_prefix(&UIP_IP_BUF->destipaddr);
		uip_ds6_set_addr_iid(&UIP_IP_BUF->destipaddr, &uip_lladdr);
	}
	else if(reg_status == UIP_ND6_ARO_SUCCESS){
		uip_ipaddr_copy(&UIP_IP_BUF->destipaddr, &UIP_IP_BUF->srcipaddr);
	}
	else{
		/** \todo  check for full cache */
		uip_create_linklocal_prefix(&UIP_IP_BUF->destipaddr);
		uip_ds6_set_addr_iid(&UIP_IP_BUF->destipaddr, &uip_lladdr);
	}
#endif

	uip_ipaddr_copy(&UIP_IP_BUF->srcipaddr, &UIP_ND6_NS_BUF->tgtipaddr);
	flags = UIP_ND6_NA_FLAG_SOLICITED | UIP_ND6_NA_FLAG_OVERRIDE;
#if UIP_CONF_ROUTER
    flags = flags | UIP_ND6_NA_FLAG_ROUTER;
#endif

    uip_ext_len = 0;
    UIP_IP_BUF->vtc = 0x60;
    UIP_IP_BUF->tcflow = 0;
    UIP_IP_BUF->flow = 0;
    UIP_IP_BUF->len[0] = 0;       /* length will not be more than 255 */
#if UIP_CONF_ROUTER
    /**in case of ARO included */
    if(flags == (UIP_ND6_NA_FLAG_SOLICITED | UIP_ND6_NA_FLAG_OVERRIDE | UIP_ND6_NA_FLAG_ROUTER)){
    	UIP_IP_BUF->len[1] = UIP_ICMPH_LEN + UIP_ND6_NA_LEN + UIP_ND6_OPT_LLAO_LEN + (UIP_ND6_OPT_ARO_LEN<<3);
    	PRINTF("ARO supported in NA message \n");
    }
    else{
    	UIP_IP_BUF->len[1] = UIP_ICMPH_LEN + UIP_ND6_NA_LEN + UIP_ND6_OPT_LLAO_LEN;
    	PRINTF("ARO not supported in NA message \n");
    }
#else
    UIP_IP_BUF->len[1] = UIP_ICMPH_LEN + UIP_ND6_NA_LEN + UIP_ND6_OPT_LLAO_LEN;
#endif

	UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
	UIP_IP_BUF->ttl = UIP_ND6_HOP_LIMIT;

	UIP_ICMP_BUF->type = ICMP6_NA;
	UIP_ICMP_BUF->icode = 0;

	UIP_ND6_NA_BUF->flagsreserved = flags;
	memset(UIP_ND6_NA_BUF->reserved, 0, sizeof(uint8_t)*3);

	/**
	 * As described in RFC 4861:
	 * For solicited advertisements, the Target Address field in the Neighbor Solicitation message
	 * that prompted this advertisement. For an unsolicited advertisement, the address whose link-layer
	 * address has changed. The Target Address MUST NOT be a multicast address.
	 */
	memcpy(&UIP_ND6_NA_BUF->tgtipaddr, &UIP_ND6_NS_BUF->tgtipaddr, sizeof(uip_ipaddr_t));

	create_llao(&uip_buf[uip_l2_l3_icmp_hdr_len + UIP_ND6_NA_LEN], UIP_ND6_OPT_TLLAO);

#if UIP_CONF_ROUTER
	/**in case of ARO included */
	if(reg_status == UIP_ND6_ARO_DUPLICATE_ADDRESS){
		create_aro(&uip_buf[uip_l2_l3_icmp_hdr_len + UIP_ND6_NA_LEN + UIP_ND6_OPT_LLAO_LEN],
				uip_htons(nd6_opt_aro->lifetime), UIP_ND6_ARO_DUPLICATE_ADDRESS, &nd6_opt_aro->eui64);
	}
	else if(reg_status == UIP_ND6_ARO_SUCCESS){
		create_aro(&uip_buf[uip_l2_l3_icmp_hdr_len + UIP_ND6_NA_LEN + UIP_ND6_OPT_LLAO_LEN],
				uip_htons(nd6_opt_aro->lifetime), UIP_ND6_ARO_SUCCESS, &nd6_opt_aro->eui64);
	}
	else{
		/** \todo  check for full cache */
		create_aro(&uip_buf[uip_l2_l3_icmp_hdr_len + UIP_ND6_NA_LEN + UIP_ND6_OPT_LLAO_LEN],
				uip_htons(nd6_opt_aro->lifetime), UIP_ND6_ARO_NCE_FULL, &nd6_opt_aro->eui64);
	}
#endif

	UIP_ICMP_BUF->icmpchksum = 0;
	UIP_ICMP_BUF->icmpchksum = ~uip_icmp6chksum();

#if UIP_CONF_ROUTER
/**in case of ARO included */
      if(flags==(UIP_ND6_NA_FLAG_SOLICITED | UIP_ND6_NA_FLAG_OVERRIDE |UIP_ND6_NA_FLAG_ROUTER)){
        uip_len =
         UIP_IPH_LEN + UIP_ICMPH_LEN + UIP_ND6_NA_LEN + UIP_ND6_OPT_LLAO_LEN + (UIP_ND6_OPT_ARO_LEN <<3);
       PRINTF("ARO supported in NA message \n");
      }
      else{
        uip_len =
          UIP_IPH_LEN + UIP_ICMPH_LEN + UIP_ND6_NA_LEN + UIP_ND6_OPT_LLAO_LEN;
        PRINTF("ARO not supported in NA message \n");
      }
#else
      uip_len =
        UIP_IPH_LEN + UIP_ICMPH_LEN + UIP_ND6_NA_LEN + UIP_ND6_OPT_LLAO_LEN;
#endif
	UIP_STAT(++uip_stat.nd6.sent);
	PRINTF("Sending NA to ");
	PRINT6ADDR(&UIP_IP_BUF->destipaddr);
	PRINTF(" from ");
	PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
	PRINTF(" with target address ");
	PRINT6ADDR(&UIP_ND6_NA_BUF->tgtipaddr);
	PRINTF("\n");
	return;
/**********************************************************
 * Create NA message as describe in RFC 6775 End          *
 **********************************************************/

discard:
  uip_clear_buf();
  return;
}
#endif /* UIP_ND6_SEND_NA */

/*------------------------------------------------------------------*/
void
uip_nd6_lowpan_ns_output(uip_ipaddr_t * src, uip_ipaddr_t * dest, uip_ipaddr_t * tgt, uint8_t aro, uint16_t lifetime)
{
  uip_ext_len = 0;
  UIP_IP_BUF->vtc = 0x60;
  UIP_IP_BUF->tcflow = 0;
  UIP_IP_BUF->flow = 0;
  UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
  UIP_IP_BUF->ttl = UIP_ND6_HOP_LIMIT;

  if(dest == NULL) {
    uip_create_solicited_node(tgt, &UIP_IP_BUF->destipaddr);
  } else {
    uip_ipaddr_copy(&UIP_IP_BUF->destipaddr, dest);
  }
  UIP_ICMP_BUF->type = ICMP6_NS;
  UIP_ICMP_BUF->icode = 0;
  UIP_ND6_NS_BUF->reserved = 0;
  uip_ipaddr_copy((uip_ipaddr_t *) &UIP_ND6_NS_BUF->tgtipaddr, tgt);
  UIP_IP_BUF->len[0] = 0;       /* length will not be more than 255 */
  /*
   * check if we add a SLLAO option: for DAD, MUST NOT, for NUD, MAY
   * (here yes), for Address resolution , MUST
   */
  if(!(uip_ds6_is_my_addr(tgt))) {
    if(src != NULL) {
      uip_ipaddr_copy(&UIP_IP_BUF->srcipaddr, src);
    } else {
      uip_ds6_select_src(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr);
    }
    if (uip_is_addr_unspecified(&UIP_IP_BUF->srcipaddr)) {
      PRINTF("Dropping NS due to no suitable source address\n");
      uip_clear_buf();
      return;
    }
    if (aro) {
      UIP_IP_BUF->len[1] =
        UIP_ICMPH_LEN + UIP_ND6_NS_LEN + UIP_ND6_OPT_LLAO_LEN+ (UIP_ND6_OPT_ARO_LEN<<3);

      create_llao(&uip_buf[uip_l2_l3_icmp_hdr_len + UIP_ND6_NS_LEN], UIP_ND6_OPT_SLLAO);
	  create_aro((uip_nd6_opt_aro *)&uip_buf[uip_l2_l3_icmp_hdr_len + UIP_ND6_NS_LEN + UIP_ND6_OPT_LLAO_LEN],
	    lifetime, UIP_ND6_ARO_SUCCESS, ds2411_id);/*status field must be set to 0 in ns messages*/
      uip_len =
            UIP_IPH_LEN + UIP_ICMPH_LEN + UIP_ND6_NS_LEN + UIP_ND6_OPT_LLAO_LEN+ (UIP_ND6_OPT_ARO_LEN<<3);
      PRINTF("ARO supported in this message\n");
#if UIP_ND6_NS_NONCE
      UIP_IP_BUF->len[1] += (UIP_ND6_OPT_NONCE_LEN <<3);
      nonce_arr[5] = ++nonce;
      create_nonce((uip_nd6_opt_nonce *)&uip_buf[uip_l2_l3_icmp_hdr_len + UIP_ND6_NS_LEN + UIP_ND6_OPT_LLAO_LEN + (UIP_ND6_OPT_ARO_LEN<<3)],
    		  nonce_arr);
      uip_len +=  (UIP_ND6_OPT_NONCE_LEN <<3);
#endif

    }
    else {
      PRINTF("ARO is not supported in this message\n");
    UIP_IP_BUF->len[1] =
      UIP_ICMPH_LEN + UIP_ND6_NS_LEN + UIP_ND6_OPT_LLAO_LEN;
    create_llao(&uip_buf[uip_l2_l3_icmp_hdr_len + UIP_ND6_NS_LEN],
                UIP_ND6_OPT_SLLAO);
    uip_len =
      UIP_IPH_LEN + UIP_ICMPH_LEN + UIP_ND6_NS_LEN + UIP_ND6_OPT_LLAO_LEN;
    }
  } else {
    uip_create_unspecified(&UIP_IP_BUF->srcipaddr);
    UIP_IP_BUF->len[1] = UIP_ICMPH_LEN + UIP_ND6_NS_LEN;
    uip_len = UIP_IPH_LEN + UIP_ICMPH_LEN + UIP_ND6_NS_LEN;
  }

  UIP_ICMP_BUF->icmpchksum = 0;
  UIP_ICMP_BUF->icmpchksum = ~uip_icmp6chksum();

  UIP_STAT(++uip_stat.nd6.sent);
  PRINTF("Sending NS to ");
  PRINT6ADDR(&UIP_IP_BUF->destipaddr);
  PRINTF(" from ");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF(" with target address ");
//  PRINT6ADDR(tgt);
  PRINT6ADDR(&UIP_ND6_NS_BUF->tgtipaddr);
  PRINTF("\n");
  return;
}

/*------------------------------------------------------------------*/
/**
 * Neighbor Advertisement Processing
 *
 * we might have to send a pkt that had been buffered while address
 * resolution was performed (if we support buffering, see UIP_CONF_QUEUE_PKT)
 *
 * As per RFC 4861, on link layer that have addresses, TLLAO options MUST be
 * included when responding to multicast solicitations, SHOULD be included in
 * response to unicast (here we assume it is for now)
 *
 * NA can be received after sending NS for DAD, Address resolution or NUD. Can
 * be unsolicited as well.
 * It can trigger update of the state of the neighbor in the neighbor cache,
 * router in the router list.
 * If the NS was for DAD, it means DAD failed
 *
 */
#if UIP_ND6_SEND_NA
static void
na_input(void)
{
  uint8_t is_llchange;
  uint8_t is_router;
  uint8_t is_solicited;
  uint8_t is_override;
  uip_lladdr_t lladdr_aligned;

  PRINTF("Received NA from ");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF(" to ");
  PRINT6ADDR(&UIP_IP_BUF->destipaddr);
  PRINTF(" with target address ");
  PRINT6ADDR((uip_ipaddr_t *) (&UIP_ND6_NA_BUF->tgtipaddr));
  PRINTF("\n");
  UIP_STAT(++uip_stat.nd6.recv);

  /*
   * booleans. the three last one are not 0 or 1 but 0 or 0x80, 0x40, 0x20
   * but it works. Be careful though, do not use tests such as is_router == 1
   */
  is_llchange = 0;
  is_router = ((UIP_ND6_NA_BUF->flagsreserved & UIP_ND6_NA_FLAG_ROUTER));
  is_solicited = ((UIP_ND6_NA_BUF->flagsreserved & UIP_ND6_NA_FLAG_SOLICITED));
  is_override = ((UIP_ND6_NA_BUF->flagsreserved & UIP_ND6_NA_FLAG_OVERRIDE));

#if UIP_CONF_IPV6_CHECKS
  if((UIP_IP_BUF->ttl != UIP_ND6_HOP_LIMIT) ||
     (UIP_ICMP_BUF->icode != 0) ||
     (uip_is_addr_mcast(&UIP_ND6_NA_BUF->tgtipaddr)) ||
     (is_solicited && uip_is_addr_mcast(&UIP_IP_BUF->destipaddr))) {
    PRINTF("NA received is bad\n");
    goto discard;
  }
#endif /*UIP_CONF_IPV6_CHECKS */

#if !UIP_CONF_ROUTER
  /* We are not interested in NA messages not coming from a router */
  if (!is_router) {
    nbr = uip_ds6_nbr_lookup(&UIP_IP_BUF->srcipaddr);
    if (nbr != NULL) {
      uip_ds6_nbr_rm(nbr);
    }
    defrt = uip_ds6_defrt_lookup(&UIP_IP_BUF->srcipaddr);
    if (defrt != NULL) {
      uip_ds6_defrt_rm(defrt);
      /* Since we are deleting a default router, we must delete also all
       * registrations with that router. */
      uip_ds6_reg_cleanup_defrt(defrt);
      /* We will also need to start sending RS, as specified in RFC 6775
       * for a router that has become unreachable */
      uip_ds6_send_rs(NULL);
    }
    goto discard;
  }
#endif /* !UIP_CONF_ROUTER */

  /* Options processing: we handle TLLAO, (if RFC6775 is used) ARO,
   * and must ignore others */
  nd6_opt_offset = UIP_ND6_NA_LEN;
  nd6_opt_llao = NULL;
  nd6_opt_aro = NULL;

  while(uip_l3_icmp_hdr_len + nd6_opt_offset < uip_len) {

#if UIP_CONF_IPV6_CHECKS
    if(UIP_ND6_OPT_HDR_BUF->len == 0) {
      PRINTF("NA received is bad\n");
      goto discard;
    }
#endif /*UIP_CONF_IPV6_CHECKS */
    switch (UIP_ND6_OPT_HDR_BUF->type) {

    case UIP_ND6_OPT_TLLAO:
      nd6_opt_llao = (uint8_t *)UIP_ND6_OPT_HDR_BUF;
      break;

    case UIP_ND6_OPT_ARO:
      nd6_opt_aro = (uip_nd6_opt_aro *)UIP_ND6_OPT_HDR_BUF;

#if UIP_CONF_IPV6_CHECKS
	  /* Here, it is need to see whether the EUI in ARO option is mine,
	   * If not, the ARO option should be ignored.
	   * This means that other nodes may be registering the same address as me.*/
      if((nd6_opt_aro->len != UIP_ND6_OPT_ARO_LEN) ||
    		  (memcmp(&nd6_opt_aro->eui64, ds2411_id, UIP_802154_LONGADDR_LEN) != 0)) {
        /* ignore this option */
        PRINTF("ARO is bad\n");
        nd6_opt_aro = NULL;
      }
#endif /* UIP_CONF_IPV6_CHECKS */
      break;

    default:
      PRINTF("ND option not supported in NA\n");
      break;
    }
    nd6_opt_offset += (UIP_ND6_OPT_HDR_BUF->len << 3);
  }

	addr = uip_ds6_addr_lookup(&UIP_ND6_NA_BUF->tgtipaddr);
	/* Message processing, including TLLAO if any */
	if(addr != NULL) {
#if UIP_ND6_DEF_MAXDADNS > 0
		if(addr->state == ADDR_TENTATIVE) {
		  uip_ds6_dad_failed(addr);
		}
#endif /*UIP_ND6_DEF_MAXDADNS > 0 */
		PRINTF("NA received is bad\n");
		goto discard;
	}
	else {
		const uip_lladdr_t *lladdr;
		nbr = uip_ds6_nbr_lookup(&UIP_ND6_NA_BUF->tgtipaddr);
		if(nbr == NULL) {
		  goto discard;
		}
		lladdr = uip_ds6_nbr_get_ll(nbr);
		if(lladdr == NULL) {
		  goto discard;
		}
		if(nd6_opt_llao != NULL) {
		  is_llchange =
			memcmp(&nd6_opt_llao[UIP_ND6_OPT_DATA_OFFSET], lladdr,
				   UIP_LLADDR_LEN);
		}
		/*************************************************
		 * process logic of ARO option in the NA message *
		 *************************************************/
		if(nd6_opt_aro != NULL) {
#if UIP_CONF_ROUTER
//#if UIP_CONF_IPV6_LOWPAN_ND
			reg = uip_ds6_if.registration_in_progress;
//#endif
			if (reg != NULL) {
				if ((nd6_opt_aro->lifetime == 0) &&
						(reg->state == REG_TO_BE_UNREGISTERED)) {
					/** If the lifetime is 0,
					 * this means that the un-registration was successful;
					 * we can delete the registration entry safely */
					uip_ds6_reg_rm(reg); /* Remove entry */
				}
				else {
					addr = uip_ds6_addr_lookup(&UIP_IP_BUF->destipaddr);

					switch(nd6_opt_aro->status) {

					case UIP_ND6_ARO_SUCCESS:
                        /* Make sure this is actually the address we are registering */
//						if (reg->addr == addr) {
						if (!memcmp(&reg->addr, &addr->ipaddr, sizeof(uip_ipaddr_t))) {
							/* Clear the NS count */
							addr->state = ADDR_PREFERRED;
							reg->state = REG_REGISTERED;
							reg->reg_count = 0;
							stimer_set(&reg->reg_lifetime, uip_ntohs(nd6_opt_aro->lifetime) * 60);
//#if UIP_CONF_IPV6_LOWPAN_ND
							uip_ds6_if.registration_in_progress = NULL;
//#endif
						}
						break;

					case UIP_ND6_ARO_DUPLICATE_ADDRESS:
						/* Remove the address */
						uip_ds6_addr_rm(addr);
						/* Clear registration_in_progress so that other registrations can occur */
//#if UIP_CONF_IPV6_LOWPAN_ND
						uip_ds6_if.registration_in_progress = NULL;
//#endif
						/* If we have registered the address with any other router, we must void
						 * such registrations by sending a ns with ARO with 0 lifetime */
						uip_ds6_reg_cleanup_addr(addr);
						break;

					case UIP_ND6_ARO_NCE_FULL:
						/* Remove entry. uip_ds6_periodic will try with other def. router
						 * if possible */
						uip_ds6_reg_rm(reg);
//#if UIP_CONF_IPV6_LOWPAN_ND
						uip_ds6_if.registration_in_progress = NULL;
//#endif
						break;

					default:
						break;
					} //switch(nd6_opt_aro->status)
				}
			} // if (reg != NULL)
#else // #if UIP_CONF_ROUTER

			addr = uip_ds6_addr_lookup(&UIP_IP_BUF->destipaddr);

			switch(nd6_opt_aro->status) {

			case UIP_ND6_ARO_SUCCESS:
				addr->state = ADDR_PREFERRED;
				PRINT6ADDR(&addr->ipaddr);
				PRINTF(" address registered successfully.\n");
//				/* Make sure this is actually my address */
//				if (uip_ds6_is_my_addr(&UIP_ND6_NA_BUF->tgtipaddr)) {
//				   PRINTF("ARO supported in this message\n");
//				}
				break;

			case UIP_ND6_ARO_DUPLICATE_ADDRESS:
				addr->state = ADDR_DEPRECATED;
				PRINT6ADDR(&addr->ipaddr);
				PRINTF(" address registered failed (Duplicate address). \n");
				goto discard;
				break;

			case UIP_ND6_ARO_NCE_FULL:
				addr->state = ADDR_TENTATIVE;
				PRINT6ADDR(&addr->ipaddr);
				PRINTF(" address registered failed (RTR cache is full).\n");
				goto discard;
				break;

            default:
            	break;
			}
#endif /*UIP_CONF_ROUTER*/
		} // if(nd6_opt_aro != NULL)

		PRINTIfADDR(uip_ds6_if.addr_list);
		/*****************************************************
		 * process logic of ARO option in the NA message End *
		 *****************************************************/

		/* NBR can not be INCOMPLETE in 6lowpan-nd*/
		if(!is_override && is_llchange) {
			if(nbr->state == NBR_REACHABLE) {
				nbr->state = NBR_STALE;
			}
			goto discard;
		}
		else {
			/**
			 *  If this is an cache override, or same lladdr, or no llao -
			 *  do updates of nbr states.
			 */
			if(is_override || !is_llchange || nd6_opt_llao == NULL) {
				if(nd6_opt_llao != NULL && is_llchange) {
					if(!extract_lladdr_from_llao_aligned(&lladdr_aligned) ||
					   nbr_table_update_lladdr((const linkaddr_t *) lladdr, (const linkaddr_t *) &lladdr_aligned, 1) == 0) {
					  /* failed to update the lladdr */
					  goto discard;
					}
				}
				/****************
				 * NUD function *
				 ****************/
				if(is_solicited) {
					nbr->state = NBR_REACHABLE;
					/* reachable time is stored in ms */
					stimer_set(&(nbr->reachable), uip_ds6_if.reachable_time / 1000);
				}
			}
		}

		if(nbr->isrouter && !is_router) {
			defrt = uip_ds6_defrt_lookup(&UIP_IP_BUF->srcipaddr);
			if(defrt != NULL) {
				uip_ds6_defrt_rm(defrt);
			}
		}
		nbr->isrouter = is_router;

	} // if(addr != NULL)

#if UIP_CONF_IPV6_QUEUE_PKT
	/* The nbr is now reachable, check if we had buffered a pkt for it */
	/*if(nbr->queue_buf_len != 0) {
		uip_len = nbr->queue_buf_len;
		memcpy(UIP_IP_BUF, nbr->queue_buf, uip_len);
		nbr->queue_buf_len = 0;
		return;
	}*/
	if(uip_packetqueue_buflen(&nbr->packethandle) != 0) {
		uip_len = uip_packetqueue_buflen(&nbr->packethandle);
		memcpy(UIP_IP_BUF, uip_packetqueue_buf(&nbr->packethandle), uip_len);
		uip_packetqueue_free(&nbr->packethandle);
		return;
	}
#endif /*UIP_CONF_IPV6_QUEUE_PKT */

discard:
  uip_clear_buf();
  return;
}
#endif /* UIP_ND6_SEND_NA */


#if UIP_CONF_ROUTER
#if UIP_ND6_SEND_RA
/*---------------------------------------------------------------------------*/
static void
rs_input(void)
{
  PRINTF("Received RS from ");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF(" to ");
  PRINT6ADDR(&UIP_IP_BUF->destipaddr);
  PRINTF("\n");
  UIP_STAT(++uip_stat.nd6.recv);

#if UIP_CONF_IPV6_CHECKS
  /*
   * Check hop limit / icmp code
   * target address must not be multicast
   * if the NA is solicited, dest must not be multicast
   */
  if((UIP_IP_BUF->ttl != UIP_ND6_HOP_LIMIT) || (UIP_ICMP_BUF->icode != 0)) {
    PRINTF("RS received is bad\n");
    goto discard;
  }
#endif /*UIP_CONF_IPV6_CHECKS */

  /* Only valid option is Source Link-Layer Address option any thing
     else is discarded */
  nd6_opt_offset = UIP_ND6_RS_LEN;
  nd6_opt_llao = NULL;

  while(uip_l3_icmp_hdr_len + nd6_opt_offset < uip_len) {
#if UIP_CONF_IPV6_CHECKS
    if(UIP_ND6_OPT_HDR_BUF->len == 0) {
      PRINTF("RS received is bad\n");
      goto discard;
    }
#endif /*UIP_CONF_IPV6_CHECKS */
    switch (UIP_ND6_OPT_HDR_BUF->type) {
    case UIP_ND6_OPT_SLLAO:
      nd6_opt_llao = (uint8_t *)UIP_ND6_OPT_HDR_BUF;
      break;
    default:
      PRINTF("ND option not supported in RS\n");
      break;
    }
    nd6_opt_offset += (UIP_ND6_OPT_HDR_BUF->len << 3);
  }
  /* Options processing: only SLLAO */
  if(nd6_opt_llao != NULL) {
#if UIP_CONF_IPV6_CHECKS
    if(uip_is_addr_unspecified(&UIP_IP_BUF->srcipaddr)) {
      PRINTF("RS received is bad\n");
      goto discard;
    } else {
#endif /*UIP_CONF_IPV6_CHECKS */
      uip_lladdr_t lladdr_aligned;
      extract_lladdr_from_llao_aligned(&lladdr_aligned);
      if((nbr = uip_ds6_nbr_lookup(&UIP_IP_BUF->srcipaddr)) == NULL) {
        /* we need to add the neighbor */
        uip_ds6_nbr_add(&UIP_IP_BUF->srcipaddr, &lladdr_aligned,
                        0, NBR_STALE, NBR_TABLE_REASON_IPV6_ND, NULL);
      } else {
        /* If LL address changed, set neighbor state to stale */
        const uip_lladdr_t *lladdr = uip_ds6_nbr_get_ll(nbr);
        if(lladdr == NULL) {
          goto discard;
        }
        if(memcmp(&nd6_opt_llao[UIP_ND6_OPT_DATA_OFFSET],
            lladdr, UIP_LLADDR_LEN) != 0) {
          uip_ds6_nbr_t nbr_data = *nbr;
          uip_ds6_nbr_rm(nbr);
          nbr = uip_ds6_nbr_add(&UIP_IP_BUF->srcipaddr, &lladdr_aligned,
                                0, NBR_STALE, NBR_TABLE_REASON_IPV6_ND, NULL);
          nbr->reachable = nbr_data.reachable;
          nbr->sendns = nbr_data.sendns;
          nbr->nscount = nbr_data.nscount;
        }
        nbr->isrouter = 0;
      }
#if UIP_CONF_IPV6_CHECKS
    }
#endif /*UIP_CONF_IPV6_CHECKS */
  }

  /* Schedule a sollicited RA */
  uip_ds6_send_ra_sollicited(&UIP_IP_BUF->srcipaddr);

discard:
  uip_clear_buf();
  return;
}

/*---------------------------------------------------------------------------*/
void
uip_nd6_lowpan_ra_output(uip_ipaddr_t * dest)
{

  UIP_IP_BUF->vtc = 0x60;
  UIP_IP_BUF->tcflow = 0;
  UIP_IP_BUF->flow = 0;
  UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
  UIP_IP_BUF->ttl = UIP_ND6_HOP_LIMIT;

  if(dest == NULL) {
    uip_create_linklocal_allnodes_mcast(&UIP_IP_BUF->destipaddr);
  } else {
    /* For sollicited RA */
    uip_ipaddr_copy(&UIP_IP_BUF->destipaddr, dest);
  }
  uip_ds6_select_src(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr);

  UIP_ICMP_BUF->type = ICMP6_RA;
  UIP_ICMP_BUF->icode = 0;

  UIP_ND6_RA_BUF->cur_ttl = uip_ds6_if.cur_hop_limit;

  UIP_ND6_RA_BUF->flags_reserved =
    (UIP_ND6_M_FLAG << 7) | (UIP_ND6_O_FLAG << 6);

  UIP_ND6_RA_BUF->router_lifetime = uip_htons(UIP_ND6_ROUTER_LIFETIME);
  //UIP_ND6_RA_BUF->reachable_time = uip_htonl(uip_ds6_if.reachable_time);
  //UIP_ND6_RA_BUF->retrans_timer = uip_htonl(uip_ds6_if.retrans_timer);
  UIP_ND6_RA_BUF->reachable_time = 0;
  UIP_ND6_RA_BUF->retrans_timer = 0;

  uip_len = UIP_IPH_LEN + UIP_ICMPH_LEN + UIP_ND6_RA_LEN;
  nd6_opt_offset = UIP_ND6_RA_LEN;

  /* Prefix option */
  for(prefix = uip_ds6_prefix_list;
      prefix < uip_ds6_prefix_list + UIP_DS6_PREFIX_NB; prefix++) {
    if((prefix->isused) && (prefix->advertise)) {
      UIP_ND6_OPT_PREFIX_BUF->type = UIP_ND6_OPT_PREFIX_INFO;
      UIP_ND6_OPT_PREFIX_BUF->len = UIP_ND6_OPT_PREFIX_INFO_LEN / 8;
      UIP_ND6_OPT_PREFIX_BUF->preflen = prefix->length;
      UIP_ND6_OPT_PREFIX_BUF->flagsreserved1 = prefix->l_a_reserved;
      UIP_ND6_OPT_PREFIX_BUF->validlt = uip_htonl(prefix->vlifetime);
      UIP_ND6_OPT_PREFIX_BUF->preferredlt = uip_htonl(prefix->plifetime);
      UIP_ND6_OPT_PREFIX_BUF->reserved2 = 0;
      uip_ipaddr_copy(&(UIP_ND6_OPT_PREFIX_BUF->prefix), &(prefix->ipaddr));
      nd6_opt_offset += UIP_ND6_OPT_PREFIX_INFO_LEN;
      uip_len += UIP_ND6_OPT_PREFIX_INFO_LEN;
    }
  }

  /* 6CO option */
#if UIP_ND6_RA_6CO
  for(context = uip_ds6_context_list;
      context < uip_ds6_context_list + UIP_DS6_6CO_NB; context++) {
    if(context->state==2) { //IN_USE_COMPRESS
      UIP_ND6_OPT_6CO_BUF->type = UIP_ND6_OPT_6CO;
      UIP_ND6_OPT_6CO_BUF->len = UIP_ND6_OPT_6CO_LEN ;
      UIP_ND6_OPT_6CO_BUF->context_len = context->length;
      UIP_ND6_OPT_6CO_BUF->res1_c_cid = 16 + context->context_id; // Flag C is set to 1
      UIP_ND6_OPT_6CO_BUF->valid_lifetime = uip_htons(context->vlifetime);
      UIP_ND6_OPT_6CO_BUF->reserved = 0;
      uip_ipaddr_copy(&(UIP_ND6_OPT_6CO_BUF->prefix), &(context->prefix));
      nd6_opt_offset += UIP_ND6_OPT_CONTEXT_INFO_LEN;
      uip_len += UIP_ND6_OPT_CONTEXT_INFO_LEN;
    }
  }
#endif

  /* ABRO option */
#if UIP_CONF_ND6_RA_ABRO
  create_abro(UIP_ND6_OPT_ABRO_BUF, 0xFFFF, 0xABCD, 0x1234);
  uip_len += UIP_ND6_OPT_ABRO_LEN << 3;
  nd6_opt_offset += UIP_ND6_OPT_ABRO_LEN << 3;
#endif

  /* Source link-layer option */
  create_llao((uint8_t *)UIP_ND6_OPT_HDR_BUF, UIP_ND6_OPT_SLLAO);

  uip_len += UIP_ND6_OPT_LLAO_LEN;
  nd6_opt_offset += UIP_ND6_OPT_LLAO_LEN;

  /* MTU option */
  UIP_ND6_OPT_MTU_BUF->type = UIP_ND6_OPT_MTU;
  UIP_ND6_OPT_MTU_BUF->len = UIP_ND6_OPT_MTU_LEN >> 3;
  UIP_ND6_OPT_MTU_BUF->reserved = 0;
  //UIP_ND6_OPT_MTU_BUF->mtu = uip_htonl(uip_ds6_if.link_mtu);
  UIP_ND6_OPT_MTU_BUF->mtu = uip_htonl(1500);

  uip_len += UIP_ND6_OPT_MTU_LEN;
  nd6_opt_offset += UIP_ND6_OPT_MTU_LEN;

  /* DNS option */
#if UIP_ND6_RA_RDNSS
  if(uip_nameserver_count() > 0) {
    uint8_t i = 0;
    uip_ipaddr_t *ip = &UIP_ND6_OPT_RDNSS_BUF->ip;
    uip_ipaddr_t *dns = NULL;
    UIP_ND6_OPT_RDNSS_BUF->type = UIP_ND6_OPT_RDNSS;
    UIP_ND6_OPT_RDNSS_BUF->reserved = 0;
    UIP_ND6_OPT_RDNSS_BUF->lifetime = uip_nameserver_next_expiration();
    if(UIP_ND6_OPT_RDNSS_BUF->lifetime != UIP_NAMESERVER_INFINITE_LIFETIME) {
      UIP_ND6_OPT_RDNSS_BUF->lifetime -= clock_seconds();
    }
    while((dns = uip_nameserver_get(i)) != NULL) {
      uip_ipaddr_copy(ip++, dns);
      i++;
    }
    UIP_ND6_OPT_RDNSS_BUF->len = UIP_ND6_OPT_RDNSS_LEN + (i << 1);
    PRINTF("%d nameservers reported\n", i);
    uip_len += UIP_ND6_OPT_RDNSS_BUF->len << 3;
    nd6_opt_offset += UIP_ND6_OPT_RDNSS_BUF->len << 3;
  }
#endif /* UIP_ND6_RA_RDNSS */

  UIP_IP_BUF->len[0] = ((uip_len - UIP_IPH_LEN) >> 8);
  UIP_IP_BUF->len[1] = ((uip_len - UIP_IPH_LEN) & 0xff);

  /*ICMP checksum */
  UIP_ICMP_BUF->icmpchksum = 0;
  UIP_ICMP_BUF->icmpchksum = ~uip_icmp6chksum();

  UIP_STAT(++uip_stat.nd6.sent);
  PRINTF("Sending RA to ");
  PRINT6ADDR(&UIP_IP_BUF->destipaddr);
  PRINTF(" from ");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF("\n");
  return;
}
#endif /* UIP_ND6_SEND_RA */
#endif /* UIP_CONF_ROUTER */

#if !UIP_CONF_ROUTER
/*---------------------------------------------------------------------------*/
void
uip_nd6_lowpan_rs_output(uip_ipaddr_t* rtr_ipaddr)
{
  UIP_IP_BUF->vtc = 0x60;
  UIP_IP_BUF->tcflow = 0;
  UIP_IP_BUF->flow = 0;
  UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
  UIP_IP_BUF->ttl = UIP_ND6_HOP_LIMIT;  
  if (rtr_ipaddr != NULL) {
      uip_ipaddr_copy(&UIP_IP_BUF->destipaddr, rtr_ipaddr);
    } else {
      uip_create_linklocal_allrouters_mcast(&UIP_IP_BUF->destipaddr);
    }
  uip_ds6_select_src(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr);
  UIP_ICMP_BUF->type = ICMP6_RS;
  UIP_ICMP_BUF->icode = 0;
  UIP_IP_BUF->len[0] = 0;       /* length will not be more than 255 */

  /*An unspecified source address MUST NOT be used in RS messages.*/
  if(uip_is_addr_unspecified(&UIP_IP_BUF->srcipaddr)) {
    PRINTF("RS message does not have specified source addres \n");
    return;
  } else {
    uip_len = uip_l3_icmp_hdr_len + UIP_ND6_RS_LEN + UIP_ND6_OPT_LLAO_LEN;
    UIP_IP_BUF->len[1] =
      UIP_ICMPH_LEN + UIP_ND6_RS_LEN + UIP_ND6_OPT_LLAO_LEN;

    create_llao(&uip_buf[uip_l2_l3_icmp_hdr_len + UIP_ND6_RS_LEN],
                UIP_ND6_OPT_SLLAO);
  }

  UIP_ICMP_BUF->icmpchksum = 0;
  UIP_ICMP_BUF->icmpchksum = ~uip_icmp6chksum();

  UIP_STAT(++uip_stat.nd6.sent);
  PRINTF("Sending RS to ");
  PRINT6ADDR(&UIP_IP_BUF->destipaddr);
  PRINTF(" from ");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF("\n");
  return;
}
/*---------------------------------------------------------------------------*/
/**
 * Process a Router Advertisement
 *
 * - Possible actions when receiving a RA: add router to router list,
 *   recalculate reachable time, update link hop limit, update retrans timer.
 * - If MTU option: update MTU.
 * - If SLLAO option: update entry in neighbor cache
 * - If prefix option: start autoconf, add prefix to prefix list
 */
void
ra_input(void)
{
  uip_lladdr_t lladdr_aligned;

  PRINTF("Received RA from ");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF(" to ");
  PRINT6ADDR(&UIP_IP_BUF->destipaddr);
  PRINTF("\n");
  UIP_STAT(++uip_stat.nd6.recv);

#if UIP_CONF_IPV6_CHECKS
  if((UIP_IP_BUF->ttl != UIP_ND6_HOP_LIMIT) ||
     (!uip_is_addr_linklocal(&UIP_IP_BUF->srcipaddr)) ||
     (UIP_ICMP_BUF->icode != 0)) {
    PRINTF("RA received is bad");
    goto discard;
  }
#endif /*UIP_CONF_IPV6_CHECKS */

  if(UIP_ND6_RA_BUF->cur_ttl != 0) {
    uip_ds6_if.cur_hop_limit = UIP_ND6_RA_BUF->cur_ttl;
    PRINTF("uip_ds6_if.cur_hop_limit %u\n", uip_ds6_if.cur_hop_limit);
  }

  if(UIP_ND6_RA_BUF->reachable_time != 0) {
    if(uip_ds6_if.base_reachable_time !=
       uip_ntohl(UIP_ND6_RA_BUF->reachable_time)) {
      uip_ds6_if.base_reachable_time = uip_ntohl(UIP_ND6_RA_BUF->reachable_time);
      uip_ds6_if.reachable_time = uip_ds6_compute_reachable_time();
    }
  }
  if(UIP_ND6_RA_BUF->retrans_timer != 0) {
    uip_ds6_if.retrans_timer = uip_ntohl(UIP_ND6_RA_BUF->retrans_timer);
  }

  /* Options processing */
  nd6_opt_offset = UIP_ND6_RA_LEN;
  while(uip_l3_icmp_hdr_len + nd6_opt_offset < uip_len) {
    if(UIP_ND6_OPT_HDR_BUF->len == 0) {
      PRINTF("RA received is bad");
      goto discard;
    }
    switch (UIP_ND6_OPT_HDR_BUF->type) {
    case UIP_ND6_OPT_SLLAO:
      PRINTF("Processing SLLAO option in RA\n");
      nd6_opt_llao = (uint8_t *) UIP_ND6_OPT_HDR_BUF;
      nbr = uip_ds6_nbr_lookup(&UIP_IP_BUF->srcipaddr);
      if(!extract_lladdr_from_llao_aligned(&lladdr_aligned)) {
        /* failed to extract llao - discard packet */
        goto discard;
      }
      if(nbr == NULL) {
        nbr = uip_ds6_nbr_add(&UIP_IP_BUF->srcipaddr, &lladdr_aligned,
                              1, NBR_STALE, NBR_TABLE_REASON_IPV6_ND, NULL);
      } else {
        const uip_lladdr_t *lladdr = uip_ds6_nbr_get_ll(nbr);
        if(lladdr == NULL) {
          goto discard;
        }
        if(nbr->state == NBR_INCOMPLETE) {
          nbr->state = NBR_STALE;
        }
        if(memcmp(&nd6_opt_llao[UIP_ND6_OPT_DATA_OFFSET],
                  lladdr, UIP_LLADDR_LEN) != 0) {
          /* change of link layer address */
          if(nbr_table_update_lladdr((const linkaddr_t *)lladdr, (const linkaddr_t *)&lladdr_aligned, 1) == 0) {
            /* failed to update the lladdr */
            goto discard;
          }
          nbr->state = NBR_STALE;
        }
        nbr->isrouter = 1;
      }
      break;
    case UIP_ND6_OPT_MTU:
      PRINTF("Processing MTU option in RA\n");
      uip_ds6_if.link_mtu =
        uip_ntohl(((uip_nd6_opt_mtu *) UIP_ND6_OPT_HDR_BUF)->mtu);
      break;
    case UIP_ND6_OPT_PREFIX_INFO:
      PRINTF("Processing PREFIX option in RA\n");
      nd6_opt_prefix_info = (uip_nd6_opt_prefix_info *) UIP_ND6_OPT_HDR_BUF;
      if((uip_ntohl(nd6_opt_prefix_info->validlt) >=
          uip_ntohl(nd6_opt_prefix_info->preferredlt))
         && (!uip_is_addr_linklocal(&nd6_opt_prefix_info->prefix))) {
        /* on-link flag related processing */
        if(nd6_opt_prefix_info->flagsreserved1 & UIP_ND6_RA_FLAG_ONLINK) {
          prefix =
            uip_ds6_prefix_lookup(&nd6_opt_prefix_info->prefix,
                                  nd6_opt_prefix_info->preflen);
          if(prefix == NULL) {
            if(nd6_opt_prefix_info->validlt != 0) {
              if(nd6_opt_prefix_info->validlt != UIP_ND6_INFINITE_LIFETIME) {
                prefix = uip_ds6_prefix_add(&nd6_opt_prefix_info->prefix,
                                            nd6_opt_prefix_info->preflen,
                                            uip_ntohl(nd6_opt_prefix_info->
                                                  validlt));
              } else {
                prefix = uip_ds6_prefix_add(&nd6_opt_prefix_info->prefix,
                                            nd6_opt_prefix_info->preflen, 0);
              }
            }
          } else {
            switch (nd6_opt_prefix_info->validlt) {
            case 0:
              uip_ds6_prefix_rm(prefix);
              break;
            case UIP_ND6_INFINITE_LIFETIME:
              prefix->isinfinite = 1;
              break;
            default:
              PRINTF("Updating timer of prefix ");
              PRINT6ADDR(&prefix->ipaddr);
              PRINTF(" new value %lu\n", uip_ntohl(nd6_opt_prefix_info->validlt));
              stimer_set(&prefix->vlifetime,
                         uip_ntohl(nd6_opt_prefix_info->validlt));
              prefix->isinfinite = 0;
              break;
            }
          }
        }
        /* End of on-link flag related processing */
        /* autonomous flag related processing */
        if((nd6_opt_prefix_info->flagsreserved1 & UIP_ND6_RA_FLAG_AUTONOMOUS)
           && (nd6_opt_prefix_info->validlt != 0)
           && (nd6_opt_prefix_info->preflen == UIP_DEFAULT_PREFIX_LEN)) {

          /* ipaddr is the generated address that need to be register */
          uip_ipaddr_copy(&ipaddr, &nd6_opt_prefix_info->prefix);
          uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
          addr = uip_ds6_addr_lookup(&ipaddr);

          if((addr != NULL) && (addr->type == ADDR_AUTOCONF)) {
            if(nd6_opt_prefix_info->validlt != UIP_ND6_INFINITE_LIFETIME) {
              /* The processing below is defined in RFC4862 section 5.5.3 e */
              if((uip_ntohl(nd6_opt_prefix_info->validlt) > 2 * 60 * 60) ||
                 (uip_ntohl(nd6_opt_prefix_info->validlt) >
                  stimer_remaining(&addr->vlifetime))) {
                PRINTF("Updating timer of address ");
                PRINT6ADDR(&addr->ipaddr);
                PRINTF(" new value %lu\n",
                       uip_ntohl(nd6_opt_prefix_info->validlt));
                stimer_set(&addr->vlifetime,
                           uip_ntohl(nd6_opt_prefix_info->validlt));
              } else {
                stimer_set(&addr->vlifetime, 2 * 60 * 60);
                PRINTF("Updating timer of address ");
                PRINT6ADDR(&addr->ipaddr);
                PRINTF(" new value %lu\n", (unsigned long)(2 * 60 * 60));
              }
              addr->isinfinite = 0;
            } else {
              addr->isinfinite = 1;
            }
          } else {
            if(uip_ntohl(nd6_opt_prefix_info->validlt) ==
               UIP_ND6_INFINITE_LIFETIME) {
              uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
            } else {
              uip_ds6_addr_add(&ipaddr, uip_ntohl(nd6_opt_prefix_info->validlt),
                               ADDR_AUTOCONF);
            }
            PRINTIfADDR(uip_ds6_if.addr_list);
          }
        }
        /* End of autonomous flag related processing */
      }
      break;
#if UIP_ND6_RA_6CO
    case UIP_ND6_OPT_6CO:
      PRINTF("Processing 6CO option in RA\n");
//      nd6_opt_6co = (uip_nd6_opt_6co*) UIP_ND6_OPT_6CO_BUF;
//      context = uip_ds6_context_lookup_by_id(nd6_opt_6co->res1_c_cid & UIP_ND6_RA_CID);
//      if (context != NULL) {
//        /* Context already exists. Update*/
//        if (nd6_opt_6co->lifetime != 0) {
//          context->length = nd6_opt_6co->preflen;
//          uip_ipaddr_copy(&context->prefix, &nd6_opt_6co->prefix);
//          if (nd6_opt_6co->res1_c_cid & UIP_ND6_RA_FLAG_COMPRESSION) {
//            context->state = IN_USE_COMPRESS;
//          } else {
//            context->state = IN_USE_UNCOMPRESS_ONLY;
//          }
//          /* Lifetime field in 6CO expressed in units of 60 seconds */
//          stimer_set(&context->vlifetime, uip_ntohs(nd6_opt_6co->lifetime) * 60);
//          context->defrt_lifetime = uip_ntohs(UIP_ND6_RA_BUF->router_lifetime);
//        } else {
//          uip_ds6_context_rm(context);
//        }
//      } else {
//        /* New context */
//        if (nd6_opt_6co->lifetime != 0) {
//          context = uip_ds6_context_add(nd6_opt_6co, uip_ntohs(UIP_ND6_RA_BUF->router_lifetime));
//          if (context != NULL) {
//            /* Lifetime field in 6CO expressed in units of 60 seconds */
//            stimer_set(&context->vlifetime, uip_ntohs(nd6_opt_6co->lifetime) * 60);
//          }
//        }
//      }
      break;
#endif /* UIP_ND6_RA_6CO */
#if UIP_ND6_RA_RDNSS
    case UIP_ND6_OPT_RDNSS:
      if(UIP_ND6_RA_BUF->flags_reserved & (UIP_ND6_O_FLAG << 6)) {
        PRINTF("Processing RDNSS option\n");
        uint8_t naddr = (UIP_ND6_OPT_RDNSS_BUF->len - 1) / 2;
        uip_ipaddr_t *ip = (uip_ipaddr_t *)(&UIP_ND6_OPT_RDNSS_BUF->ip);
        PRINTF("got %d nameservers\n", naddr);
        while(naddr-- > 0) {
          PRINTF(" nameserver: ");
          PRINT6ADDR(ip);
          PRINTF(" lifetime: %lx\n", uip_ntohl(UIP_ND6_OPT_RDNSS_BUF->lifetime));
          uip_nameserver_update(ip, uip_ntohl(UIP_ND6_OPT_RDNSS_BUF->lifetime));
          ip++;
        }
      }
      break;
#endif /* UIP_ND6_RA_RDNSS */
    default:
      PRINTF("ND option not supported in RA\n");
      break;
    }
    nd6_opt_offset += (UIP_ND6_OPT_HDR_BUF->len << 3);
  }

  defrt = uip_ds6_defrt_lookup(&UIP_IP_BUF->srcipaddr);
  if(UIP_ND6_RA_BUF->router_lifetime != 0) {
    if(nbr != NULL) {
      nbr->isrouter = 1;
    }
    if(defrt == NULL) {
      uip_ds6_defrt_add(&UIP_IP_BUF->srcipaddr,
                        (unsigned long)(uip_ntohs(UIP_ND6_RA_BUF->router_lifetime)));

      /**********************************************************************
       * The host triggers sending NS messages containing an ARO when a new *
       * address is configured, when it discovers a new default router      *
       **********************************************************************/

//      uip_nd6_lowpan_ns_output(&UIP_IP_BUF->destipaddr, &UIP_IP_BUF->srcipaddr,
//			&UIP_IP_BUF->srcipaddr,1, (uint16_t) uip_ntohs(UIP_ND6_RA_BUF->router_lifetime));
//      PRINT6ADDR(&ipaddr);

      /* The generated address (ipaddr) is as source address of NS message */
	  uip_nd6_lowpan_ns_output(&ipaddr, &UIP_IP_BUF->srcipaddr,
			  &UIP_IP_BUF->srcipaddr,1, (uint16_t) uip_ntohs(UIP_ND6_RA_BUF->router_lifetime));
      tcpip_ipv6_output();
      return;

    } else {
      stimer_set(&(defrt->lifetime),
                 (unsigned long)(uip_ntohs(UIP_ND6_RA_BUF->router_lifetime)));
    }
  } else {
    if(defrt != NULL) {
      uip_ds6_defrt_rm(defrt);
    }
  }

#if UIP_CONF_IPV6_QUEUE_PKT
  /* If the nbr just became reachable (e.g. it was in NBR_INCOMPLETE state
   * and we got a SLLAO), check if we had buffered a pkt for it */
  /*  if((nbr != NULL) && (nbr->queue_buf_len != 0)) {
    uip_len = nbr->queue_buf_len;
    memcpy(UIP_IP_BUF, nbr->queue_buf, uip_len);
    nbr->queue_buf_len = 0;
    return;
    }*/
  if(nbr != NULL && uip_packetqueue_buflen(&nbr->packethandle) != 0) {
    uip_len = uip_packetqueue_buflen(&nbr->packethandle);
    memcpy(UIP_IP_BUF, uip_packetqueue_buf(&nbr->packethandle), uip_len);
    uip_packetqueue_free(&nbr->packethandle);
    return;
  }

#endif /*UIP_CONF_IPV6_QUEUE_PKT */
   etimer_stop(&uip_ds6_timer_rs);
discard:
  uip_clear_buf();
  return;
}
#endif /* !UIP_CONF_ROUTER */
/*------------------------------------------------------------------*/
/* ICMPv6 input handlers */
#if UIP_ND6_SEND_NA
UIP_ICMP6_HANDLER(ns_input_handler, ICMP6_NS, UIP_ICMP6_HANDLER_CODE_ANY,
                  ns_input);
UIP_ICMP6_HANDLER(na_input_handler, ICMP6_NA, UIP_ICMP6_HANDLER_CODE_ANY,
                  na_input);
#endif

#if UIP_CONF_ROUTER && UIP_ND6_SEND_RA
UIP_ICMP6_HANDLER(rs_input_handler, ICMP6_RS, UIP_ICMP6_HANDLER_CODE_ANY,
                  rs_input);
#endif

#if !UIP_CONF_ROUTER
UIP_ICMP6_HANDLER(ra_input_handler, ICMP6_RA, UIP_ICMP6_HANDLER_CODE_ANY,
                  ra_input);
#endif
/*---------------------------------------------------------------------------*/
void
uip_nd6_init()
{

#if UIP_ND6_SEND_NA
  /* Only handle NSs if we are prepared to send out NAs */
  uip_icmp6_register_input_handler(&ns_input_handler);

  /*
   * Only handle NAs if we are prepared to send out NAs.
   * This is perhaps logically incorrect, but this condition was present in
   * uip_process and we keep it until proven wrong
   */
  uip_icmp6_register_input_handler(&na_input_handler);
#endif

#if UIP_CONF_ROUTER && UIP_ND6_SEND_RA
  /* Only accept RS if we are a router and happy to send out RAs */
  uip_icmp6_register_input_handler(&rs_input_handler);
#endif

#if !UIP_CONF_ROUTER
  /* Only process RAs if we are not a router */
  uip_icmp6_register_input_handler(&ra_input_handler);
#endif
}
//#endif /*UIP_CONF_IPV6_LOWPAN_ND*/
/*---------------------------------------------------------------------------*/
 /** @} */
