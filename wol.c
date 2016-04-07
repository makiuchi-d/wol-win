/******************************************************************************
*	wol : Wake On Lan
*
*
******************************************************************************/
#pragma comment(lib,"wsock32.lib")

#include <stdio.h>
#include <stdlib.h>
#include <winsock.h>

#define MACLEN 6
#define MPSIZE (6+16*MACLEN)


// MACアドレスからマジックパケットを作る
// 引数はchar[MACLEN]で。
unsigned char *make_magicpacket(unsigned char *mac)
{
	static unsigned char mp[MPSIZE] = {
		0xff,0xff,0xff,0xff,0xff,0xff
	};
	unsigned char *p = mp + 6;
	int i;

	for(i=0;i<16;i++){
		memcpy(p,mac,MACLEN);
		p += MACLEN;
	}

	return mp;
}


// パラメータ解析用
int macstr_to_array(char *str,unsigned char *array)
{
	int r = sscanf(str,"%x-%x-%x-%x-%x-%x",
					array,array+1,array+2,array+3,array+4,array+5);

	return (r==MACLEN)? 0:1;	// 失敗=1
}

int ipstr_to_addr(char *str,long *addr)	// 失敗=1
{
	long _addr = inet_addr(str);
	if(_addr==INADDR_NONE && strcmp(str,"255.255.255.255")){
		// INADDR_NONE = -1 && 255.255.255.255 = -1
		// ホスト名
		struct hostent *host;
		host = gethostbyname(str);
		if(host==NULL)
			return 1;
		if(host->h_addr_list==NULL || host->h_addr_list[0]==NULL)
			return 1;
		*addr = *(long *)(host->h_addr_list[0]);
	}
	else{
		*addr = _addr;
	}
	return 0;
}

// 表示とか
void ws_error(const char *s)
{
	fprintf(stderr,"Winsock error (%d) : %s\n",h_errno,s);
};

void print_usage(void)
{
	puts("Wake on LAN for Win32");
	puts("Send Magic Packet to target Device.");
	puts("Usage: wol <MAC> [IP]");
	puts("  MAC : Physical address of target device. (xx-xx-xx-xx-xx-xx in hexadecimal)");
	puts("  IP  : IP address (subnet ID) or hostname. (xx.xx.xx.xx in decimal)");
	puts("              When you omitted the IP parametor,");
	puts("              wol send to broadcast (255.255.255.255).");
	puts("Magic Packet is trademark of Advanced Micro Devices, Inc.");
}

/*=============================================================================
*	main()
*============================================================================*/
int main(int argc,char *argv[])
{
	WSADATA info;
	SOCKET sock;
	struct sockaddr_in addr;
	BOOL t = TRUE;
	unsigned char mac[MACLEN];
	unsigned char *p;

	int res;
	char *ipaddr_str;
	unsigned long ipaddr;

	if(argc<2){
		print_usage();
		return 0;
	}

	// Winsock 初期化
	res = WSAStartup(0x0101,&info);	// ver1.1を要求
	if(res==SOCKET_ERROR){
		ws_error("Winsock initialize.");
		return -1;
	}

	// パラメータ解読
	res = macstr_to_array(argv[1],mac);
	if(res){
		fprintf(stderr,"Irregular parameter (MAC = %s)\n",argv[1]);
		return 0;
	}

	ipaddr_str = (argc>2)? argv[2] : "255.255.255.255";
	res = ipstr_to_addr(ipaddr_str,&ipaddr);
	if(res){
		fprintf(stderr,"Irregular parameter (IP = %s)\n",argv[2]);
		return 0;
	}

#ifdef DEBUG
	printf("MAC : %02x.%02x.%02x.%02x.%02x.%02x\n",
		   mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	printf("IP  : %d.%d.%d.%d\n",
		   ipaddr&0xff,(ipaddr>>8)&0xff,(ipaddr>>16)&0xff,(ipaddr>>24)&0xff);
#endif

	// ソケット設定
	sock = socket(AF_INET, SOCK_DGRAM, 0);	//データグラム(UDP)
	if(sock==INVALID_SOCKET){
		ws_error("Socket creation.");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(9);	// wolは9番に送るらしい。
	addr.sin_addr.S_un.S_addr = ipaddr;
	memset(addr.sin_zero, 0, 8);	// パディング

	res = setsockopt(sock,SOL_SOCKET,SO_BROADCAST,(char *)&t,sizeof(t));
	if(res==SOCKET_ERROR){
		ws_error("Enableing broad cast.");
		return -1;
	}

	p = make_magicpacket(mac);
#ifdef DEBUG
	printf("sending to %d.%d.%d.%d:%d\n",
		   addr.sin_addr.S_un.S_un_b.s_b1,addr.sin_addr.S_un.S_un_b.s_b2,
		   addr.sin_addr.S_un.S_un_b.s_b3,addr.sin_addr.S_un.S_un_b.s_b4,
		   ntohs(addr.sin_port));
	printf("...Magic packet...\n");
	for(res=0;res<MPSIZE;res+=6)
		printf("%02d: %02x %02x %02x %02x %02x %02x\n",res/6,p[res],p[res+1],p[res+2],p[res+3],p[res+4],p[res+5]);
#endif
	res = sendto(sock,p,MPSIZE,0,(struct sockaddr *)&addr,sizeof(addr));
	if(res==SOCKET_ERROR){
		ws_error("Sending packet.");
		return -1;
	}

	res = closesocket(sock);
	if(res==SOCKET_ERROR){
		ws_error("Socket close.");
		return -1;
	}

	res = WSACleanup();
	if(res==SOCKET_ERROR){
		ws_error("Winsock cleanup.");
		return -1;
	}

	return 0;
}
