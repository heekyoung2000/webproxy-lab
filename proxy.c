#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
//caching
#define LRU_MAGIC_NUMBER 9999
#define CACHE_OBJS_COUNT 10

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void doit(int clientfd); //쓰레드 서버의 동작
void parse_uri(char *uri, char *hostname, char *path, int *port); //받은 URI 처리함수
void build_http_header(char *http_header, char *hostname,char *path, int port, rio_t *client_rio); //엔드 서버로 넘기 헤더 생성
int connect_endServer(char *hostname, int port, char *http_header); //프록시와 엔드 서버 연결
void *thread(void* vargsp); // 쓰레드 생성

void cache_init(); //캐시 초기화
int cache_find(char *url); //캐시 찾기
int cache_eviction(); //제일 오래된 캐시 빼기
void cache_LRU(int index); //Last Recently Used 갱신
void cache_uri(char *uri, char *buf);// 캐시에서 빠진 곳에 정보 입력
void readerPre(int i); //읽기 시작
void readerAfter(int i);//읽기 종료

typedef struct {
    char cache_obj[MAX_OBJECT_SIZE];
    char cache_url[MAXLINE];
    int LRU;
    int isEmpty;

    int readCnt;            /*count of readers*/
    sem_t wmutex;           /*protects accesses to cache*/
    sem_t rdcntmutex;       /*protects accesses to readcnt*/

    int writeCnt;           
    sem_t wtcntMutex;       /*protects accesses to wtcnt*/
    sem_t queue;            /*protects read - write starvation*/

}cache_block;

typedef struct {
    cache_block cacheobjs[CACHE_OBJS_COUNT];  /*ten cache blocks*/
}Cache;

Cache cache;

/*
  main() - 프록시 서버에 사용할 포트 번호를 인자로 받아, 
  프록시 서버가 클라이언트와 연결할 연결 소켓 clientfd 생성 후 doit() 함수 실행
*/
int main(int argc, char **argv) {//argc: 인자 개수, argv : 인자 배열
  int listenfd, *clientfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen; //소켓 구조의 길이를 정하는 변수명 설정
  struct sockaddr_storage clientaddr; //클라이언트의 ip 주소를 저장하는 저장소 설정
  pthread_t tid; // 쓰레드 식별자

  cache_init(); // cache 초기화

  if (argc != 2) { //입력 인자 2개가 아니면 
    fprintf(stderr, "usage: %s <port>\n", argv[0]); //에러 메세지 출력
    exit(1);
  }
  Signal(SIGPIPE, SIG_IGN); //master thread 종료 방지 - 이미 닫힌 소켓에 데이터 보내기를 했을 때 서버가 죽는 현상을 방지
  listenfd = Open_listenfd(argv[1]); //argv[1]=port번호에 연결 요청을 받을 준비가 된 듣기 식별자를 return

  // 연결 시마다 쓰레드 생성
  while (1) {
    clientlen = sizeof(clientaddr); //

    clientfd = Malloc(sizeof(int));
    *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
  
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s). \n", hostname, port);

    /* sequential handle the client transaction */
    // doit(clientfd); 
    // Close(clientfd);

    // doit()과 Close()를 쓰레드 안에서 수행
    Pthread_create(&tid, NULL, thread, clientfd);
  }
  return 0;
}

/*
    thread() - 새롭게 생성된 쓰레드 안에서 클라이언트와의 통신을 수행한다
*/
void* thread(void *vargs){
    int clientfd = *((int*)vargs);
    Pthread_detach(pthread_self());  // 자기 자신을 분리
    Free(vargs);
    doit(clientfd);
    Close(clientfd);
}

