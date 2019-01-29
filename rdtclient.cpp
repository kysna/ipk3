/* RDTCLIENT, Michal Kysilko, xkysil00 */

#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include "udt.h"



#define WINDOW_SIZE 10
#define MAXLINE 500
#define DATA_SIZE 300


using namespace std;

in_addr_t remote_addr = 0x7f000001;
in_port_t remote_port = 0;
in_port_t local_port = 0;

typedef struct packets
{
	string xml;
	bool ack;
	int timer;


} Tpckt;


void kill_it_with_fire(int signum)
{
	cerr << "Ukonceno signalem: " << signum << endl;
	exit(EXIT_SUCCESS);
}


//zpracovani parametru programu
int getparam(int argc,char **argv){
	

	if((argc == 2) and (strcmp(argv[1],"-h") == 0)){
		cout << "HELP:" << endl <<"./rdtclient –s source_port –d dest_port" << endl; 
		exit(EXIT_SUCCESS);
	}

	char c;
	while ((c = getopt(argc, argv, "s:d:")) != -1) {
		switch (c) {
			case 's':
				local_port = atoi(optarg);
 				break;
			case 'd':
				remote_port = atoi(optarg);			
				break;
			case '?':
				break;
			default: 
				break;
		}
	}

	if((local_port == 0) or (remote_port == 0)){
		cerr << "Chybi parametr source port nebo destination port." << endl;
		return -1;
	}
	return 0;
}

//vrati id paketu
int read_packet_id(string packet){
	
	int id = -1;
	packet = packet.substr(packet.find("<header sn=") + string("<header sn=").size());
	packet = packet.substr(0, packet.find(" pc="));
	id = atoi(packet.c_str()); 
	return id;
}

//vytvori paket k odeslani
string create_xml(string a, int packet_id, unsigned pc){
	
	string packet;
	char packetid[128];
	char packetcount[128];
	sprintf(packetid, "%d", packet_id);
	sprintf(packetcount, "%d", pc);
	packet = "<rdt-segment id=\"xkysil00\"><header sn=" + string(packetid) + " pc=" + packetcount + "></header><data>"+ a +"</data></rdt-segment>";
	return packet;

}



int main(int argc, char **argv){


	signal(SIGINT, kill_it_with_fire);
	signal(SIGTERM, kill_it_with_fire);

	string xml;
	string input_part = "";
	string input = "";
	int mysocket;
	char msg[MAXLINE];
	memset(msg, 0, MAXLINE);

	if(getparam(argc,argv) != 0)
		return -1;


	mysocket = udt_init(local_port);	
		
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	fd_set fd;
	FD_ZERO(&fd);
	FD_SET(mysocket, &fd);
	FD_SET(STDIN_FILENO, &fd);


	vector<Tpckt> ready_packets;
	vector<Tpckt> sliding_window;


	int packet_id = 1;
	int current_packet_id = 1; //index posledniho potvrzeneho packetu
	Tpckt tmp;
	int status;
	bool no_packets_left = false;


	//precte vstup
	while(cin){
		getline(cin, input_part); //nacte radek a ulozi do promenne
		input = input + input_part + "\n"; //z radku vytvori jeden celistvy string
	}
	input = input.substr(0, input.size()-1); //smaze posledni odradkovani 

	vector<string> text_data;

	//vytvori pakety
	while(!input.empty()){
		input_part = input.substr(0, DATA_SIZE); //vezme input[0] az input[MAXLINE] a ulozi je do jinne promenne

		//posune zacatek stringu tam, kde jsme skoncili (input[MAXLINE])
		input = input.substr(input.size() < DATA_SIZE ? input.size() : DATA_SIZE);
		text_data.push_back(input_part);
	}
	
	for(size_t k=0; k<text_data.size(); k++){
		if(k == (text_data.size()-1))
			xml = create_xml(text_data[k], 0, text_data.size()); //posledni packet ma id 0
		else
			xml = create_xml(text_data[k], packet_id, text_data.size());
		
		tmp.timer = -1;
		tmp.xml = xml;
		tmp.ack = false;
		ready_packets.push_back(tmp);
		packet_id++;
	}


	while(select(mysocket+1, &fd, NULL, NULL, NULL)){


		//cteni zprav od serveru - ACK
		if(FD_ISSET(mysocket, &fd)){
			status = udt_recv(mysocket, msg, MAXLINE, NULL, NULL);	
			msg[status] = 0;

			if(read_packet_id(msg) == 1){
				sliding_window.clear();
			}
			else if(read_packet_id(msg) == 2){  //prislo ACK na posledni packet -> done
				close(mysocket);
				return 0;
			}
			else{
				for(size_t k=0; k< sliding_window.size(); k++){
					sliding_window[k].timer = -1;
				}	
			}	
		}

		//naplnovani sliding window, pokud neni plne a pokud je cim naplnovat
		if((sliding_window.size() < WINDOW_SIZE) && (no_packets_left != true)){
			for(unsigned i=0; i< WINDOW_SIZE; i++){

				sliding_window.push_back(ready_packets[current_packet_id-1]);
				if(read_packet_id(ready_packets[current_packet_id-1].xml) == 0){ //posledni paket
					no_packets_left = true;
					break;
				}
				current_packet_id++;
			}
		}


		//odesilani obsahu sliding window
		if((sliding_window.size() == WINDOW_SIZE) || 
		  ((ready_packets.size() - current_packet_id) < WINDOW_SIZE)){

			for(size_t i=0; i<sliding_window.size(); i++){ 

				//paketu jeste neprislo ack a nema timer nebo mu vyprsel
				if(sliding_window[i].ack == false &&
				   ((sliding_window[i].timer == -1) || ((sliding_window[i].timer + 2) < time(NULL)))){
					sliding_window[i].timer = time(NULL);
					if (!udt_send(	mysocket, 
							remote_addr, 
							remote_port, 
							(void *)sliding_window[i].xml.c_str(), 
							sliding_window[i].xml.size())) {
						cerr << "Doslo k chybe pri odesilani zpravy." << endl;
					}
				}
			}
		}

		FD_ZERO(&fd);
		FD_SET(mysocket, &fd);
		FD_SET(STDIN_FILENO, &fd);
		memset(msg, 0, MAXLINE);	
	}

	close(mysocket);
	return 0;
}
