### 11.10 문제 정리
* 문제 의도 : form 형식의 HTML 파일을 만들어서 입력 받은 2개의 인자를 동적 컨텐츠로 표시할 것

**결과**<br>
입력<br><Br>
브라우저 창에 
```
ip주소/cgi-bin/form-adder
```
작성 후 form에 입력하고 결과 확인<br>

### 11.11 문제 정리
* 문제 의도 : HTTP 메소드를 GET메소드가 아닌 HEAD메소드를 사용해서 출력할 것

**HEAD 메소드란?**
> 리소스를 GET메서드로 요청했을 때 응답으로 오는 헤더부분만 요청하는 메소드이다.

**HEAD 메소드를 사용하는 이유가 뭘까?**<br>
1. HEAD 메소드는 caching을 사용하는 클라이언트가 가장 최근 접속한 이후 document가 바뀌었는지 보기 위해 사용한다. 또한 본문에 포함된 모든 정보를 모두 얻는 것보다 header부분만 가진 응답을 필요로 할 때 사용된다.<br>

**결과**<br>

<p>입력</p>

```c
HEAD / HTTP/1.1
```

클라이언트 출력
```c
HTTP/1.0 200 OK
Server: Tiny Web Server
Connection: close
Content-length: 282
Content-type: text/html
``` 

서버 출력
```c
HEAD / HTTP/1.1
Response headers:
HTTP/1.0 200 OK
Server: Tiny Web Server
Connection: close
Content-length: 282
Content-type: text/html
```

<p>GET 메소드를 사용했을 때와는 클라이언트 출력이 다르다.</p>
* 클라이언트 출력

```c
HTTP/1.0 200 OK
Server: Tiny Web Server
Connection: close
Content-length: 282
Content-type: text/html

<!DOCTYPE html>
<html>
<head><title>test</title></head>
<body> 
<img align="middle" src="godzilla.gif">
Dave O'Hallaron
<video width="320" height="240" controls>
    <source src="cloud.mp4" type="video/mp4">
    Your browser does not support the video tag.
</video>
</body>
</html>
```

<p>🙄 문제점 </p>
정적에서는 GET과 HEAD 메소드가 차이가 있는데 동적에서는 똑같은 결과가 나오고 있다 왜그럴까..?<br>


참고한 사이트<br>
[HEAD 메소드를 사용하는 이유](https://straw961030.tistory.com/246)
