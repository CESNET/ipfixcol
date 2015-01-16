/* Copyright (c) 2002-2011 InMon Corp. Licensed under the terms of the InMon sFlow licence: */
/* http://www.inmon.com/technology/sflowlicense.txt */

#if defined(__cplusplus)
extern "C" {
#endif

//#ifdef WIN32
//#include "config_windows.h"
//#else
//#include "config.h"
//#endif

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <setjmp.h>

#ifdef WIN32
#else
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <endian.h>
#endif

#include "sflow.h" // sFlow v5
#include "sflowtool.h" // sFlow v2/4

#include <ipfixcol.h>

/*
#ifdef DARWIN
#include <architecture/byte_order.h>
#define bswap_16(x) NXSwapShort(x)
#define bswap_32(x) NXSwapInt(x)
#else
#include <byteswap.h>
#endif
*/

#ifndef PRIu64
# ifdef WIN32
#  define PRIu64 "I64u"
# else
#  define PRIu64 "llu"
# endif
#endif

#define YES 1
#define NO 0

static uint16_t numOfFlowSamples;

/* define my own IP header struct - to ease portability */
struct myiphdr
  {
    uint8_t version_and_headerLen;
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
};

/* same for tcp */
struct mytcphdr
  {
    uint16_t th_sport;		/* source port */
    uint16_t th_dport;		/* destination port */
    uint32_t th_seq;		/* sequence number */
    uint32_t th_ack;		/* acknowledgement number */
    uint8_t th_off_and_unused;
    uint8_t th_flags;
    uint16_t th_win;		/* window */
    uint16_t th_sum;		/* checksum */
    uint16_t th_urp;		/* urgent pointer */
};

/* and UDP */
struct myudphdr {
  uint16_t uh_sport;           /* source port */
  uint16_t uh_dport;           /* destination port */
  uint16_t uh_ulen;            /* udp length */
  uint16_t uh_sum;             /* udp checksum */
};

/* and ICMP */
struct myicmphdr
{
  uint8_t type;		/* message type */
  uint8_t code;		/* type sub-code */
  /* ignore the rest */
};


/* tcpdump file format */

struct pcap_file_header {
  uint32_t magic;
  uint16_t version_major;
  uint16_t version_minor;
  uint32_t thiszone;	/* gmt to local correction */
  uint32_t sigfigs;	/* accuracy of timestamps */
  uint32_t snaplen;	/* max length saved portion of each pkt */
  uint32_t linktype;	/* data link type (DLT_*) */
};

typedef struct _SFForwardingTarget {
  struct _SFForwardingTarget *nxt;
  struct in_addr host;
  uint32_t port;
  struct sockaddr_in addr;
  int sock;
} SFForwardingTarget;

typedef enum { SFLFMT_FULL=0, SFLFMT_PCAP, SFLFMT_LINE, SFLFMT_NETFLOW, SFLFMT_FWD, SFLFMT_CLF } EnumSFLFormat;

typedef struct _SFConfig {
  /* sflow(R) options */
  uint16_t sFlowInputPort;
  /* netflow(TM) options */
  uint16_t netFlowOutputPort;
  struct in_addr netFlowOutputIP;
  int netFlowOutputSocket;
  uint16_t netFlowPeerAS;
  int disableNetFlowScale;
  /* tcpdump options */
  char *readPcapFileName;
  FILE *readPcapFile;
  struct pcap_file_header readPcapHdr;
  char *writePcapFile;
  EnumSFLFormat outputFormat;
  uint32_t tcpdumpHdrPad;
  u_char zeroPad[100];
  int pcapSwap;

  SFForwardingTarget *forwardingTargets;

  /* vlan filtering */
  int gotVlanFilter;
#define FILTER_MAX_VLAN 4096
  u_char vlanFilter[FILTER_MAX_VLAN + 1];

  /* content stripping */
  int removeContent;

  /* options to restrict IP socket / bind */
  int listen4;
  int listen6;
  int listenControlled;
} SFConfig;

/* make the options structure global to the program */
static SFConfig sfConfig;

/* define a separate global we can use to construct the common-log-file format */
typedef struct _SFCommonLogFormat {
#define SFLFMT_CLF_MAX_LINE 2000
  int valid;
  char client[64];
  char http_log[SFLFMT_CLF_MAX_LINE];
} SFCommonLogFormat;

static SFCommonLogFormat sfCLF;
static const char *SFHTTP_method_names[] = { "-", "OPTIONS", "GET", "HEAD", "POST", "PUT", "DELETE", "TRACE", "CONNECT" };

typedef struct _SFSample {
  SFLAddress sourceIP;
  SFLAddress agent_addr;
  uint32_t agentSubId;

  /* the raw pdu */
  u_char *rawSample;
  uint32_t rawSampleLen;
  u_char *endp;
  time_t pcapTimestamp;

  /* decode cursor */
  uint32_t *datap;

  uint32_t datagramVersion;
  uint32_t sampleType;
  uint32_t ds_class;
  uint32_t ds_index;

  /* generic interface counter sample */
  SFLIf_counters ifCounters;

  /* sample stream info */
  uint32_t sysUpTime;
  uint32_t sequenceNo;
  uint32_t sampledPacketSize;
  uint32_t samplesGenerated;
  uint32_t meanSkipCount;
  uint32_t samplePool;
  uint32_t dropEvents;

  /* the sampled header */
  uint32_t packet_data_tag;
  uint32_t headerProtocol;
  u_char *header;
  int headerLen;
  uint32_t stripped;

  /* header decode */
  int gotIPV4;
  int gotIPV4Struct;
  int offsetToIPV4;
  int gotIPV6;
  int gotIPV6Struct;
  int offsetToIPV6;
  int offsetToPayload;
  SFLAddress ipsrc;
  SFLAddress ipdst;
  uint32_t dcd_ipProtocol;
  uint32_t dcd_ipTos;
  uint32_t dcd_ipTTL;
  uint32_t dcd_sport;
  uint32_t dcd_dport;
  uint32_t dcd_tcpFlags;
  uint32_t ip_fragmentOffset;
  uint32_t udp_pduLen;

  /* ports */
  uint32_t inputPortFormat;
  uint32_t outputPortFormat;
  uint32_t inputPort;
  uint32_t outputPort;

  /* ethernet */
  uint32_t eth_type;
  uint32_t eth_len;
  u_char eth_src[8];
  u_char eth_dst[8];

  /* vlan */
  uint32_t in_vlan;
  uint32_t in_priority;
  uint32_t internalPriority;
  uint32_t out_vlan;
  uint32_t out_priority;
  int vlanFilterReject;

  /* extended data fields */
  uint32_t num_extended;
  uint32_t extended_data_tag;
#define SASAMPLE_EXTENDED_DATA_SWITCH 1
#define SASAMPLE_EXTENDED_DATA_ROUTER 4
#define SASAMPLE_EXTENDED_DATA_GATEWAY 8
#define SASAMPLE_EXTENDED_DATA_USER 16
#define SASAMPLE_EXTENDED_DATA_URL 32
#define SASAMPLE_EXTENDED_DATA_MPLS 64
#define SASAMPLE_EXTENDED_DATA_NAT 128
#define SASAMPLE_EXTENDED_DATA_MPLS_TUNNEL 256
#define SASAMPLE_EXTENDED_DATA_MPLS_VC 512
#define SASAMPLE_EXTENDED_DATA_MPLS_FTN 1024
#define SASAMPLE_EXTENDED_DATA_MPLS_LDP_FEC 2048
#define SASAMPLE_EXTENDED_DATA_VLAN_TUNNEL 4096

  /* IP forwarding info */
  SFLAddress nextHop;
  uint32_t srcMask;
  uint32_t dstMask;

  /* BGP info */
  SFLAddress bgp_nextHop;
  uint32_t my_as;
  uint32_t src_as;
  uint32_t src_peer_as;
  uint32_t dst_as_path_len;
  uint32_t *dst_as_path;
  /* note: version 4 dst as path segments just get printed, not stored here, however
   * the dst_peer and dst_as are filled in, since those are used for netflow encoding
   */
  uint32_t dst_peer_as;
  uint32_t dst_as;

  uint32_t communities_len;
  uint32_t *communities;
  uint32_t localpref;

  /* user id */
#define SA_MAX_EXTENDED_USER_LEN 200
  uint32_t src_user_charset;
  uint32_t src_user_len;
  char src_user[SA_MAX_EXTENDED_USER_LEN+1];
  uint32_t dst_user_charset;
  uint32_t dst_user_len;
  char dst_user[SA_MAX_EXTENDED_USER_LEN+1];

  /* url */
#define SA_MAX_EXTENDED_URL_LEN 200
#define SA_MAX_EXTENDED_HOST_LEN 200
  uint32_t url_direction;
  uint32_t url_len;
  char url[SA_MAX_EXTENDED_URL_LEN+1];
  uint32_t host_len;
  char host[SA_MAX_EXTENDED_HOST_LEN+1];

  /* mpls */
  SFLAddress mpls_nextHop;

  /* nat */
  SFLAddress nat_src;
  SFLAddress nat_dst;

  /* counter blocks */
  uint32_t statsSamplingInterval;
  uint32_t counterBlockVersion;

  /* exception handler context */
  jmp_buf env;

#define ERROUT stderr

#ifdef DEBUG
# define SFABORT(s, r) abort()
# undef ERROUT
# define ERROUT stdout
#else
# define SFABORT(s, r) longjmp((s)->env, (r))
#endif

#define SF_ABORT_EOS 1

} SFSample;

/* Cisco netflow version 5 record format */

typedef struct _NFFlow5 {
  uint32_t srcIP;
  uint32_t dstIP;
  uint32_t nextHop;
  uint16_t if_in;
  uint16_t if_out;
  uint32_t frames;
  uint32_t bytes;
  uint64_t firstTime;
  uint64_t lastTime;
  uint16_t srcPort;
  uint16_t dstPort;
  uint8_t pad1;
  uint8_t tcpFlags;
  uint8_t ipProto;
  uint8_t ipTos;
  uint16_t srcAS;
  uint16_t dstAS;
  uint8_t srcMask;  /* No. bits */
  uint8_t dstMask;  /* No. bits */
  uint16_t pad2;
} NFFlow5;

typedef struct _ipfix_packet {
  struct ipfix_header hdr;
  NFFlow5 flow; /* normally an array, but here we always send just 1 at a time */
} ipfix_packet;

static void readFlowSample_header(SFSample *sample);
static void readFlowSample(SFSample *sample, int expanded, char *packet);

/*_________________---------------------------__________________
  _________________        printHex           __________________
  -----------------___________________________------------------
*/

static u_char bin2hex(int nib) { return (nib < 10) ? ('0' + nib) : ('A' - 10 + nib); }

int printHex(const u_char *a, int len, u_char *buf, int bufLen, int marker, int bytesPerOutputLine)
{
  int b = 0, i = 0;
  for(; i < len; i++) {
    u_char byte;
    if(b > (bufLen - 10)) break;
    if(marker > 0 && i == marker) {
      buf[b++] = '<';
      buf[b++] = '*';
      buf[b++] = '>';
      buf[b++] = '-';
    }
    byte = a[i];
    buf[b++] = bin2hex(byte >> 4);
    buf[b++] = bin2hex(byte & 0x0f);
    if(i > 0 && (i % bytesPerOutputLine) == 0) buf[b++] = '\n';
    else {
      // separate the bytes with a dash
      if (i < (len - 1)) buf[b++] = '-';
    }
  }
  buf[b] = '\0';
  return b;
}

/*_________________---------------------------__________________
  _________________       URLEncode           __________________
  -----------------___________________________------------------
*/

char *URLEncode(char *in, char *out, int outlen)
{
  register char c, *r = in, *w = out;
  int maxlen = (strlen(in) * 3) + 1;
  if(outlen < maxlen) return "URLEncode: not enough space";
  while ((c = *r++)) {
    if(isalnum(c)) *w++ = c;
    else if(isspace(c)) *w++ = '+';
    else {
      *w++ = '%';
      *w++ = bin2hex(c >> 4);
      *w++ = bin2hex(c & 0x0f);
    }
  }
  *w++ = '\0';
  return out;
}


/*_________________---------------------------__________________
  _________________      IP_to_a              __________________
  -----------------___________________________------------------
*/

char *IP_to_a(uint32_t ipaddr, char *buf)
{
  u_char *ip = (u_char *)&ipaddr;
  sprintf(buf, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return buf;
}


/*_________________---------------------------__________________
  _________________    sampleFilterOK         __________________
  -----------------___________________________------------------
*/

int sampleFilterOK(SFSample *sample)
{
  // the vlan filter will only reject a sample if both in_vlan and out_vlan are rejected. If the
  // vlan was not reported in an SFLExtended_Switch struct, but was only picked up from the 802.1q header
  // then the out_vlan will be 0,  so to be sure you are rejecting vlan 1,  you may need to reject both
  // vlan 0 and vlan 1.
  return(sfConfig.gotVlanFilter == NO
	 || sfConfig.vlanFilter[sample->in_vlan]
	 || sfConfig.vlanFilter[sample->out_vlan]);
}

/*_________________---------------------------__________________
  _________________    lengthCheck            __________________
  -----------------___________________________------------------
*/

static void lengthCheck(SFSample *sample, char *description, u_char *start, int len) {
  uint32_t actualLen = (u_char *)sample->datap - start;
  uint32_t adjustedLen = ((len + 3) >> 2) << 2;
  if(actualLen != adjustedLen) {
//    MSG_ERROR("sflow conversion", "lengthCheck() error");
  }
}

/*_________________---------------------------__________________
  _________________     decodeLinkLayer       __________________
  -----------------___________________________------------------
  store the offset to the start of the ipv4 header in the sequence_number field
  or -1 if not found. Decode the 802.1d if it's there.
*/

#define NFT_ETHHDR_SIZ 14
#define NFT_MAX_8023_LEN 1500

#define NFT_MIN_SIZ (NFT_ETHHDR_SIZ + sizeof(struct myiphdr))

static void decodeLinkLayer(SFSample *sample)
{
  u_char *start = (u_char *)sample->header;
  u_char *end = start + sample->headerLen;
  u_char *ptr = start;
  uint16_t type_len;

  /* assume not found */
  sample->gotIPV4 = NO;
  sample->gotIPV6 = NO;

  if(sample->headerLen < NFT_ETHHDR_SIZ) {
	  return; /* not enough for an Ethernet header */
  }

  memcpy(sample->eth_dst, ptr, 6);
  ptr += 6;

  memcpy(sample->eth_src, ptr, 6);
  ptr += 6;
  type_len = (ptr[0] << 8) + ptr[1];
  ptr += 2;

  if(type_len == 0x8100) {
    /* VLAN  - next two bytes */
    uint32_t vlanData = (ptr[0] << 8) + ptr[1];
    uint32_t vlan = vlanData & 0x0fff;
    ptr += 2;
    /*  _____________________________________ */
    /* |   pri  | c |         vlan-id        | */
    /*  ------------------------------------- */
    /* [priority = 3bits] [Canonical Format Flag = 1bit] [vlan-id = 12 bits] */
    sample->in_vlan = vlan;
    /* now get the type_len again (next two bytes) */
    type_len = (ptr[0] << 8) + ptr[1];
    ptr += 2;
  }

  /* now we're just looking for IP */
  if(sample->headerLen < NFT_MIN_SIZ) {
	  return; /* not enough for an IPv4 header */
  }

  /* peek for IPX */
  if(type_len == 0x0200 || type_len == 0x0201 || type_len == 0x0600) {
#define IPX_HDR_LEN 30
#define IPX_MAX_DATA 546
    int ipxChecksum = (ptr[0] == 0xff && ptr[1] == 0xff);
    int ipxLen = (ptr[2] << 8) + ptr[3];
    if(ipxChecksum &&
       ipxLen >= IPX_HDR_LEN &&
       ipxLen <= (IPX_HDR_LEN + IPX_MAX_DATA))
      /* we don't do anything with IPX here */
      return;
  }

  if(type_len <= NFT_MAX_8023_LEN) {
    /* assume 802.3+802.2 header */
    /* check for SNAP */
    if(ptr[0] == 0xAA &&
       ptr[1] == 0xAA &&
       ptr[2] == 0x03) {
      ptr += 3;
      if(ptr[0] != 0 ||
	 ptr[1] != 0 ||
	 ptr[2] != 0) {
	return; /* no further decode for vendor-specific protocol */
      }
      ptr += 3;
      /* OUI == 00-00-00 means the next two bytes are the ethernet type (RFC 2895) */
      type_len = (ptr[0] << 8) + ptr[1];
      ptr += 2;
    } else {
      if (ptr[0] == 0x06 &&
	  ptr[1] == 0x06 &&
	  (ptr[2] & 0x01)) {
	/* IP over 8022 */
	ptr += 3;
	/* force the type_len to be IP so we can inline the IP decode below */
	type_len = 0x0800;
      }
      else return;
    }
  }

  /* assume type_len is an ethernet-type now */
  sample->eth_type = type_len;

  if(type_len == 0x0800) {
    /* IPV4 */
    if((end - ptr) < sizeof(struct myiphdr)) return;
    /* look at first byte of header.... */
    /*  ___________________________ */
    /* |   version   |    hdrlen   | */
    /*  --------------------------- */
    if((*ptr >> 4) != 4) return; /* not version 4 */
    if((*ptr & 15) < 5) return; /* not IP (hdr len must be 5 quads or more) */
    /* survived all the tests - store the offset to the start of the ip header */
    sample->gotIPV4 = YES;
    sample->offsetToIPV4 = (ptr - start);
  }

  if(type_len == 0x86DD) {
    /* IPV6 */
    /* look at first byte of header.... */
    if((*ptr >> 4) != 6) return; /* not version 6 */
    /* survived all the tests - store the offset to the start of the ip6 header */
    sample->gotIPV6 = YES;
    sample->offsetToIPV6 = (ptr - start);
  }
}

/*_________________---------------------------__________________
  _________________       decode80211MAC      __________________
  -----------------___________________________------------------
  store the offset to the start of the ipv4 header in the sequence_number field
  or -1 if not found.
*/

#define WIFI_MIN_HDR_SIZ 24

static void decode80211MAC(SFSample *sample)
{
  u_char *start = (u_char *)sample->header;
  u_char *ptr = start;

  /* assume not found */
  sample->gotIPV4 = NO;
  sample->gotIPV6 = NO;

  if(sample->headerLen < WIFI_MIN_HDR_SIZ) return; /* not enough for an 80211 MAC header */

  uint32_t fc = (ptr[1] << 8) + ptr[0];  // [b7..b0][b15..b8]
  uint32_t control = (fc >> 2) & 3;
  uint32_t toDS = (fc >> 8) & 1;
  uint32_t fromDS = (fc >> 9) & 1;


  ptr += 2;

  ptr += 2;

  switch(control) {
  case 0: // mgmt
  case 1: // ctrl
  case 3: // rsvd
  break;

  case 2: // data
    {

      u_char *macAddr1 = ptr;
      ptr += 6;
      u_char *macAddr2 = ptr;
      ptr += 6;
      u_char *macAddr3 = ptr;
      ptr += 6;
      ptr += 2;

      // ToDS   FromDS   Addr1   Addr2  Addr3   Addr4
      // 0      0        DA      SA     BSSID   N/A (ad-hoc)
      // 0      1        DA      BSSID  SA      N/A
      // 1      0        BSSID   SA     DA      N/A
      // 1      1        RA      TA     DA      SA  (wireless bridge)

      u_char *srcMAC = NULL;
      u_char *dstMAC = NULL;

      if(toDS) {
	dstMAC = macAddr3;
	if(fromDS) {
	  srcMAC = ptr; // macAddr4.  1,1 => (wireless bridge)
	  ptr += 6;
	}
	else srcMAC = macAddr2;  // 1,0
      }
      else {
	dstMAC = macAddr1;
	if(fromDS) srcMAC = macAddr3; // 0,1
	else srcMAC = macAddr2; // 0,0
      }

      if(srcMAC) {
	memcpy(sample->eth_src, srcMAC, 6);
      }
      if(dstMAC) {
	memcpy(sample->eth_dst, dstMAC, 6);
      }
    }
  }
}

/*_________________---------------------------__________________
  _________________     decodeIPLayer4        __________________
  -----------------___________________________------------------
*/

static void decodeIPLayer4(SFSample *sample, u_char *ptr) {
  u_char *end = sample->header + sample->headerLen;
  if(ptr > (end - 8)) {
    // not enough header bytes left
    return;
  }
  switch(sample->dcd_ipProtocol) {
  case 1: /* ICMP */
    {
      struct myicmphdr icmp;
      memcpy(&icmp, ptr, sizeof(icmp));
      sample->dcd_sport = icmp.type;
      sample->dcd_dport = icmp.code;
      sample->offsetToPayload = ptr + sizeof(icmp) - sample->header;
    }
    break;
  case 6: /* TCP */
    {
      struct mytcphdr tcp;
      int headerBytes;
      memcpy(&tcp, ptr, sizeof(tcp));
      sample->dcd_sport = ntohs(tcp.th_sport);
      sample->dcd_dport = ntohs(tcp.th_dport);
      sample->dcd_tcpFlags = tcp.th_flags;
      headerBytes = (tcp.th_off_and_unused >> 4) * 4;
      ptr += headerBytes;
      sample->offsetToPayload = ptr - sample->header;
    }
    break;
  case 17: /* UDP */
    {
      struct myudphdr udp;
      memcpy(&udp, ptr, sizeof(udp));
      sample->dcd_sport = ntohs(udp.uh_sport);
      sample->dcd_dport = ntohs(udp.uh_dport);
      sample->udp_pduLen = ntohs(udp.uh_ulen);
      sample->offsetToPayload = ptr + sizeof(udp) - sample->header;
    }
    break;
  default: /* some other protcol */
    sample->offsetToPayload = ptr - sample->header;
    break;
  }
}

/*_________________---------------------------__________________
  _________________     decodeIPV4            __________________
  -----------------___________________________------------------
*/

static void decodeIPV4(SFSample *sample)
{
  if(sample->gotIPV4) {
    u_char *ptr = sample->header + sample->offsetToIPV4;
    /* Create a local copy of the IP header (cannot overlay structure in case it is not quad-aligned...some
       platforms would core-dump if we tried that).  It's OK coz this probably performs just as well anyway. */
    struct myiphdr ip;
    memcpy(&ip, ptr, sizeof(ip));
    /* Value copy all ip elements into sample */
    sample->ipsrc.type = SFLADDRESSTYPE_IP_V4;
    sample->ipsrc.address.ip_v4.addr = ip.saddr;
    sample->ipdst.type = SFLADDRESSTYPE_IP_V4;
    sample->ipdst.address.ip_v4.addr = ip.daddr;
    sample->dcd_ipProtocol = ip.protocol;
    sample->dcd_ipTos = ip.tos;
    sample->dcd_ipTTL = ip.ttl;
    /* check for fragments */
    sample->ip_fragmentOffset = ntohs(ip.frag_off) & 0x1FFF;
    if(sample->ip_fragmentOffset > 0) {
    }
    else {
      /* advance the pointer to the next protocol layer */
      /* ip headerLen is expressed as a number of quads */
      ptr += (ip.version_and_headerLen & 0x0f) * 4;
      decodeIPLayer4(sample, ptr);
    }
  }
}

/*_________________---------------------------__________________
  _________________     decodeIPV6            __________________
  -----------------___________________________------------------
*/

static void decodeIPV6(SFSample *sample)
{
  uint32_t label;
  uint32_t nextHeader;
  u_char *end = sample->header + sample->headerLen;

  if(sample->gotIPV6) {
    u_char *ptr = sample->header + sample->offsetToIPV6;

    // check the version
    {
      int ipVersion = (*ptr >> 4);
      if(ipVersion != 6) {
	return;
      }
    }

    // get the tos (priority)
    sample->dcd_ipTos = *ptr++ & 15;
    // 24-bit label
    label = *ptr++;
    label <<= 8;
    label += *ptr++;
    label <<= 8;
    label += *ptr++;
    ptr += 2;
    // if payload is zero, that implies a jumbo payload
    // next header
    nextHeader = *ptr++;

    // TTL
    sample->dcd_ipTTL = *ptr++;

    {// src and dst address
      sample->ipsrc.type = SFLADDRESSTYPE_IP_V6;
      memcpy(&sample->ipsrc.address, ptr, 16);
      ptr +=16;
      sample->ipdst.type = SFLADDRESSTYPE_IP_V6;
      memcpy(&sample->ipdst.address, ptr, 16);
      ptr +=16;
    }

    // skip over some common header extensions...
    // http://searchnetworking.techtarget.com/originalContent/0,289142,sid7_gci870277,00.html
    while(nextHeader == 0 ||  // hop
	  nextHeader == 43 || // routing
	  nextHeader == 44 || // fragment
	  // nextHeader == 50 || // encryption - don't bother coz we'll not be able to read any further
	  nextHeader == 51 || // auth
	  nextHeader == 60) { // destination options
      uint32_t optionLen, skip;
      nextHeader = ptr[0];
      optionLen = 8 * (ptr[1] + 1);  // second byte gives option len in 8-byte chunks, not counting first 8
      skip = optionLen - 2;
      ptr += skip;
      if(ptr > end) return; // ran off the end of the header
    }

    // now that we have eliminated the extension headers, nextHeader should have what we want to
    // remember as the ip protocol...
    sample->dcd_ipProtocol = nextHeader;
    decodeIPLayer4(sample, ptr);
  }
}

/*_________________---------------------------__________________
  _________________   sendNetFlowDatagram     __________________
  -----------------___________________________------------------
*/


static void sendNetFlowDatagram(SFSample *sample, char *packet)
{
  ipfix_packet pkt;
  uint32_t now = (uint32_t)time(NULL);
  uint32_t bytes;
  // ignore fragments
  if(sample->ip_fragmentOffset > 0) return;
  // count the bytes from the start of IP header, with the exception that
  // for udp packets we use the udp_pduLen. This is because the udp_pduLen
  // can be up tp 65535 bytes, which causes fragmentation at the IP layer.
  // Since the sampled fragments are discarded, we have to use this field
  // to get the total bytes estimates right.
  if(sample->udp_pduLen > 0) bytes = sample->udp_pduLen;
  else bytes = sample->sampledPacketSize - sample->stripped - sample->offsetToIPV4;

  memset(&pkt, 0, sizeof(pkt));

  // version, length and sequence number will be set in input plugin
//  pkt.hdr.sysUpTime = htonl(now % (3600 * 24)) * 1000;  /* pretend we started at midnight (milliseconds) */
  pkt.hdr.export_time = htonl(now);
  ((struct ipfix_header *)packet)->export_time = pkt.hdr.export_time;

  pkt.flow.srcIP = sample->ipsrc.address.ip_v4.addr;
  pkt.flow.dstIP = sample->ipdst.address.ip_v4.addr;
  pkt.flow.nextHop = sample->nextHop.address.ip_v4.addr;
  pkt.flow.if_in = htons((uint16_t)sample->inputPort);
  pkt.flow.if_out= htons((uint16_t)sample->outputPort);

  if(!sfConfig.disableNetFlowScale) {
    pkt.flow.frames = htonl(sample->meanSkipCount);
    pkt.flow.bytes = htonl(sample->meanSkipCount * bytes);
  }
  else {
    /* set the sampling_interval header field too (used to be a 16-bit reserved field) */
    pkt.flow.frames = htonl(1);
    pkt.flow.bytes = htonl(bytes);
  }

  pkt.flow.firstTime = pkt.hdr.export_time;  /* set the start and end time to be now (in milliseconds since last boot) */
  pkt.flow.lastTime =  pkt.hdr.export_time;
  pkt.flow.srcPort = htons((uint16_t)sample->dcd_sport);
  pkt.flow.dstPort = htons((uint16_t)sample->dcd_dport);
  pkt.flow.tcpFlags = sample->dcd_tcpFlags;
  pkt.flow.ipProto = sample->dcd_ipProtocol;
  pkt.flow.ipTos = sample->dcd_ipTos;

  if(sfConfig.netFlowPeerAS) {
    pkt.flow.srcAS = htons((uint16_t)sample->src_peer_as);
    pkt.flow.dstAS = htons((uint16_t)sample->dst_peer_as);
  }
  else {
    pkt.flow.srcAS = htons((uint16_t)sample->src_as);
    pkt.flow.dstAS = htons((uint16_t)sample->dst_as);
  }

  pkt.flow.srcMask = (uint8_t)sample->srcMask;
  pkt.flow.dstMask = (uint8_t)sample->dstMask;

  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);

  pkt.flow.firstTime = htobe64((tp.tv_sec * 1000) + (tp.tv_nsec/1000000));
  pkt.flow.lastTime = htobe64((tp.tv_sec * 1000) + (tp.tv_nsec/1000000));

  memcpy(packet + IPFIX_HEADER_LENGTH + numOfFlowSamples*sizeof(pkt.flow), &pkt.flow, sizeof(pkt.flow));
  numOfFlowSamples++;
}

/*_________________---------------------------__________________
  _________________   read data fns           __________________
  -----------------___________________________------------------
*/

static uint32_t getData32_nobswap(SFSample *sample) {
  uint32_t ans = *(sample->datap)++;
  // make sure we didn't run off the end of the datagram.  Thanks to
  // Sven Eschenberg for spotting a bug/overrun-vulnerabilty that was here before.
  if((u_char *)sample->datap > sample->endp) {
    SFABORT(sample, SF_ABORT_EOS);
  }
  return ans;
}

static uint32_t getData32(SFSample *sample) {
  return ntohl(getData32_nobswap(sample));
}


static uint64_t getData64(SFSample *sample) {
  uint64_t tmpLo, tmpHi;
  tmpHi = getData32(sample);
  tmpLo = getData32(sample);
  return (tmpHi << 32) + tmpLo;
}

static void skipBytes(SFSample *sample, uint32_t skip) {
  int quads = (skip + 3) / 4;
  sample->datap += quads;
  if(skip > sample->rawSampleLen || (u_char *)sample->datap > sample->endp) {
    SFABORT(sample, SF_ABORT_EOS);
  }
}

static uint32_t getString(SFSample *sample, char *buf, uint32_t bufLen) {
  uint32_t len, read_len;
  len = getData32(sample);
  // truncate if too long
  read_len = (len >= bufLen) ? (bufLen - 1) : len;
  memcpy(buf, sample->datap, read_len);
  buf[read_len] = '\0';   // null terminate
  skipBytes(sample, len);
  return len;
}

static uint32_t getAddress(SFSample *sample, SFLAddress *address) {
  address->type = getData32(sample);
  if(address->type == SFLADDRESSTYPE_IP_V4)
    address->address.ip_v4.addr = getData32_nobswap(sample);
  else if (address->type == SFLADDRESSTYPE_IP_V6){
    memcpy(&address->address.ip_v6.addr, sample->datap, 16);
    skipBytes(sample, 16);
  }
  return address->type;
}

static void skipTLVRecord(SFSample *sample, uint32_t tag, uint32_t len, char *description) {
  skipBytes(sample, len);
}

/*_________________---------------------------__________________
  _________________    readExtendedSwitch     __________________
  -----------------___________________________------------------
*/

static void readExtendedSwitch(SFSample *sample)
{
  sample->in_vlan = getData32(sample);
  sample->in_priority = getData32(sample);
  sample->out_vlan = getData32(sample);
  sample->out_priority = getData32(sample);

  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_SWITCH;
}

/*_________________---------------------------__________________
  _________________    readExtendedRouter     __________________
  -----------------___________________________------------------
*/

static void readExtendedRouter(SFSample *sample)
{
  getAddress(sample, &sample->nextHop);
  sample->srcMask = getData32(sample);
  sample->dstMask = getData32(sample);

  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_ROUTER;
}

/*_________________---------------------------__________________
  _________________  readExtendedGateway_v2   __________________
  -----------------___________________________------------------
*/

static void readExtendedGateway_v2(SFSample *sample)
{
  sample->my_as = getData32(sample);
  sample->src_as = getData32(sample);
  sample->src_peer_as = getData32(sample);

  // clear dst_peer_as and dst_as to make sure we are not
  // remembering values from a previous sample - (thanks Marc Lavine)
  sample->dst_peer_as = 0;
  sample->dst_as = 0;

  sample->dst_as_path_len = getData32(sample);
  /* just point at the dst_as_path array */
  if(sample->dst_as_path_len > 0) {
    sample->dst_as_path = sample->datap;
    /* and skip over it in the input */
    skipBytes(sample, sample->dst_as_path_len * 4);
    // fill in the dst and dst_peer fields too
    sample->dst_peer_as = ntohl(sample->dst_as_path[0]);
    sample->dst_as = ntohl(sample->dst_as_path[sample->dst_as_path_len - 1]);
  }

  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_GATEWAY;
}

/*_________________---------------------------__________________
  _________________  readExtendedGateway      __________________
  -----------------___________________________------------------
*/

static void readExtendedGateway(SFSample *sample)
{
  uint32_t segments;
  uint32_t seg;

  if(sample->datagramVersion >= 5) {
    getAddress(sample, &sample->bgp_nextHop);
  }

  sample->my_as = getData32(sample);
  sample->src_as = getData32(sample);
  sample->src_peer_as = getData32(sample);
  segments = getData32(sample);

  // clear dst_peer_as and dst_as to make sure we are not
  // remembering values from a previous sample - (thanks Marc Lavine)
  sample->dst_peer_as = 0;
  sample->dst_as = 0;

  if(segments > 0) {
    for(seg = 0; seg < segments; seg++) {
      uint32_t seg_len;
      uint32_t i;
      skipBytes(sample, 4);
      seg_len = getData32(sample);
      for(i = 0; i < seg_len; i++) {
	uint32_t asNumber;
	asNumber = getData32(sample);
	/* mark the first one as the dst_peer_as */
	if(i == 0 && seg == 0) sample->dst_peer_as = asNumber;
	/* make sure the AS sets are in parentheses */
	/* mark the last one as the dst_as */
	if(seg == (segments - 1) && i == (seg_len - 1)) sample->dst_as = asNumber;
      }
    }
  }
  sample->communities_len = getData32(sample);
  /* just point at the communities array */
  if(sample->communities_len > 0) sample->communities = sample->datap;
  /* and skip over it in the input */
  skipBytes(sample, sample->communities_len * 4);
 
  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_GATEWAY;

  sample->localpref = getData32(sample);
}

/*_________________---------------------------__________________
  _________________    readExtendedUser       __________________
  -----------------___________________________------------------
*/

static void readExtendedUser(SFSample *sample)
{

  if(sample->datagramVersion >= 5) {
    sample->src_user_charset = getData32(sample);
  }

  sample->src_user_len = getString(sample, sample->src_user, SA_MAX_EXTENDED_USER_LEN);

  if(sample->datagramVersion >= 5) {
    sample->dst_user_charset = getData32(sample);
  }

  sample->dst_user_len = getString(sample, sample->dst_user, SA_MAX_EXTENDED_USER_LEN);

  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_USER;
}

/*_________________---------------------------__________________
  _________________    readExtendedUrl        __________________
  -----------------___________________________------------------
*/

static void readExtendedUrl(SFSample *sample)
{
  sample->url_direction = getData32(sample);
  sample->url_len = getString(sample, sample->url, SA_MAX_EXTENDED_URL_LEN);
  if(sample->datagramVersion >= 5) {
    sample->host_len = getString(sample, sample->host, SA_MAX_EXTENDED_HOST_LEN);
  }
  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_URL;
}


/*_________________---------------------------__________________
  _________________       mplsLabelStack      __________________
  -----------------___________________________------------------
*/

static void mplsLabelStack(SFSample *sample, char *fieldName)
{
  SFLLabelStack lstk;
  lstk.depth = getData32(sample);
  /* just point at the lablelstack array */
  if(lstk.depth > 0) lstk.stack = (uint32_t *)sample->datap;
  /* and skip over it in the input */
  skipBytes(sample, lstk.depth * 4);
}

/*_________________---------------------------__________________
  _________________    readExtendedMpls       __________________
  -----------------___________________________------------------
*/

static void readExtendedMpls(SFSample *sample)
{
  getAddress(sample, &sample->mpls_nextHop);
  mplsLabelStack(sample, "mpls_input_stack");
  mplsLabelStack(sample, "mpls_output_stack");

  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_MPLS;
}

/*_________________---------------------------__________________
  _________________    readExtendedNat        __________________
  -----------------___________________________------------------
*/

static void readExtendedNat(SFSample *sample)
{
  getAddress(sample, &sample->nat_src);
  getAddress(sample, &sample->nat_dst);
  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_NAT;
}


/*_________________---------------------------__________________
  _________________    readExtendedMplsTunnel __________________
  -----------------___________________________------------------
*/

static void readExtendedMplsTunnel(SFSample *sample)
{
#define SA_MAX_TUNNELNAME_LEN 100
  getData32(sample);
  getData32(sample);
  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_MPLS_TUNNEL;
}

/*_________________---------------------------__________________
  _________________    readExtendedMplsVC     __________________
  -----------------___________________________------------------
*/

static void readExtendedMplsVC(SFSample *sample)
{
#define SA_MAX_VCNAME_LEN 100
  getData32(sample);
  getData32(sample);
  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_MPLS_VC;
}

/*_________________---------------------------__________________
  _________________    readExtendedMplsFTN    __________________
  -----------------___________________________------------------
*/

static void readExtendedMplsFTN(SFSample *sample)
{
#define SA_MAX_FTN_LEN 100
  getData32(sample);
  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_MPLS_FTN;
}

/*_________________---------------------------__________________
  _________________  readExtendedMplsLDP_FEC  __________________
  -----------------___________________________------------------
*/

static void readExtendedMplsLDP_FEC(SFSample *sample)
{
  getData32(sample);
  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_MPLS_LDP_FEC;
}

/*_________________---------------------------__________________
  _________________  readExtendedVlanTunnel   __________________
  -----------------___________________________------------------
*/

static void readExtendedVlanTunnel(SFSample *sample)
{
  SFLLabelStack lstk;
  lstk.depth = getData32(sample);
  /* just point at the lablelstack array */
  if(lstk.depth > 0) lstk.stack = (uint32_t *)sample->datap;
  /* and skip over it in the input */
  skipBytes(sample, lstk.depth * 4);
  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_VLAN_TUNNEL;
}

/*_________________---------------------------__________________
  _________________  readExtendedWifiPayload  __________________
  -----------------___________________________------------------
*/

static void readExtendedWifiPayload(SFSample *sample)
{
  readFlowSample_header(sample);
}

/*_________________---------------------------__________________
  _________________  readExtendedWifiRx       __________________
  -----------------___________________________------------------
*/

static void readExtendedWifiRx(SFSample *sample)
{
  skipBytes(sample, 6);
}

/*_________________---------------------------__________________
  _________________  readExtendedWifiTx       __________________
  -----------------___________________________------------------
*/

static void readExtendedWifiTx(SFSample *sample)
{
  skipBytes(sample, 6);
}

/*_________________---------------------------__________________
  _________________  readFlowSample_header    __________________
  -----------------___________________________------------------
*/

static void readFlowSample_header(SFSample *sample)
{
  sample->headerProtocol = getData32(sample);
  sample->sampledPacketSize = getData32(sample);
  if(sample->datagramVersion > 4) {
    // stripped count introduced in sFlow version 5
    sample->stripped = getData32(sample);
  }
  sample->headerLen = getData32(sample);

  sample->header = (u_char *)sample->datap; /* just point at the header */
  skipBytes(sample, sample->headerLen);
  {
    char scratch[2000];
    printHex(sample->header, sample->headerLen, (u_char *)scratch, 2000, 0, 2000);
  }

  switch(sample->headerProtocol) {
    /* the header protocol tells us where to jump into the decode */
	  case SFLHEADER_ETHERNET_ISO8023:
		decodeLinkLayer(sample);
		break;
	  case SFLHEADER_IPv4:
		sample->gotIPV4 = YES;
		sample->offsetToIPV4 = 0;
		break;
	  case SFLHEADER_IPv6:
		sample->gotIPV6 = YES;
		sample->offsetToIPV6 = 0;
		break;
	  case SFLHEADER_IEEE80211MAC:
		decode80211MAC(sample);
		break;
	  case SFLHEADER_ISO88024_TOKENBUS:
	  case SFLHEADER_ISO88025_TOKENRING:
	  case SFLHEADER_FDDI:
	  case SFLHEADER_FRAME_RELAY:
	  case SFLHEADER_X25:
	  case SFLHEADER_PPP:
	  case SFLHEADER_SMDS:
	  case SFLHEADER_AAL5:
	  case SFLHEADER_AAL5_IP:
	  case SFLHEADER_MPLS:
	  case SFLHEADER_POS:
	  case SFLHEADER_IEEE80211_AMPDU:
	  case SFLHEADER_IEEE80211_AMSDU_SUBFRAME:
		break;
	  default:
		  // error - undefined header protocol
		  exit(-12);
  }

  if(sample->gotIPV4) {
    // report the size of the original IPPdu (including the IP header)
    decodeIPV4(sample);
  } else if(sample->gotIPV6) {
    // report the size of the original IPPdu (including the IP header)
    decodeIPV6(sample);
  }

}

/*_________________---------------------------__________________
  _________________  readFlowSample_ethernet  __________________
  -----------------___________________________------------------
*/

static void readFlowSample_ethernet(SFSample *sample)
{
  sample->eth_len = getData32(sample);
  memcpy(sample->eth_src, sample->datap, 6);
  skipBytes(sample, 6);
  memcpy(sample->eth_dst, sample->datap, 6);
  skipBytes(sample, 6);
  sample->eth_type = getData32(sample);
}


/*_________________---------------------------__________________
  _________________    readFlowSample_IPv4    __________________
  -----------------___________________________------------------
*/

static void readFlowSample_IPv4(SFSample *sample)
{
  sample->headerLen = sizeof(SFLSampled_ipv4);
  sample->header = (u_char *)sample->datap; /* just point at the header */
  skipBytes(sample, sample->headerLen);
  {
    SFLSampled_ipv4 nfKey;
    memcpy(&nfKey, sample->header, sizeof(nfKey));
    sample->sampledPacketSize = ntohl(nfKey.length);
    sample->ipsrc.type = SFLADDRESSTYPE_IP_V4;
    sample->ipsrc.address.ip_v4 = nfKey.src_ip;
    sample->ipdst.type = SFLADDRESSTYPE_IP_V4;
    sample->ipdst.address.ip_v4 = nfKey.dst_ip;
    sample->dcd_ipProtocol = ntohl(nfKey.protocol);
    sample->dcd_ipTos = ntohl(nfKey.tos);
    sample->dcd_sport = ntohl(nfKey.src_port);
    sample->dcd_dport = ntohl(nfKey.dst_port);
    switch(sample->dcd_ipProtocol) {
    case 1: /* ICMP */
      /* not sure about the dest port being icmp type
	 - might be that src port is icmp type and dest
	 port is icmp code.  Still, have seen some
	 implementations where src port is 0 and dst
	 port is the type, so it may be safer to
	 assume that the destination port has the type */
      break;
    case 6: /* TCP */
      sample->dcd_tcpFlags = ntohl(nfKey.tcp_flags);
      break;
    case 17: /* UDP */
      break;
    default: /* some other protcol */
      break;
    }
  }
}

/*_________________---------------------------__________________
  _________________    readFlowSample_IPv6    __________________
  -----------------___________________________------------------
*/

static void readFlowSample_IPv6(SFSample *sample)
{
  sample->header = (u_char *)sample->datap; /* just point at the header */
  sample->headerLen = sizeof(SFLSampled_ipv6);
  skipBytes(sample, sample->headerLen);
  {
    SFLSampled_ipv6 nfKey6;
    memcpy(&nfKey6, sample->header, sizeof(nfKey6));
    sample->sampledPacketSize = ntohl(nfKey6.length);
    sample->ipsrc.type = SFLADDRESSTYPE_IP_V6;
    memcpy(&sample->ipsrc.address.ip_v6, &nfKey6.src_ip, 16);
    sample->ipdst.type = SFLADDRESSTYPE_IP_V6;
    memcpy(&sample->ipdst.address.ip_v6, &nfKey6.dst_ip, 16);
    sample->dcd_ipProtocol = ntohl(nfKey6.protocol);
    sample->dcd_sport = ntohl(nfKey6.src_port);
    sample->dcd_dport = ntohl(nfKey6.dst_port);
    switch(sample->dcd_ipProtocol) {
    case 1: /* ICMP */
      /* not sure about the dest port being icmp type
	 - might be that src port is icmp type and dest
	 port is icmp code.  Still, have seen some
	 implementations where src port is 0 and dst
	 port is the type, so it may be safer to
	 assume that the destination port has the type */
      break;
    case 6: /* TCP */
      sample->dcd_tcpFlags = ntohl(nfKey6.tcp_flags);
      break;
    case 17: /* UDP */
      break;
    default: /* some other protcol */
      break;
    }
  }
}

/*_________________----------------------------__________________
  _________________  readFlowSample_memcache   __________________
  -----------------____________________________------------------
*/

static void readFlowSample_memcache(SFSample *sample)
{
	char key[SFL_MAX_MEMCACHE_KEY+1];
#define ENC_KEY_BYTES (SFL_MAX_MEMCACHE_KEY * 3) + 1
	skipBytes(sample, 8);
	getString(sample, key, SFL_MAX_MEMCACHE_KEY);
	skipBytes(sample, 16);
}

/*_________________----------------------------__________________
  _________________  readFlowSample_http       __________________
  -----------------____________________________------------------
*/

static void readFlowSample_http(SFSample *sample, uint32_t tag)
{
  char uri[SFL_MAX_HTTP_URI+1];
  char host[SFL_MAX_HTTP_HOST+1];
  char referrer[SFL_MAX_HTTP_REFERRER+1];
  char useragent[SFL_MAX_HTTP_USERAGENT+1];
  char xff[SFL_MAX_HTTP_XFF+1];
  char authuser[SFL_MAX_HTTP_AUTHUSER+1];
  char mimetype[SFL_MAX_HTTP_MIMETYPE+1];
  uint32_t method;
  uint32_t protocol;
  uint32_t status;
  uint64_t resp_bytes;

  method = getData32(sample);
  protocol = getData32(sample);

  getString(sample, uri, SFL_MAX_HTTP_URI);
  getString(sample, host, SFL_MAX_HTTP_HOST);
  getString(sample, referrer, SFL_MAX_HTTP_REFERRER);
  getString(sample, useragent, SFL_MAX_HTTP_USERAGENT);
  if(tag == SFLFLOW_HTTP2) {
	getString(sample, xff, SFL_MAX_HTTP_XFF);
  }
  getString(sample, authuser, SFL_MAX_HTTP_AUTHUSER);
  getString(sample, mimetype, SFL_MAX_HTTP_MIMETYPE);
  if(tag == SFLFLOW_HTTP2) {
	  skipBytes(sample, 8);
  }
  resp_bytes = getData64(sample);
  skipBytes(sample, 4);
  status = getData32(sample);

  if(sfConfig.outputFormat == SFLFMT_CLF) {
	time_t now = time(NULL);
	char nowstr[200];
	strftime(nowstr, 200, "%d/%b/%Y:%H:%M:%S %z", localtime(&now));
	snprintf(sfCLF.http_log, SFLFMT_CLF_MAX_LINE, "- %s [%s] \"%s %s HTTP/%u.%u\" %u %"PRIu64" \"%s\" \"%s\"",
		 authuser[0] ? authuser : "-",
		 nowstr,
		 SFHTTP_method_names[method],
		 uri[0] ? uri : "-",
		 protocol / 1000,
		 protocol % 1000,
		 status,
		 (long long unsigned int) resp_bytes,
		 referrer[0] ? referrer : "-",
		 useragent[0] ? useragent : "-");
	sfCLF.valid = YES;
  }
}


/*_________________----------------------------__________________
  _________________  readFlowSample_APP        __________________
  -----------------____________________________------------------
*/

static void readFlowSample_APP(SFSample *sample)
{
  char application[SFLAPP_MAX_APPLICATION_LEN];
  char operation[SFLAPP_MAX_OPERATION_LEN];
  char attributes[SFLAPP_MAX_ATTRIBUTES_LEN];
  char status[SFLAPP_MAX_STATUS_LEN];
  getString(sample, application, SFLAPP_MAX_APPLICATION_LEN);
  getString(sample, operation, SFLAPP_MAX_OPERATION_LEN);
  getString(sample, attributes, SFLAPP_MAX_ATTRIBUTES_LEN);
  getString(sample, status, SFLAPP_MAX_STATUS_LEN);
  skipBytes(sample, 20);
}


/*_________________----------------------------__________________
  _________________  readFlowSample_APP_CTXT   __________________
  -----------------____________________________------------------
*/

static void readFlowSample_APP_CTXT(SFSample *sample)
{
  char application[SFLAPP_MAX_APPLICATION_LEN];
  char operation[SFLAPP_MAX_OPERATION_LEN];
  char attributes[SFLAPP_MAX_ATTRIBUTES_LEN];
  getString(sample, application, SFLAPP_MAX_APPLICATION_LEN);
  getString(sample, operation, SFLAPP_MAX_OPERATION_LEN);
  getString(sample, attributes, SFLAPP_MAX_ATTRIBUTES_LEN);
}

/*_________________---------------------------------__________________
  _________________  readFlowSample_APP_ACTOR_INIT  __________________
  -----------------_________________________________------------------
*/

static void readFlowSample_APP_ACTOR_INIT(SFSample *sample)
{
  char actor[SFLAPP_MAX_ACTOR_LEN];
  getString(sample, actor, SFLAPP_MAX_ACTOR_LEN);
}

/*_________________---------------------------------__________________
  _________________  readFlowSample_APP_ACTOR_TGT   __________________
  -----------------_________________________________------------------
*/

static void readFlowSample_APP_ACTOR_TGT(SFSample *sample)
{
  char actor[SFLAPP_MAX_ACTOR_LEN];
  getString(sample, actor, SFLAPP_MAX_ACTOR_LEN);
}


/*_________________----------------------------__________________
  _________________   readExtendedSocket4      __________________
  -----------------____________________________------------------
*/

static void readExtendedSocket4(SFSample *sample)
{
  char buf[51];
  skipBytes(sample, 4);
  sample->ipsrc.type = SFLADDRESSTYPE_IP_V4;
  sample->ipsrc.address.ip_v4.addr = getData32_nobswap(sample);
  sample->ipdst.type = SFLADDRESSTYPE_IP_V4;
  sample->ipdst.address.ip_v4.addr = getData32_nobswap(sample);
  skipBytes(sample, 8);
  if(sfConfig.outputFormat == SFLFMT_CLF) {
    memcpy(sfCLF.client, buf, 50);
    sfCLF.client[50] = '\0';
  }

}

/*_________________----------------------------__________________
  _________________ readExtendedProxySocket4   __________________
  -----------------____________________________------------------
*/

static void readExtendedProxySocket4(SFSample *sample)
{
  skipBytes(sample, 20);
}

/*_________________----------------------------__________________
  _________________  readExtendedSocket6       __________________
  -----------------____________________________------------------
*/

static void readExtendedSocket6(SFSample *sample)
{
  char buf[51];
  skipBytes(sample, 4);
  sample->ipsrc.type = SFLADDRESSTYPE_IP_V6;
  memcpy(&sample->ipsrc.address.ip_v6, sample->datap, 16);
  skipBytes(sample, 16);
  sample->ipdst.type = SFLADDRESSTYPE_IP_V6;
  memcpy(&sample->ipdst.address.ip_v6, sample->datap, 16);
  skipBytes(sample, 24);

  if(sfConfig.outputFormat == SFLFMT_CLF) {
    memcpy(sfCLF.client, buf, 51);
    sfCLF.client[50] = '\0';
  }
}

/*_________________----------------------------__________________
  _________________ readExtendedProxySocket6   __________________
  -----------------____________________________------------------
*/

static void readExtendedProxySocket6(SFSample *sample)
{
  SFLAddress ipsrc, ipdst;
  skipBytes(sample, 4);
  ipsrc.type = SFLADDRESSTYPE_IP_V6;
  memcpy(&ipsrc.address.ip_v6, sample->datap, 16);
  skipBytes(sample, 16);
  ipdst.type = SFLADDRESSTYPE_IP_V6;
  memcpy(&ipdst.address.ip_v6, sample->datap, 16);
  skipBytes(sample, 24);
}

/*_________________---------------------------__________________
  _________________    readFlowSample_v2v4    __________________
  -----------------___________________________------------------
*/

static void readFlowSample_v2v4(SFSample *sample, char *packet)
{
  sample->samplesGenerated = getData32(sample);
  {
    uint32_t samplerId = getData32(sample);
    sample->ds_class = samplerId >> 24;
    sample->ds_index = samplerId & 0x00ffffff;
  }

  sample->meanSkipCount = getData32(sample);
  sample->samplePool = getData32(sample);
  sample->dropEvents = getData32(sample);
  sample->inputPort = getData32(sample);
  sample->outputPort = getData32(sample);
  sample->packet_data_tag = getData32(sample);

  switch(sample->packet_data_tag) {

  case INMPACKETTYPE_HEADER: readFlowSample_header(sample); break;
  case INMPACKETTYPE_IPV4:
    sample->gotIPV4Struct = YES;
    readFlowSample_IPv4(sample);
    break;
  case INMPACKETTYPE_IPV6:
    sample->gotIPV6Struct = YES;
    readFlowSample_IPv6(sample);
    break;
  default: break;
  }

  sample->extended_data_tag = 0;
  {
    uint32_t x;
    sample->num_extended = getData32(sample);
    for(x = 0; x < sample->num_extended; x++) {
      uint32_t extended_tag;
      extended_tag = getData32(sample);
      switch(extended_tag) {
      case INMEXTENDED_SWITCH: readExtendedSwitch(sample); break;
      case INMEXTENDED_ROUTER: readExtendedRouter(sample); break;
      case INMEXTENDED_GATEWAY:
	if(sample->datagramVersion == 2) readExtendedGateway_v2(sample);
	else readExtendedGateway(sample);
	break;
      case INMEXTENDED_USER: readExtendedUser(sample); break;
      case INMEXTENDED_URL: readExtendedUrl(sample); break;
      default: break;
      }
    }
  }

  if(sampleFilterOK(sample)) {
	  if(sfConfig.netFlowOutputSocket && (sample->gotIPV4 || sample->gotIPV4Struct)) sendNetFlowDatagram(sample, packet);
  }
}

/*_________________---------------------------__________________
  _________________    readFlowSample         __________________
  -----------------___________________________------------------
*/

static void readFlowSample(SFSample *sample, int expanded, char *packet)
{
  uint32_t num_elements, sampleLength;
  u_char *sampleStart;
  
  sampleLength = getData32(sample);
  sampleStart = (u_char *)sample->datap;
  
   if((u_char *)sample->datap + sampleLength > sample->endp) {
    SFABORT(sample, SF_ABORT_EOS);
  }
  
  sample->samplesGenerated = getData32(sample);
  if(expanded) {
    sample->ds_class = getData32(sample);
    sample->ds_index = getData32(sample);
  }
  else {
    uint32_t samplerId = getData32(sample);
    sample->ds_class = samplerId >> 24;
    sample->ds_index = samplerId & 0x00ffffff;
  }

  sample->meanSkipCount = getData32(sample);
  sample->samplePool = getData32(sample);
  sample->dropEvents = getData32(sample);
  if(expanded) {
    sample->inputPortFormat = getData32(sample);
    sample->inputPort = getData32(sample);
    sample->outputPortFormat = getData32(sample);
    sample->outputPort = getData32(sample);
  }
  else {
    uint32_t inp, outp;
    inp = getData32(sample);
    outp = getData32(sample);
    sample->inputPortFormat = inp >> 30;
    sample->outputPortFormat = outp >> 30;
    sample->inputPort = inp & 0x3fffffff;
    sample->outputPort = outp & 0x3fffffff;
  }

  // clear the CLF record
  sfCLF.valid = NO;
  sfCLF.client[0] = '\0';

  num_elements = getData32(sample);
  {
    uint32_t el;
    for(el = 0; el < num_elements; el++) {
      uint32_t tag, length;
      u_char *start;
      tag = getData32(sample);
      length = getData32(sample);
      start = (u_char *)sample->datap;

      switch(tag) {
		case SFLFLOW_HEADER:     readFlowSample_header(sample); break;
		case SFLFLOW_ETHERNET:   readFlowSample_ethernet(sample); break;
		case SFLFLOW_IPV4:       readFlowSample_IPv4(sample); break;
		case SFLFLOW_IPV6:       readFlowSample_IPv6(sample); break;
		case SFLFLOW_MEMCACHE:   readFlowSample_memcache(sample); break;
		case SFLFLOW_HTTP:       readFlowSample_http(sample, tag); break;
		case SFLFLOW_HTTP2:      readFlowSample_http(sample, tag); break;
		case SFLFLOW_APP:        readFlowSample_APP(sample); break;
		case SFLFLOW_APP_CTXT:   readFlowSample_APP_CTXT(sample); break;
		case SFLFLOW_APP_ACTOR_INIT: readFlowSample_APP_ACTOR_INIT(sample); break;
		case SFLFLOW_APP_ACTOR_TGT: readFlowSample_APP_ACTOR_TGT(sample); break;
		case SFLFLOW_EX_SWITCH:  readExtendedSwitch(sample); break;
		case SFLFLOW_EX_ROUTER:  readExtendedRouter(sample); break;
		case SFLFLOW_EX_GATEWAY: readExtendedGateway(sample); break;
		case SFLFLOW_EX_USER:    readExtendedUser(sample); break;
		case SFLFLOW_EX_URL:     readExtendedUrl(sample); break;
		case SFLFLOW_EX_MPLS:    readExtendedMpls(sample); break;
		case SFLFLOW_EX_NAT:     readExtendedNat(sample); break;
		case SFLFLOW_EX_MPLS_TUNNEL:  readExtendedMplsTunnel(sample); break;
		case SFLFLOW_EX_MPLS_VC:      readExtendedMplsVC(sample); break;
		case SFLFLOW_EX_MPLS_FTN:     readExtendedMplsFTN(sample); break;
		case SFLFLOW_EX_MPLS_LDP_FEC: readExtendedMplsLDP_FEC(sample); break;
		case SFLFLOW_EX_VLAN_TUNNEL:  readExtendedVlanTunnel(sample); break;
		case SFLFLOW_EX_80211_PAYLOAD: readExtendedWifiPayload(sample); break;
		case SFLFLOW_EX_80211_RX: readExtendedWifiRx(sample); break;
		case SFLFLOW_EX_80211_TX: readExtendedWifiTx(sample); break;
	/* case SFLFLOW_EX_AGGREGATION: readExtendedAggregation(sample); break; */
		case SFLFLOW_EX_SOCKET4: readExtendedSocket4(sample); break;
		case SFLFLOW_EX_SOCKET6: readExtendedSocket6(sample); break;
		case SFLFLOW_EX_PROXYSOCKET4: readExtendedProxySocket4(sample); break;
		case SFLFLOW_EX_PROXYSOCKET6: readExtendedProxySocket6(sample); break;
		default: skipTLVRecord(sample, tag, length, "flow_sample_element"); break;
		}
		lengthCheck(sample, "flow_sample_element", start, length);
	  }
	}
  lengthCheck(sample, "flow_sample", sampleStart, sampleLength);

  if(sampleFilterOK(sample)) {
	  if (sample->gotIPV4) {
		  sendNetFlowDatagram(sample, packet);
	  }
  }
}

/*_________________---------------------------__________________
  _________________      readSFlowDatagram    __________________
  -----------------___________________________------------------
*/

static void readSFlowDatagram(SFSample *sample, char *packet)
{
  uint32_t samplesInPacket;

  /* check the version */
  sample->datagramVersion = getData32(sample);
  if(sample->datagramVersion != 2 &&
     sample->datagramVersion != 4 &&
     sample->datagramVersion != 5) {
	 return;
  }

  /* get the agent address */
  uint32_t addr_type = getAddress(sample, &sample->agent_addr);
  if (addr_type != SFLADDRESSTYPE_IP_V4 && addr_type != SFLADDRESSTYPE_IP_V6) {
	  SFABORT(sample, SF_ABORT_EOS);
  }

  /* version 5 has an agent sub-id as well */
  if(sample->datagramVersion >= 5) {
    sample->agentSubId = getData32(sample);
  }

  sample->sequenceNo = getData32(sample);  /* this is the packet sequence number */
  sample->sysUpTime = getData32(sample);
  samplesInPacket = getData32(sample);

  /* now iterate and pull out the flows and counters samples */
  {
    uint32_t samp = 0;
    for(; samp < samplesInPacket; samp++) {
      if((u_char *)sample->datap >= sample->endp) {
    	  SFABORT(sample, SF_ABORT_EOS);
      }

      // just read the tag, then call the approriate decode fn
      sample->sampleType = getData32(sample);
      if(sample->datagramVersion >= 5) {
    	  switch(sample->sampleType) {
    	  	  case SFLFLOW_SAMPLE: readFlowSample(sample, NO, packet); break;
    	  	  case SFLFLOW_SAMPLE_EXPANDED: readFlowSample(sample, YES, packet); break;
    	  	  default: skipBytes(sample, getData32(sample)); break;
    	  }
      } else {
    	  switch(sample->sampleType) {
    	  	  case FLOWSAMPLE: readFlowSample_v2v4(sample, packet); break;
    	  	  case COUNTERSSAMPLE: skipBytes(sample, getData32(sample)); break;
    	  	  default: break;
    	  }
      }
    }
  }
}

uint16_t Process_sflow(void *packet, ssize_t packet_len) {

SFSample 	sample;
int 		exceptionVal;

	memset(&sample, 0, sizeof(sample));

	if ((sample.rawSample = malloc(packet_len)) == NULL) {
		printf("Warning: sFlow rawSample malloc() failed");
		sample.rawSample = packet;
	} else {
		memcpy(sample.rawSample, packet, packet_len);
	}
	sample.rawSampleLen = packet_len;

	numOfFlowSamples = 0;
	if((exceptionVal = setjmp(sample.env)) == 0)	{
		// TRY
		sample.datap = (uint32_t *)sample.rawSample;
		sample.endp = (u_char *)sample.rawSample + sample.rawSampleLen;
		readSFlowDatagram(&sample, packet);
	}
	if (sample.rawSample != packet) {
		free(sample.rawSample);
	}
	return numOfFlowSamples;
} // End of Process_sflow
