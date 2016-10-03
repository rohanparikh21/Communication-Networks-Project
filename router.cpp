
#include "common.h"

#include "newport.h"

using namespace std;
#define MAX_INTERFACE 10
#define PING_SIZE 1200
#define HEADER_SIZE 5
#define FTTimeOut 15
#define PRTTimeOut 120
pthread_mutex_t mutexshare;
int interfaceNo=0;
struct cShared{
	LossyReceivingPort *my_recv_port;
	mySendingPort *my_send_port;
	vector <Packet*> tobeSent[MAX_INTERFACE];
	vector <unsigned char> contentID ;
	vector <unsigned char> Interface;
	vector <unsigned char> hopCount;
	vector <unsigned char> time;	
	vector <unsigned char>	PRT_ReqcontentID;
	vector <unsigned char> PRT_hostID;
	vector <unsigned char> PRT_Interface;
	vector <unsigned char> PRT_time;
	int intNo;
};

void printFFTable(struct cShared *x)
{
	pthread_mutex_lock (&mutexshare);
	cout<<"\n------------------------Forwarding Table-------------------------"<<endl;
	printf("|\tC_ID\t|\tInt.\t|\tHops\t|\tTime\t|\n");
	cout<<"-----------------------------------------------------------------"<<endl;
	for(int i=0;i<x->contentID.size();i++){
		printf("|\t%d\t|\t%d\t|\t%d\t|\t%d\t|\n",x->contentID[i],x->Interface[i],x->hopCount[i],x->time[i]);
	cout<<"-----------------------------------------------------------------"<<endl;
	
	}
	pthread_mutex_unlock (&mutexshare);
}

void printPFTable(struct cShared *x)
{
	pthread_mutex_lock (&mutexshare);
	cout<<"\n----------------------Pending Request Table----------------------"<<endl;
	printf("|\tC_ID\t|\tHost\t|\tInt.\t|\tTime\t|\n");
	cout<<"-----------------------------------------------------------------"<<endl;
	for(int i=0;i<x->PRT_ReqcontentID.size();i++){
		printf("|\t%d\t|\t%d\t|\t%d\t|\t%d\t|\n",x->PRT_ReqcontentID[i],x->PRT_hostID[i],x->PRT_Interface[i],x->PRT_time[i]);
	cout<<"-----------------------------------------------------------------"<<endl;
	}
	pthread_mutex_unlock (&mutexshare);
	
}
void *timerFunction(void *arg){
	struct cShared *sh = (struct cShared *)arg;
	while(1)
	{
		pthread_mutex_lock (&mutexshare);
		for(int i=0;i<sh->time.size();i++)
		{
			int x=sh->time[i];
			x--;
			if(x==0)
			{
					printf("Time Out occured FF Table for %d\n",sh->contentID[i]);
					sh->contentID.erase(sh->contentID.begin()+i);
					sh->hopCount.erase(sh->hopCount.begin()+i);
					sh->Interface.erase(sh->Interface.begin()+i);
					sh->time.erase(sh->time.begin()+i);
					i--;
			}
			else
				sh->time[i]=x;
		}
			
		for(int i=0;i<sh->PRT_time.size();i++)
		{
			int y=sh->PRT_time[i];	
			y--;
			if(y==0)
			{
					printf("Time Out occured PRT Table for %d\n",sh->PRT_ReqcontentID[i]);
					sh->PRT_ReqcontentID.erase(sh->PRT_ReqcontentID.begin()+i);
					sh->PRT_hostID.erase(sh->PRT_hostID.begin()+i);
					sh->PRT_Interface.erase(sh->PRT_Interface.begin()+i);
					sh->PRT_time.erase(sh->PRT_time.begin()+i);
					i--;
			}
			else
				sh->time[i]=y;
		}
		pthread_mutex_unlock (&mutexshare);
		usleep(1000000);
	}
}

