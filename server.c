/**********************************************************************************
 *
 *  Tccgi  
 *  Auth : MADAO
 *  License : MIT
 *  compiler by tcc link:http://bellard.org/tcc/
 *  cgi :  https://www.ietf.org/rfc/rfc3875
 *
 **********************************************************************************/

#define __NAME__ "Tccgi"
#define __VERSION__ "0.2.4"
    
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _MSC_VER
#include "winsock2.h"
#endif

#define DEFAULT_PORT 9527
#define SOCKET_BACKLOG 24
#define SOCKET_KEEP_TIME 30
#define MAX_CLIENT 256
#define BUFFER_SIZE 8192

#define PATH_LENGTH 1024
#define QUERY_STR_LEN 1024
#define MAX_PARAMS 16
#define PARAM_LENGTH 512
#define ENV_LENGTH 8192
#define MAX_HEADER 16
#define HEADER_LENGTH 1024
#define MAX_BODY 524288

#define INDEX_PAGE "index.html"
#define CGI_EXTNAME "cgi"
#define CGI_TIMEOUT 3

#define PUSH_ENV(x, y, z) {x += strlen(x) + 1; sprintf(x, "%s=%s", y, z);}
#define PUSH_ENV_D(x, y, z) {x += strlen(x) + 1; sprintf(x, "%s=%d", y, z);}
//#define Logln(...) do{char _logbuff[1024]; sprintf(_logbuff, __VA_ARGS__); strcat(_logbuff, "\n"); write(STDOUT_FILENO,_logbuff,strlen(_logbuff));} while(0);

#define Logln(...) do{char _logbuff[1024]; sprintf(_logbuff, __VA_ARGS__); strcat(_logbuff, "\n"); fwrite(_logbuff, strlen(_logbuff), 1, stdout); fflush(stdout);} while(0);
#define SOCPERROR Logln("Socket Error : %d\n", WSAGetLastError());//perror(errstr)

typedef struct _Request {
    char buff[BUFFER_SIZE];
    char path[PATH_LENGTH];
    char query_string[QUERY_STR_LEN];
    char params[PATH_LENGTH + QUERY_STR_LEN];
    char request_uri[PATH_LENGTH + QUERY_STR_LEN];
    char remote_addr[60];
    char script_name[PATH_LENGTH];
    char method[8];
    int remote_port;
} Request;

typedef struct _Response {
    int code;
    int header_num;
    int body_length;
    char phrase[24];
    char header[MAX_HEADER*2][PARAM_LENGTH];
    char* body;
} Response;

typedef enum _StatusFlag
{
    WAIT, READ, WRITE, CLOSED
} StatusFlag;

typedef struct _Client {
    Response response;
    Request request;
    SOCKET fd;
    StatusFlag flag;
    struct sockaddr_in address;
    time_t active;
} Client;

#define HTTP_CODE_NUM 18
char HTTP_CODE[HTTP_CODE_NUM][50] = {
    "200", "OK",
    "400", "Bad Request",
    "403", "Forbidden",
    "404", "Not Found",
    "405", "Method Not Allowed",
    "406", "Not Acceptable",
    "408", "Request Timeout",
    "414", "Request-URI Too Long",
    "500", "Internal Server Error"
};

#define MIME_TYPE_NUM 12
char MIME_TYPE[MIME_TYPE_NUM][50] = {
    "html", "text/html",
    "js", "application/x-javascript",
    "json", "application/json; charset=utf-8",
    "css", "text/css",
    "png", "image/png",
    "jpg", "image/jpeg"
};

// Global
char DEFAULT_TYPE[] = "application/octet-stream";
int bind_port = 9527;
int cgi_timeout = 3;
char cgi_ext[10];
BOOL verbose = FALSE;

char www_root[2048];
size_t cgi_ext_len;

Client* clients[MAX_CLIENT];
int top_client = 0;

void reset_client(Client* client) {
    // SOCKET fd = client->fd;
    // client->fd = fd;
    // memset(&client->response, 0, sizeof(Response));
    // memset(&client->request, 0, sizeof(Request));
    client->flag = READ;
    memset((Request*)&client->request, 0, sizeof(Request));
    memset(client->response.body, 0, sizeof(char)*MAX_BODY);
    client->response.header_num = 0;
    client->response.code = 0;
    client->response.body_length = 0;
    client->response.body_length = 0;
}

