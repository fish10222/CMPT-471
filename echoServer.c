/* #include <stdlib.h> */
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <pthread.h> //for threading , link with lpthread


#define PROTOPORT    33455 /* default protocol port number for ipv4 */
#define PROTOPORT6   33446 /* default protocol port number for ipv6 */
#define QLEN         24    /* size of request queue                 */
#define MAX_FILENAME 255   /* max file name size                    */

int   visits       =   0;      /* counts client connections              */
void *connection_handler(void *);
/*------------------------------------------------------------------------
 * Program:   echoserver
 *
 * Purpose:   allocate a TCP and UDP socket and then repeatedly 
 *            executes the following:
 *      (1) wait for the next TCP connection or TCP packet or 
 *          UDP packet from a client
 *      (2) when accepting a TCP connection a child process is spawned
 *          to deal with the TCP data transfers on that new connection
 *      (2) when a TCP segment arrives it is served by the child process.
 *          The arriving segment is echoed back to the client
 *      (2) when a UDP packet arrives it is echoed back to the client. 
 *      (3) when the TCP connection is terminated the child then terminates
 *      (4) go back to step (1)
 *
 * Syntax:    server [[ ipv4port ] [ipv6port] [buffer size] [buffer size6]]
 * 
 *       port  - protocol port number to use
 *       buffer size  - MSS of each packet sent
 *
 * Note:      The port argument is optional.  If no port is specified,
 *         the server uses the default given by PROTOPORT.
 *
 *------------------------------------------------------------------------
 */
