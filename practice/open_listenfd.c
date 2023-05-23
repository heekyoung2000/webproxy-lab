#include "csapp.h"

/* 서버의 해당 서비스, 즉 포트 번호를 입력받는다. */
int open_listenfd(char *port)
{
    struct addrinfo hints, *listp, *p;
    int listenfd, optval=1;

    /* Get a list of potential server addresses */
    /* 해당 포트와 연결할 수 있는 서버의 소켓 주소 리스트를 반환한다. */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* Accept connect. TCP 프로토콜. */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* 듣기 소켓으로 && 내가 사용하는 IP 형식에 맞는 IP 주소만 */
    hints.ai_flags |= AI_NUMERICSERV;            /* service 인자를 port 번호만 받겠다. */
    
    /* 해당 port 번호에 대응하는 IP주소를 찾아보자. 
    듣기 소켓을 찾는 AI_PASSIVE flag를 설정했으므로 hostname은 NULL이다. */
    Getaddrinfo(NULL, port, &hints, &listp);


    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor */
        /* 해당 와일드카드 주소의 형식에 맞는 소켓 만들기 */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, 
                               p->ai_protocol)) < 0)
            continue;  /* Socket failed, try the next */

        /* 
        Eliminates "Address already in use" error from bind 
        이전 바인딩에서 포트가 프로세스와 연결되었으므로 
        해당 포트를 다시 쓰려면 포트(서버)를 재시작해야 한다. 30초정도 걸린다.
        */
        Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int));

        /* Bind the descriptor to the address */
        /* 우리가 만든 소켓과 우리가 찾은 와일드카드 주소와 바인드한다. */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break; /* Success */
        Close(listenfd); /* Bind failed, try the next */
    }
    
    /* Clean up */
    Freeaddrinfo(listp);
    if (!p) /* No address worked */
        return -1;

    /* Make it a listening socket ready to accept conn. requests */
    if (listen(listenfd, LISTENQ) < 0) {
        Close(listenfd);
        return -1;
    }
    return listenfd;
}