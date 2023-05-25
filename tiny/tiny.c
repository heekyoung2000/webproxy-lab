// /* $begin tinymain */
// //GET /cgi-bin/adder?123&456 HTTP/1.0
// //3.39.230.91
// //11.7 입력 http://3.39.230.91:8000/cloud.mp4

#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

// port 번호를 인자로 받아 클라이언트 요청이 올때마다 새로 연결 소켓을 만들어 doit()함수를 호출한다.

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // 어떤 주소라도 넉넉한 공간을 얻을 수 있음

  /* Check command line args */
  if (argc != 2) { // command line을 확인했다.
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /*해당 포트 번호에 해당하는 듣기 소켓 식별자를 열어준다.*/
  listenfd = Open_listenfd(argv[1]); //듣기 소켓 오픈

  /*클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit() 호출*/
  while (1) { // 반복적으로 연결 요청 접수
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    
    /*연결이 성공했다는 메세지를 위해, Getnameinfo를 호출하면서 hostname과 port가 채워진다.*/
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    /*doit 함수 실행*/
    doit(connfd);   // line:netp:tiny:doit
    /*서버 연결 식별자를 닫아준다.*/
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd) // 클라이언트의 요청 라인을 확인해 정적, 동적 컨텐츠인지를 구분하고 각각의 서버에 보낸다.
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 클라이언트에게서 받은 요청(rio)로 채워진다.
  char filename[MAXLINE], cgiargs[MAXLINE]; //parse_uri를 통해 채워진다.
  rio_t rio;
  
  /* Read request line and headers */
  //요청 라인을 읽고 분석한다.
  Rio_readinitb(&rio, fd); //rio 버퍼와 fd, 여기서는 서버의 connfd를 연결시켜준다.
  Rio_readlineb(&rio, buf, MAXLINE); //그리고 rio(==connfd)에 있는 string 한줄을 모두 buf로 옮긴다.
  printf("Request headers:\n");
  printf("%s", buf); //요청 라인 buf="GET /godzilla.gif HTTP/1.1\0"을 표준 출력만 해줌
  sscanf(buf, "%s %s %s", method, uri, version); //BUF에서 문자열 3개를 읽더와 method, uri, version이라는 문자열에 저장한다. 

  if(strcasecmp(method, "GET")) { //만약 클라이언트가 다른 method 요청시 error 띄움 - tiny는 get만 가능
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); // 요청 라인을 뺀 나머지 요청 헤더들을 무시 한다.(그냥 프린트)

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs); //uri를 파일이름과 cgi 인자 스트림으로 분석 - cgiargs가 1이면 정적, 0이면 동적

  if(stat(filename, &sbuf) < 0) { // 만일 이 파일이 디스크 상에 없으면 에러메시지를 즉시 클라이언트에 보내고 리턴
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if(is_static) { /* Serve static content (정적 컨텐츠)*/
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 이 파일이 보통 파일 & 읽기 권한을 가지고 있다. 
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); // 클라이언트에게 제공 정적 서버에 파일의 사이즈를 같이 보낸다. 
  }
  else { /* Serve dynamic content */
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //이 파일이 동적콘텐츠에서 실행이 가능한지
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // 실행 가능하다면 동적 컨텐츠 제공
  }
}

//에러 메세지와 응답 본체를 서버 소켓을 통해 클라이언트에 보낸다.
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body,longmsg, cause);
  sprintf(body, "%s<hr><em>The Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));


  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

//클라이언트가 버퍼 rp에 보낸 나머지 요청 헤더들은 무시한다.
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) { // buf와 'r\n\'을 실행한 후 같으면 0, 다르면 1로 반환한다.
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

//HTTP URI를 분석하는 함수
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if(!strstr(uri, "cgi-bin")) { /* Static content(정적 컨텐츠일 때) */
    strcpy(cgiargs, ""); //CGI 인자 스트링을 지우고 
    strcpy(filename, "."); 
    strcat(filename, uri); //uri를 ./index.html 같은 상대 리눅스 경로이름으로 변환한다.
    if(uri[strlen(uri) - 1] == '/'){ //uri가 '/'문자로 끝난다면 기본 파일이름을 추가한다.
      strcat(filename, "home.html");
    }
      
    return 1;
  }
  else { /* Dynamic content(동적 컨텐츠일때) */
    ptr = index(uri, '?'); 
    if(ptr) { // 모든 CGI 인자들을 추출하고 
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);//나머지 URI 부분을 상대 리눅스 파일 이름으로 변환한다.
    return 0;
  }
}

