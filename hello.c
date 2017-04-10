#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define PRINT_ENV(x) {printf("<b>%s</b> : %s<br>", x, getenv(x));}

extern char** environ;

void EnumEnvironment() {
	printf("<pre>");
	for (int i = 0; environ[i] != NULL; i++)
    {
        printf("%s\n", environ[i]);
    }
	printf("</pre>");
	/*
    PRINT_ENV("SERVER_NAME");
    PRINT_ENV("QUERY_STRING");
    PRINT_ENV("SERVER_SOFTWARE");
    PRINT_ENV("GATEWAY_INTERFACE");
    PRINT_ENV("SERVER_PROTOCOL");
    PRINT_ENV("REQUEST_METHOD");
    PRINT_ENV("PATH_INFO");
    PRINT_ENV("SCRIPT_NAME");
    PRINT_ENV("REMOTE_ADDR");
    PRINT_ENV("REMOTE_PORT");
	*/
}

int main(int argc, char* argv[]) {
    printf("Content-Type: text/html; charset=utf-8\nX-CGI-test:1234345\n\n");
    char* query_string = getenv("QUERY_STRING");
    printf("<!DOCTYPE html><h1>It works!</h1>");
    int i = 0;
    if (argc > 0) printf("<h3>args</h3>");
    while(argc > 0) {
        printf("<h4><b>%d.</b> %s</h4>", i+1, argv[i]);
        ++i; argc--;
    }
    printf("<h3>Environment:</h3>");
    EnumEnvironment();
    return 0;
}