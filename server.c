#include <pthread.h>
#include "funzioniConnessione.h"
#include "costanti.h"

//Costanti generiche
#define DIMENSIONE_CODA_RICHIESTE 10

struct userInfo{
	char username[100];
	char IP[100];
	int porta;
	int stato;
	char messaggi_offline[100];
	struct userInfo *next;
};

struct userInfo *lista_utenti;
pthread_mutex_t utilizzo_lista;

int creaSocketAscolto(int);
void attendiRichieste(int);
void* gestisciConnessione(void*);
int riceviInformazioni(int, char*, int*);
int registraUsername(int, char*, int, char*, int*);
int deregistra(int, char*, int*);
int mostraUtenti(int);
int gestisciMessaggi(int);
int aggiungiUser(char*, char*, int, char*);
void eliminaUser(char*);
void recuperaListaUtenti(char*);
int checkLista(char*, char*, int*, struct userInfo**);
void aggiungiMessaggioOffline(struct userInfo*, char*);
void mettiOffline(char*);

int main(int argc, char* argv[]){
	int porta_locale, sd;

	//Inizializzazione della lista utenti
	lista_utenti = NULL;

	//Si ricava la porta su cui bisogna essere in ascolto
	porta_locale = atoi(argv[1]);

	//Creazione del socket
	sd = creaSocketAscolto(porta_locale);
	if(sd == -1)
		exit(0);

	//Mettiamo il server in attesa di richieste da gestire
	attendiRichieste(sd);

	return 0;
}

//Crea un socket su cui il server aspetta richieste di connessione
int creaSocketAscolto(int porta){
	int sd, ret;
	struct sockaddr_in server_addr;

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd == -1){
		perror("Errore creazione socket: ");
		return -1;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET ;
	server_addr.sin_port = htons(porta);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(ret == -1){
		perror("Errore bind: ");
		return -1;
	}

	ret = listen(sd, DIMENSIONE_CODA_RICHIESTE);
	if(ret == -1){
		perror("Errore listen: ");
		return -1;
	}

	printf("Creazione del socket di ascolto (%d)\n", sd);

	return sd;
}

//Attende che arrivi una richiesta sul socket d'ascolto e crea un thread per gestirla
void attendiRichieste(int socket_ascolto){
	int ret, new_socket, len;
	struct sockaddr_in client_addr;
	pthread_t thread_id;

	len = sizeof(client_addr);

	//Inizializzazione della variabile condition
	pthread_mutex_init(&utilizzo_lista, NULL);

	while(1){
		new_socket = accept(socket_ascolto, (struct sockaddr*)&client_addr, (socklen_t*)&len);
		if(new_socket == -1){
			perror("Errore accept: ");
			continue;
		}

		printf("Nuova richiesta di connessione\n");

		ret = pthread_create(&thread_id, NULL, gestisciConnessione, (void*)&new_socket);
		if(ret == -1){
			printf("Errore creazione thread\n");
			continue;
		}
	}	

	printf("Chiusura del server\n");
	close(socket_ascolto);
}

//Gestisce le connessioni con i client rispondendo alle varie richieste di questi
//Da notare che, se il client si disconnette in un qualsiasi momento, il server farà comunque la riceviTCPInt che in tal caso restituirà 0
void* gestisciConnessione(void* socket){
	int sd = *(int*)socket, comando, ret, porta_client, isRegistrato = NOT_REGISTERED;
	char IP_client[MAX_STRING_LENGTH], username[MAX_STRING_LENGTH];

	//Riceviamo le informazioni relative al client
	if(riceviInformazioni(sd, IP_client, &porta_client) == -1){
		printf("Connessione persa\n");
		close(sd);
		pthread_exit(NULL);
	}

	printf("Accettata connessione da %s (porta %d)\n", IP_client, porta_client);

	//Aspettiamo un comando dal client, dopodiché lo si esegue
	while(1){
		//Aspettiamo che il client invii il codice relativo alla sua richiesta
		ret = riceviTCPInt(sd, &comando);
		if(ret == -1 || ret == 0)
			break;

		//Individuiamo la funzione opportuna da chiamare
		switch(comando){
			case REGISTER:		ret = registraUsername(sd, IP_client, porta_client, username, &isRegistrato); break;
			case DEREGISTER:	ret = deregistra(sd, username, &isRegistrato); break;
			case SHOW_USER: 	ret = mostraUtenti(sd); break;
			case SEND_MSG: 		ret = gestisciMessaggi(sd); break;
			default: 			ret = -1; break;
		}
		if(ret == -1)
			break;
	}

	if(isRegistrato == REGISTERED)
		mettiOffline(username);

	printf("Client disconnesso (socket %d)\n", sd);
	close(sd);
	pthread_exit(NULL);
}

//Riceve IP e porta del client. Ritorna -1 in caso di errore, 0 altrimenti
int riceviInformazioni(int sd, char *IP_client, int *porta_client){
	if(riceviTCPStringa(sd, IP_client) == -1)
		return -1;
	if(riceviTCPInt(sd, porta_client) == -1)
		return -1;
	return 0;
}