/*
  doit() - 클라이언트의 요청을 수신 및 파싱
  1) 엔드 서버의 hostname, path, port를 가져오기 
  2) 엔드 서버에 보낼 요청 라인과 헤더를 만들 변수들을 생성
  3) 프록시 서버와 엔드 서버를 연결하고 엔드 서버의 응답 메세지를 클라이언트에 전송
*/
void doit(int fd) {
  int end_serverfd;  // the end server file descriptor

  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char endserver_http_header [MAXLINE];
  // store the request line arguments
  char hostname[MAXLINE],path[MAXLINE];
  int port;

  // server rio is endServer's rio
  rio_t client_rio, server_rio;

  // 클라이언트가 보낸 요청 헤더에서 method, uri, version을 가져옴
  Rio_readinitb(&client_rio, fd);
  Rio_readlineb(&client_rio, buf, MAXLINE);
  printf("Request headers to proxy:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);  // read the client request line

  // 지원하지 않는 method인 경우 예외 처리
  char uri_store[100]; // caching
  strcpy(uri_store,uri); //caching - store the original uri
  if(strcasecmp(method,"GET")){
    printf("Proxy does not implement the method");
    return;
  }
  // if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
  //   client_error(fd, method, "501", "Not implemented", 
  //     "Proxy does not implement this method");
  //   return;
  // }

  int cache_index;
  if((cache_index=cache_find(uri_store))!=-1){/*in cache then return the cache content*/
    readerPre(cache_index);
    Rio_writen(fd,cache.cacheobjs[cache_index].cache_obj,strlen(cache.cacheobjs[cache_index].cache_obj));
    readerAfter(cache_index);
    cache_LRU(cache_index);
    return;
    }
  // 프록시 서버가 엔드 서버로 보낼 정보들을 파싱
  parse_uri(uri, hostname, path, &port);
  // 프록시 서버가 엔드 서버로 보낼 요청 헤더들을 생성
  build_http_header(endserver_http_header, hostname, path, port, &client_rio);

  /* 프록시 서버와 엔드 서버를 연결 */
  end_serverfd = connect_endServer(hostname, port, endserver_http_header);
  if (end_serverfd < 0) {
    printf("connection failed\n");
    return;
  }
  // 엔드 서버에 HTTP 요청 헤더 전송
  Rio_readinitb(&server_rio, end_serverfd);
  Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

  /* 엔드 서버로부터 응답 메세지를 받아 클라이언트에게 전송 */
  //caching
  char cachebuf[MAX_OBJECT_SIZE];
  int sizebuf=0;

  size_t n;
  while((n = Rio_readlineb(&server_rio, buf, MAXLINE))!= 0) {

    sizebuf+=n; // caching
    if(sizebuf <MAX_OBJECT_SIZE) strcat(cachebuf,buf); //caching

    // printf("proxy received %d bytes,then send \n", n);
    Rio_writen(fd, buf, n); // fd -> client와 proxy 연결 소켓. proxy 관점.
  }
  Close(end_serverfd);

  /*store it*/
  if(sizebuf < MAX_OBJECT_SIZE){
    cache_uri(uri_store,cachebuf);
  }
}


/*
  parse_uri() - 클라이언트의 uri를 파싱해 서버의 hostname, path, port를 찾음
*/
void parse_uri(char *uri, char *hostname, char *path, int *port) {
  *port = 80;  // default port

  char* pos = strstr(uri, "//");  // http://이후의 string들
  pos = pos != NULL ? pos + 2 : uri;  // http:// 없어도 가능
  
  /* port와 path를 파싱 */
  char *pos2 = strstr(pos, ":");
  if(pos2 != NULL) {
    *pos2 = '\0';
    sscanf(pos, "%s", hostname);
    sscanf(pos2 + 1, "%d%s", port, path);  // port change from 80 to client-specifying port
  } else {
    pos2 = strstr(pos, "/");
    if(pos2 != NULL) {
      *pos2 = '\0';
      sscanf(pos, "%s", hostname);
      *pos2 = '/';
      sscanf(pos2, "%s", path);
    }
    else 
      sscanf(pos, "%s", hostname);
  }
  return;
}

/*
  build_http_header() - 클라이언트로부터 받은 요청 헤더를 정제해서 프록시 서버가 엔드 서버에 보낼 요청 헤더를 생성
*/
void build_http_header(
  char *http_header,
  char *hostname,
  char *path,
  int port,
  rio_t *client_rio
) {
  char buf[MAXLINE]; 
  char request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

  // 응답 라인 생성
  sprintf(request_hdr, requestlint_hdr_format, path);

  // 클라이언트 요청 헤더들에서 Host header와 나머지 header들을 구분해서 넣어줌
  while(Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
    if(strcmp(buf,endof_hdr) == 0) break;  // EOF, '\r\n' 만나면 끝

    /* 호스트 헤더 찾기 */
    if(!strncasecmp(buf, host_key, strlen(host_key))) {
      strcpy(host_hdr, buf);
      continue;
    }
    /* 나머지 헤더 찾기 */
    if(strncasecmp(buf, connection_key, strlen(connection_key))
          && strncasecmp(buf,proxy_connection_key,strlen(proxy_connection_key))
          && strncasecmp(buf,user_agent_key,strlen(user_agent_key)))
    {
        strcat(other_hdr,buf);
    }
  }
  if(strlen(host_hdr) == 0)
    sprintf(host_hdr, host_hdr_format, hostname);
  
  // 프록시 서버가 엔드 서버로 보낼 요청 헤더 작성
  sprintf(http_header,"%s%s%s%s%s%s%s",
          request_hdr,
          host_hdr,
          conn_hdr,
          prox_hdr,
          user_agent_hdr,
          other_hdr,
          endof_hdr);
  return;
}

/*
  connect_endServer() - 프록시 서버와 엔드 서버를 연결
*/
inline int connect_endServer(char *hostname, int port, char *http_header) {
  char portStr[100];
  sprintf(portStr, "%d", port);
  return Open_clientfd(hostname, portStr);
}

void client_error(int fd, char *cause, char *errnum, 
    char *shortmsg, char *longmsg) 
{
  char buf[MAXLINE], body[MAXBUF];

  // Build the HTTP response body
  sprintf(body, "<html><title>Proxy Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s : %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s : %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Proxy Web server</em>\r\n", body);

  // Print the HTTP response
  sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-Type : text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-Length : %d\r\n\r\n", (int)strlen(body));

  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// Cache function

//캐시 초기화
void cache_init(){
    int i;
    for(i=0;i<CACHE_OBJS_COUNT;i++){
        cache.cacheobjs[i].LRU = 0;
        cache.cacheobjs[i].isEmpty = 1;
        Sem_init(&cache.cacheobjs[i].wmutex,0,1);
        Sem_init(&cache.cacheobjs[i].rdcntmutex,0,1);
        cache.cacheobjs[i].readCnt = 0;

        cache.cacheobjs[i].writeCnt = 0;
        Sem_init(&cache.cacheobjs[i].wtcntMutex,0,1);
        Sem_init(&cache.cacheobjs[i].queue,0,1);
    }
}
//starvation, rdcnt 잠금, 첫 읽기시에는 쓰기 잠금
void readerPre(int i){
    P(&cache.cacheobjs[i].queue);
    P(&cache.cacheobjs[i].rdcntmutex);
    cache.cacheobjs[i].readCnt++;
    if(cache.cacheobjs[i].readCnt==1) P(&cache.cacheobjs[i].wmutex);
    V(&cache.cacheobjs[i].rdcntmutex);
    V(&cache.cacheobjs[i].queue);
}
//읽기 종료 후 readcnt 감소
void readerAfter(int i){
    P(&cache.cacheobjs[i].rdcntmutex);
    cache.cacheobjs[i].readCnt--;
    if(cache.cacheobjs[i].readCnt==0) V(&cache.cacheobjs[i].wmutex);
    V(&cache.cacheobjs[i].rdcntmutex);

}
//읽기와 유사하나 starvation요소만 빠짐
void writePre(int i){
    P(&cache.cacheobjs[i].wtcntMutex);
    cache.cacheobjs[i].writeCnt++;
    if(cache.cacheobjs[i].writeCnt==1) P(&cache.cacheobjs[i].queue);
    V(&cache.cacheobjs[i].wtcntMutex);
    P(&cache.cacheobjs[i].wmutex);
}

void writeAfter(int i){
    V(&cache.cacheobjs[i].wmutex);
    P(&cache.cacheobjs[i].wtcntMutex);
    cache.cacheobjs[i].writeCnt--;
    if(cache.cacheobjs[i].writeCnt==0) V(&cache.cacheobjs[i].queue);
    V(&cache.cacheobjs[i].wtcntMutex);
}

/*find url is in the cache or not */
int cache_find(char *url){
    int i;
    for(i=0;i<CACHE_OBJS_COUNT;i++){
        readerPre(i);
        if((cache.cacheobjs[i].isEmpty==0) && (strcmp(url,cache.cacheobjs[i].cache_url)==0)){
            readerAfter(i);    
            break;
        }
        readerAfter(i);
    }
    if(i>=CACHE_OBJS_COUNT) return -1; /*can not find url in the cache*/
    return i;
}

/*find the empty cacheObj or which cacheObj should be evictioned*/
int cache_eviction(){
    int min = LRU_MAGIC_NUMBER;
    int minindex = 0;
    int i;
    for(i=0; i<CACHE_OBJS_COUNT; i++)
    {
        readerPre(i);
        if(cache.cacheobjs[i].isEmpty == 1){/*choose if cache block empty */
            minindex = i;
            readerAfter(i);
            break;
        }
        if(cache.cacheobjs[i].LRU< min){    /*if not empty choose the min LRU(가장 안쓰인것)*/
            minindex = i;
            readerAfter(i);
            continue;
        }
        readerAfter(i);
    }

    return minindex;
}
/*update the LRU number except the new cache one*/
void cache_LRU(int index){

    writePre(index);
    cache.cacheobjs[index].LRU = LRU_MAGIC_NUMBER;
    writeAfter(index);

    int i;
    for(i=0; i<CACHE_OBJS_COUNT; i++)    {
        if(i==index)
            continue;
        writePre(i);
        if(cache.cacheobjs[i].isEmpty==0){
            cache.cacheobjs[i].LRU--;
        }
        writeAfter(i);
    }
}
/*cache the uri and content in cache*/
void cache_uri(char *uri,char *buf){


    int i = cache_eviction();

    writePre(i);/*writer P*/

    strcpy(cache.cacheobjs[i].cache_obj,buf);
    strcpy(cache.cacheobjs[i].cache_url,uri);
    cache.cacheobjs[i].isEmpty = 0;

    writeAfter(i);/*writer V*/

    cache_LRU(i); //LRU 갱신
}