int main(argc, argv)
int   argc;
char   *argv[];
{
   struct   protoent    *udpptrp; /*pointer to tcp protocol table entry   */
   struct   protoent    *tcpptrp; /*pointer to udp protocol table entry   */
   struct   protoent    *tcpptrp6;/*pointer to tcp protocol table entry v6*/
   struct   sockaddr_in sadu;     /* structure to hold server's address   */
   struct   sockaddr_in sad;      /* structure to hold server's address   */
   struct   sockaddr_in6 sad6;    /* structure to hold server's address v6*/
   struct   sockaddr_in cad;      /* structure to hold client's address   */
   struct   sockaddr_in6 cad6;    /* structure to hold cilent's address v6*/
   int      tcpsd, udpsd, tcpsd6; /* server socket descriptors            */
   int      connfd;               /* client socket descriptor             */
   int      maxfdp1;              /* maximum descriptor plus 1            */
   int      port, port6;          /* protocol port number                 */
   int      *new_sock;            /* socket for threading                 */
   char     *echobuf;             /* buffer for string the server sends   */
   size_t   lenbuf, lenbuf6;      /* length of buffer                     */
   size_t   maxlen;
   size_t   lenbuf_type;
   int      segmentcnt;           /* cumulative # of segments received    */
   int      packetcnt;            /* cumulative # of packets received     */
   int      tcpcharcntin;         /* cumulative # of octets received      */
   int      tcpcharcntout;        /* cumulative # of octets sent          */
   int      udpcharcnt;           /* cumulative # of octets received      */
   int      nread;                /* # of octets received in one read     */
   int      nwrite;               /* # of octets sent in one write        */
   int      retval;               /* function return flag for testing     */ 
   struct   timeval tval;         /* max time to wait before next select  */
   socklen_t      len, len6;      /* length of the socket address struct  */
   pid_t    pid;
   fd_set   descset;
   int      val;
   int      ip_type;              /* which type of connection is it       */
   FILE     *file;
   unsigned long filesize;
   struct stat statebuff;
   char filename[MAX_FILENAME];
   int send_size;

   /* Initialize variables                                                */
   packetcnt = 0;
   segmentcnt = 0;
   udpcharcnt = 0;
   tcpcharcntin = 0;
   tcpcharcntout = 0;
   file = NULL;
   memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure      */
   memset((char *)&cad,0,sizeof(cad)); /* clear sockaddr structure      */
   // memset((char *)&sadu,0,sizeof(sadu));/* clear sockaddr structure     */
   memset((char *)&sad6,0,sizeof(sad6)); /* clear sockaddr structure  v6*/
   memset((char *)&cad6,0,sizeof(cad6)); /* clear sockaddr structure  v6*/
   
   sad.sin_family = AF_INET;           /* set family to Internet        */
   sad.sin_addr.s_addr = INADDR_ANY;   /* set the local IP address      */
   sad6.sin6_family = AF_INET6;        /* set family to Internet      v6*/
   sad6.sin6_addr = in6addr_any;       /* set the local IP address    v6*/
   // sadu.sin_family = AF_INET;          /* set family to Internet        */
   // sadu.sin_addr.s_addr = INADDR_ANY;  /* set the local IP address      */
   cad.sin_family = AF_INET;           /* set family to Internet        */
   cad6.sin6_family = AF_INET6;        /* set family to Internet      v6*/

   /* Check for command-line arguments                                  */
   /* If there are not arguments print an information message           */
   if (argc <= 1) {
      fprintf (stderr, "Command line arguments are required\n");
      fprintf (stderr, "In order the required arguments are:\n");
      fprintf (stderr, "port of remote communication endpoint\n");
      fprintf (stderr, "          Default value 33455\n");
      fprintf (stderr, "buffer size equals MSS for each packet\n");
      fprintf (stderr, "          Default value 1448\n");
      fprintf (stderr, "port6 of remote communication endpoint\n");
      fprintf (stderr, "          Default value 33446\n");
      fprintf (stderr, "buffer6 size equals MSS for each packet\n");
      fprintf (stderr, "          Default value 1280\n");
      fprintf (stderr, "To accept any particular default replace\n");
      fprintf (stderr, "the variable with a . in the argument list\n");
      exit(0);
   }

   /* Check command-line argument for buffer size  and extract          */
   /* Default buffer size is 1448                                       */
   /* ---to use default use . as argument or give no argument           */
   /* print error message and exit in case of error in reading          */
   lenbuf=1400;
   lenbuf6=1280;
   //Set IPv4 Buffer
   if ( (argc > 3) && strncmp(argv[3],".", 1)!=0 ) { 
       lenbuf =  atoi(argv[3]);
   } else {
       lenbuf = 1448;
   }
   //Set IPv6 Buffer
   if ( (argc > 4) && strncmp(argv[4],".", 1)!=0 ) {
       lenbuf6 =  atoi(argv[4]);
   } else {
       lenbuf6 = 1280;
   }
   if (lenbuf > lenbuf6){
      maxlen = lenbuf;
   } else {
      maxlen = lenbuf6;
   }
   echobuf = malloc(maxlen*sizeof(int) );
   if (echobuf == NULL) {
      fprintf(stderr,"echo buffer not created, size %ld\n", maxlen);
      exit(1);
   }


   /* Check command-line argument for the protocol port and extract      */
   /* port number if one is specified.  Otherwise, use the   default     */
   /* port value given by constant PROTOPORT                             */
   /* check the resulting port number to assure it is valid (>0)         */
   /* convert the valid port number to network byte order and insert it  */
   /* ---  into the socket address structure.                            */
   /* OR print an error message and exit if the port is invalid          */
   
   //Set IPv4 Port
   if (argc > 1 && strncmp(argv[1], ".", 1) != 0) {
      port = atoi(argv[1]); 
   } else {
      port = PROTOPORT;  
   }
   if (port > 0) {
      sad.sin_port = htons((u_short)port);
      // sadu.sin_port = htons((unsigned short)port);
   }
   else {      
      fprintf(stderr,"bad port number %s\n",argv[1]);
      free(echobuf);
      exit(1);
   }
   //Set IPv6 Port
   if (argc > 2 && strncmp(argv[2], ".", 1) != 0) {
      port6 = atoi(argv[2]); 
   } else {
      port6 = PROTOPORT;  
   }
   if (port6 > 0) {
      sad6.sin6_port = htons((u_short)port6);
   }
   else {      
      fprintf(stderr,"bad port6 number %s\n",argv[2]);
      free(echobuf);
      exit(1);
   }


   // /* Map UDP transport protocol name to a pointer to a protocol number  */
   // /* Create a udp socket with socket descriptor udpsd                   */
   // /* Bind a local address to the udp socket                             */
   // /* If any of these three processes fail an explanatory error message  */
   // /* --- will be printed to stderr and the server will terminate        */
   // if ( ((long int)(udpptrp = getprotobyname("udp"))) == 0) {
   //    fprintf(stderr, "cannot map \"udp\" to protocol number");
   //    free(echobuf);
   //    exit(1);
   // }
   // udpsd = socket(AF_INET, SOCK_DGRAM, udpptrp->p_proto);
   // if (udpsd < 0) {
   //    fprintf(stderr, "udp socket creation failed\n");
   //    free(echobuf);
   //    exit(1);
   // }
   // if (bind(udpsd, (struct sockaddr *)&sadu, sizeof(sadu)) < 0) {
   //    fprintf(stderr,"udpbind failed\n");
   //    free(echobuf);
   //    close(udpsd);
   //    exit(1);
   // }


   /* Map TCP transport protocol name to protocol number                 */
   /* Create a tcp socket with a socket descriptor tcpsd                 */
   /* Bind a local address to the tcp socket                             */
   /* Put the TCP socket in passive listen state and specify the lengthi */
   /* --- of request queue                                               */
   /* If any of these four processes fail an explanatory error message   */
   /* --- will be printed to stderr and the server will terminate        */
   if ( ((long int)(tcpptrp = getprotobyname("tcp"))) == 0) {
      fprintf(stderr, "cannot map \"tcp\" to protocol number");
      free(echobuf);
      // close(udpsd);
      exit(1);
   }
   tcpsd = socket(AF_INET, SOCK_STREAM, tcpptrp->p_proto);
   tcpsd6 = socket(AF_INET6, SOCK_STREAM, tcpptrp->p_proto);
   if (tcpsd < 0) {
      fprintf(stderr, "tcp socket creation failed\n");
      free(echobuf);
      close(tcpsd6);
      // close(udpsd);
      exit(1);
   }
   if (tcpsd6 < 0) {
      fprintf(stderr, "tcp socket6 creation failed\n");
      free(echobuf);
      // close(udpsd);
      close(tcpsd);
      exit(1);
   }
   if (bind(tcpsd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
      fprintf(stderr,"tcp bind failed\n");
      free(echobuf);
      // close(udpsd);
      close(tcpsd);
      close(tcpsd6);
      exit(1);
   }
   if (bind(tcpsd6, (struct sockaddr *)&sad6, sizeof(sad6)) < 0) {
      fprintf(stderr,"tcp bind6 failed\n");
      free(echobuf);
      // close(udpsd);
      close(tcpsd);
      close(tcpsd6);
      exit(1);
   }
   if (listen(tcpsd, QLEN) < 0) {
      fprintf(stderr,"listen failed\n");
      free(echobuf);
      // close(udpsd);
      close(tcpsd);
      close(tcpsd6);
      exit(1);
   }
   if (listen(tcpsd6, QLEN) < 0) {
      fprintf(stderr,"listen6 failed\n");
      free(echobuf);
      close(udpsd);
      close(tcpsd);
      close(tcpsd6);
      exit(1);
   }
   /* add 12 bytes for extra options in default header */
   val = lenbuf+12;
   if( setsockopt(tcpsd, IPPROTO_TCP, TCP_MAXSEG, &val, sizeof(val) ) < 0 ) {
	printf("ERROR setting MAXSEG TCP-A\n");
   } 
   val = IP_PMTUDISC_DONT;
   if( setsockopt(tcpsd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val) ) < 0 ) {
	printf("ERROR setting MTU DISCOVER option for tcp A\n");
   }
   val = lenbuf6+12;
   if( setsockopt(tcpsd6, IPPROTO_TCP, TCP_MAXSEG, &val, sizeof(val) ) < 0 ) {
	printf("ERROR setting MAXSEG TCP-A6\n");
   } 
   val = IP_PMTUDISC_DONT;
   if( setsockopt(tcpsd6, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val) ) < 0 ) {
	printf("ERROR setting MTU DISCOVER option for tcp6 A\n");
   }

   /* Main server loop - accept and handle requests                     */
   /* Define the descriptor set for select, unset all descriptors       */
   /* determine the largest descriptor in use                           */
   FD_ZERO(&descset);
   // if (udpsd < tcpsd) maxfdp1 = tcpsd + 1;
   // else maxfdp1 = udpsd + 1;
   if (tcpsd > tcpsd6){
      maxfdp1 = tcpsd + 1;
   } else {
      maxfdp1 = tcpsd6 + 1;
   }

   /* Repeatedly check each socket for arriving data                     */
   /* On each pass through the for loop do each of the following:        */
   /* --- set the descriptors for the TCP and UDP sockets in descset     */
   /* --- check each socket for TCP connection requests or UDP data      */
   /* --- process TCP connection requests or incoming UDP data           */
   val = IP_PMTUDISC_DONT;
   if( setsockopt(tcpsd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val) ) < 0) {
	printf("ERROR setting MTU DISCOVER option B");
   }
   if( setsockopt(tcpsd6, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val) ) < 0) {
	printf("ERROR setting MTU DISCOVER6 option B");
   }
   printf("listening...\n");
   for( ; ; ) {
      FD_SET(tcpsd, &descset);
      FD_SET(tcpsd6, &descset);
      if ( (retval = select(maxfdp1, &descset, NULL, NULL, &tval) ) < 0 ) {
         if (errno == EINTR) {
            	printf("EINTR; select was interrupted \n");  
                continue;
         }
         else if (retval == -1) {
            fprintf(stderr,"select error\n");
  	      }
         /*  timed out, no descriptors ready */
         else if (retval == 0) {
            if( (pid = waitpid( -1, NULL, WNOHANG) ) > 0) {
                printf("child process %d terminated \n", pid);
            }
                printf("retval 0\n");
         }
      }

      if( (pid = waitpid( -1, NULL, WNOHANG) ) > 0) {
         /*printf("child process %d terminated \n", pid);*/
      }

      if (FD_ISSET(tcpsd, &descset)){
         ip_type = 4;
      }
      else if (FD_ISSET(tcpsd6, &descset)){
         ip_type = 6;
      }
   

      if (FD_ISSET(tcpsd, &descset) || FD_ISSET(tcpsd6, &descset)) { /* processing tcp data                                         */ 
         len = sizeof(sad);
         len6 = sizeof(sad6);
         if (ip_type == 4){
            lenbuf_type = lenbuf;
         }
         else if (ip_type == 6){
            lenbuf_type = lenbuf6;
         }
         printf("Connection recieved version %d\n", ip_type);
         /* !!!!!!!!!!!!!!!!!!start child process!!!!!!!!!!!!!!!!!!!!!!!*/
         if( ( fork()) == 0) {
            /* accept incoming data from new TCP connection                */
            /* spawn new TCP connection server to service new connection   */
            val = IP_PMTUDISC_DONT;
            if (ip_type == 4){
               if( setsockopt(tcpsd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val) ) < 0) {
                  printf("ERROR setting tcp MTU DISCOVER D\n");
               }
               while(connfd = accept(tcpsd, (struct sockaddr *)&cad, &len)){
                  pthread_t sniffer_thread;
                  new_sock = malloc(1);
                  *new_sock = connfd;
               }
            }
            else if (ip_type == 6){
               if( setsockopt(tcpsd6, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val) ) < 0) {
                  printf("ERROR setting tcp6 MTU DISCOVER D\n");
               }
               while (connfd = accept(tcpsd6, (struct sockaddr *)&cad6, &len6)){
                  pthread_t sniffer_thread;
                  new_sock = malloc(1);
                  *new_sock = connfd;
               }
            }
            /* close listening sockets                                  */
            // close(tcpsd);
            // close(tcpsd6);  

	      }
         /* !!!!!!!!!!!!!!!!!!!!end child process!!!!!!!!!!!!!!!!!!!!!!!*/


         /* close TCP connection in the parent                          */
	      close(connfd);
      }
   }
}