void *RouterTransProc(void *arg){

	struct cShared *sh = (struct cShared *)arg;
	mySendingPort *myLocalport=sh->my_send_port;
	int x=sh->intNo;
	//printf("Started transmitting thread %d\n",x);

	
	while(1)
	{
		if(!sh->tobeSent[x].empty())
		{
			pthread_mutex_lock (&mutexshare);
			//printf("Sending packet on %d Packet No %d\n",x,sh->tobeSent[x].front()->accessHeader()->getOctet(1));	
			myLocalport->sendPacket(sh->tobeSent[x].front());
			sh->tobeSent[x].erase(sh->tobeSent[x].begin());
			pthread_mutex_unlock (&mutexshare);
			//usleep(100000);
			
		}
	}
	return NULL;
}
void *RouterRecvProc(void *arg){
	
	struct cShared *sh = (struct cShared *)arg;
	LossyReceivingPort *myLocalport=sh->my_recv_port;
	int x=sh->intNo;
	//printf("Started receiving thread %d\n",x);
	PacketHdr *hdr;
	PacketHdr *sendhdr;
	unsigned char type;
	Packet *sendPacket;
	while(1)
	{	
		
		Packet *recvPacket;
		
		recvPacket = myLocalport->receivePacket();
		sendPacket = new Packet();
		sendhdr = sendPacket->accessHeader();
		hdr = recvPacket->accessHeader();
		type = hdr->getOctet(0);
		sendhdr->setOctet(type,0);
		
		
		// Announcement Packet
		if(type=='2')								
		{
			unsigned char contentid = hdr->getOctet(1);
			unsigned char hops = hdr->getOctet(2);
			hops++;
			pthread_mutex_lock (&mutexshare);
			
			int pos=(find(sh->contentID.begin(),sh->contentID.end(),contentid)-(sh->contentID.begin()));
			if (pos>=sh->contentID.size())					// ContentID not in the forwarding table
			{
				printf("Announcement packet : contentNo= %d , hops : %d\n", contentid,hops);
				sh->contentID.push_back(contentid);
				sh->Interface.push_back(x);
				sh->hopCount.push_back(hops);
				sh->time.push_back(FTTimeOut);
				sendhdr->setOctet(contentid,1);
				sendhdr->setOctet(hops,2);

					for(int i=0;i<interfaceNo;i++)
					{
						if(i!=x)
						sh->tobeSent[i].push_back(sendPacket);		
					}
				pthread_mutex_unlock (&mutexshare);
				printFFTable(sh);
			}
			else
			{	
				if((hops)<sh->hopCount[pos])
				{
					
					printf("Announcement update packet : contentNo= %d, hops : %d\n", contentid,hops);
					sh->Interface[pos]=x;
					sh->hopCount[pos]=(hops);
					sh->time[pos]=FTTimeOut;
					sendhdr->setOctet(contentid,1);
					sendhdr->setOctet(hops,2);;
					for(int i=0;i<interfaceNo;i++)
					{
						if(i!=x)
						sh->tobeSent[i].push_back(sendPacket);		
					}
					pthread_mutex_unlock (&mutexshare);
					printFFTable(sh);
				}
				
				else
				{
					pthread_mutex_unlock (&mutexshare);
					//continue;
					//cout<<"Drop Packet"<<endl;
					//printf("Dropped packet : contentNo= %d, hops : %d\n", contentid,hops);
					//Do nothing -- Drop Packet;
				}
			}

		//	pthread_mutex_lock (&mutexshare);
			/*
			hdr->setOctet(hops,2);
			for(int i=0;i<interfaceNo;i++)
				{
					
					if(i!=x)
						sh->tobeSent[i].push_back(recvPacket);		
				}
		*/
		//	pthread_mutex_unlock (&mutexshare);
		}
		else if(type=='0')
		{
			
			
			unsigned char contentid = hdr->getOctet(1);
			unsigned char hostid = hdr->getOctet(2);
			pthread_mutex_lock (&mutexshare);
			int pos=(find(sh->contentID.begin(),sh->contentID.end(),contentid)-(sh->contentID.begin()));
			if (pos==sh->contentID.size())					// ContentID not in the forwarding table
			{
					//cout<<"Drop the Request Packet as no content Found"<<endl;
					pthread_mutex_unlock (&mutexshare);
					continue;
			}
			pthread_mutex_unlock (&mutexshare);
			for(int i=0;i<sh->PRT_ReqcontentID.size() || sh->PRT_ReqcontentID.empty();i++)
			{
				
				if(sh->PRT_ReqcontentID.empty())
				{
						
						printf("Request packet content No= %d, Host ID = %d\n",contentid,hostid);
						pthread_mutex_lock (&mutexshare);
						cout<<"New request arrived..creating the table"<<endl;
						
						sh->PRT_ReqcontentID.push_back(contentid);
						sh->PRT_Interface.push_back(x);
						sh->PRT_hostID.push_back(hostid);
						sh->PRT_time.push_back(PRTTimeOut);
						sendhdr->setOctet(contentid,1);
						sendhdr->setOctet(hostid,2);
						sh->tobeSent[sh->Interface[pos]].push_back(sendPacket);
						pthread_mutex_unlock (&mutexshare);
						printPFTable(sh);
						break;
				}
				pthread_mutex_lock (&mutexshare);
				if(sh->PRT_ReqcontentID[i]==contentid && sh->PRT_hostID[i]==hostid)
				{
					
						printf("Request packet content No= %d, Host ID = %d\n",contentid,hostid);
						
						cout<<"Found in PRT... Updating the table.."<<endl;
						sendhdr->setOctet(contentid,1);
						sendhdr->setOctet(hostid,2);
						sh->tobeSent[sh->Interface[pos]].push_back(sendPacket);
						//sh->tobeSent[sh->Interface[pos]].push_back(recvPacket);
						sh->PRT_time[i]=PRTTimeOut;
						
						pthread_mutex_unlock (&mutexshare);
						printPFTable(sh);
						break;
				}
				else
				{
						printf("Request packet content No= %d, Host ID = %d\n",contentid,hostid);
						cout<<"New request arrived..Adding to the table"<<endl;
						sh->PRT_ReqcontentID.push_back(contentid);
						sh->PRT_Interface.push_back(x);
						sh->PRT_hostID.push_back(hostid);
						sh->PRT_time.push_back(PRTTimeOut);
						sendhdr->setOctet(contentid,1);
						sendhdr->setOctet(hostid,2);
						sh->tobeSent[sh->Interface[pos]].push_back(sendPacket);
						//sh->tobeSent[sh->Interface[pos]].push_back(recvPacket);
						pthread_mutex_unlock (&mutexshare);
						printPFTable(sh);
						break;
				}
			}
		}
		else if(type=='1')
		{
			unsigned char contentid = hdr->getOctet(1);
			unsigned char hostid = hdr->getOctet(2);
			
			pthread_mutex_lock (&mutexshare);
			for(int i=0;i<sh->PRT_ReqcontentID.size();i++)
			{
				
				if(sh->PRT_ReqcontentID[i]==contentid && sh->PRT_hostID[i]==hostid)
				{
					printf("Response Packet arrived : %d\n",(contentid));
					sh->tobeSent[sh->PRT_Interface[i]].push_back(recvPacket);
					sh->PRT_ReqcontentID.erase(sh->PRT_ReqcontentID.begin()+i);
					sh->PRT_hostID.erase(sh->PRT_hostID.begin()+i);
					sh->PRT_Interface.erase(sh->PRT_Interface.begin()+i);
					sh->PRT_time.erase(sh->PRT_time.begin()+i);
				}
			}
			pthread_mutex_unlock (&mutexshare);
			printPFTable(sh);
			
		}
		
	}
	cout<<"exiting"<<endl;
	return NULL;
}
// Router ID 1,2,3,.....
// Interface 0,1,2,...
void startClient(int routerID,int interfaceNo){
	printf("Router Booted...\n");	
	int p,cnt=0;
	ifstream myfile;
	Address *dst_addr[interfaceNo];
	myfile.open("dst_add.txt");
	if(myfile)
	{
		while(myfile>>p)
		{
			dst_addr[cnt]=new Address("localhost",p);
			cnt++;
		}
	}
	pthread_mutex_init(&mutexshare, NULL);
	
	Address *my_recv_addr[interfaceNo];
	Address *my_send_addr[interfaceNo];
	
	mySendingPort *my_send_port[MAX_INTERFACE];
	LossyReceivingPort *my_recv_port[MAX_INTERFACE];
	int recvPortNo=5000+(routerID*10);
	int transPortNo=6000+(routerID*10);
	try{
		
		for (int i=0;i<interfaceNo;i++)
		{
				
			my_recv_addr[i] = new Address("localhost", recvPortNo+i);
			my_send_addr[i] = new Address("localhost", transPortNo+i);
			my_send_port[i] = new mySendingPort();
			my_send_port[i]->setAddress(my_send_addr[i]);
			my_send_port[i]->setRemoteAddress(dst_addr[i]);
			my_recv_port[i] = new LossyReceivingPort(0.01);
			my_recv_port[i]->setAddress(my_recv_addr[i]);
			my_send_port[i]->init();
			my_recv_port[i]->init();
		}
	} catch(const char *reason ){
		cerr << "Exception:" << reason << endl;
		return;
	}
	
	struct cShared *sh;
	sh = (struct cShared*)malloc(sizeof(struct cShared));
	sh->intNo=0;
	//sh->my_recv_port = new ReceivingPort[interfaceNo];
	//sh->my_recv_port=(ReceivingPort *) malloc(interfaceNo*sizeof(ReceivingPort));
	
	pthread_t timerThread;
	pthread_create(&timerThread,0,&timerFunction,sh);
	pthread_t recvThread[interfaceNo];
	pthread_t transThread[interfaceNo];
	for(int i=0;i<interfaceNo;i++)
	{	
		sh->intNo=i;
		sh->my_recv_port= my_recv_port[i];
		sh->my_send_port= my_send_port[i];
		pthread_create(recvThread+i, 0, &RouterRecvProc, sh);
		pthread_create(transThread+i, 0, &RouterTransProc, sh);
		usleep(5000);
		
	}
	
	for(int i=0;i<interfaceNo;i++)
	{
		pthread_join(recvThread[i], NULL);
		pthread_join(transThread[i], NULL);
		}
		pthread_join(timerThread,NULL);
	
}
	

int main(int argc, char *argv[]) {
	int routerID;
	
	if(argc>2){
		routerID = atoi(argv[1]);
		
		interfaceNo= atoi(argv[2]);
		if(interfaceNo>MAX_INTERFACE)
		{
			cout<<"Maximum Interfaces allowed";
			cout<<MAX_INTERFACE<<endl;
			return EXIT_FAILURE;
		}	
	}
	
	else
	{
		cout<<"Please call the Router in following format ./router <routerID> <NoOfInterfaces>\n";
		return EXIT_FAILURE;
		}
	startClient(routerID,interfaceNo);
	return EXIT_SUCCESS;
}


