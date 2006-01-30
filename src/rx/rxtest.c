/*
 * Simple RX test program
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include "rx.h"
#include "rx_null.h"
#include "rx_globals.h"

#include <rx/rxkad.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>

static void usage(char *);
static void do_client(struct sockaddr *);
static void *client_thread_send(void *);
static void do_server(unsigned short);
static int32_t rxtest_ExecuteRequest(struct rx_call *call);

extern char *optarg;
extern int optind;

#define DEFAULT_WRITE_SIZE 3072
#define DEFAULT_DATA_SIZE 3072*32768

static int disable_flow_control = 0;
static int send_direction = 0;
static int frame_size = 0;
static int data_size = DEFAULT_DATA_SIZE;
static int write_size = DEFAULT_WRITE_SIZE;
static int tcp_send_size = 0;
static int tcp_recv_size = 0;
static int rx_window_size = 0;
static int number_threads = 1;
static int do_auth = 0;
static char princ_name[1024];
static char cell_name[1024];
unsigned char *databuf;
struct thread_info {
	pthread_t tid;
	struct rx_connection *conn;
	int totalbytes;
	time_t elapsed;
};

#define RX_SERVER_ID 987
int
main(int argc, char *argv[])
{
	int err, c;

	if (argc < 3) {
		usage(argv[0]);
	}

	princ_name[0] = '\0';
	cell_name[0] = '\0';

	while ((c = getopt(argc, argv, "aAcC:f:m:n:p:R:sS:w:x:")) != EOF)
		switch (c) {
		case 'a':
			disable_flow_control++;
			break;
		case 'A':
			do_auth++;
			break;
		case 'c':
			if (send_direction == -1) {
				fprintf(stderr, "%s: Cannot set both -c and "
						"-s\n", argv[0]);
				exit(1);
			}
			send_direction = 1;
			break;
		case 'C':
			strncpy(cell_name, optarg, sizeof(cell_name));
			cell_name[sizeof(cell_name) - 1] = '\0';
			break;
		case 'f':
			frame_size = atoi(optarg);
			break;
		case 'm':
			number_threads = atoi(optarg);
			break;
		case 'n':
			data_size = atoi(optarg);
			break;
		case 'p':
			strncpy(princ_name, optarg, sizeof(princ_name));
			princ_name[sizeof(princ_name) - 1] = '\0';
			break;
		case 'R':
			tcp_recv_size = atoi(optarg);
			break;
		case 's':
			if (send_direction == 1) {
				fprintf(stderr, "%s: Cannot set both -c and "
						"-s\n", argv[0]);
				exit(1);
			}
			send_direction = -1;
			break;
		case 'S':
			tcp_send_size = atoi(optarg);
			break;
		case 'w':
			write_size = atoi(optarg);
			break;
		case 'x':
			rx_window_size = atoi(optarg);
			break;
		case '?':
		default:
			usage(argv[0]);
		}
			

	if (strcmp(argv[optind], "client") == 0) {
		struct addrinfo ai, *res;

		if (argc - optind != 3) {
			usage(argv[0]);
		}

		memset((void *) &ai, 0, sizeof(ai));

		ai.ai_family = AF_INET;
		ai.ai_socktype = SOCK_STREAM; /* XXX */

		err = getaddrinfo(argv[optind + 1], argv[optind + 2],
				  &ai, &res);

		if (err) {
			fprintf(stderr, "Error: %s\n", gai_strerror(err));
			exit(1);
		}

		do_client(res->ai_addr);

		freeaddrinfo(res);
	} else if (strcmp(argv[optind], "server") == 0) {
		struct addrinfo ai, *res;

		if (argc - optind != 2) {
			usage(argv[0]);
		}

		memset((void *) &ai, 0, sizeof(ai));

		ai.ai_family = AF_INET;
		ai.ai_flags = AI_PASSIVE;
		ai.ai_socktype = SOCK_STREAM; /* XXX */

		err = getaddrinfo(NULL, argv[optind + 1], &ai, &res);

		if (err) {
			fprintf(stderr, "Error: %s\n", gai_strerror(err));
			exit(1);
		}

		do_server(((struct sockaddr_in *) res->ai_addr)->sin_port);

		freeaddrinfo(res);
	} else {
		usage(argv[0]);
	}

	exit(0);
}

static void
usage(char *argv0)
{
	fprintf(stderr, "Usage: %s client [options] host port\n"
			"       %s server port\n", argv0, argv0);
	fprintf(stderr, "\nOptions:\n\n");
	fprintf(stderr, "\t-c\tTransfer data from client to server "
		"(default)\n");
	fprintf(stderr, "\t-s\tTransfer data from server to client\n");
	fprintf(stderr, "\t-w N\tUse a write size of N (default: %d)\n",
		DEFAULT_WRITE_SIZE);
	fprintf(stderr, "\t-n N\tTransfer N bytes (default: %d)\n",
		DEFAULT_DATA_SIZE);
	fprintf(stderr, "\t-R N\tSet TCP receive buffer to N (default "
		"set by operating system)\n");
	fprintf(stderr, "\t-S N\tSet TCP send buffer to N (default "
		"set by operating system)\n");
	fprintf(stderr, "\t-f N\tUse a frame size of N\n");
	fprintf(stderr, "\t-a\tDisable flow control\n");
	fprintf(stderr, "\t-m N\tCreate N simultaneous threads\n");
	fprintf(stderr, "\t-x N\tSet RxTCP window size to N\n");
	fprintf(stderr, "\t-A\tPerform authentication\n");
	fprintf(stderr, "\t-p\tPrincipal to use for authentication\n");
	exit(1);
}