void *connection_handler(void *socket_desc){
   	      tval.tv_sec = 5;
   	      tval.tv_usec = 0;
   	      if( setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, &tval, sizeof(tval) ) < 0 ) {
		         printf("ERROR setting RCVTIMEOUT for tcp\n");
  	         }

            
            for( ; ; ) {
	            if ( ( nread = read(socket_desc, echobuf, lenbuf ) ) < 0) {
                  /*  read nread bytes of data from the      TCP socket */
		            if (errno == EINTR){
                     /* Interrupted before read try again later         */
                     fprintf(stderr, "EINTR reading from TCP socket\n");
                     continue;
		            }
                  else {
                     fprintf(stderr, "error reading from TCP socket\n");
                     free(echobuf);
	                  close(connfd);
		               exit(1);
                  }  
               }
               else if (nread > 0 ) {
                  /*  echo nread bytes of data extracted from TCP socket*/
                  /*  increment the number of segment received by 1     */
                  /*  increment the number of bytes echoed by nwrite    */
	               segmentcnt++;
                  tcpcharcntin += 8*nread;
                  // nwrite = write(connfd, echobuf, nread);
                  tcpcharcntout += 8*nwrite;
                  
                  strcpy(filename,echobuf);
                  file = fopen(echobuf, "r");

                  if (file == NULL){
                     write(connfd, "COULD NOT OPEN REQUESTED FILE", strlen("COULD NOT OPEN REQUESTED FILE"));
                     free(echobuf);
                     close(connfd);
                     exit(1);                  
                  }
                  else{
                     strcpy(echobuf, "");
                     filesize = -1;

                     if (stat(filename, &statebuff) >= 0){
                        filesize = statebuff.st_size;
                     }
                     if (filesize == -1){
                        fprintf(stderr, "unable to get file size");
                        free(echobuf);
                        close(connfd);
                        exit(1);
                     }
                     sprintf(echobuf, "FILE SIZE IS %ld bytes\n", filesize);
                     // printf("%s", echobuf);
                     nwrite = write(connfd, echobuf, strlen(echobuf));
                     sleep(1);
                  }
                  // SEND FILE
                  send_size = 0;
                  while (1){
                     val = fread(echobuf, sizeof(char), lenbuf_type, file);
                     send_size += val;
                     nwrite = write(connfd, echobuf, val);
                     if (nwrite < 0){
                        fprintf(stderr, "fail to send file");
                        free(echobuf);
                        close(connfd);
                        exit(1);
                     }
                     if (send_size >= filesize){
                        break;
                     }
                     sleep(1);
                  }
               }
	            else {
                  /*  nread=0, no data extracted, no data to be echoed  */
                  fprintf(stderr, "no data\n");
		            break;
               } 
            }
            fprintf(stdout, "The number of buffers transmitted is");
            fprintf(stdout, " %d\n", segmentcnt);
            fprintf(stdout, "The number of bytes transmitted is");
            fprintf(stdout, " %d\n", tcpcharcntout);
            fprintf(stdout, "The number of buffers recieved is");
            fprintf(stdout, " %d\n", segmentcnt);
            fprintf(stdout, "The number of bytes received is");
            fprintf(stdout, " %d\n", tcpcharcntin);
            fprintf(stdout, "If there is no fragmentation in the ");
            fprintf(stdout, "IP layer\n the number of buffers is ");
            fprintf(stdout, "the number of TCP segments\n");
            free(echobuf);
	         close(socket_desc);
	         exit(0);
}