Client* create_client() {
    Client *client = (Client*)malloc(sizeof(Client));
    memset(client, 0, sizeof(Client));
    client->response.body = (char*)malloc(sizeof(char)*MAX_BODY);
    client->active = time(NULL);
    client->flag = READ;
    return client;
}

void free_client(Client* client) {
    if (client->response.body) {
        free(client->response.body);
    }
    free(client);
}

Client* get_client(SOCKET fd) {
    int cur = 0;
    Client* c = NULL;
    while(cur < top_client) {
        if (fd == clients[cur]->fd) {
            c = clients[cur];
            break;
        }
        cur++;
    }
    return c;
}

int add_client(Client* c) {
    if (top_client + 1 >= MAX_CLIENT) {
        Logln("Too many clients");
        // return 0;
        exit(500);
    }
    clients[top_client] = c;
    top_client++;
    return top_client;
}

void rm_client(Client* client) {
    int cur = 0;
    int top = top_client;
    Client* temp = client;
    while(cur < top) {
        if (temp == clients[cur]) {
            if (top == top_client) {
                top -= 1;
            }
            clients[cur] = clients[cur + 1];
            temp = clients[cur + 1];
        }
        cur++;
    }
    top_client = top;
}

char* strsep_s(char *buff, char* cdr, char delim, size_t len) {
    size_t i = 0;
    while (i < len && *cdr != '\0' && *cdr != delim) {
        *buff = *cdr;
        ++buff; ++cdr; ++i;
    }
    *buff = '\0'; 
    if (*cdr == '\0') {
        return NULL;
    }
    if (i < len) {
        ++cdr;
    }
    return cdr;
}

int parse_head(const char *data, size_t len, Request *req) {
    size_t i = 0, pi = 0;
    BOOL has_query = FALSE;
    while (i < 6 && data[i] != ' ') {
        req->method[i] = data[i];
        ++i;
    }
    req->method[i] = '\0';
    ++i;
    while (pi < PATH_LENGTH && data[i] != '?' && data[i] != ' ') {
        req->path[pi++] = data[i++];
    }
    req->path[pi] = '\0';

    if (data[i] == '?') has_query = TRUE;
    ++i; pi = 0;
    while(has_query && data[i] != ' ' && pi < QUERY_STR_LEN) {
        req->query_string[pi++] = data[i++];
    }
    req->query_string[pi] = '\0';
    return 0;
}

int clear_buffer(char *buffer, size_t buffsize) {
    size_t i = 0, del = 0;
    while (i + del < buffsize) {
        buffer[i] = buffer[i + del];
        if (buffer[i] == 13 && i + del + 1 < buffsize && buffer[i + del + 1] == 10) {
            buffer[i] = 10;
            ++del;
        }
        ++i;
    }
    return buffsize - del;
}

void build_cgi_req(Request *req, const char* path) {
    sprintf(req->script_name, "%s%s", req->path, cgi_ext);
    if (NULL == strchr(req->query_string, '=') && strlen(req->query_string) > 1) {
        sprintf(req->params, "%s %s", path, req->query_string);
        char* p = strchr(req->params, ' ');
        while('\0' != *p) {
            if ('+' == *p) {
                *p = ' ';
            }
            p++;
        }
    } else {
        strcpy(req->params, path);
    }
}

void build_cgi_env(char* env, Request* req) {
    memset(env, 0, ENV_LENGTH);
    sprintf(env, "%s=%s", "SERVER_NAME", "Boom shaka Laka");
    // env += strlen(env) + 1;
    // sprintf(env, "%s=%s", "PATH", req->path);
    PUSH_ENV(env, "QUERY_STRING", req->query_string);
    PUSH_ENV(env, "SERVER_SOFTWARE", __NAME__);
    PUSH_ENV(env, "GATEWAY_INTERFACE", "CGI/1.1");
    PUSH_ENV(env, "SERVER_PROTOCOL", "HTTP/1.1");
    PUSH_ENV_D(env, "SERVER_PORT", bind_port);
    PUSH_ENV(env, "REQUEST_METHOD", req->method);
    PUSH_ENV(env, "PATH_INFO", req->path);
    PUSH_ENV(env, "SCRIPT_NAME", req->script_name);
    PUSH_ENV(env, "REMOTE_ADDR", req->remote_addr);
    PUSH_ENV_D(env, "REMOTE_PORT", req->remote_port);
    // PUSH_ENV(env, "REQUEST_URI", req->request_uri)
    env = env + strlen(env);
    sprintf(env, "%c%c", 0, 0);
}

