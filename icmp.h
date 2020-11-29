/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * A stripped version of ICMP header definitions from include/uapi/linux/icmp.h
 *
 * Shawn Chen <shuoshuc@cs.cmu.edu>
 */
#ifndef _ICMP_H
#define _ICMP_H

#include <linux/types.h>

#define ICMP_ACTIVE_TDN_ID	7	/* Active TDN ID change		*/

struct icmphdr {
  __u8		type;
  __u8		code;
  __sum16	checksum;
  union {
	struct {
		__be16	id;
		__be16	sequence;
	} echo;
	__be32	gateway;
	struct {
		__be16	__unused;
		__be16	mtu;
	} frag;
#ifdef CONFIG_TDTCP
	/* The first byte of a 4-byte word is TDN ID. Include a 3-byte array as
	 * placeholder for the rest 3 bytes in the same word.
	 */
	struct {
		__u8	id;
		__u8	__unused[3];
	} active_tdn;
#endif
	__u8	reserved[4];
  } un;
};

#endif /* _ICMP_H */
