#include "csapp.h"

/* 연결을 원하는 서버 호스트의 IP 주소(이름)와 원하는 서비스의 포트 번호를 입력받는다. */
int open_clientfd(char *hostname, char *port) {
  int clientfd;
  struct addrinfo hints, *listp, *p;

    /* 
        Get a list of potential server addresses 
        서버의 호스트와 포트 번호에 대응되는 IP 주소(소켓 주소 구조체)의 리스트를 반환받는다.
    */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;  /* Open a connection */
    hints.ai_flags = AI_NUMERICSERV;  /* service인자를 포트 번호로 받는다. */
    hints.ai_flags |= AI_ADDRCONFIG;  /* Recommended for connections. |를 사용해 flag를 추가한다. */
    getaddrinfo(hostname, port, &hints, &listp);

    /* 
        서버의 hostname과 port에 대응하는 addrinfo 리스트를 돌면서
        그 형식과 일치하는 소켓을 성공적으로 만들고, 
        서버와의 연결이 성공할 때까지 적합한 소켓 구조체를 찾는다.
     */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor */
        if ((clientfd = socket(p->ai_family, p->ai_socktype, 
                               p->ai_protocol)) < 0)  // 식별자를 리턴 못하고 -1 리턴한다면.
            continue; /* Socket failed, try the next */

        /* Connect to the server */
        // socket()으로 만든 클라이언트 호스트의 소켓의 식별자, 서버의 주소를 넣고 서버와 연결을 시도한다.
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)  //
            break; /* Success */

        Close(clientfd); /* 연결 실패하면 소켓 닫아주고 다음 소켓 진행 */
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) /* All connects failed */
        return -1;
    else    /* The last connect succeeded */
        return clientfd;
}