static void
do_client(struct sockaddr *s)
{
	struct sockaddr_in *sin = (struct sockaddr_in *) s;
	struct rx_connection *conn;
	struct rx_call *call;
	struct rx_securityClass *secureobj;
	int ret, secureindex = 0, i, totalbytes, code;
	struct timeval stp, etp;
	double seconds;
	unsigned char c = 0;

	ret = rx_Init(0);

	if (ret) {
		fprintf(stderr, "rx_Init failed\n");
		exit(1);
	}
	if (do_auth) {
		struct ktc_principal sname;
		struct ktc_token ttoken;
		struct afsconf_dir *tdir;
		struct afsconf_cell info;
		afs_int32 code;

		afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);

		if (!tdir) {
			fprintf(stderr, "Unable to open cell database\n");
			exit(1);
		}

		code = afsconf_GetCellInfo(tdir, cell_name[0] != '\0' ?
						 cell_name : NULL,
					   NULL, &info);

		if (code) {
			fprintf(stderr, "Unable to open cell database (%d)\n",
				code);
			exit(1);
		}

		if (princ_name[0] == '\0') {
			strcpy(sname.name, "afs");
			sname.instance[0] = '\0';
		} else {
			char *p = strchr(princ_name, '.');

			if (p != NULL)
				p = '\0';

			strncpy(sname.name, princ_name, sizeof(sname.name));
			sname.name[sizeof(sname.name) - 1] = '\0';

			if (p != NULL &&
			    p - princ_name < sizeof(sname.name) - 1) {
				strncpy(sname.instance, p + 1,
					sizeof(sname.instance));
				sname.instance[sizeof(sname.instance) - 1] = '\0';
			} else
				sname.instance[0] = '\0';
		}
	} else 
		secureobj = rxnull_NewClientSecurityObject();

	databuf = malloc(write_size);
	for (i = 0; i < write_size; i++) {
		databuf[i] = c++;
	}

	if (frame_size)
		rx_TcpSetFrameSize(frame_size);

	if (disable_flow_control) {
		printf("Disabling flow control\n");
		rx_TcpSetFlowControl(0);
	}

	if (tcp_send_size)
		rx_TcpSetSendBufsize(tcp_send_size);

	if (tcp_recv_size)
		rx_TcpSetRecvBufsize(tcp_recv_size);

	if (rx_window_size)
		rx_TcpSetWindowSize(rx_window_size);

	conn = rx_NewConnection(sin->sin_addr.s_addr, ntohs(sin->sin_port),
				RX_SERVER_ID, secureobj, secureindex);

	if (!conn) {
		fprintf(stderr, "Failed to contact server\n");
		exit(1);
	}

	if (number_threads > 1) {
		struct thread_info *ti_array;
		int errnum;
		double aggregate_rate = 0;

		ti_array = (struct thread_info *)
			   malloc(sizeof(struct thread_info) * number_threads);

		if (ti_array == NULL) {
			fprintf(stderr, "Unable to allocate thread array!\n");
			exit(1);
		}

		for (i = 0; i < number_threads; i++) {
			ti_array[i].conn = conn;
			errnum = pthread_create(&(ti_array[i].tid), NULL, 
						client_thread_send,
						&ti_array[i]);

			if (errnum) {
				fprintf(stderr, "pthread_create failed: %s\n",
					strerror(errnum));
				exit(1);
			}
		}

		for (i = 0; i < number_threads; i++)
			pthread_join(ti_array[i].tid, NULL);

		for (i = 0; i < number_threads; i++) {
			printf("Stats for thread %d:\n", i);
			printf("\t%d bytes, %.3f MBytes/second (%.3f Mbps)\n",
			       ti_array[i].totalbytes,
			       ti_array[i].totalbytes / ti_array[i].elapsed /
								1.0E6,
			       ti_array[i].totalbytes / ti_array[i].elapsed /
								1.0E6 * 8);
			aggregate_rate += ti_array[i].totalbytes /
							ti_array[i].elapsed;
		}

		printf("\nAggregate rate: %.3f Mbytes/second (%.3f Mbits)\n",
		       aggregate_rate / 1.0E6, aggregate_rate / 1.0E6 * 8);

	} else {

		call = rx_NewCall(conn);

		if (gettimeofday(&stp, NULL) < 0) {
			fprintf(stderr, "gettimeofday failed: %s\n",
				strerror(errno));
			exit(1);
		}

		for (i = 0, totalbytes = 0;
		     totalbytes + write_size < data_size; i++) {
			
			ret = rx_Write(call, (char *) databuf, write_size);

			if (ret != write_size) {
				fprintf(stderr, "Short write (%d)\n", ret);
				exit(1);
			}
			totalbytes += write_size;
		}

		ret = rx_Write(call, (char *) databuf, data_size - totalbytes);

		totalbytes += data_size - totalbytes;

		code = rx_EndCall(call, 0);

		if (gettimeofday(&etp, NULL) < 0) {
			fprintf(stderr, "gettimeofday() failed: %s\n",
				strerror(errno));
			exit(1);
		}

		seconds = etp.tv_sec - stp.tv_sec;

		if (etp.tv_usec >= stp.tv_usec) {
			seconds += (etp.tv_usec - stp.tv_usec) / 1.0E6;
		} else {
			seconds += -1.0 + (etp.tv_usec - stp.tv_usec + 1E6)
								/ 1.0E6;
		}

		printf("%d bytes, %.3f MBytes/sec (%.3f Mbps)\n", totalbytes,
		       totalbytes / seconds / 1.0E6,
		       totalbytes / seconds / 1.0E6 * 8);

		printf("RPC returned %d\n", code);

	}

	printf("Statistics:\n");
	printf("\tData packets sent: %d\n", rxi_tcp_data_packets_sent);
	printf("\tSEQ packets sent: %d\n", rxi_tcp_ack_packets_sent);
	printf("\tData packets received: %d\n", rxi_tcp_data_packets_received);
	printf("\tSEQ packets received: %d\n", rxi_tcp_ack_packets_received);
	printf("\tNumber of times transmit window closed: %d\n",
	       rxi_tcp_transmit_window_closed);
	return;
}

