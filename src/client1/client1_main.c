/*
* TEMPLATE
*/
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h> // timeval
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h> // inet_aton()
#include <sys/un.h> // unix sockets
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h> // SCNu16
#include <fcntl.h>
#include "../sockwrap.h"
#include "../errlib.h"

#define MAXBUFFER 1024
#define debug 0 //debug = 1 for debug mode

char *prog_name = "Client_Transfer";
int sock_cpy = -1;
FILE *fp = NULL;

/*****************************
*       CLIENT TCP          *
* -invia nome file          *
* -lo riceve dal server     *
* -salva nella repository   *
* -stampa stdout messaggio  *
* -timeout 15 secondi       *
*****************************/

void handlersigpipe(int sig);
void handlersigint(int sig);
int new_connection(char *address, char *port);

int main (int argc, char *argv[])
{
  int s;
  char buffer[MAXBUFFER], buffer2[MAXBUFFER];
  uint32_t dim, dim_p, time;
  int i, k, x, flag = 0;

  /***********************
  Parametri per la Select
  ************************/
  ssize_t n;
  struct timeval tval;
  fd_set cset;

  /********************
  Controllo Parametri
  ********************/
  if(argc < 4){
    printf("Client - [ERROR] Numero parametro scorretto\n");
    return -1;
  }
  if(atoi(argv[2]) < 1024){
    printf("Client - [ERROR] Porta non valida\n");
    return -2;
  }

  if(debug == 1)
    printf("Client UP!\nPolitecnico di Torino\nAuthor: Davide Antonino Giorgio\nNumber: s262709\nversion: 3.6\n\n");

  signal(SIGPIPE, handlersigpipe);
  signal(SIGINT, handlersigint);
  //start connection. 's' is the socket.
  sock_cpy = s = new_connection(argv[1], argv[2]);

  /*************************
  * Transfer phase - LOOP *
  *************************/
  if(debug == 1)
    printf("argc: %d\n\n", argc);
  for(i = 3; i < argc; i++){

    /*****************
    Imposto il timer
    ******************/
    int t = 15;
    tval.tv_sec = t;
    tval.tv_usec = 0;

    FD_ZERO(&cset);        //azzero il vettore del file descriptor
    FD_SET(s, &cset);      //imposto la casella corrispondente al socket

    /***********************
    * Sending start string *
    ************************/
    memset(buffer, '\0', MAXBUFFER);
    sprintf(buffer, "GET %s\r\n", argv[i]);                //creo la stringa per il get
    if(debug == 1)
      printf("DEBUG - Client - sto facendo la sendn di '%s'\n", buffer);
    if( sendn(s, buffer, strlen(buffer), MSG_NOSIGNAL) < ((ssize_t) strlen(buffer)) ){
      printf("(%s) - Error at 'sending start string'\n", prog_name);
      exit(1);
    }
    if(debug == 1)
      printf("DEBUG - Client - Stringa inizio trasferimento inviata al server: '%s'\nstrlen: %ld\n", buffer, strlen(buffer));

    /***************************
    * Reading server response *
    ***************************/
    memset(buffer, '\0', MAXBUFFER);

    if((n = select(FD_SETSIZE, &cset, NULL, NULL, &tval)) == -1){
      close(s);
      err_sys ("(%s) error - select() failed", prog_name);
    }
    if(n > 0){
      if ( (n = readn(s, buffer, 5)) < 0 ){
        close(s);
        err_sys ("(%s) error - readn() failed", prog_name);
      }
    }else{
      close(s);
      err_quit("No response after %d seconds\n", t);
    }

    /*****************
    Imposto il timer
    ******************/
    t = 15;
    tval.tv_sec = t;
    tval.tv_usec = 0;
    FD_ZERO(&cset);        //azzero il vettore del file descriptor
    FD_SET(s, &cset);      //imposto la casella corrispondente al socket

    /******************************
    * Controllo stringa iniziale *
    ******************************/
    if (strcmp(buffer, "-ERR\r") == 0) {
      if((n = select(FD_SETSIZE, &cset, NULL, NULL, &tval)) == -1){
        close(s);
        err_sys ("(%s) error - select() failed", prog_name);
      }
      if(n > 0){
        if ( (n = readn(s, buffer, 1)) < 0 ){
          close(s);
          err_sys ("(%s) error - readn() failed", prog_name);
        }
      }else{
        close(s);
        err_quit("No response after %d seconds\n", t);
      }

      if (buffer[0] == '\n') {
        printf("Client - '-ERR' from Server\n");
        break;
      }
    }
    if(debug == 1)
      printf("DEBUG - Client - Stringa inizio trasferimento ricevuta dal server: '%s'\n", buffer);
    if(strcmp(buffer, "+OK\r\n") != 0){
      printf("Client - Error at start protocol string\n");
      return -5;
    }
    if(debug == 1)
      printf("DEBUG - Client - controllo stringa iniziale passato\n");


    /*****************
    Imposto il timer
    ******************/
    t = 15;
    tval.tv_sec = t;
    tval.tv_usec = 0;
    FD_ZERO(&cset);        //azzero il vettore del file descriptor
    FD_SET(s, &cset);      //imposto la casella corrispondente al socket

    /*****************************
    *  Lettura dimensione file  *
    *****************************/
    if((n = select(FD_SETSIZE, &cset, NULL, NULL, &tval)) == -1)
      err_sys ("(%s) error - select() failed", prog_name);
    if(n > 0){
      if ( (n = readn(s, &dim, sizeof(dim))) < 0 ){
        close(s);
        err_sys ("(%s) error - readn() failed", prog_name);
      }
    }else{
      close(s);
      err_quit("No response after %d seconds\n", t);
    }

    if(debug == 1)
      printf("DEBUG - Client - dimensione buffer ricevuto %lu\n", (unsigned long) dim);
    dim = ntohl(dim);
    dim_p = dim;
    if(debug == 1)
      printf("DEBUG - Client - dimensione buffer ricevuto con ntohl %lu\n", (unsigned long) dim);

    if(debug == 1)
      printf("DEBUG - Client - Filename: '%s'\n", argv[i]);

    //check if the path include a directory
    for (k = 0; k < strlen(argv[i]); k++)
    if(argv[i][k] == '/'){
      flag = 1;
      x = k;
    }
    if (flag != 0)
    for (k = 0; k < strlen(argv[i])-x; k++)
      argv[i][k] = argv[i][x+k+1];
    if(debug == 1)
      printf("DEBUG - Client - Filename(modificato): '%s'\nk = %d\n", argv[i], k);
    flag = 0;
    x = 0;
    k = 0;

    if((fp = fopen(argv[i], "wb")) == NULL){                //opening file
      printf("Error - File Open\n");
      close(s);
      exit(EXIT_FAILURE);
    }

    /**************************
    *  Scrittura nuovo file  *
    **************************/
    memset(buffer, '\0', MAXBUFFER);
    while(dim > MAXBUFFER){     //scrivo tutti di dimensioni MAXBUFFER
      /*****************
      Imposto il timer
      ******************/
      t = 15;
      tval.tv_sec = t;
      tval.tv_usec = 0;
      FD_ZERO(&cset);        //azzero il vettore del file descriptor
      FD_SET(s, &cset);      //imposto la casella corrispondente al socket

      if((n = select(FD_SETSIZE, &cset, NULL, NULL, &tval)) == -1){
        close(s);
        fclose(fp);
        err_sys ("(%s) error - select() failed", prog_name);
      }
      if(n > 0){
        if ( (n = readn(s,  buffer, MAXBUFFER)) < 0 ){
          close(s);
          fclose(fp);
          err_sys ("(%s) error - readn() failed", prog_name);
        }else if(n < MAXBUFFER){
          printf("(%s) error - reading file_buffer\n", prog_name);
          fclose(fp);
          remove(argv[i]);    //removing old file
          close(s);
          exit(EXIT_FAILURE);
        }
      }else{
        close(s);
        fclose(fp);
        err_quit("No response after %d seconds\n", t);
      }
      dim -= n;
      //dim -= Readn(s,  buffer, MAXBUFFER);
      if((fwrite(buffer, 1, MAXBUFFER, fp)) < n){
        printf("(%s) - fwrite failed!\n", prog_name);
        close(s);
        fclose(fp);
        exit(EXIT_FAILURE);
      }
      memset(buffer, '\0', MAXBUFFER);
    }

    /*****************
    Imposto il timer
    ******************/
    t = 15;
    tval.tv_sec = t;
    tval.tv_usec = 0;
    FD_ZERO(&cset);        //azzero il vettore del file descriptor
    FD_SET(s, &cset);      //imposto la casella corrispondente al socket

    if((n = select(FD_SETSIZE, &cset, NULL, NULL, &tval)) == -1){
      close(s);
      fclose(fp);
      err_sys ("(%s) error - select() failed", prog_name);
    }
    if(n > 0){
      if ( (n = readn(s, buffer2, dim)) < 0 ){
        close(s);
        fclose(fp);
        err_sys ("(%s) error - readn() failed", prog_name);
      }else if(n < dim){
        printf("(%s) error - reading file_buffer\n", prog_name);
        fclose(fp);
        close(s);
        exit(EXIT_FAILURE);
      }
    }else{
      close(s);
      fclose(fp);
      err_quit("No response after %d seconds\n", t);
    }

    dim -= fwrite(buffer2, dim, 1, fp);   //scrivo l'ultimo pezzetto
    fclose(fp);


    /*****************
    Imposto il timer
    ******************/
    t = 15;
    tval.tv_sec = t;
    tval.tv_usec = 0;
    FD_ZERO(&cset);        //azzero il vettore del file descriptor
    FD_SET(s, &cset);      //imposto la casella corrispondente al socket

    /***************************************
    *  Lettura timestamp ultima modifica  *
    ***************************************/
    if((n = select(FD_SETSIZE, &cset, NULL, NULL, &tval)) == -1){
      close(s);
      err_sys ("(%s) error - select() failed", prog_name);
    }
    if(n > 0){
      if ( (n = readn(s, &time, sizeof(time))) < 0 ){
        close(s);
        err_sys ("(%s) error - readn() failed", prog_name);
      }
    }else{
      close(s);
      err_quit("No response after %d seconds\n", t);
    }

    if(debug == 1){
      printf("DEBUG - Client - Tempo ricevuto (network): %lu\n", (unsigned long) time);
      time = ntohl(time);
      printf("DEBUG - Client - Tempo ricevuto (host): %lu\n", (unsigned long) time);
    }else if(debug == 0){
      time = ntohl(time);
      printf("\t***File transfer success***\nFile name: %s\nFile size: %lu Byte\nTimestamp last modification: %lu\n\n", argv[i], (unsigned long) dim_p, (unsigned long) time);
    }
  }

  /*******************************
  *        Closing Socket       *
  *******************************/
  if(debug == 1)
    printf("Closing connection...\n");
  Close(s);
  if(debug == 1)
    printf("Connection closed\n");
  return 0;
}


