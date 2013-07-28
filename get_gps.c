#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include "nxjson.h"
#include "get_gps.h"

#define RECV_BUFFER_SIZE 2048

// compile : gcc loop.c get_gps.c nxjson.c -o gpsloop


int gps_socket;


char recv_buffer[RECV_BUFFER_SIZE + 1];
int recv_bytes,send_bytes;

int connect_gps()
{
	int sock,error_no;
	char * error_string;
	struct sockaddr_in gpsd_sock_addr;


	memset(&gpsd_sock_addr,0,sizeof(gpsd_sock_addr));
	gpsd_sock_addr.sin_family = AF_INET;
	gpsd_sock_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	gpsd_sock_addr.sin_port = htons(2947);

	if((sock = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP )) < 0)
	{
		error_no = errno;
		error_string = strerror(error_no);
		fprintf(stderr,"Error creating socket: %d : %s\n",error_no,error_string);
		return(-1);
	}

	if(connect(sock,(struct sockaddr *) &gpsd_sock_addr,sizeof(gpsd_sock_addr)) < 0)
	{
		error_no = errno;
		error_string = strerror(error_no);
		fprintf(stderr,"Error opening socket: %d : %s\n",error_no,error_string);
		return(-1);
	}
	return(sock);
}

int start_gps(int sock,char *recv_buffer)
{
	int error_no;
	char * error_string;
	int recv_bytes;
	int iloop;
	//receive version ....
	for(iloop=0;iloop<3;iloop++)
	{
		if ((recv_bytes = recv(sock, recv_buffer, RECV_BUFFER_SIZE, MSG_DONTWAIT)) <= 0)
		{
			error_no = errno;
			if(11 != error_no)
			{
				error_string = strerror(error_no);
				fprintf(stderr,"Error reding from  socket: %d : %s\n",error_no,error_string);
			}
		}
		else
		{
			recv_buffer[RECV_BUFFER_SIZE] = '\0';
			fprintf(stderr,"Read %d bytes - %s\n",recv_bytes,recv_buffer);
		}
		sleep(1);
	}

	//send start ?WATCH={"enable":true,"json":true}
	char start_cmd[]="?WATCH={\"enable\":true,\"json\":true}\n";
	send_bytes = send(sock,start_cmd,strlen(start_cmd),0);
	fprintf(stderr,"Start command sent bytes %d - %s\n",send_bytes,start_cmd);

	for(iloop=0;iloop<9;iloop++)
	{
		if ((recv_bytes = recv(sock, recv_buffer, RECV_BUFFER_SIZE, MSG_DONTWAIT)) <= 0)
		{
			error_no = errno;
			if(11 != error_no) 
			{
				error_string = strerror(error_no);
				fprintf(stderr,"Error reding from  socket: %d : %s\n",error_no,error_string);
			}
		}
		else
		{
			recv_buffer[recv_bytes] = '\0';
			//printf("Read %d bytes - %s\n",recv_bytes,recv_buffer);
			fprintf(stderr,"Read %d bytes\n",recv_bytes);
		}
		fprintf(stderr,"Started loop  %d\n",iloop);
		usleep(500000);
	}
	fprintf(stderr,"end start GPS\n");
}

int gps_stop(int sock,char *recv_buffer)
{
	int error_no;
	char * error_string;
	int recv_bytes;
	int iloop;
	//send stop ?WATCH={"enable":false}
	char stop_cmd[]="?WATCH={\"enable\":false}\n";
	send_bytes = send(sock,stop_cmd,strlen(stop_cmd),0);
	fprintf(stderr,"Stop command sent bytes %d - %s\n",send_bytes,stop_cmd);


	for(iloop=0;iloop<5;iloop++)
	{
		if ((recv_bytes = recv(sock, recv_buffer, RECV_BUFFER_SIZE, MSG_DONTWAIT)) <= 0)
		{
			error_no = errno;
			if(11 != error_no) 
			{
			error_string = strerror(error_no);
			fprintf(stderr,"Error reding from  socket: %d : %s\n",error_no,error_string);
			}
		}
		else
		{
			recv_buffer[recv_bytes+1] = '\0';
			//printf("Read %d bytes - %s\n",recv_bytes,recv_buffer);
			fprintf(stderr,"Read %d bytes\n",recv_bytes);
		}
		fprintf(stderr,"Stopped loop %d\n",iloop);
		usleep(200000);
	}
	close(sock);
	fprintf(stderr,"end stop GPS\n");
}

int read_gps(int sock,char *recv_buffer)
{
	int error_no;
	char *error_string;
	int recv_bytes;

	if ((recv_bytes = recv(sock, recv_buffer, RECV_BUFFER_SIZE, MSG_DONTWAIT)) <= 0)
	{
		error_no = errno;
		if(11 != error_no) 
		{
		error_string = strerror(error_no);
		recv_buffer[0] = '\0';
		fprintf(stderr,"Error reding from  socket: %d : %s\n",error_no,error_string);
		}
		return(0);
	}
	else
	{
		recv_buffer[recv_bytes + 1] = '\0';
		//printf("Read %d bytes - %s\n",recv_bytes,recv_buffer);
		//printf("Read %d bytes\n",recv_bytes);
	}
	return(recv_bytes);
}

int gps_init(void)
{
	if(-1 == (gps_socket = connect_gps()))
	{
		fprintf(stderr,"Could not connect to GPSD\n");
		return(0);
	}
	start_gps(gps_socket,&recv_buffer[0]);
}



int gps_disable(void)
{
	gps_stop(gps_socket,&recv_buffer[0]);
}

int parse_json_tpv(sTPV *stpv)
{
	char *first,*last,*eol;
	const nx_json* key;
	const char* time;

	first=eol=&recv_buffer[0];

	recv_bytes=read_gps(gps_socket,recv_buffer);

	while(1)
	{
		if((eol=index(eol+1,'\r')) != NULL)
		{
			int eolpos=eol-(char *)&recv_buffer;
			//printf("EOLPOS %d\n",eolpos);

			const nx_json *json = nx_json_parse(first,NULL);

			if( !json)
			{
				fprintf(stderr,"json failed\n");
				fprintf(stderr,"%s\n",first);
				return(-1);
			}
			key=nx_json_get(json,"class");
			if(0 == strcmp(key->text_value,"TPV"))
			{
				//printf("Type %d Value: %s\n",key->type,key->text_value);
				stpv->lat=nx_json_get(json,"lat")->dbl_value;
				stpv->lon=nx_json_get(json,"lon")->dbl_value;
				stpv->speed=nx_json_get(json,"speed")->dbl_value;
				time=nx_json_get(json,"time")->text_value;
				strncpy(stpv->time,time,32);
				stpv->time[31] = '\0';
				//printf("LAT: %12.9lf \nLon: %12.9lf \nSpeed: %6.2lf Time: %s\n",stpv->lat,stpv->lon,stpv->speed,time);
				nx_json_free(json);
				return(1);
			}
			//key=nx_json_get(json,"time");
			//printf("Time Type %d Value: %s\n",key->type,key->text_value);
			nx_json_free(json);
			first=eol;
		}
		else
		{
			break;
			return(0);
		}
	}
}

