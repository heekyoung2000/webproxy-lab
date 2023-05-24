/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"


int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0,n2=0;

  /*Extract the two argument*/
  /*서버 프로세스가 만들어준 QUERT_STRING 환경 변수를 gentenv로 가져와 buf에 넣음*/
  if((buf = getenv("QUERY_STRING"))!=NULL){
    p = strchr(buf,'&'); // 포인터 p는 &가 있는 곳의 주소
    *p = '\0'; // &를 \0으로 바꿔주고
    strcpy(arg1,buf); // & 앞에 있었던 인자
    strcpy(arg2,p+1); // & 뒤에 있었던 인자
    n1 = atoi(arg1);
    n2 = atoi(arg2);
   
  }
  
  /*Make the response body*/
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add. com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>",content);
  sprintf(content,"%sThe answer is: %d + %d = %d\r\n<p>",content,n1,n2,n1+n2);
  sprintf(content, " %sThanks for visiting!\r\n",content);

  /*Generate the HTTP response*/
  /*CGI 프로세스가 부모인 서버 프로세스 대신 채워 주어야 하는 응답 헤더값*/
  printf("Connection: close\r\n");
  printf("Content-length:%d\r\n",(int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content); // 응답 본체를 클라이언트 표준 출력!
  fflush(stdout);
  exit(0);


}