int new_connection(char *address, char *port){
  struct addrinfo hint, *re, *re0;
  int sok;
  char *cause;
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = PF_INET;   //accetto connessioni solo IPV4
  hint.ai_socktype = SOCK_STREAM;

  if(getaddrinfo(address, port, &hint, &re0)){  //in re0 abbiamo agganciato la testa della lista delle risoluzioni
    err_quit("error at getaddrinfo\n");
  }
  sok = -1;
  for(re = re0; re != NULL; re = re->ai_next){  //scorro la lista degli indirizzi
    sok = socket(re->ai_family, re->ai_socktype, re->ai_protocol);
    if(sok < 0){
      cause = "socket";
      continue;
    }
    if(connect(sok, re->ai_addr, re->ai_addrlen) < 0){
      cause = "connect";
      close(sok);
      sok = -1;
      continue;
    }
    break;    //qui abbiamo risolto un alias e ci siamo connessi. Esco dal ciclo
  }
  freeaddrinfo(re0);  //libero la lista delle strutture, rilascio le risorse
  if(sok < 0)
    err_quit("error at %s. Closing...\n", cause);
  return sok;
}

void handlersigpipe(int sig){
  if(sig == SIGPIPE){
    if(sock_cpy >= 0)
      Close(sock_cpy);
    if( fp != NULL)
      fclose(fp);
    exit(SIGPIPE);
  }
}

void handlersigint(int sig){
  if(sig == SIGINT){
    if(sock_cpy >= 0)
      Close(sock_cpy);
    if( fp != NULL)
      fclose(fp);
    exit(SIGINT);
  }
}
