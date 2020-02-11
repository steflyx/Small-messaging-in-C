#include <signal.h>
#include "funzioniConnessione.h"
#include "costanti.h"

//Costanti generiche
#define ARGOMENTI_NECESSARI 4
#define CARATTERE_TERMINAZIONE ".\n"

void inizializzaVariabiliConnessione(int*, int*, char*, char*, char**);
int apriConnessioneTCP(int, char*);
int inviaInformazioni(int, int, char*);
void stampaComandi();
void gestisciInterazioneUtente(int);
int riceviComando();
void registraUsername(int, char*, int*, char*);
void deregistra(int, int*, char*);
void mostraUtenti(int);
void inviaMessaggio(int, char*, char*, int);
void leggiMessaggio(char*, char*, int);
int inviaUDPStringa(char*, char*, int);
int apriConnessioneUDP(int);
void attendiMessaggioUDP(int);
void chiudiConnessioni(pid_t, int, int);

int main(int argc, char* argv[]){
	int porta_locale, porta_server, socket_server, ret, socket_UDP;
	char IP_locale[MAX_STRING_LENGTH], IP_server[MAX_STRING_LENGTH];

	//Controllo sugli argomenti passati all'avvio
	if(argc != ARGOMENTI_NECESSARI + 1){
		printf("Argomenti non validi\n");
		exit(0);		
	}

	//Inizializzazione porte e indirizzi IP
	inizializzaVariabiliConnessione(&porta_locale, &porta_server, IP_locale, IP_server, argv);
	
	//Connessione con il server
	socket_server = apriConnessioneTCP(porta_server, IP_server);
	if(socket_server == -1)
		exit(0);

	//Invio delle informazioni relative al client
	ret = inviaInformazioni(socket_server, porta_locale, IP_locale);
	if(ret == -1)
		exit(0);

	//Apertura della connessione UDP per la ricezione di messaggi
	socket_UDP = apriConnessioneUDP(porta_locale);
	if(socket_UDP == -1)
		exit(0);

	//Creazione di un processo figlio che ascolta i messaggi UDP
	pid_t pid = fork();
	if(pid == -1)
		exit(0);

	if(pid == 0)
		attendiMessaggioUDP(socket_UDP);

	//Da qui in poi, l'utente può usare il programma
	stampaComandi();
	gestisciInterazioneUtente(socket_server);

	//Gestione dell'uscita dal programma (chiusura dei socket e terminazione del figlio)
	printf("Client disconnesso\n");
	chiudiConnessioni(pid, socket_server, socket_UDP);

	return 0;
}

//Trasferisce i parametri passati dall'utente nelle variabili di connessione
void inizializzaVariabiliConnessione(int* porta_locale,int* porta_server,char* IP_locale,char* IP_server,char** argv){
	*porta_locale = atoi(argv[2]);
	*porta_server = atoi(argv[4]);
	strcpy(IP_locale, argv[1]);
	strcpy(IP_server, argv[3]);
}

//Apre una connessione con il server
int apriConnessioneTCP(int porta_server, char* IP_server){
	int sd, ret;
	struct sockaddr_in server_addr;

	//Creazione del socket per comunicare col server
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd == -1){
		perror("Errore creazione socket: ");
		return -1;
	}

	//Creazione della struttura con le info per il socket
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_port = htons(porta_server);
	server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, IP_server, &server_addr.sin_addr);

	//Connessione al server
	ret = connect(sd, (struct sockaddr*)&server_addr, (socklen_t)sizeof(server_addr));
	if(ret == -1){
		perror("Errore connect: ");
		return -1;
	}

	printf("Connessione al server %s (porta %d) effettuata con successo\n",IP_server, porta_server);

	return sd;
}

//Invia al server l'IP del client e la porta su cui è in attesa di messaggi UDP
int inviaInformazioni(int socket_server, int porta_locale, char* IP_locale){

	//Invio dell'IP
	if(inviaTCPStringa(socket_server, IP_locale) == -1)
		return -1;

	//Invio della porta
	if(inviaTCPInt(socket_server, porta_locale) == -1)
		return -1;

	return 0;
}