int add_header(Response* res, const char* name, const char* value) {
    if (res->header_num < MAX_HEADER) {
        int i = res->header_num * 2;
        strcpy(res->header[i], name);
        strcpy(res->header[i+1], value);
        res->header_num += 1;
        return 0;
    }
    return -1;
}

int send_response(Client* const client) {
    SOCKET conn = client->fd;
    Response* res = (Response*) &client->response;
    char header_buff[HEADER_LENGTH];
    char headers[HEADER_LENGTH*MAX_HEADER];
    sprintf(headers, "HTTP/1.1 %d %s\r\n", res->code, res->phrase);
    memset(header_buff, '\0', HEADER_LENGTH);
    for(int i = 0; i < res->header_num; i++) {
        sprintf(header_buff, "%s: %s\r\n", res->header[i*2], res->header[i*2+1]);
        strcat(headers, header_buff);
    }
    // add server name
    sprintf(header_buff, 
        "Content-Length: %d\r\nServer: %s %s\r\nConnection: keep-alive\r\n\r\n", 
        res->body_length, __NAME__, __VERSION__);
    strcat(headers, header_buff);
    if (send(conn, headers, strlen(headers), 0) == 0) {
        return 1;
    }
    if (res->body_length > 0) {
        if (send(conn, res->body, res->body_length, 0) == 0) {
            return 1;
        }
    }
    // closesocket(conn);
    return 0;
}

void http_response_code(int code, Client* const client) {
    Response* res = (Response*)&client->response;
    res->code = 500;
    strcpy(res->phrase, "Internal Server Error");
    int i = 0;
    do{
        if (atoi(HTTP_CODE[i]) == code) {
            res->code = code;
            strcpy(res->phrase, HTTP_CODE[i+1]);
            break;
        }
        i += 2;
    } while(i < HTTP_CODE_NUM);
    add_header(res, "Content-Type", "text/html");
    sprintf(res->body, "<!DOCTYPE html>\n<center><h1>%d %s</h1><hr>Powered By Tccgi</center>", res->code, res->phrase);
    res->body_length = strlen(res->body);
    client->flag = WRITE;
}

char* mime_type(char *type, const char* path) {
	char ext[8];
	size_t p,e;
    for (int i = 0; i < MIME_TYPE_NUM; i+=2) {
		sprintf(ext, ".%s", MIME_TYPE[i]);
		p = strlen(path);
		e = strlen(ext);
		if (p > e && _stricmp(ext, (char*)(path + p - e)) == 0){
            strcpy(type, MIME_TYPE[i+1]);
            return type;
        }
    }
    strcpy(type, DEFAULT_TYPE);
    return type;
}