// 클라이언트가 원하는 정적 컨텐츠 디렉토리를 받아온다. 응답라인과 헤더를 작성하고 서버에게 보낸다. 그 후 정적 컨텐츠 파일을 읽어 그 응답 본체를 클라이언트에 보낸다.
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE],buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype); // 파일 이름의 접미어 부분을 검사해서 파일 타입을 결정
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 클라이언트에 응답줄과 응답 헤더를 보낸다.
  sprintf(buf, "%sServer : Tiny Web Server\r\n", buf); // 응답 헤더 작성
  sprintf(buf, "%sConnection : close\r\n", buf);
  sprintf(buf, "%sContent-length : %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type : %s\r\n\r\n", buf, filetype);

  Rio_writen(fd, buf, strlen(buf)); //요청한 파일의 내용을 연결 식별자 FD로 복사해서 응답 본체를 보낸다.
  printf("Response headers : \n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // 읽기 위해 FILENAME을 오픈하고  srcfd 식별자를 얻어온다.
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 리눅스 mmap 함수는 요청한 파일을 가상 메모리 영역으로 매핑한다.
  Close(srcfd); // 매핑한 후에는 필요가 없으므로 srcfd식별자를 닫는다. - 닫지 않으면 치명적일 수 있는 메모리 누수가 발생할 수 있다.
  Rio_writen(fd, srcp, filesize); // rio_writen 함수는 주소 srcp에서 시작하는 filesize 바이트를 클라이언트의 연결 식별자로 복사한다.
  Munmap(srcp, filesize); // 가상메모리 주소를 반환한다. - 치명적일 수 있는 메모리 누스를 피하는게 중요하다. (free함수와 같은 기능을 한다고 생각함)

   /*11.9*/
  // srcfd = Open(filename,O_RDONLY,0);
  // srcp  = (char *)Malloc(filesize);
  // Rio_readn(srcfd,srcp,filesize);
  // Close(srcfd);
  // Rio_writen(fd,srcp,filesize);
  // free(srcp);
}

/*
 * get_filetype - Derive file type from filename
 여러가지 정적 컨텐츠 타입을 지원 - html,gif,png,jpeg, 무형식 테스트 파일
 */
void get_filetype(char *filename, char *filetype) 
{
  if(strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if(strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if(strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if(strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpg");
  else if(strstr(filename,".mp4")){//11.7
    strcpy(filetype,"vedio/mp4");
  }
  else
    strcpy(filetype, "text/plain");
}

//클라이언트가 원하는 동적 컨텐츠 디렉토리를 받아온다. 응답 라인과 헤더를 작성하고 서버에게 보낸다.
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); //클라이언트에 성공을 알려주는 응답라인을 보냄
  Rio_writen(fd, buf, strlen(buf)); //
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if(Fork() == 0) { /* Child */ // 새로운 자식 프로세스를 fork한다.
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // 자식은 query_string 환경 변수를 요청 uri의 cgi 인자들로 초기화한다.
    Dup2(fd, STDOUT_FILENO);                /* Redirect stdout to client */ // 자식은 자식의 표준 출력을 연결 파일 식별자로 재지정한다.
    Execve(filename, emptylist, environ);   /* Run CGI program */ // 그 후 cgi 프로그램 실행
  }/*else가 필요함-자식 프로세스가 0이 걸릴 수 있음*/
  Wait(NULL); /* Parent waits for and reaps child */ // 부모는 자식이 종료되어 정리되는 것을 기다리기 위해 wait함수에서 블록된다.
}