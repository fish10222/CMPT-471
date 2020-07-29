#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/select.h>
#include <regex.h>
#include <unistd.h>
#include <ctype.h>

#define PROTOPORT 33455    /* default ipv4 protocol port number      */
#define PROROPORT6 33446   /* default ipv6 protocol port number      */
#define BUFSIZE 1440 /* default buffer size for ipv4 */
#define BUFSIZE6 1280 /* default buffer size for ipv6 */
#define MAX_MSG_LEN 50 /* maximun message lenghth */

extern  int      errno;
char    localhost[] =   "localhost";      /* default host name             */
char    defaultfile[] = "fileToTransfer"; /* default file name        */
char    outfilename[] = "outputfile";
/*------------------------------------------------------------------------
 * Program:   tcpechoclient
 *
 * Purpose:   allocate a socket, connect to a server, open a file, 
 *            send the contents of the file through the socket to the
 *            echo server, read the echoed data from the socket and 
 *            write it into an output file
 *
 * Syntax:    tcpechoclient [[[[host [port]] [hostl] [reqfile]] [outfile]] [lenbuff]] 
 *       host        - name of a host on which server is executing
 *       port        - protocol port number server is using
 *       infile      - input file containing data to be echoed
 *       outfile     - output file into which echoed data is to be written
 *       hostl       - name of host on which the client is executing
 *                     (not used in this application)
 *       lenbuf      - MSS, size of read and write buffers
 *
 * Note:   All arguments are optional.  If no host name is specified,
 *         the client uses "localhost"; if no protocol port is
 *         specified, the client uses the default given by PROTOPORT.
 *------------------------------------------------------------------------
 */
