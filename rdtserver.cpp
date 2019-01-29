/* RDTSERVER, Michal Kysilko, xkysil00 */

#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <vector>
#include <signal.h>
#include "udt.h"


#define WINDOW_SIZE 10
#define MAXLINE 500


using namespace std;

in_addr_t remote_addr = 0x7f000001;
in_port_t remote_port = 0;
in_port_t local_port = 0;


void kill_it_with_fire(int signum)
{
	cerr << "Ukonceno signalem: " << signum << endl;
	exit(EXIT_SUCCESS);
}

//vrati id packetu
int read_packet_count(string packet){
	
	int count = -1;
	packet = packet.substr(packet.find("pc=") + string("pc=").size());
	packet = packet.substr(0, packet.find("></header>"));
	count = atoi(packet.c_str()); 
	return count;
}

//vrati id packetu
int read_packet_id(string packet){
	
	int id = -1;
	packet = packet.substr(packet.find("<header sn=") + string("<header sn=").size());
	packet = packet.substr(0, packet.find("></header>"));
	id = atoi(packet.c_str()); 
	return id;
}

//vrati data, ktera paket nese
string read_packet_data(string packet){
	
	packet = packet.substr(packet.find("<data>") + string("<data>").size());
	packet = packet.substr(0, packet.find("</data>")); 
	return packet;
}

//vrati vytvoreny ACK paket
string create_xml(int packet_id){
	
	string packet;
	char packetid[128];
	sprintf(packetid, "%d", packet_id);
	packet = "<rdt-segment id=\"xkysil00\"><header sn=" + string(packetid) + "></header><data></data></rdt-segment>";
	return packet;

}

//zpracovani parametru programu
int getparam(int argc,char **argv){


	if((argc == 2) and (strcmp(argv[1],"-h") == 0)){
		cout << "HELP:" << endl <<"./rdtserver –s source_port –d dest_port" << endl; 
		exit(EXIT_SUCCESS);
	}

	int c;
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

	if((local_port == 0) || (remote_port == 0)){
		cerr << "Chybi parametr source port nebo destination port." << endl;
		return -1;
	}
	return 0;
}

int main(int argc, char **argv){

	signal(SIGINT, kill_it_with_fire);
	signal(SIGTERM, kill_it_with_fire);


	int mysocket;
	int status = 0;
	char msg[MAXLINE];
	memset(msg, 0, MAXLINE);

	if(getparam(argc,argv) != 0){
		return -1;
	}
	
	mysocket = udt_init(local_port);

	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

	fd_set fd;
	FD_ZERO(&fd);
	FD_SET(mysocket, &fd);
	FD_SET(STDIN_FILENO, &fd);


	int counter = 0;
	int sn = -1;
	string result = "";
	string response;
	string tmp;


	vector<int> accepted_sn;
	vector<string> packets;
	int round = 1;
	bool end = false;
	bool already_accepted = false;
	string last;
	int packets_total = 0;

	while (select(mysocket+1, &fd, NULL, NULL, NULL)){



		if (FD_ISSET(mysocket, &fd)){
			status = udt_recv(mysocket, msg, MAXLINE, NULL, NULL);
			msg[status] = 0;

			packets_total = read_packet_count(msg);
			sn = read_packet_id(msg);

			for(int i=0; i<round*WINDOW_SIZE; i++){
				if(sn == i){
					for(size_t k=0; k<accepted_sn.size(); k++){
						if(accepted_sn[k] == sn)
							already_accepted = true;
					}
					if(already_accepted != true){	//prijima pouze pakety, ktere jeste nema
						accepted_sn.push_back(sn);
						packets.push_back(msg);
						counter++;
					}
					else
						already_accepted = false;
						if(counter == packets_total){
						end = true;
					}
				}	
			}
 

			round++;			

			if((counter % WINDOW_SIZE) == 0 || end == true){
				if(end == true)	//posledni paket
					response = create_xml(2);

				else if((packets.size() % WINDOW_SIZE) == 0) //prisly vsechny pakety
					response = create_xml(1);

				else  //nektere pakety chybi
					response = create_xml(0);	


				if (!udt_send(	mysocket, 
						remote_addr, 
						remote_port, 
						(void *)response.c_str(), 
						response.size())) {
					cerr << "Doslo k chybe pri odesilani zpravy." << endl;
				}


				if(end == true){ //tisk a reinicializace
;
					for(size_t j=0; j<packets.size(); j++){
						for(size_t i=0; i<packets.size(); i++){
							if((unsigned)read_packet_id(packets[i]) == j+1){
								tmp = read_packet_data(packets[i]);
								fwrite(tmp.c_str(), tmp.length(), 1, stdout);
							}
							if((unsigned)read_packet_id(packets[i]) == 0){
								last = read_packet_data(packets[i]);
							}							
						}
					}
					fwrite(last.c_str(), last.length(), 1, stdout);

					fflush(stdout);
					result = "";
					accepted_sn.clear();
					packets.clear();
					counter = 0;	
					end = false;	
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
