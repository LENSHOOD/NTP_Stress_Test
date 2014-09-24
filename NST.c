/***********************************************************************
NTP Stress Test
	Send plenty of NTP requests to test how many responses the server
	can produce.
date: 2014/9/24 Version 1.1.1
**********************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

//NTP Packet header
#define PORTNUM 123
#define LI 0x03
#define VN 0x04
#define MODE 0x03
#define STRATUM 0x00
#define POLL 0x04
#define PREC 0xfa

#define NTP_LEN 48
#define NTP_LEN_RCV 384
#define JAN_1970 0x83aa7e80
#define NTPFRAC(x) (4294*(x)+((1981*(x))>>11))
#define USEC(x) (((x)>>12-759*((((X)>>10)+32768)>>16))

//NTP 64bit format
struct ntptime
{
	uint32_t coarse;
	uint32_t fine;
};
//NTP packet structure
struct ntppacket
{
	//2bits LI,3bits VN,3bits Mode
	uint8_t li_vn_mode;
	//8bits Stratum
	uint8_t stratum;
	//8bits Poll(with signed)
	int8_t poll;
	//8Bits Precision(with signed)
	uint8_t precision;
	//32bits root delay(with signed)
	int32_t root_delay;
	//32bits root dispersion
	int32_t root_dispersion;
	//32bits reference identifier
	int32_t reference_identifier;
	//64bit reference time stamp
	struct ntptime reference_timestamp;
	//64bit originate time stamp
	struct ntptime originage_timestamp;
	//64bit receive time stamp
	struct ntptime receive_timestamp;
	//64bit transmit time stamp
	struct ntptime transmit_timestamp;
};
//structure use to passing parameter
struct sockpack
{
	int32_t fd;
	struct sockaddr_in s_sockaddr;
};
//structure use to passing parameter
struct parathread
{
	uint8_t *send_data;
	uint8_t *recv_data;
	int32_t size;
	int32_t client_fd;
	struct sockaddr_in server_sockaddr;
	uint32_t *count;
};

//This function build a entire NTP client packet
void Build_Packet(struct ntppacket* packet, uint8_t *data);
//Initialize a socket
struct sockpack Init_Soacket(int para, char *str[]);
//Send the initial packet
void Send_NTP_Packet(uint8_t *data, int32_t size, int32_t client_fd, struct sockaddr_in server_sockaddr);
//Receive the data that server returned
void Receive_NTP_Packet(uint8_t *data, int32_t *size, int32_t client_fd, struct sockaddr_in server_sockaddr, uint32_t *count);
//Thread handler
void *Get_thread(struct parathread *para);

int main(int argc, char *argv[])
{
	uint32_t i,err,count=0,max_count;
	//NTP standard packet
	struct ntppacket *send_packet,*recv_packet;
	//use to passing parameters
	struct sockpack send_pack;
	//packet for send
	uint8_t send_data[NTP_LEN],recv_data[NTP_LEN*8];
	int32_t sin_size;
	//use to passing parameters
	struct parathread pass_para;
	//thread array
	pthread_t *udp_thread = (pthread_t*)malloc(sizeof(pthread_t));

	//display information
	if(argc != 3)
	{
		printf("usage: NTP_Stress_Test [IP Address] [Maximum Connect Numbers]\n");
		exit(0);
	}

	//string to number
	//max_count = (uint32_t)(*argv[2] - 0x30);
	max_count = atoi(argv[2]);
	memset(udp_thread,0,(size_t)max_count);

	send_packet = (struct ntppacket *)malloc(sizeof(struct ntppacket));
	//remove comment if need to use "rcvpacket"
	//rcvpacket = (struct ntppacket *)malloc(sizeof(struct ntppacket));

	//build NTP send packet
	Build_Packet(send_packet,send_data);

	//initialize socket
	send_pack = Init_Soacket(argc,argv);
	sin_size = sizeof(struct sockaddr);

	//build parameter structure
	pass_para.send_data = send_data;
	pass_para.recv_data = recv_data;
	pass_para.client_fd = send_pack.fd;
	pass_para.size = sin_size;
	pass_para.server_sockaddr = send_pack.s_sockaddr;
	pass_para.count = &count;

	//create threads
	for(i=0;i<max_count;i++)
	{
		if((err = pthread_create(udp_thread+i,NULL,(void*)Get_thread,&pass_para)) != 0)
		{
				fprintf(stderr,"An Error Occurred:%s, Send Failed!\n",strerror(errno));
				exit(1);
		}
		//pthread_join(udp_thread[i],NULL);
		//pthread_detach(udp_thread[i]);
		//usleep(1000);
	}
	//wait thread to end
	for(i=0;i<max_count;i++)
	{
		pthread_join(udp_thread[i],NULL);
	}
	//close
	close(send_pack.fd);
	printf("Maximum Valid Connections:%d\n ",count);
	exit(0);
}


void Build_Packet(struct ntppacket* packet, uint8_t *data)
{
	time_t time_Local;
	uint32_t temp;
	//header
	packet->li_vn_mode = (uint8_t)((uint8_t)MODE|(uint8_t)(VN<<3)|(uint8_t)(LI<<6));
	packet->stratum = (uint8_t)STRATUM;
	packet->poll = (int8_t)POLL;
	packet->precision = (uint8_t)PREC;

	packet->root_delay = (int32_t)(1<<16);
	packet->root_dispersion = (int32_t)(1<<16);
	packet->reference_identifier = 0;

	//no need to set other three time stamps, just stay zero
	time(&time_Local);
	packet->transmit_timestamp.coarse = (int32_t)(JAN_1970+time_Local);
	packet->transmit_timestamp.fine = (int32_t)(NTPFRAC(time_Local));

	//build packet
	temp = htonl(((packet -> li_vn_mode) << 24)|((packet -> stratum) << 16)|((packet -> poll) << 8)|(packet -> precision));
	memcpy(data,&temp,sizeof(temp));
	temp = htonl(packet->root_delay);
	memcpy(data+4,&temp,sizeof(temp));
	temp = htonl(packet->root_dispersion);
	memcpy(data+8,&temp,sizeof(temp));
	temp = htonl(packet->reference_identifier);
	memcpy(data+12,&temp,sizeof(temp));
	temp = htonl(packet->transmit_timestamp.coarse);
	memcpy(data+40,&temp,sizeof(temp));
	temp = htonl(packet->transmit_timestamp.fine);
	memcpy(data+44,&temp,sizeof(temp));

}

struct sockpack Init_Soacket(int para, char *str[])
{
	int32_t client_fd;
	struct sockaddr_in server_sockaddr;
	struct sockpack send_pack;
	//struct hostent *host;
	//struct in_addr aimaddr;

	//initialize the sockaddr
	memset(&server_sockaddr,0,sizeof(struct sockaddr_in));

	//obtain host name/IP from argv
	if(para != 3)
	{
		printf("Please input your host name/IP address as a parameter!\n");
		exit(1);
	}
	if((inet_aton(str[1],&server_sockaddr.sin_addr)) == -1)
	{
		printf("Get IP error!\n");
		exit(1);
	}
	/*if((host = gethostbyname(argv[1])) == NULL)
	{
		printf("Get host error!\n");
		exit(1);
	}*/

	//create a socket
	if((client_fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) == -1)
	{
		fprintf(stderr,"An Error Occured:%s, Create Failed!\n",strerror(errno));
		exit(1);
	}

	//fill the sockaddr with server information that you want to connect
	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_port = htons(PORTNUM);
	/*server_sockaddr.sin_addr = *((struct in_addr *)host->h_addr);*/

	//return the socket packet
	send_pack.fd = client_fd;
	send_pack.s_sockaddr = server_sockaddr;
	return(send_pack);
}

