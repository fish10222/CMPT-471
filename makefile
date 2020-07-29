server: echoServer.c
	gcc echoServer.c -o server
	gcc tcpEchoClient.c -o client