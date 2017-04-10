## Tccgi

简单的玩具型http服务器，支持基础的cgi请求。可以通过[tcc](http://bellard.org/tcc/)编译。  

> 已知在选择64位tcc时会导致select的句柄异常，建议使用32位编译。

### 构建：
  
`winsock2.h` 来自[MinGW](http://mingw.org/)项目的win32API。  
依赖`ws2_32.dll`,使用：
```  
tiny_imdef.exe ws2_32.dll
```
生成`ws2_32.def` 执行:
```  
tcc.exe server.c ws2_32.def
```  

测试例子同样由`tcc`编译生成  
```
tcc hello.c -o hello.cgi
```  

构建完成， 访问`http://127.0.0.1:9527/hello` : ) 


### 支持程度：
  
普通静态文件(限制大小512kB)，cgi输出(512kB)。  
仅支持`GET`请求  
支持的环境变量：
```  
SERVER_NAME
QUERY_STRING
SERVER_SOFTWARE
GATEWAY_INTERFACE
SERVER_PROTOCOL
REQUEST_METHOD
PATH_INFO
SCRIPT_NAME
REMOTE_ADDR
REMOTE_PORT
```

cgi支持`Content-Type`与`Status`头部，不支持`Location`头  
  
### 执行：
```  
server.exe [-d www_root | -p port | -t cgi_timeout | -e cgi_extname | -v]
Usage:
-d root directory
-p port        default port is 9527
-t cgi timeout default is 3 seconds
-e extname     default is cgi
-v verbose

# 测试hello.bat作为cgi脚本 打开 http://127.0.0.1:9527/hello.bat
server.exe -e bat 
```  

### 历史版本

[https://github.com/LaoQi/icode/tree/master/tccgi](https://github.com/LaoQi/icode/tree/master/tccgi)

### License : MIT 

### winsock2.h [License](http://mingw.org/license)
