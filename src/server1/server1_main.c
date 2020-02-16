#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>    // timeval
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>   // inet_aton()
#include <sys/un.h>      // unix sockets
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>    // SCNu16
#include <sys/stat.h>
#include <unistd.h>

#include "../sockwrap.h"
#include "../errlib.h"

#define MAXBUFFER 1024
#define debug 0 //debug = 1 for debug mode

char *prog_name = "Server_Transfer_iterativo";
int sock_1_cpy = -1, sock_2_cpy = -1;
FILE *fp = NULL;

/******************************
*       SERVER TCP           *
* -riceve nome file          *
* -invia il file richiesto   *
* -timeout 15 secondi        *
******************************/

void handlersigpipe(int sig);
void handlersigint(int sig);

int main (int argc, char *argv[])
{
  struct sockaddr_in saddr, caddr;
  int s1, s2, flag_err = 0;
  int backlog = 20;
  socklen_t addr_size;
  char buffer_1[MAXBUFFER];
  uint32_t sz, sx, res;
  struct stat stru_stat;


  /***********************
  Parametri per la Select
  ************************/
  ssize_t n;
  struct timeval tval;
  fd_set cset;


  /******************************
  *     Controllo parametri    *
  ******************************/
  if(argc != 2){
    printf("[ERROR] Numero parametro scorretto/n");
    return -1;
  }
  if(atoi(argv[1]) < 1024){
    printf("[ERROR] Porta non valida\n");
    return -2;
  }


  /******************************
  *           Socket           *
  ******************************/

  sock_2_cpy = s2 = Socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  sock_1_cpy = s1 = Socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

  /*opzionale perche' abbiamo usato S maiuscola nella socket*/
  if((s2 < 0) || (s1 < 0)){
    printf("Error at Socket\n");
    return -3;
  }

  if(debug == 1)
    printf("Server UP!\nPolitecnico di Torino\nAuthor: Davide Antonino Giorgio\nNumber: s262709\nversion: 3.2\n\n");

  memset(&caddr, 0, sizeof(caddr));
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port   = htons(atoi(argv[1]));
  saddr.sin_addr.s_addr = INADDR_ANY;

  /******************************
  *       Binding Phase        *
  ******************************/
  //s1 sarà il socket passivo
  if(debug == 1)
    showAddr("Binding to address", &saddr);
  Bind(s1, (struct sockaddr *) &saddr, sizeof(saddr));


  /******************************
  *           Listen           *
  ******************************/
  //In ascolto sul socket s1. Socket passivo
  Listen(s1, backlog);
  if(debug == 1)
    printf("Listening at socket %d with backlog = %d\n", s1, backlog);

  //header di gestione dei sigpipe
  signal(SIGPIPE, handlersigpipe);
  signal(SIGINT, handlersigint);

  /*Outer Loop Server*/
  while(1){

    /******************************
    *      Imposto il Timer      *
    ******************************/
    int t = 15;
    tval.tv_sec = t;
    tval.tv_usec = 0;

    FD_ZERO(&cset);        //azzero il vettore del file descriptor
    FD_SET(s1, &cset);     //imposto la casella corrispondente al socket

    /******************************
    *    Accept new connection   *
    ******************************/
    addr_size = sizeof(struct sockaddr_in);

    if((n = select(FD_SETSIZE, &cset, NULL, NULL, NULL)) == -1){
      printf ("(%s) error - select() failed\n", prog_name);
      break;
    }
    if(n > 0){
      if ( (s2 = accept(s1, (struct sockaddr *) &caddr, &addr_size)) < 0 ){
        printf ("(%s) error - accept() failed", prog_name);
        break;
      }
    }
    if(debug == 1){
      showAddr("Accepted connectio from", &caddr);
      printf("New Socket = %d\n", s2);
    }

    FD_ZERO(&cset);        //azzero il vettore del file descriptor
    FD_SET(s2, &cset);     //imposto la casella corrispondente al socket

    /*Inner Server Loop*/
    while(1){

      /******************************
      *      Imposto il Timer      *
      ******************************/
      t = 15;
      tval.tv_sec = t;
      tval.tv_usec = 0;
      FD_ZERO(&cset);        //azzero il vettore del file descriptor
      FD_SET(s2, &cset);     //imposto la casella corrispondente al socket

      //*GETTING 'GET '
      memset(buffer_1, '\0', MAXBUFFER);
      if((n = select(FD_SETSIZE, &cset, NULL, NULL, &tval)) == -1){
        printf("(%s) error - select() failed", prog_name);
        break;
      }
      if(n > 0){
        if ( (n = readn(s2, (char *)buffer_1, 4)) < 0 ){
          sprintf(buffer_1, "-ERR\r\n");
          sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
          printf ("(%s) error - readn() failed", prog_name);
          break;
        }
      }else{
        printf("No response after %d seconds\n", t);
        sprintf(buffer_1, "-ERR\r\n");
        sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        break;
      }

      if(n == 0){
        if(debug == 1)
          printf("\nDEBUG - Server - Ready for another GET\n");
        break;
      }


      /******************************
      *     Controllo su 'GET '    *
      ******************************/
      if(debug == 1)
        printf("\n\n------------------------------\n");
      buffer_1[4] = '\0';
      if(debug == 1)
        printf("DEBUG - Server - Prima stringa protocollo ricevuta: '%s'\n", buffer_1);
      if(strcmp(buffer_1, "GET ") != 0){
        sprintf(buffer_1, "-ERR\r\n");
        sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        printf("Server - Connection closed. No match 'GET '\n");
        break;
      }

      /******************************
      *      Imposto il Timer      *
      ******************************/
      t = 15;
      tval.tv_sec = t;
      tval.tv_usec = 0;
      FD_ZERO(&cset);        //azzero il vettore del file descriptor
      FD_SET(s2, &cset);     //imposto la casella corrispondente al socket

      /******************************
      *      Getting Filename      *
      ******************************/
      memset(buffer_1, '\0', MAXBUFFER);
      if((n = select(FD_SETSIZE, &cset, NULL, NULL, &tval)) == -1){
        printf ("(%s) error - select() failed\n", prog_name);
        break;
      }
      if(n > 0){
        if ( (n = readline_unbuffered(s2, buffer_1, MAXBUFFER)) < 0 ){
          printf ("(%s) error - readline_unbuffered() failed\n", prog_name);
          sprintf(buffer_1, "-ERR\r\n");
          sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
          break;
        }
      }else{
        printf("No response after %d seconds\n", t);
        sprintf(buffer_1, "-ERR\r\n");
        if( sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL) < strlen(buffer_1) ){
          printf("Error at sendn(). Trying again..\n");
          sprintf(buffer_1, "-ERR\r\n");
          sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        }
        break;
      }

      if(!((buffer_1[strlen(buffer_1)-2] == '\r') && (buffer_1[strlen(buffer_1)-1] == '\n'))){
        sprintf(buffer_1, "-ERR\r\n");
        if( sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL) < strlen(buffer_1) ){
          printf("Error at sendn(). Trying again...\n");
          sprintf(buffer_1, "-ERR\r\n");
          sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        }
        printf("Server - Connection closed. Incorrect protocol terminator\n");
        break;
      }

      buffer_1[strlen(buffer_1)-2] = '\0';
      if(debug == 1)
        printf("DEBUG - Server - Filename: '%s'\n", buffer_1);

      //Controllo su esistenza file
      if(stat(buffer_1, &stru_stat) != 0){
        printf("Server - Connection closed. file '%s' do not exist!\n", buffer_1);
        sprintf(buffer_1, "-ERR\r\n");
        if( sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL) < strlen(buffer_1) ){
          printf("Error at sendn(). Trying again...\n");
          sprintf(buffer_1, "-ERR\r\n");
          sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        }
        break;
      }
      //Controllo se directory
      if (S_ISDIR(stru_stat.st_mode)) {
        sprintf(buffer_1, "-ERR\r\n");
        sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        printf("Secrver - Connection closed. You can not transfer a directory\n");
        break;
      }
      //Apertura file
      if((fp = fopen(buffer_1, "r")) == NULL){
        sprintf(buffer_1, "-ERR\r\n");
        sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        printf("Server - Connection closed. file %s do not exist!\n", buffer_1);
        break;
      }

      /*******************************
      * Catturo dimensione del file *
      *******************************/
      fseek(fp, 0, SEEK_END);
      sz = ftell(fp);
      rewind(fp);
      if(debug == 1){
        printf("DEBUG - Server - dimensione del file: %lu\n", (unsigned long) sz);
        printf("DEBUG - Server - dimensione che sto inviando: %lu\n", (unsigned long) htonl(sz));
      }

      /*******************************
      *     Sending start string    *
      *******************************/
      memset(buffer_1, '\0', MAXBUFFER);
      sprintf(buffer_1, "+OK\r\n");
      if(debug == 1)
        printf("DEBUG - Server - stringa che sto inviando: '%s'\n", buffer_1);
      if ( sendn(s2, buffer_1, strlen(buffer_1), 0) < strlen(buffer_1)){
        sprintf(buffer_1, "-ERR\r\n");
        sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        printf("Server - Connection closed.\n");
        if( fp != NULL )
          fclose(fp);
        fp = NULL;
        break;
      }

      /*******************************
      *    Sending file dimension   *
      *******************************/
      sx = htonl(sz);                  //using network byte order
      if ( sendn(s2, &sx, sizeof(sx), 0) < ((ssize_t) sizeof(sx)) ){
        sprintf(buffer_1, "-ERR\r\n");
        sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        printf("Server - Connection closed.\n");
        if( fp != NULL )
          fclose(fp);
        fp = NULL;
        break;
      }

      /*******************************
      *     Sending file content    *
      *******************************/
      memset(buffer_1, '\0', MAXBUFFER);
      while((res = fread(buffer_1, 1, MAXBUFFER, fp)) == MAXBUFFER){
        if(sendn(s2, buffer_1, res, MSG_NOSIGNAL) != ((ssize_t) res)){
          sprintf(buffer_1, "-ERR\r\n");
          sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
          printf("(%s) - Error during 'sending file content'\n", prog_name);
          flag_err = 1;
          if( fp != NULL )
            fclose(fp);
          fp = NULL;
          break;
        }
        memset(buffer_1, '\0', MAXBUFFER);
      }
      if(flag_err == 1)
        break;
      if( sendn(s2, buffer_1, res, MSG_NOSIGNAL) < ((ssize_t) res) ){
        printf("(%s) - Error during 'sending file content'\n", prog_name);
        sprintf(buffer_1, "-ERR\r\n");
        sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        if( fp != NULL )
          fclose(fp);
        fp = NULL;
        break;
      }

      /*******************************
      *    Sending file timestamp   *
      *******************************/
      if(debug == 1)
        printf("DEBUG - Server - Tempo della struct: %lu\n", stru_stat.st_mtime);
      sx = htonl(stru_stat.st_mtime);
      if(debug == 1)
        printf("DEBUG - Server - Tempo della struct convertito: %lu\n", (unsigned long) sx);
      if ( sendn(s2, &sx, sizeof(sx), MSG_NOSIGNAL) < sizeof(sx) ){
        sprintf(buffer_1, "-ERR\r\n");
        sendn(s2, buffer_1, strlen(buffer_1), MSG_NOSIGNAL);
        printf("(%s) - Connection closed.\n", prog_name);
        if( fp != NULL )
          fclose(fp);
        fp = NULL;
        break;
      }
      if(fp != NULL)
        fclose(fp);
      fp = NULL;
      if(debug == 1)
        printf("------------------------------\n");
    }

    /*******************************
    *        Closing Socket       *
    *******************************/
    printf("(%s) - ending service of client\n", prog_name);
    close(s2);
  }
  return 0;
}


void handlersigpipe(int sig){
  if(sig == SIGPIPE){
    printf("signal SIGPIPE possible ctrl+c client\n");
    if(sock_2_cpy >= 0){
      printf("(%s) - Closing socket: %d\n", prog_name, sock_2_cpy);
      close(sock_2_cpy);
    }
    if( fp != NULL )
      fclose(fp);
    fp = NULL;
  }
  else
    printf("(%s) - signal not SIGPIPE\n", prog_name);
}

void handlersigint(int sig){
  if(sig == SIGINT){
    if(sock_1_cpy >= 0){
      printf("(%s) - Closing socket: %d\n", prog_name, sock_1_cpy);
      close(sock_1_cpy);
    }
    if(sock_2_cpy >= 0){
      printf("(%s) - Closing socket: %d\n", prog_name, sock_2_cpy);
      close(sock_2_cpy);
    }
    if(fp != NULL)
      fclose(fp);
    fp = NULL;
    exit(SIGINT);
  }
  else
    printf("(%s) - signal not SIGINT\n", prog_name);
}