int static_file(const char *path, Client* const client) {
    Response* res = (Response*)&client->response;
    FILE * fp;
    errno_t err;
    if ((err = fopen_s(&fp, path, "rb")) != 0) {
        Logln("file %s error!", path);
        http_response_code(500, client);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    int length = ftell(fp);
    if (length >= MAX_BODY) {
        fclose(fp);
        Logln("File too large!");
        http_response_code(500, client);
        return 0;
    }
    rewind(fp);
    fread(res->body, 1, length, fp);
    fclose(fp);
    res->code = 200;
    strcpy(res->phrase, "OK");
    res->body_length = length;
    char type[50];
    mime_type(type, path);
    add_header(res, "Content-Type", type);
    client->flag = WRITE;
    return 0;
}

int cgi_parse(Client* const client, HANDLE hProcess, HANDLE hReadPipe) {
    Response* response = (Response*)&client->response;
    int dwRet;
    DWORD bytesInPipe, bytesRead;
    char* cgi_buff = (char*)malloc(sizeof(char)*MAX_BODY);
    char header_buff[1024];
    char line_buff[1024];
    
    dwRet = WaitForSingleObject(hProcess, cgi_timeout*1000);
    if (dwRet == WAIT_TIMEOUT) {
        Logln("Process timeout");
        // test kill 通过杀死子进程释放socket
        TerminateProcess(hProcess, 0);
        http_response_code(408, client);
        return 0;
    }
    if (dwRet == WAIT_FAILED) {
        Logln("Process error : %d", GetLastError());
        return 1;
    }
    // reset cgi_buff
    memset(cgi_buff, 0, MAX_BODY);
    if ( !PeekNamedPipe(hReadPipe, cgi_buff, MAX_BODY, &bytesRead, &bytesInPipe, NULL) ) {
        return 2;
    }
    // conver \r\n to \n
    clear_buffer(cgi_buff, bytesRead + 1);
    // check CGI-field
    response->code = 200;
    strcpy(response->phrase, "OK");

    char *cdr = cgi_buff;
    cdr = strsep_s(line_buff, cdr, '\n', 1024);
    if (strncmp("Content-Type", line_buff, 12) == 0) {
        char* ct = strchr(line_buff, ':');
        if (ct == NULL || strlen(ct) < 3) {
            return 3;
        }
        ++ct;
        while(*ct == ' ') {++ct;}
        add_header(response, "Content-Type", ct);
    } else if (strncmp("Status", line_buff, 6) == 0) {
        char code[4];
        char *p = line_buff;
        int cur = 0;
        while(strlen(p) > 2 && cur < 3) {
            if (*p >= '0' && *p <= '9') {
                code[cur] = *p;
                ++cur;
            } else if (cur > 0) {
                return 4;
            }
            ++p;
        }
        ++p;
        code[cur] = '\0';
        response->code = atoi(code);
        strcpy(response->phrase, p);
    } else if (strncmp("Location", line_buff, 8) == 0) {
        // @todo
        add_header(response, "Location", "");
    } else {
        Logln("Error cgi content");
        return 5;
    }
    // check "\n\n"
    int i = 0;
    memset(header_buff, '\0', HEADER_LENGTH);
    cdr = strsep_s(line_buff, cdr, '\n', 1024);
    while (cdr != NULL && strlen(line_buff) > 1 && i++ < MAX_HEADER) {
        char* p = line_buff;
        p = strsep_s(header_buff, p, ':', 1024);
        if (p == NULL || strlen(p) == 1) {
            return 6;
        }
        while(' ' == *p) {++p;}
        add_header(response, header_buff, p);
        cdr = strsep_s(line_buff, cdr, '\n', 1024);
    }
    if (i >= MAX_HEADER || strlen(line_buff) > 0) {
        return 10;
    }
    size_t body_length = 0;
    if (cdr != NULL) {
        body_length = strlen(cdr);
        strncpy(response->body, cdr, body_length);
    }
    free(cgi_buff);
    response->body_length = body_length;
    client->flag = WRITE;
    return  0;
}

int cgi_process(Client* const client, const char* cmd) {
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    char *lpEnv = (char*)malloc(sizeof(char)*ENV_LENGTH);

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL; //使用系统默认的安全描述符
    sa.bInheritHandle = TRUE; //一定要为TRUE，不然句柄不能被继承。  bug: socket 也被继承，无法关闭

    CreatePipe(&hReadPipe,&hWritePipe,&sa,0); //创建pipe内核对象,设置好hReadPipe,hWritePipe.
    STARTUPINFO si;
    PROCESS_INFORMATION pi; 
    si.cb = sizeof(STARTUPINFO);
    GetStartupInfo(&si); 
    si.hStdError = hWritePipe; //set stderr hWritePipe
    si.hStdOutput = hWritePipe; //set stdout hWritePipe
    si.wShowWindow = SW_HIDE;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    Logln(client->request.params);
    build_cgi_env(lpEnv, (Request*)&client->request);
    if (0 == CreateProcess(cmd, client->request.params, NULL, NULL, TRUE, 0, lpEnv, NULL, &si, &pi)) {
        Logln("Error create %d", GetLastError());
        http_response_code(500, client);
        return 0;
    }
    int ret;
    if ((ret = cgi_parse(client, pi.hProcess, hReadPipe)) != 0) {
        Logln("Cgi error %d", ret);
        http_response_code(500, client);
    }
    
    CloseHandle(hReadPipe);
    CloseHandle(hWritePipe);
    free(lpEnv);
    return 0;
}

int dispatch(Client* const client) {

    int len;

    char* buffer = (char*)malloc(sizeof(char)*BUFFER_SIZE);
    memset(buffer, 0, BUFFER_SIZE);
    len = recv(client->fd, buffer, BUFFER_SIZE, 0);

    int rtn = 0;
    do {

        // close socket
        if (len == 0 || len == SOCKET_ERROR) {
            if (len == SOCKET_ERROR) {
                SOCPERROR;
            }
            rtn = -1; break;
        }

        if (verbose) {
            Logln("\r\n%s", buffer);
        }

        Request* req = &client->request;
        char* address = inet_ntoa(client->address.sin_addr);

        if (parse_head(buffer, len, req) != 0) {
            Logln("Bad request from %s", address);
            Logln("Recv : %s", buffer);
            http_response_code(400, client);
            break;
        }

        if (!verbose) {
            Logln("%s: %s %s", req->method, address, req->path);
        }

        // just support get
        if (0 != _stricmp(req->method, "GET")) {
            http_response_code(405, client);
            break;
        }

        strcpy(req->remote_addr, address);
        req->remote_port = client->address.sin_port;

        char path[PATH_LENGTH];
        char cgi_path[PATH_LENGTH];
        strcpy(path, www_root);
        strcat(path, req->path);
        if (0 == strcmp(req->path, "/")) {
            strcat(path, INDEX_PAGE);
        }

        if (0 == _access(path, 0) && 0 != strcmp((path + strlen(path) - cgi_ext_len), cgi_ext)) {
            rtn = static_file(path, client);
            break;
        } else {
            sprintf(cgi_path, "%s%s", path, cgi_ext);
            if (0 == _access(path, 0)) {
                build_cgi_req(req, path);
                rtn = cgi_process(client, path);
                break;
            } else if (0 == _access(cgi_path, 0)) {
                build_cgi_req(req, cgi_path);
                rtn = cgi_process(client, cgi_path);
                break;
            }
        }

        http_response_code(404, client);
    } while (0);
    free(buffer);
    return rtn;
}

SOCKET init_socket() {
    WSADATA Ws;
    if ( WSAStartup(MAKEWORD(2,2), &Ws) != 0 ) { 
        SOCPERROR;
        return -4;
    };
    //sockfd
    SOCKET server_fd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);

    //sockaddr_in
    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(bind_port);
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
        SOCPERROR;
        return -1;
    } 
    
    int nNetTimeout = 1000;
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&nNetTimeout, sizeof(nNetTimeout)) == SOCKET_ERROR ||
        setsockopt(server_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&nNetTimeout, sizeof(nNetTimeout)) == SOCKET_ERROR) {
        SOCPERROR;
        return -1;
    }

    ///bind，，success return 0，error return -1
    if(bind(server_fd,(struct sockaddr *)&server_sockaddr,sizeof(server_sockaddr)) == SOCKET_ERROR) {
        SOCPERROR;
        return -2;
    }

    //listen，success return 0，error return -1
    if(listen(server_fd, SOCKET_BACKLOG) == SOCKET_ERROR) {
        SOCPERROR;
        return -3;
    }

    if (server_fd < 0) {
        SOCPERROR;
        return -4;
    }
    return server_fd;
}

