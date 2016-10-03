#include "common.h"
#include "newport.h"
#include <pthread.h>
#include <iostream>
#include <vector>
#include <string.h>
#include <fstream>
#include <cstdlib>

using namespace std;

const int MAX_BUFFER = 1500;
unsigned char HOST_ID = 0;
vector<unsigned char> ListOfContents;

struct RPShared{
    LossyReceivingPort *recv_port;
    mySendingPort *send_port;
};

struct SPShared{
    mySendingPort *send_port;
};

struct Indicator{
    unsigned char receive_ind;
    unsigned char content_requested_id;
};

struct Indicator *ind;

void *MakeAnnouncementPacket(void *arg){
    cout << "Start: Make announcement function." <<endl;
    
    struct SPShared *sp = (struct SPShared *)arg;
    while(1){
        
        int i;
        Packet *packet = new Packet();
        PacketHdr *hdr;
        
        for(i = 0; i< ListOfContents.size(); i++){
            packet = new Packet();
            hdr = packet->accessHeader();
            hdr->setOctet('2',0);
            hdr->setOctet(ListOfContents.at(i),1);
            hdr->setOctet(0,2);
           
            sp->send_port->sendPacket(packet);
            //sp->send_port->setACKflag(true);
            //sp->send_port->timer_.startTimer(0);
        }

        usleep(10000000);
    }
}

void StartPeriodicAnnouncements(mySendingPort *my_port, pthread_t *t){
    
    cout << "Start: Periodic announcements." << endl;
    struct SPShared *sp;
    sp = (struct SPShared*)malloc(sizeof(struct SPShared));
    sp->send_port = my_port;
    pthread_create(&(*t), 0, &MakeAnnouncementPacket, sp);
    
}

void *ReceiveProcedure(void *arg){
    
    cout << "Start: Receive procedure." << endl;
    struct RPShared *rp = (struct RPShared *)arg;
    Packet *recvPacket;
    PacketHdr *hdr;
    Packet *data_packet;
    PacketHdr *data_packet_hdr;
    unsigned char packet_id;
    unsigned char content_id;
    unsigned char host_id_requesting;
    while(1){
        
        recvPacket = rp->recv_port->receivePacket();
        hdr = recvPacket->accessHeader();
        packet_id = hdr->getOctet(0);
        if(packet_id == '0'){
            cout<<"\n---------------------------------------------------------"<<endl;
            cout << "Message: Serving a request." << endl;
            int i;
            content_id = hdr->getOctet(1);
            host_id_requesting = hdr->getOctet(2);
            printf("Message: Content ID requested is: %d\n",content_id);
            for(i=0; i<ListOfContents.size(); i++){
                if(ListOfContents.at(i) == content_id){
                    data_packet = new Packet();
                    data_packet_hdr = data_packet->accessHeader();
                    data_packet_hdr->setOctet('1',0);
                    data_packet_hdr->setOctet(content_id,1);
                    data_packet_hdr->setOctet(host_id_requesting,2);
                    //Get the file and store as payload
                    string x = "Content" + to_string((int)content_id) + ".txt";
                    
                    cout<<x<<endl;
                    char *filename = const_cast<char*>(x.c_str());
                    //char *filename="Content4.txt";
                    char buffer[MAX_BUFFER];
                    ifstream f;
                    f.open(filename);
                    int iterator = 0;
                    while(!f.eof()){
                        buffer[iterator] = f.get();
                        iterator++;
                    }
                    f.close();
                    data_packet_hdr->setIntegerInfo(iterator,3);
                    data_packet->fillPayload(iterator,buffer);
                    rp->send_port->sendPacket(data_packet);
                    cout << "Message: Requested content sent." << endl;
                    
                }
                else{
                    cout << "Message: Content not present, drop packet." << endl;
                }
            }
           cout<<"\n---------------------------------------------------------"<<endl; 
        }
        else if(packet_id == '1'){
            // Case when a data packet is received
            cout<<"\n---------------------------------------------------------"<<endl;
            cout << "Message: Data packet received." << endl;
            unsigned char id_requested;
            char* temp;
            
            id_requested = hdr->getOctet(1);
            if(id_requested == ind->content_requested_id){
                cout << "Message:  Data packet is correct." << endl;
                ind->receive_ind = '1';
                temp = recvPacket->getPayload();
                string y = "Content" + to_string((int)id_requested) + ".txt";
                char *fname = const_cast<char*>(y.c_str());
                ofstream f;
                f.open(fname);
                f << temp;
                f.close();
                rp->send_port->setACKflag(true);
                rp->send_port->timer_.stopTimer();
                ListOfContents.push_back(id_requested);
            }
            cout<<"\n---------------------------------------------------------"<<endl;   
            
        }
        else if(packet_id == '2'){
                    
            // Drop packet because hosts don't accept packet announcements
           
        }
        else{
            cout << "Packet ID not proper" << endl;
        }
    }
}