//Realizza il protocollo per registrare l'username lato server. 
//Ritorna -1 in caso di errore, 0 altrimenti
//In caso di successo setta username e isRegistrato
int registraUsername(int sd, char *IP_client, int porta_client, char *username, int *isRegistrato){
	int isOk;
	char msg[MAX_STRING_LENGTH];

	printf("Richiesta register (socket %d)\n", sd);

	//Riceviamo l'username dell'utente
	if(riceviTCPStringa(sd, username) == -1)
		return -1;

	//Aggiungiamo l'user alla lista degli utenti
	isOk = aggiungiUser(username, IP_client, porta_client, msg);

	//L'utente esiste già ed è online
	if(isOk == UTENTE_GIA_REGISTRATO){
		printf("Rifiutata register per l'username %s perche' gia' online\n", username);
		if(inviaTCPInt(sd, UTENTE_GIA_REGISTRATO) == -1)
			return -1;
		return 0;
	}

	//La registrazione è andata a buon fine
	printf("Registrato utente %s\n", username);
	*isRegistrato = REGISTERED;

	//Non ci sono messaggi offline
	if(strlen(msg) == 0){
		if(inviaTCPInt(sd, NESSUN_MSG_IN_ATTESA) == -1)
			return -1;
		printf("Nessun messaggio da inviare a %s\n", username);
		return 0;
	}

	//Invio di messaggi offline
	if(inviaTCPInt(sd, MSG_IN_ATTESA) == -1) //Il client si aspetta questo codice
		return -1;
	if(inviaTCPStringa(sd, msg) == -1)
		return -1;
	printf("Inviati messaggi offline a %s\n", username);

	return 0;
}

//Realizza il lato server del protocollo di deregistrazione
int deregistra(int sd, char *username, int *isRegistrato){

	printf("Richiesta deregistrazione da parte di %s (socket %d)\n", username, sd);

	//Eliminazione dell'utente dalla lista
	eliminaUser(username);

	//Aggiornamento delle variabili di connessione
	printf("Deregistrato user %s\n", username);
	*isRegistrato = NOT_REGISTERED;

	return 0;
}

//Realizza il lato server del protocollo per la visualizzazione dei client. Ritorna -1 in caso di errore, 0 altrimenti
int mostraUtenti(int sd){
	char lista[MAX_STRING_LENGTH];

	printf("Richiesta una !who (socket %d)\n", sd);

	//Prepariamo il messaggio
	recuperaListaUtenti(lista);

	//Inviamo la lista
	if(inviaTCPStringa(sd, lista) == -1)
		return -1;

	printf("Gestita !who (socket %d)\n", sd);
	return 0;
}

//Realizza il lato server del protocollo per inviare messaggi fra utenti. Ritorna -1 in caso di errore, 0 altrimenti
//Da notare il fatto che la lista deve rimanere bloccata dal momento in cui cerco se l'username è presente
//al momento in cui o aggiungo un messaggio offline o mi rendo conto che non mi serve più la lista 
int gestisciMessaggi(int sd){
	int risposta, porta_destinatario;
	char username[MAX_STRING_LENGTH], msg[MAX_STRING_LENGTH], IP_destinatario[MAX_STRING_LENGTH];
	struct userInfo *user;

	//Ricezione dell'username destinatario
	if(riceviTCPStringa(sd, username) == -1)
		return -1;

	printf("Richiesta informazioni per una !send a %s (socket %d)\n", username, sd);

	//Controllo della lista
	pthread_mutex_lock(&utilizzo_lista);
	risposta = checkLista(username, IP_destinatario, &porta_destinatario, &user);
	
	//Invio del risultato
	if(inviaTCPInt(sd, risposta) == -1){
		pthread_mutex_unlock(&utilizzo_lista);
		return -1;
	}

	//Destinatario non esiste
	if(risposta == DESTINATARIO_NON_PRESENTE)
		printf("Send rifiutata: l'utente %s non esiste\n", username);

	//Destinatario offline
	if(risposta == DESTINATARIO_OFFLINE){
		//Ricezione del messaggio offline
		if(riceviTCPStringa(sd, msg) == -1){
			pthread_mutex_unlock(&utilizzo_lista);
			return -1;
		}
		aggiungiMessaggioOffline(user, msg);

		printf("Utente %s offline, archiviato messaggio\n", username);
	}

	//A questo punto abbiamo finito sicuramente di usare la lista
	pthread_mutex_unlock(&utilizzo_lista);

	//Destinatario online
	if(risposta == DESTINATARIO_ONLINE){
		//Invio delle informazioni
		if(inviaTCPStringa(sd, IP_destinatario) == -1)
			return -1;
		if(inviaTCPInt(sd, porta_destinatario) == -1)
			return -1;

		printf("Utente %s online, inviate informazioni\n", username);
	}

	return 0;
}