void main_loop() {

    SOCKET server_fd = init_socket();
    if (server_fd < 0) {
        exit(10);
    }

    Logln("Server start...");
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;
    struct timeval tv;
    
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    int test = 0;
    while(TRUE) {
        // Sleep(1000);
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        FD_SET(server_fd, &readfds);

        time_t now = time(NULL);
        for (int i = 0; i < top_client; i++) {
            Client* client = clients[i];
            if (now - client->active > SOCKET_KEEP_TIME) {
                client->flag = CLOSED;
            }
            if (client->flag != CLOSED) {
                FD_SET(client->fd, &exceptfds);
            }
            switch(client->flag) {
                case WAIT:
                case READ:
                    FD_SET(client->fd, &readfds);
                    break;
                case WRITE:
                    FD_SET(client->fd, &writefds);
                    break;
                case CLOSED:
                    Logln("%s closed",inet_ntoa(client->address.sin_addr));
                    closesocket(client->fd);
                    rm_client(client);
                    free_client(client);
                    i--;
                    break;
                default:
                    break;
            }
        }
        if (select(0, &readfds, &writefds, &exceptfds, &tv) == SOCKET_ERROR) {
            SOCPERROR;
            exit(10);
        }
        if (FD_ISSET(server_fd, &readfds)) {
            //DWORD address_len = 60;
            Client* client = create_client();
            int length = sizeof(client->address);
            client->fd = accept(server_fd, (SOCKADDR *)&client->address, &length);
            if (client->fd > 0) {
                //WSAAddressToString((LPSOCKADDR)&client_addr, sizeof(SOCKADDR), NULL, (char*)&client->address, &address_len);
                add_client(client);
            } else {
                free_client(client);
                Logln("failed");
            }
            Logln("accept %d", client->fd);
        }
        for (int i = 0; i < top_client; i++) {
            Client* client = clients[i];
            if (FD_ISSET(client->fd, &exceptfds)) {
                client->flag = CLOSED;
            } else if (FD_ISSET(client->fd, &readfds)) {
                if (client->flag == READ) {
                    client->active = now;
                    if (dispatch(client) != 0) {
                        client->flag = CLOSED;
                    }
                }
            } else if (FD_ISSET(client->fd, &writefds)) {
                if(client->flag == WRITE) {
                    client->active = now;
                    if (send_response(client) != 0){
                        client->flag = CLOSED;
                    } else {
                        reset_client(client);
                    }
                }
            }
        }
        //Logln("read %d write %d except %d client %d loop %d", readfds.fd_count, writefds.fd_count, exceptfds.fd_count, top_client, test++);
    }
    closesocket(server_fd);
}