//Stampa a video tutti i comandi
void stampaComandi(){
	printf("\n");
	printf("Sono disponibili i seguenti comandi:\n");
	printf("!help --> mostra l'elenco dei comandi disponibili\n");
	printf("!register username --> registra il client presso il server\n");
	printf("!deregister --> de-registra il client presso il server\n");
	printf("!who --> mostra l'elenco degli utenti disponibili\n");
	printf("!send username --> invia un messaggio ad un altro utente\n");
	printf("!quit --> disconnette il client dal server ed esce\n");
	printf("\n");
}

//Gestisce i comandi richiesti dall'utente chiamando le funzioni opportune
void gestisciInterazioneUtente(int socket_server){
	int isRegistrato = NOT_REGISTERED;
	char username_inserito[MAX_STRING_LENGTH], username_locale[MAX_STRING_LENGTH];

	while(1){
		//Stampa del nome
		if(isRegistrato == REGISTERED)
			printf("%s", username_locale);
		printf(">");

		//Gestione del comando inserito dall'utente
		switch(riceviComando(username_inserito)){
			case HELP: 			stampaComandi(); break;
			case REGISTER:		registraUsername(socket_server, username_inserito, &isRegistrato, username_locale); break;
			case DEREGISTER:	deregistra(socket_server, &isRegistrato, username_locale); break;
			case SHOW_USER: 	mostraUtenti(socket_server); break;
			case SEND_MSG: 		inviaMessaggio(socket_server, username_inserito, username_locale, isRegistrato); break;
			case QUIT:			return;
			default: 			printf("Comando non valido\n");
		}
	}
}

//Attende che l'utente digiti un comando e lo gestisce
int riceviComando(char* username){
	char comando[MAX_STRING_LENGTH], comando_copia[MAX_STRING_LENGTH];
	char* comando_sup;
	fgets(comando, MAX_STRING_LENGTH, stdin);

	//Si elimina il carattere di invio per semplificare i controlli
	comando[strlen(comando)-1] = '\0';
	if(strlen(comando) == 0){
		printf("Inserire un comando\n");
		return 0;
	}

	//Il comando viene spezzato (nel caso si sia chiesta una register o una send)
	strcpy(comando_copia, comando);
	comando_sup = strtok(comando_copia, " ");
	strcpy(comando, comando_sup);
	comando_sup = strtok(NULL, " ");
	if(comando_sup != NULL)
		strcpy(username, comando_sup);
	
	//Si individua il comando inviato
	if(!strcmp(comando, "!help"))
		return HELP;
	if(!strcmp(comando, "!register"))
		return REGISTER;
	if(!strcmp(comando, "!deregister"))
		return DEREGISTER;
	if(!strcmp(comando, "!who"))
		return SHOW_USER;
	if(!strcmp(comando, "!send"))
		return SEND_MSG;
	if(!strcmp(comando, "!quit"))
		return QUIT;

	return -1;
}

//Permette di registrare un utente. Il protocollo è il seguente:
//	- Invio del codice relativo a questo comando
//	- Invio dell'username
//	- Il server risponde con un codice: UTENTE_GIA_REGISTRATO, NESSUN_MSG_IN_ATTESA, MSG_IN_ATTESA
//	- Il server procede ad inviare i messaggi offline, se ci sono
//Se l'utente si era già registrato, stampa un messaggio di errore
//Se tutto va bene si aggiorna il valore di usernamelocale e di isRegistrato
void registraUsername(int socket_server, char* username, int *isRegistrato, char *username_locale){
	int risposta;
	char msg[MAX_STRING_LENGTH];

	//Controlliamo se era già registrato
	if(*isRegistrato == REGISTERED){
		printf("Sei già registrato come %s\n", username_locale);
		return;
	}

	//Invio del codice associato alla registrazione
	if(inviaTCPInt(socket_server, REGISTER) == -1)
		return;

	//Invio del proprio username
	if(inviaTCPStringa(socket_server, username) == -1)
		return;

	//Ricezione della risposta del server
	if(riceviTCPInt(socket_server, &risposta) == -1)
		return;

	//Utente già esistente e online
	if(risposta == UTENTE_GIA_REGISTRATO){
		printf("Esiste un utente online con questo nome\n");
		return;
	}

	//Registrazione riuscita
	printf("Registrazione avvenuta con successo\n");
	strcpy(username_locale, username);
	*isRegistrato = REGISTERED;

	//Nessun messaggio da ricevere
	if(risposta == NESSUN_MSG_IN_ATTESA)
		return;

	//Ci sono messaggi offline da ricevere
	printf("Ci sono messaggi offline\n");

	//Ricezione dei messaggi offline
	if(riceviTCPStringa(socket_server, msg) == -1)
		return;
	printf("%s\n", msg);
}