int main(argc, argv)
int   argc;
char   *argv[];
{
   struct   hostent  *ptrh;     /* pointer to a host table entry         */
   struct   protoent *ptrp;     /* pointer to a protocol table entry     */
   struct   sockaddr_in sad;    /* structure to hold an IP address       */
   struct   sockaddr_in6 sad6;  /* structure to hold an IP address       */
   size_t   lenbuf;             /* length of input and output buffers  v6*/
   int      ip_type; 
   int      maxfdp1;            /* maximum descriptor value,             */
   int      sd;                 /* socket descriptor                     */
   int      port;               /* protocol port number                  */
   char    *host;               /* pointer to host name                  */
   char    *hostl;              /* pointer to host name                  */   
   int      EOFFlag;            /* flag, set to 1 when input file at EOF */
   char    *sendbuf;            /* buffer for data going to the server   */
   char    *recvbuf;            /* buffer for data from the server       */
   char    *file_name;          /* name of requested file to server      */
   char     message[MAX_MSG_LEN];/*array for messages that may be used   */
   FILE    *infile;             /* file descriptor for input file        */
   FILE    *outfile;            /* file descriptor for output file       */
   char    *outfile_name;
   fd_set   descset;            /* set of file and socket descriptors    */
                                /* for select                            */
   size_t   file_size; 
   size_t   recv_size;    
   ssize_t  nread;              /* number of bytes read by a read        */
   size_t   nwrite;             /* number of bytes written by a write    */
   int      charsin;            /* number of characters sent out through */
                                /* the socket                            */
   int      charsout;           /* number of characters received through */
                                /* the socket                            */
   int      buffersin;          /* number of buffers sent out through    */
                                /* the socket                            */
   int      buffersout;         /* number of buffers received through    */
                                /* the socket                            */
   int      val; 
   int      protocol;           /* type of connection protocol           */ 
   char*    i;
   regex_t  regex;
   regmatch_t match[1];
   
   /* Initialize variables */
   recv_size = 0;
   charsin = 0;
   charsout = 0;
   buffersin = 0;
   EOFFlag = 0;
   buffersout = 0;
   infile = NULL;
   outfile = NULL;
   sendbuf = NULL;
   recvbuf = NULL;
   memset((char *)&sad,0,sizeof(sad));   /* clear sockaddr structure      */
   memset((char *)&sad6,0,sizeof(sad6)); /* clear sockaddr structure    v6*/
   sad.sin_family = AF_INET;             /* set family to Internet        */
   sad6.sin6_family = AF_INET6;

   /* Check for command-line arguments                                  */
   /* If there are not arguments print an information message           */
   if (argc <= 1) {   
      fprintf (stderr, "Command line arguments are required\n");
      fprintf (stderr, "In order the required arguments are:\n");
      fprintf (stderr, "IP address of remote communication endpoint:\n");
      fprintf (stderr, "	Default value localhost\n");
      fprintf (stderr, "port of remote communication endpoint\n");
      fprintf (stderr, "	  Default value 20004\n");
      fprintf (stderr, "IP address of local communication endpoint\n");
      fprintf (stderr, "	  Default value localhost\n");
      fprintf (stderr, "input filename (contains data to be echoed)\n");
      fprintf (stderr, "	  Default value inputfile\n");
      fprintf (stderr, "output filename (containing echoed data\n");
      fprintf (stderr, "	  Default value outputfile\n");
      fprintf (stderr, "buffer size equals MSS for each packet\n");
      fprintf (stderr, "          Default value 1448\n");
      fprintf (stderr, "To accept any particular default replace\n");
      fprintf (stderr, "the variable with a . in the argument list\n");
      exit(0);
   }

   /* Check host argument and assign host name. */
   /* Default filename is inputfile, to use default use ? as argument   */
   /* Convert host name to equivalent IP address and copy to sad. */
   /* if host argument specified   */
   if ( (argc > 1) && strncmp(argv[1],".", 1)!=0) {  
      host = argv[1];
      ip_type = 4;     
   } 
   else {
      host = localhost;
   }
   ptrh = gethostbyname(host);
   if ( ((char *)ptrh) == NULL ) {
      if (((char *)ptrh) == NULL){
         fprintf(stderr,"invalid host: %s\n", host);
         exit(1);
      }
      ip_type = 6;
   }

   /* Check command-line argument for buffer size  and extract          */
   /* Default buffer size is 1448                                       */ 
   /* ---to use default use . as argument or give no argument           */
   /* print error message and exit in case of error in reading          */
   if ( (argc > 6) && strncmp(argv[6],".", 1)!=0 ) {   
       lenbuf =  atoi(argv[6]);
   } else {
      if (ip_type == 4){
         lenbuf = BUFSIZE;
      }
      else{
         lenbuf = BUFSIZE6;
      }
       
   }
   sendbuf = malloc(lenbuf*sizeof(int) );
   if (sendbuf == NULL) { 
      fprintf(stderr,"send buffer not created, size %s\n",argv[5]);
      exit(1);
   }
   recvbuf = malloc(lenbuf*sizeof(int) );
   if (recvbuf == NULL) { 
      fprintf(stderr,"receive buffer not created size %s\n",argv[5]);
      free(sendbuf);
      exit(1);
   }

   /* Check command-line argument for output filename and extract       */
   /* Default filename is outputfile                                    */
   /* ---to use default use . as argument or give no argument           */
   /* open filename for writing                                         */
   /* print error message and exit in case of error in open             */
   if ( (argc > 5) && strncmp(argv[5],".", 1)!=0 ) {   
      outfile_name = argv[5];
   } else {
      outfile_name = outfilename;
   }

   
   // /* Check command-line argument for input filename and extract        */
   // /* Default filename is inputfile, to use default use . as argument   */
   // /* ---to use default use . as argument or give no argument 3 or 4    */
   // /* open file for reading                                             */
   // /* print error message and exit if file not found                    */
   // if ( (argc > 3) && strncmp(argv[3], ".", 1)!=0 ) {   
   //    infile = fopen(argv[3], "r");
   // } else {
   //    infile = fopen( "inputfile", "r");
   // }
   // if (infile == NULL) { 
   //    fprintf(stderr,"input file not found %s\n",argv[3]);
   //    close(fileno(outfile));
   //    free(sendbuf);
   //    free(recvbuf);
   //    exit(1);
   // }

   if ((argc > 4) && strncmp(argv[4], ".", 1) != 0){
     file_name = argv[4];
   }
   else{
     file_name = defaultfile;
   }

   /* Check command-line argument for client name                       */
   /* Default client name is localhost, to use default use . as argument   */
   /* ---to use default use . as argument or give no argument 3 or 4    */
   if ((argc > 3) && strncmp(argv[3], ".", 1) != 0)
   {
     hostl = argv[3];
   }
   else
   {
     hostl = localhost;
   }
   
   /* Check command-line argument for protocol port and extract         */
   /* port number if one is extracted.  Otherwise, use the default      */
   /* to use default given by constant PROTOPORT use . as argument      */
   /* Value will be converted to an integer and checked for validity    */
   /* An invalid port number will cause the application to terminate    */
   /* Map TCP transport protocol name to protocol number.               */
   if ( (argc > 2) && strncmp(argv[2],".", 1)!=0) {  
      port = atoi(argv[2]);
   } else {
      if (ip_type == 4){
         port = PROTOPORT;
      } else {
         port = PROROPORT6;
      }
      
   }
   if (port > 0 && ip_type == 4) {
      sad.sin_port = htons((unsigned short)port);
   } else if (port > 0 && ip_type == 6) {
      sad6.sin6_port = htons((unsigned short)port);
   } else {
      fprintf(stderr,"bad port number %s\n",argv[2]);
      free(sendbuf);
      free(recvbuf);
      exit(1);
   }
   if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
      fprintf(stderr, "cannot map \"tcp\" to protocol number");
      free(sendbuf);
      free(recvbuf);
      exit(1);
   }


   
   /* Create a TCP socket, and connect it the the specified server      */ 
   if (ip_type == 4){
      printf("Make IPv4 socket\n");
      memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);
      sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
      inet_ntop(AF_INET, &(sad.sin_addr), message, MAX_MSG_LEN);
      val = (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0);
   } else {
      printf("Make IPv6 socket\n");
      memcpy(&sad6.sin6_addr, ptrh->h_addr, ptrh->h_length);
      sd = socket(PF_INET6, SOCK_STREAM, ptrp->p_proto);
      inet_ntop(AF_INET6, &(sad6.sin6_addr), message, MAX_MSG_LEN);
      val = (connect(sd, (struct sockaddr *)&sad6, sizeof(sad6)) < 0);
   }
   if (sd < 0) {
      fprintf(stderr, "socket creation failed\n");
      free(sendbuf);
      free(recvbuf);
      exit(1);
   }
   // val = IP_PMTUDISC_DONT;
   // if(setsockopt(sd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val) ) < 0 ) {
	//    printf("Error setting MTU discover A");
   // }
   // val = lenbuf+12;
   // if(setsockopt(sd, IPPROTO_TCP, TCP_MAXSEG, &val, sizeof(val) ) < 0 ){
	//    printf("Error setting MAXSEG ofption A");
   // }

   if (val < 0) {
      fprintf(stderr,"connect failed\n");
      close(sd);
      free(sendbuf);
      free(recvbuf);
      exit(1);
   }

   /* Send the file name to server */
   strcpy(sendbuf, file_name);
   if (send(sd, sendbuf, strlen(file_name), 0) < 0){
      fprintf(stderr, "file name was not sent\n");
      close(sd);
      free(sendbuf);
      free(recvbuf);
      exit(1);
   }

   if (recv(sd, recvbuf, lenbuf, 0) < 0){
      fprintf(stderr, "response was not recieved\n");
      close(sd);
      free(sendbuf);
      free(recvbuf);
      exit(1);
   }


   if (!strcmp(recvbuf, "COULD NOT OPEN REQUESTED FILE")){
      fprintf(stderr, "server does not have %s\n", file_name);
      close(sd);
      free(sendbuf);
      free(recvbuf);
      exit(1);
   } else {
      if (regcomp(&regex, "FILE SIZE IS [0-9]+ bytes", REG_EXTENDED)){
         fprintf(stderr, "regex blew up\n");
         close(sd);
         free(sendbuf);
         free(recvbuf);
         exit(1);
      }
      if (!regexec(&regex, recvbuf, 1, match, 0)){
         for (i=recvbuf;!isdigit(*i);i++){};
         file_size = strtol(i, &i, 10);
         printf("file size is %zu\n", file_size);
         regfree(&regex);
      } else {
         fprintf(stderr, "Invalid message: %s\n", recvbuf);
         close(sd);
         free(sendbuf);
         free(recvbuf);
         exit(1);
      }
   }
   outfile = fopen( outfile_name, "wb+");
   if (outfile == NULL) { 
      fprintf(stderr,"output file not created %s\n", outfile_name);
      close(sd);
      free(sendbuf);
      free(recvbuf);
      exit(1);
   }
   while ((val = recv(sd, recvbuf, lenbuf, 0)) > 0){
      recv_size += val;
      fwrite(recvbuf, sizeof(char), val, outfile);
      if (recv_size >= file_size){
         break;
      }
   }
   if (val < 0){
      fprintf(stderr, "failed to transimit file\n");
      fclose(outfile);
      close(sd);
      free(sendbuf);
      free(recvbuf);
      exit(1);
   }
   printf("Transmission complete\n");
   fclose(outfile);
   close(sd);
   free(sendbuf);
   free(recvbuf);
   /* Terminate the client program gracefully. */
   exit(0);
}
 
