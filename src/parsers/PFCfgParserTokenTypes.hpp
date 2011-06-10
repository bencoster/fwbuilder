#ifndef INC_PFCfgParserTokenTypes_hpp_
#define INC_PFCfgParserTokenTypes_hpp_

/* $ANTLR 2.7.7 (20100319): "pf.g" -> "PFCfgParserTokenTypes.hpp"$ */

#ifndef CUSTOM_API
# define CUSTOM_API
#endif

#ifdef __cplusplus
struct CUSTOM_API PFCfgParserTokenTypes {
#endif
	enum {
		EOF_ = 1,
		NEWLINE = 4,
		LINE_COMMENT = 5,
		INCLUDE_COMMAND = 6,
		WORD = 7,
		EQUAL = 8,
		ANTISPOOF = 9,
		ALTQ = 10,
		QUEUE = 11,
		SET = 12,
		TIMEOUT = 13,
		// "ruleset-optimization" = 14
		LITERAL_optimization = 15,
		LITERAL_aggressive = 16,
		LITERAL_conservative = 17,
		// "high-latency" = 18
		LITERAL_normal = 19,
		LITERAL_satellite = 20,
		LITERAL_limit = 21,
		LITERAL_loginterface = 22,
		// "block-policy" = 23
		DROP = 24,
		RETURN = 25,
		// "state-policy" = 26
		// "if-bound" = 27
		LITERAL_floating = 28,
		// "state-defaults" = 29
		// "require-order" = 30
		LITERAL_fingerprints = 31,
		LITERAL_skip = 32,
		ON = 33,
		OPENING_BRACE = 34,
		COMMA = 35,
		CLOSING_BRACE = 36,
		LITERAL_debug = 37,
		LITERAL_reassemble = 38,
		LITERAL_hostid = 39,
		// "tcp.first" = 40
		// "tcp.opening" = 41
		// "tcp.established" = 42
		// "tcp.closing" = 43
		// "tcp.finwait" = 44
		// "tcp.closed" = 45
		// "udp.first" = 46
		// "udp.single" = 47
		// "udp.multiple" = 48
		// "icmp.first" = 49
		// "icmp.error" = 50
		// "other.first" = 51
		// "other.single" = 52
		// "other.multiple" = 53
		LITERAL_frag = 54,
		LITERAL_interval = 55,
		// "src.track" = 56
		// "adaptive.start" = 57
		// "adaptive.end" = 58
		INT_CONST = 59,
		LITERAL_frags = 60,
		LITERAL_states = 61,
		// "src-nodes" = 62
		LITERAL_tables = 63,
		// "tables-entries" = 64
		SCRUB = 65,
		MATCH = 66,
		TABLE = 67,
		LESS_THAN = 68,
		GREATER_THAN = 69,
		PERSIST = 70,
		CONST_WORD = 71,
		COUNTERS = 72,
		FILE = 73,
		STRING = 74,
		EXLAMATION = 75,
		COLON = 76,
		NETWORK = 77,
		BROADCAST = 78,
		PEER = 79,
		SELF = 80,
		IPV4 = 81,
		NUMBER = 82,
		SLASH = 83,
		NO = 84,
		NAT = 85,
		PASS = 86,
		MINUS = 87,
		STATIC_PORT = 88,
		RDR = 89,
		OPENING_PAREN = 90,
		CLOSING_PAREN = 91,
		PORT = 92,
		IPV6 = 93,
		STAR = 94,
		BITMASK = 95,
		RANDOM = 96,
		SOURCE_HASH = 97,
		HEX_KEY = 98,
		STRING_KEY = 99,
		ROUND_ROBIN = 100,
		STICKY_ADDRESS = 101,
		BINAT = 102,
		BLOCK = 103,
		RETURN_RST = 104,
		TTL = 105,
		RETURN_ICMP = 106,
		RETURN_ICMP6 = 107,
		IN_WORD = 108,
		OUT_WORD = 109,
		LOG = 110,
		ALL = 111,
		USER = 112,
		TO = 113,
		QUICK = 114,
		INET = 115,
		INET6 = 116,
		PROTO = 117,
		IP = 118,
		ICMP = 119,
		IGMP = 120,
		TCP = 121,
		UDP = 122,
		RDP = 123,
		RSVP = 124,
		GRE = 125,
		ESP = 126,
		AH = 127,
		EIGRP = 128,
		OSPF = 129,
		IPIP = 130,
		VRRP = 131,
		L2TP = 132,
		ISIS = 133,
		FROM = 134,
		URPF_FAILED = 135,
		ANY = 136,
		NO_ROUTE = 137,
		ROUTE_TO = 138,
		REPLY_TO = 139,
		GROUP = 140,
		LITERAL_fragment = 141,
		LITERAL_crop = 142,
		// "drop-ovl" = 143
		// "no-df" = 144
		// "min-ttl" = 145
		// "max-mss" = 146
		// "random-id" = 147
		FLAGS = 148,
		ICMP_TYPE = 149,
		ICMP_CODE = 150,
		LITERAL_echorep = 151,
		LITERAL_unreach = 152,
		LITERAL_squench = 153,
		LITERAL_redir = 154,
		LITERAL_althost = 155,
		LITERAL_echoreq = 156,
		LITERAL_routeradv = 157,
		LITERAL_routersol = 158,
		LITERAL_timex = 159,
		LITERAL_paramprob = 160,
		LITERAL_timereq = 161,
		LITERAL_timerep = 162,
		LITERAL_inforeq = 163,
		LITERAL_inforep = 164,
		LITERAL_maskreq = 165,
		LITERAL_maskrep = 166,
		LITERAL_trace = 167,
		LITERAL_dataconv = 168,
		LITERAL_mobredir = 169,
		// "ipv6-where" = 170
		// "ipv6-here" = 171
		LITERAL_mobregreq = 172,
		LITERAL_mobregrep = 173,
		LITERAL_photuris = 174,
		// "net-unr" = 175
		// "host-unr" = 176
		// "proto-unr" = 177
		// "port-unr" = 178
		LITERAL_needfrag = 179,
		LITERAL_srcfail = 180,
		// "net-unk" = 181
		// "host-unk" = 182
		LITERAL_isolate = 183,
		// "net-prohib" = 184
		// "host-prohib" = 185
		// "net-tos" = 186
		// "host-tos" = 187
		// "filter-prohib" = 188
		// "host-preced" = 189
		// "cutoff-preced" = 190
		// "redir-net" = 191
		// "redir-host" = 192
		// "redir-tos-net" = 193
		// "redir-tos-host" = 194
		// "normal-adv" = 195
		// "common-adv" = 196
		LITERAL_transit = 197,
		LITERAL_reassemb = 198,
		LITERAL_badhead = 199,
		LITERAL_optmiss = 200,
		LITERAL_badlen = 201,
		// "unknown-ind" = 202
		// "auth-fail" = 203
		// "decrypt-fail" = 204
		ICMP6_TYPE = 205,
		TAGGED = 206,
		TAG = 207,
		KEEP = 208,
		MODULATE = 209,
		SYNPROXY = 210,
		STATE = 211,
		LABEL = 212,
		EXIT = 213,
		QUIT = 214,
		INTRFACE = 215,
		ICMP6 = 216,
		IGRP = 217,
		IPSEC = 218,
		NOS = 219,
		PCP = 220,
		PIM = 221,
		PPTP = 222,
		RIP = 223,
		SNP = 224,
		HOST = 225,
		RANGE = 226,
		LOG_LEVEL_ALERTS = 227,
		LOG_LEVEL_CRITICAL = 228,
		LOG_LEVEL_DEBUGGING = 229,
		LOG_LEVEL_EMERGENCIES = 230,
		LOG_LEVEL_ERRORS = 231,
		LOG_LEVEL_INFORMATIONAL = 232,
		LOG_LEVEL_NOTIFICATIONS = 233,
		LOG_LEVEL_WARNINGS = 234,
		LOG_LEVEL_DISABLE = 235,
		LOG_LEVEL_INACTIVE = 236,
		Whitespace = 237,
		HEX_CONST = 238,
		NEG_INT_CONST = 239,
		HEX_DIGIT = 240,
		DIGIT = 241,
		NUM_3DIGIT = 242,
		NUM_HEX_4DIGIT = 243,
		NUMBER_ADDRESS_OR_WORD = 244,
		PIPE_CHAR = 245,
		NUMBER_SIGN = 246,
		PERCENT = 247,
		AMPERSAND = 248,
		APOSTROPHE = 249,
		PLUS = 250,
		DOT = 251,
		SEMICOLON = 252,
		QUESTION = 253,
		COMMERCIAL_AT = 254,
		OPENING_SQUARE = 255,
		CLOSING_SQUARE = 256,
		CARET = 257,
		UNDERLINE = 258,
		TILDE = 259,
		DOUBLE_QUOTE = 260,
		NULL_TREE_LOOKAHEAD = 3
	};
#ifdef __cplusplus
};
#endif
#endif /*INC_PFCfgParserTokenTypes_hpp_*/