/*FUNZIONI SULLA LISTA UTENTI*/


//Aggiunge username alla lista dei client
//Ritorna 0 se l'username esiste già ed è online, 1 altrimenti
//Se ci sono messaggi offline, li copia in msg e li cancella
//IMPORTANTE: la lista è condivisa fra i thread, quindi questa funzione va eseguita in mutua esclusione
int aggiungiUser(char* username, char* IP_user, int porta_user, char *msg){
	struct userInfo *punt = lista_utenti;
	int ret;

	pthread_mutex_lock(&utilizzo_lista);

	//Scorriamo la lista fino a trovare un username corrispondente o fino alla fine
	for(; punt != NULL; punt=punt->next)
		if(strcmp(username, punt->username) == 0)
			break;

	//L'username esiste già
	if(punt != NULL){
		if(punt->stato == ONLINE)
			ret = UTENTE_GIA_REGISTRATO;
		else {
			punt->stato = ONLINE;
			strcpy(msg, punt->messaggi_offline);
			strcpy(punt->messaggi_offline, "");
			ret = REGISTRAZIONE_OK;
		}
	}
	else{
		//L'username non esiste
		punt = malloc(sizeof(struct userInfo));
		strcpy(punt->username, username);
		strcpy(punt->IP, IP_user);
		punt->porta = porta_user;
		punt->stato = ONLINE;
		strcpy(punt->messaggi_offline, "");
		punt->next = lista_utenti; //Inserimento in testa
		lista_utenti = punt;
		strcpy(msg, "");
		ret = REGISTRAZIONE_OK;
	}

	pthread_mutex_unlock(&utilizzo_lista);
	return ret;
}

//Elimina l'user associato a questo nome
//Come 'aggiungiUser' va eseguita in mutua esclusione
void eliminaUser(char* username){
	struct userInfo *punt = lista_utenti, *sup = NULL;
	pthread_mutex_lock(&utilizzo_lista);

	//Scorrimento della lista
	for(; punt != NULL; punt=punt->next){
		if(strcmp(username, punt->username) == 0)
			break;
		sup = punt;
	}

	//Non si è trovato l'user
	if(punt == NULL){
		pthread_mutex_unlock(&utilizzo_lista);
		return;
	}

	//Eliminazione dell'utente
	if(sup != NULL)
		sup->next = punt->next;
	else lista_utenti = punt->next;

	free(punt); 
	pthread_mutex_unlock(&utilizzo_lista);
}

//Inserisce in 'lista' la lista di tutti gli utenti già pronta per l'invio al client
//Come 'aggiungiUser' va eseguita in mutua esclusione
void recuperaListaUtenti(char *lista){
	struct userInfo *punt = lista_utenti;
	strcpy(lista, "");
	pthread_mutex_lock(&utilizzo_lista);

	//Scorrimento degli utenti e progressivo aggiornamento della lista
	for(; punt != NULL; punt = punt->next){
		strcat(lista, "\t");
		strcat(lista, punt->username);
		if(punt->stato == ONLINE)
			strcat(lista, " (online)\n");
		else strcat(lista, " (offline)\n");
	}

	pthread_mutex_unlock(&utilizzo_lista);
}

//Mette l'utente in stato offline
//Come 'aggiungiUser' va eseguita in mutua esclusione
void mettiOffline(char *username){
	struct userInfo *punt = lista_utenti;
	pthread_mutex_lock(&utilizzo_lista);

	//Scorrimento degli utenti
	for(; punt != NULL; punt = punt->next)
		if(strcmp(username, punt->username) == 0)
			break;

	punt->stato = OFFLINE;
	
	pthread_mutex_unlock(&utilizzo_lista);
}

//Controlla se esiste 'username'. Se esiste ed è online, imposta le variabili IP e porta. Se esiste ed è offline, imposta solo user
//Ritorna 0 se non esiste, 1 se esiste ma è offline, 2 altrimenti
//Le due funzioni successive vanno eseguite in mutua esclusione, ma a differenza delle altre, qui non è gestita internamente
int checkLista(char* username, char* IP_destinatario, int* porta_destinatario, struct userInfo **user){
	*user = lista_utenti;

	//Scorriamo la lista
	for(; *user != NULL; *user = (*user)->next)
		if(strcmp((*user)->username, username) == 0)
			break;

	//L'utente non esiste
	if(*user == NULL)
		return DESTINATARIO_NON_PRESENTE;

	//Esiste ma è offline
	if((*user)->stato == OFFLINE)
		return DESTINATARIO_OFFLINE;

	//Esiste ed è online
	strcpy(IP_destinatario, (*user)->IP);
	*porta_destinatario = (*user)->porta;
	return DESTINATARIO_ONLINE;
}

//Aggiunge un messaggio offline a 'user'
//Da eseguire in mutua esclusione
void aggiungiMessaggioOffline(struct userInfo *user, char *msg){
	strcat(user->messaggi_offline, "\n");
	strcat(user->messaggi_offline, msg);
}