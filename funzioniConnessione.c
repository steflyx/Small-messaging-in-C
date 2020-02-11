#include "funzioniConnessione.h"

//Invia un intero tramite connessione TCP. Ritorna -1 in caso di errore, 0 altrimenti
int inviaTCPInt(int socket, int msg){
	int ret;
	uint16_t u_msg;

	u_msg = htons(msg);
	ret = send(socket, (void*)&u_msg, sizeof(uint16_t), 0);
	if(ret < sizeof(uint16_t)){
		perror("Errore di connessione: ");
		return -1;
	}

	return 0;
}

//Invia una stringa tramite connessione TCP. Ritorna -1 in caso di errore, 0 altrimenti
int inviaTCPStringa(int socket, char* msg){
	int ret, len;

	//Invio della dimensione della stringa
	len = strlen(msg)+1;
	if(inviaTCPInt(socket, len) == -1)
		return -1;

	//Invio della stringa
	ret = send(socket, (void*)msg, len, 0);
	if(ret < len){
		perror("Errore di connessione: ");
		return -1;
	}

	return 0;
}

//Si mette in attesa di ricevere un intero. Ritorna -1 in caso di errore, 0 se il sender si è disconnesso, 1 altrimenti
int riceviTCPInt(int socket, int* msg){
	uint16_t u_msg;
	int ret;

	ret = recv(socket, (void*)&u_msg, sizeof(uint16_t), MSG_WAITALL);

	//Errore di connessione
	if(ret == -1){
		perror("Errore di connessione: "); 
		return -1;
	}

	//Sender disconnesso
	if(ret == 0)
		return 0;
	
	//Tutto ok
	*msg = ntohs(u_msg);
	return 1;
}

//Si mette in attesa di ricevere una stringa. Ritorna -1 in caso di errore, 0 altrimenti
int riceviTCPStringa(int socket, char* msg){
	int lmsg, ret;

	//Ricezione della lunghezza della stringa
	if(riceviTCPInt(socket, &lmsg) == -1 )
		return -1;

	//Ricezione della stringa
	ret = recv(socket, (void*)msg, lmsg, MSG_WAITALL);
	if(ret == -1 || ret == 0){
		perror("Errore di connessione: ");
		return -1;
	}

	return 0;
}