//Deregistra l'utente. Il protocollo è il seguente:
//	- Invio del codice della richiesta
//	- Il server elimina l'utente
//La funzione pone 'isRegistrato' a NOT_REGISTERED se ha successo
void deregistra(int socket_server, int* isRegistrato, char* username){
	//Controllo sulla correttezza
	if(*isRegistrato == NOT_REGISTERED){
		printf("Bisogna prima essere registrati\n");
		return;
	}

	//Invio del codice
	if(inviaTCPInt(socket_server, DEREGISTER) == -1)
		return;

	//Assegnamento della variabile
	*isRegistrato = NOT_REGISTERED;

	printf("Deregistrazione avvenuta con successo\n");
}

//Chiede al server una lista degli utenti. Il protocollo è il seguente:
//	- Invio del codice della richiesta
//	- Ricezione di un messaggio contenente la lista degli utenti (già pronta da stampare)
void mostraUtenti(int socket){
	char lista[MAX_STRING_LENGTH];

	//Invio della richiesta
	if(inviaTCPInt(socket, SHOW_USER) == -1)
		return;

	//Ricezione della lista
	if(riceviTCPStringa(socket, lista) == -1)
		return;

	//Stampa della lista
	printf("Clienti registrati:\n");
	printf("%s\n", lista);
}

//Permette all'utente di inviare un messaggio ad un altro utente. Il protocollo è il seguente:
//	- Invio del codice della richiesta al server
//	- Invio dell'username destinatario
//	- Ricezione della risposta del server. Le possibilità sono
//		1)Username inesistente 
//		2)Utente offline 	   
//		3)Utente online        
//	Utente offline:
//		- Invio del messaggio al server
//	Utente online:
//		- Ricezione delle informazioni per la connessione
//		- Invio tramite UDP (username + messaggio)
void inviaMessaggio(int socket_server, char* username, char* username_locale, int isRegistrato){
	int risposta, porta_destinatario;
	char msg[MAX_STRING_LENGTH], IP_destinatario[MAX_STRING_LENGTH];

	//Controllo sulla registrazione
	if(isRegistrato == NOT_REGISTERED){
		printf("Bisogna prima essere registrati\n");
		return;
	}

	//Invio richiesta
	if(inviaTCPInt(socket_server, SEND_MSG) == -1)
		return;

	//Invio username destinatario
	if(inviaTCPStringa(socket_server, username) == -1)
		return;

	//Risposta del server
	if(riceviTCPInt(socket_server, &risposta) == -1)
		return;

	//Utente non esistente
	if(risposta == DESTINATARIO_NON_PRESENTE){
		printf("Impossibile connettersi a %s: utente inesistente.\n", username);
		return;		
	}

	//Immissione del messaggio da parte dell'utente
	leggiMessaggio(msg, username_locale, risposta);

	//Utente offline
	if(risposta == DESTINATARIO_OFFLINE){
		if(inviaTCPStringa(socket_server, msg) == -1)
			return;
		printf("Messaggio inviato offline.\n");
	}

	//Utente online
	if(risposta == DESTINATARIO_ONLINE){
		//Ricezione delle informazioni per contattare il destinatario
		if(riceviTCPStringa(socket_server, IP_destinatario) == -1)
			return;
		if(riceviTCPInt(socket_server, &porta_destinatario) == -1)
			return;

		//Invio del messaggio
		if(inviaUDPStringa(msg, IP_destinatario, porta_destinatario) == -1)
			return;
		printf("Messaggio istantaneo inviato. IP %s, porta %d\n", IP_destinatario, porta_destinatario);
	}
}