void Send_NTP_Packet(uint8_t *data, int32_t size, int32_t client_fd, struct sockaddr_in server_sockaddr)
{
	if((sendto(client_fd,data,NTP_LEN,0,(struct sockaddr*)&server_sockaddr,size) == -1))
	{
		fprintf(stderr,"An Error Occurred:%s, Send Failed!\n",strerror(errno));
		exit(1);
	}
	else
		printf("Send Packet successful!\n");
}

void Receive_NTP_Packet(uint8_t *data, int32_t *size, int32_t client_fd, struct sockaddr_in server_sockaddr, uint32_t *count)
{
	struct sockaddr_in client_sockaddr;
	if((recvfrom(client_fd,data,NTP_LEN*8,0,(struct sockaddr*)&client_sockaddr,size) == -1))
		{
			fprintf(stderr,"An Error Occurred:%s, Receive Failed!\n",strerror(errno));
			exit(1);
		}
		else
		{
			fprintf(stdout,"From %s ",inet_ntoa(client_sockaddr.sin_addr));
			int32_t temp;
			memcpy(&temp,data,4);
			fprintf(stdout,"LI_VN_MODE_strtum_poll_precision:%x\n",ntohl(temp));
			(*count)++;
			printf("responded connections: %d\n",*count);
		}
}

void *Get_thread(struct parathread *para)
{
	//in case of server timeout, set time limit
	fd_set recv_ready;
	struct timeval block_time;
	FD_ZERO(&recv_ready);
	FD_SET(para->client_fd, &recv_ready);
	block_time.tv_sec = 2;
	block_time.tv_usec = 0;

	//send initial packet
	Send_NTP_Packet(para->send_data,para->size,para->client_fd,para->server_sockaddr);
	//receive data or timeout and exit
	if(select((para->client_fd)+1,&recv_ready,NULL,NULL,&block_time) > 0)
	{
		Receive_NTP_Packet(para->recv_data,&(para->size),para->client_fd,para->server_sockaddr,para->count);
	}
	else
	{
		printf("Server not response!\n");
	}
	pthread_exit((void*)0);
	return((void*)0);
}