static void *
client_thread_send(void *arg)
{
	struct thread_info *ti = (struct thread_info *) arg;
	struct rx_call *call;
	int i, ret, code;
	struct timeval stp, etp;

	if (gettimeofday(&stp, NULL) < 0) {
		fprintf(stderr, "gettimeofday failed: %s\n",
			strerror(errno));
		exit(1);
	}

	call = rx_NewCall(ti->conn);

	for (i = 0, ti->totalbytes = 0;
	     ti->totalbytes + write_size < data_size; i++) {
		
		ret = rx_Write(call, (char *) databuf, write_size);

		if (ret != write_size) {
			fprintf(stderr, "Short write (%d)\n", ret);
			exit(1);
		}
		ti->totalbytes += write_size;
	}

	ret = rx_Write(call, (char *) databuf, data_size - ti->totalbytes);

	ti->totalbytes += data_size - ti->totalbytes;

	code = rx_EndCall(call, 0);

	if (gettimeofday(&etp, NULL) < 0) {
		fprintf(stderr, "gettimeofday() failed: %s\n",
			strerror(errno));
		exit(1);
	}

	ti->elapsed = etp.tv_sec - stp.tv_sec;

	if (etp.tv_usec >= stp.tv_usec) {
		ti->elapsed += (etp.tv_usec - stp.tv_usec) / 1.0E6;
	} else {
		ti->elapsed += -1.0 + (etp.tv_usec - stp.tv_usec + 1E6) / 1.0E6;
	}

	return NULL;
}

static void
do_server(unsigned short port)
{
	struct rx_service *service;
	struct rx_securityClass *secureobj = rxnull_NewServerSecurityObject();
	int ret, secureindex = 1;

	ret = rx_Init(port);

	if (ret) {
		fprintf(stderr, "rx_Init failed\n");
		exit(1);
	}

	if (frame_size)
		rx_TcpSetFrameSize(frame_size);

	if (disable_flow_control) {
		printf("Disabling flow control\n");
		rx_TcpSetFlowControl(0);
	}

	if (tcp_send_size)
		rx_TcpSetSendBufsize(tcp_send_size);

	if (tcp_recv_size)
		rx_TcpSetRecvBufsize(tcp_recv_size);

	if (rx_window_size)
		rx_TcpSetWindowSize(rx_window_size);

	service = rx_NewService(0, RX_SERVER_ID, "rxtest", &secureobj,
				secureindex, rxtest_ExecuteRequest);

	if (service == NULL) {
		fprintf(stderr, "Can't create server\n");
		exit(1);
	}

	rx_StartServer(1);
	return;
}

static int32_t
rxtest_ExecuteRequest(struct rx_call *call)
{
	int cc, nbytes = 0, ncall = call->tcpCallNumber;
	char buf[16384];

	printf("Got call (callno = %d)\n", ncall);

	do {
		cc = rx_Read(call, buf, sizeof(buf));
		nbytes += cc;
	} while (cc > 0);

	printf("Call ended, callno = %d, cc = %d, %d bytes transmitted\n",
	       ncall, cc, nbytes);

	printf("Statistics:\n");
	printf("\tData packets sent: %d\n", rxi_tcp_data_packets_sent);
	printf("\tSEQ packets sent: %d\n", rxi_tcp_ack_packets_sent);
	printf("\tData packets received: %d\n", rxi_tcp_data_packets_received);
	printf("\tSEQ packets received: %d\n", rxi_tcp_ack_packets_received);
	printf("\tNumber of times transmit window closed: %d\n",
	       rxi_tcp_transmit_window_closed);
	return 0;
}