//Legge un messaggio digitato da tastiera, fermandosi quando l'utente inserisce un punto singolo nella riga
//Il messaggio completo è già pronto per essere inviato sulla chat
void leggiMessaggio(char *msg_completo, char *username_locale, int stato_destinatario){
	char riga[MAX_STRING_LENGTH], msg[MAX_STRING_LENGTH];

	printf("Puoi digitare il messaggio\n");

	//E' meglio raccogliere tutto in un unico messaggio
	while(1){
		fgets(riga, MAX_STRING_LENGTH, stdin);
		if(strcmp(riga, CARATTERE_TERMINAZIONE) == 0)
			break;
		strcat(msg, riga);
	}

	//Incapsulamento del messaggio con l'aggiunta di username e stato	
	strcpy(msg_completo, "");
	strcpy(msg_completo, username_locale);
	if(stato_destinatario == DESTINATARIO_ONLINE)
		strcat(msg_completo, " (messaggio istantaneo)> ");
	else strcat(msg_completo, " (messaggio offline)> ");
	strcat(msg_completo, msg);
}

//Invia una stringa con protocollo UDP. Ritorna -1 in caso di errore, 0 altrimenti
int inviaUDPStringa(char *msg, char *IP_destinatario, int porta_destinatario){
	int ret, sd, len;
	uint16_t lmsg;
	struct sockaddr_in dest_addr;

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd == -1){
		perror("Errore invio messaggio UDP: ");
		return -1;
	}

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET ;
	dest_addr.sin_port = htons(porta_destinatario);
	inet_pton(AF_INET, IP_destinatario, &dest_addr.sin_addr);

	//Invio dimensione della stringa
	len = strlen(msg) + 1;
	lmsg = htons(len);
	ret = sendto(sd, (void*)&lmsg, sizeof(uint16_t), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
	if(ret < sizeof(uint16_t)){
		perror("Errore invio messaggio UDP: ");
		return -1;
	}

	//Invio della stringa
	ret = sendto(sd, msg, len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
	if(ret < len){
		perror("Errore invio messaggio UDP: ");
		return -1;
	}

	return 0;
}

//Apre una connessione UDP sulla porta indicata. Ritorna il socket creato in caso di successo, -1 altrimenti
int apriConnessioneUDP(int porta){
	int ret, sd;	
	struct sockaddr_in local_addr;
	
	//Inizializzazione della connessione
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd == -1){
		perror("Errore apertura socket UDP: ");
		return -1;
	}
	
	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(porta);
	local_addr.sin_addr.s_addr = INADDR_ANY;
	ret = bind(sd, (struct sockaddr*)&local_addr, sizeof(local_addr));
	if(ret == -1){
		perror("Errore apertura socket UDP: ");
		return -1;
	}

	printf("Ricezione messaggi istantanei su porta %d\n", porta);

	return sd;
}

//Mette il processo in attesa di un messaggio UDP
void attendiMessaggioUDP(int socket_UDP){
	int ret, len;
	uint16_t lmsg;
	uint32_t indirizzo_iniziale, indirizzo_finale;
	char buf[MAX_STRING_LENGTH];
	struct sockaddr_in sender_addr;
	int addrlen = sizeof(sender_addr);

	//Attesa sulla porta UDP
	while(1) {
		//Ricezione della dimensione del messaggio
		ret = recvfrom(socket_UDP, (void*)&lmsg, sizeof(lmsg), 0, (struct sockaddr*)&sender_addr, (socklen_t*)&addrlen);
		if(ret < sizeof(lmsg))
			continue;
		len = ntohs(lmsg);
		indirizzo_iniziale = sender_addr.sin_addr.s_addr;

		//Ricezione del messaggio
		ret = recvfrom(socket_UDP, buf, len, 0, (struct sockaddr*)&sender_addr, (socklen_t*)&addrlen);
		if(ret < len)
			continue;
		indirizzo_finale = sender_addr.sin_addr.s_addr;

		//Controlliamo se per caso si sono mischiati i messaggi da parte di due client diversi
		if(indirizzo_iniziale != indirizzo_finale)
			continue;

		printf("\n\n");
		printf("%s\n", buf);
		printf("Continuare a digitare il comando di prima\n");
	}
}

//Chiude tutti i socket e uccide il processo figlio
void chiudiConnessioni(pid_t processoFiglio, int socket_server, int socket_UDP){
	kill(processoFiglio, SIGKILL);
	close(socket_server);
	close(socket_UDP);	
}