void StartReceivingthread(LossyReceivingPort *my_port, mySendingPort *s_my_port, pthread_t *t){
    
    cout << "Start: Packet Receiving thread." << endl;
    struct RPShared *rp;
    rp = (struct RPShared*)malloc(sizeof(struct RPShared));
    rp->recv_port = my_port;
    rp->send_port = s_my_port;
    pthread_create(&(*t), 0, &ReceiveProcedure, rp);

    
}

void MakeRequest(mySendingPort *my_port, unsigned char content_id){
    
    Packet *new_request_packet = new Packet();
    PacketHdr *hdr = new_request_packet->accessHeader();
    hdr->setOctet('0',0);
    hdr->setOctet(content_id,1);
    hdr->setOctet(HOST_ID,2);
    my_port->sendPacket(new_request_packet);
    cout<<"Packet Sent"<<endl;
    my_port->setACKflag(false);
    my_port->lastPkt_= new_request_packet;
    my_port->timer_.startTimer(5);
    ind->receive_ind = '0';
    ind->content_requested_id = content_id;
    
}




int main(int argc, const char * argv[])
{
   
    
    cout << "Start: Host bootup." <<endl;
     // Code to set the HOST_ID global variable
    cout << "Message: Enter a unique id to be set for your host." << endl;
    int tmp;
    cin >> tmp;
    HOST_ID=tmp;
	Address *my_recv_addr;
	Address *my_send_addr;
	Address *dst_addr;
	mySendingPort *my_send_port;
	LossyReceivingPort *my_recv_port;
    const char* hname = "localhost";
    
	try{
        // Set up all ports and addresses	
			my_recv_addr = new Address(hname, 7000+(tmp*10));
			my_send_addr = new Address(hname, 7000+(tmp*10)+1);
		if(argc>1)	
			dst_addr =  new Address(hname, atoi(argv[1]));
		else
		{
			cout<<"Wrong arguments, input format should be ./ping client <recv_addr> <send_addr> <destination addr>";
			return EXIT_FAILURE;
		}
		my_send_port = new mySendingPort();
		my_send_port->setAddress(my_send_addr);
		my_send_port->setRemoteAddress(dst_addr);
		my_recv_port = new LossyReceivingPort(0.01);
		my_recv_port->setAddress(my_recv_addr);
		my_send_port->init();
		my_recv_port->init();
        
       
        
       
        // Code to start the receiving thread and periodic announcements
        
        pthread_t thread1; // Thread1 for periodic anouncements
        pthread_t thread2; // Thread2 for receiving thread
        StartPeriodicAnnouncements(my_send_port, &thread1);
        StartReceivingthread(my_recv_port, my_send_port, &thread2);
        usleep(10000);
        // Allocate memory to indicator structure
        
        ind = (struct Indicator*)malloc(sizeof(struct Indicator));
        
        // Switch case to set up the content insertion, deletion and request dialogue
        char ch='y';
        int ans,x=0;
        char content_id_requested=0;
        
        while(ch=='y'||ch=='Y')
        {
            cout<<"1.) Do you want to ENTER any content to the host?: "<<endl;
            cout<<"2.) Do you want to make a request for any content?:  "<<endl;
            cout<<"3.) Do you want to DELETE any content from the host:  "<<endl;
            cout<<"Your choice: ";
            cin>>ans;
            
            
            switch(ans)
            {
                case 1:
                    cout<<"";
                    //Enter the content_id input to the vector
                    int id;
                    cout << "Please enter the content_id to be inserted: " <<endl;
                    cin >> id;
                    ListOfContents.push_back(id);
                    break;
                
                case 2:
                    cout<<"";
                    //Request content procedure called
                    cout<<"Please input the content id: "<<endl;
                    cin>>content_id_requested;
                    x=atoi(&content_id_requested);
                    ind->receive_ind = '0';
                    ind->content_requested_id = content_id_requested;
                    MakeRequest(my_send_port, x);
                    break;
                    
                case 3:
                    //Delete the content_id input from the vector
                    int id_rec;
                    int i;
                    cout << "Please enter the content_id to be deleted: " <<endl;
                    cin >> id_rec;
                    for(i=0 ; i<ListOfContents.size(); i++){
                        if(ListOfContents.at(i) == id_rec){
                        
                            printf("Deleted Content %d\n",id_rec);
                            ListOfContents.erase(ListOfContents.begin()+i);
                            }
                        
                    }
                    break;
                    
                default:
                    cout<<"Please enter the proper choice."<<endl;
                    
            }
            
            cout<<endl<<"Do you want to continue? \n";
            cin>>ch;
        }
        
        pthread_join(thread1, NULL);
        pthread_join(thread2, NULL);
        
	} catch(const char *reason ){
		cerr << "Exception:" << reason << endl;
		return 0;
	}
    
    return 0;
}