int main(int argc, char* argv[]) {

    bind_port = DEFAULT_PORT;
    cgi_timeout = CGI_TIMEOUT;
    sprintf(cgi_ext,".%s", CGI_EXTNAME);
    cgi_ext_len = strlen(cgi_ext);
    // get root path
    getcwd(www_root, MAX_PATH);
    while(argc > 1) {
        if (0 == strcmp(argv[argc - 1], "-v")) {
            verbose = TRUE;
            argc -= 1;
            continue;
        }
        if (argc > 2) {
            if (0 == strcmp(argv[argc - 2], "-p") && atoi(argv[argc-1]) > 0) {
                bind_port = atoi(argv[argc - 1]);
            } else if (0 == strcmp(argv[argc - 2], "-t") && atoi(argv[argc - 1]) > 0) {
                cgi_timeout = atoi(argv[argc - 1]);
            } else if (0 == strcmp(argv[argc - 2], "-e") && strlen(argv[argc - 1]) < 10) {
                sprintf(cgi_ext,".%s", argv[argc - 1]);
                cgi_ext_len = strlen(cgi_ext);
            } else if (0 == strcmp(argv[argc - 2], "-d") && strlen(argv[argc - 1]) < MAX_PATH) {
                strcpy(www_root, argv[argc - 1]);
            }
            argc -= 2;
            continue;
        }
        break;
    }
    
    if (argc > 1) {
        printf("Usage:\n\
 -d root directory\n\
 -p port\tdefault port is %d\n\
 -t cgi timeout\tdefault is %d seconds\n\
 -e extname\tdefault is %s\n\
 -v verbose\n", DEFAULT_PORT, CGI_TIMEOUT, CGI_EXTNAME);
            exit(1);
    }
    
    Logln("www_root : %s\nCGI extname : %s\nBind port : %d\nCGI timeout : %d seconds",
        www_root, cgi_ext + 1, bind_port, cgi_timeout);
    main_loop();
    return 0;
}
