
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<signal.h>
#include<sys/types.h>          
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<unistd.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include"threadpool.h"
#define BUFLEN 256
#define h_addr h_addr_list[0]

#define  A500 "500 Internal Server Error"
#define  length_500 "144"
#define  BODY_500  "Some server side error."

#define  A501 "501 Not supported"
#define  length_501 "129"
#define  BODY_501 "Method is not supported."

#define  A404 "404 Not Found"
#define  length_404 "112"
#define  BODY_404  "File not found."

#define  A400 "400 Bad Request"
#define  length_400 "113"
#define  BODY_400  "Bad Request."

#define  A403 "403 Forbidden"
#define  length_403 "111"
#define  BODY_403  "Access denied."


char* find_host(char* request);
int check_first_line_request(char* request);
int  read_from_filter(char* file_name);
char* build_request(char* request);
void usage();
char* build_error_string(char* type_err , char* length,char* err);
int check_if_host_in_filter(char* host);
char* get_host(char* request);
int get_port(char*host);
char* build_client_side(char* host,int port,char* request);
int  proxy_management(void * u);
void free_filter();
int check_argv_arg(char* arg);
char** filter;
int size_row=0;

int main(int argc, char* argv[]){
    // check if have 5 arg
    if(argc<5){
        usage();
        return 0;
    }
    // check if is number 
    if(check_argv_arg(argv[1])==-1){
        usage();
        return 0 ;
    }
     if(check_argv_arg(argv[2])==-1){
        usage();
        return 0 ;
    }
     if(check_argv_arg(argv[3])==-1){
        usage();
        return 0 ;
    }
    int port =atoi(argv[1]);

    int nums_of_thread=atoi(argv[2]);
    int num_of_jobs = atoi(argv[3]);
    //dont get 0 
    if(num_of_jobs==0 || nums_of_thread==0 || nums_of_thread>MAXT_IN_POOL || port <=0){
        usage();
        return 0;
    }
    // check if have file name 
    char* file_name = argv[4];
    if(file_name==NULL){
        usage();
        return 0;
    }
    // check if file exists
    int check  =read_from_filter(file_name);
    if(check==-1){
        usage();
        return 0;
    }
    check =0;
    //create  the threadpool for doing the work
    threadpool* p = create_threadpool(nums_of_thread);
    if(p==NULL)
        return -1;
    // proxy side
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd<0){
        destroy_threadpool(p);
        perror("socket\n");
        exit(1);
    }
    serv_addr.sin_family =AF_INET;
    serv_addr.sin_addr.s_addr=INADDR_ANY;
    serv_addr.sin_port = htons(port);
    check = bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr));
    if(check<0){
        perror("bind\n");
        exit(1);
    }       
    check = listen(sockfd,5);
    if(check==-1){
        perror("listen\n");
        exit(1);
    }   
    int  fd [num_of_jobs];
    
    // socket  each thread have socket 
    for(int i=0;i<num_of_jobs;i++){
        fd[i] = accept(sockfd,NULL,NULL);// with for job to come in
        if(fd[i]<0){
            perror("failed on accept\n");
            continue;
        }
	    dispatch(p,proxy_management,(void*)&fd[i]);// sand the thread to work 
    }
    destroy_threadpool(p);
    close(sockfd);// proxy side
    free_filter();
    return 0;
}
void usage(){
    printf("Usage:  proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
}
/*
Checks the first line to request that 
there are 3 arguments and they are three 
in the desired format
*/
int check_first_line_request(char * request ){
    char tmp_request[strlen(request)];
    strcpy(tmp_request,request);
    char Get[4] = "GET\0";
    char http_0[9] ="HTTP/1.0\0";
    char http_1[9] ="HTTP/1.1\0";
    int check=0;
    //int get =0;
    //int http =0;
    const char s[2] = " ";
    char * tmp_get  = strtok(tmp_request,s);
    if(tmp_get==NULL){
        return -1;
    }
    char* path = strchr(request,' ');
    path++;
    int i=0;
    while(path[i]!=' '){
        i++;
    }
    char tmp_path [strlen(path)];
    strcpy(tmp_path,path);
    strtok(tmp_path,s);
    char * tmp_http = request + strlen(tmp_get) +strlen(tmp_path) + 2;
    i=0;
    while(tmp_http[i]!=' '){
        i++;
    }
    if(tmp_get == NULL || tmp_http == NULL || tmp_path ==NULL){// bed request
        check=2;
    }
    if(strcmp(http_0,tmp_http)!=0 && strcmp(http_1,tmp_http)!=0){//bed request
        return 2;;
    }
    if(strcmp(Get,tmp_get)!=0){// err =501
        return 1;
    }
    return check;
    
}
/*
The function reads from a file assuming it is
 otherwise returns nothing into a global 2D array
*/
int  read_from_filter(char* file_name){
    FILE* f = fopen(file_name,"r");
    if(f==NULL){
        printf("no such file \n");
        return -1;
    }
    filter = (char**)malloc(sizeof(char*));
    if(filter==NULL){
        perror("malloc\n");
        exit(1);
    }
    size_row=1;
    ssize_t nread  =0;
    size_t len = 0;
    int i=0;
    while(1){
        filter[i]=NULL;
        if((nread=getline(&filter[i],&len,f))!=-1){
            i++;
            size_row++;
            filter =(char**)realloc(filter,size_row*sizeof(char*));
            if(filter==NULL){
                perror("realloc\n");
                exit(1);
            } 

        }
        else{
            break;
        }
    }
    fclose(f);
    size_row=size_row-1;
    return 0;
}
/*
The function goes over the global dimensional
 array and checks whether the address 
 we found is within that array
*/
int check_if_host_in_filter(char* host){
    int check=0;
    int i;
    char tmp_host[strlen(host)+2];
    char t[strlen(host)+4];
    strcpy(tmp_host,host);
    strcpy(t,host);
    strcat(t,"\r\n");
    strcat(tmp_host,"\n");
    for(i=0;i<size_row;i++){
        if(i<size_row-1){
            if(strcmp(filter[i],tmp_host)==0){
                check=-1;
            }
        }
        else{
            if(strcmp(filter[i],t)==0){
                check=-1;
            }
        }
    }
    return  check;
}
/*
The function builds the error arrays so that we do 
not have to run from the error files when we run, 
we build error arrays 
so that if you encounter an error simply call 
from the array directly to the browser
The function is aided by the errors 
I made on the top of the program
*/
char * build_error_string(char* type_err , char *length,char* err){
    int size_err = strlen("HTTP/1.0");
    size_err+=strlen(type_err);
    size_err+=10*strlen("\r\n");
    size_err+=strlen("Server: webserver/1.0");
    size_err+=strlen("Content-Type: text/html");
    size_err+=strlen("Content-Length: ");
    size_err+=strlen(length);
    size_err+=strlen("Connection: close");
    size_err+=strlen("<HTML><HEAD><TITLE>");
    size_err+=strlen(type_err);
    size_err+=strlen("</H4>");
    size_err+=strlen("</BODY></HTML>");
    char* response_err =(char*)malloc(sizeof(char)*(size_err+250));
    if(response_err==NULL){
        perror("malloc\n");
        exit(1);
    }
    memset(response_err,'\0',size_err);
    strcat(response_err,"HTTP/1.0 ");
    strcat(response_err,type_err);
    strcat(response_err,"\r\n");
    strcat(response_err,"Server: webserver/1.0");
    strcat(response_err,"\r\n");
    strcat(response_err,"Content-Type: text/html");
    strcat(response_err,"\r\n");
    strcat(response_err,"Content-Length: ");
    strcat(response_err,length);
    strcat(response_err,"\r\n");
    strcat(response_err,"Connection: close");
    strcat(response_err,"\r\n");
    strcat(response_err,"\r\n");
    strcat(response_err,"<HTML><HEAD><TITLE>");
    strcat(response_err,type_err);
    strcat(response_err,"</TITLE></HEAD>");
    strcat(response_err,"\r\n");
    strcat(response_err,"<BODY><H4>");
    strcat(response_err,type_err);
    strcat(response_err,"</H4>");
    strcat(response_err,"\r\n");
    strcat(response_err,err);
    strcat(response_err,"\r\n");
    strcat(response_err,"</BODY></HTML>");
    strcat(response_err,"\r\n");
    return response_err;
}
/*
The function finds the address from the client's 
request and dynamically assigns it and returns it
*/
char* get_host(char* request){
    if(request==NULL){
        return NULL;
    }
    char* p = request;
    for(p+=5;p[0]==' ';p++);
    char tmp[strlen(request)];
    int j=0;
    for(int i=0;i<strlen(p);i++){
        if(p[i]=='\r'|| p[i]==':'){
            break;
        }
        else{
            tmp[i]=p[i];
            j++;
        }
    }
    char * host=(char*)calloc(j+1,sizeof(char));
    if(host==NULL){
        perror("celloc\n");
        exit(1);
    }
    memset(host,'\0',j+1);
    strncpy(host,tmp,j);
    return host;
}

/*
The function checks if a port other than 80
 appears if it is then it sets it otherwise 
 it puts a default 80 port
*/
int get_port(char* request ){
   char * p  = request;
    p=p+6;
    int port=0;
    char * tmp_port= strchr(p,':');
    if(tmp_port==NULL ){//user dont input port
        return 80;
    }
    else{
        char t[strlen(tmp_port)];
        tmp_port =tmp_port+1;
        int i=0;
        while(tmp_port[i]!='\r'){
            t[i]=tmp_port[i];
            i++;
        }

        port=atoi(t);
    }
    return port;
}

/*
The function finds the line in the request where
 the host line appears and returns this line until \ r
*/
char*  find_host(char* request){
    if(request==NULL){
        return NULL;
    }
    char tmp_host_1[5] ="Host:";
    char * tmp_host_2 = strstr(request,tmp_host_1);
    if(tmp_host_2==NULL){
        return NULL;
    }
    strtok(tmp_host_2,"\r");
    return tmp_host_2;
}

/*
The function rebuilds the request as it 
receives it from the customer so that the request is 
valid so that I was able to meet all the
 goals and profits I checked what was possible
*/
char* build_request(char* request){
    if(request==NULL){
        return NULL;
    }
    const char s[3] ="\r";
    char tmp[strlen(request)];
    strcpy(tmp,request);
    char tmp_2[strlen(request)];
    strcpy(tmp_2,request);
    
    char* firstLine = strtok(tmp,s);
    if(firstLine==NULL){
        return NULL;
    }
    char tmp_host_1[5] ="Host:";
    char * tmp_host_2 = strstr(tmp_2,tmp_host_1);
    if(tmp_host_2==NULL){  
        return NULL;
    }
    strtok(tmp_host_2,"\r");
    if(tmp_host_2==NULL){  
        return NULL;
    }
    int size_request = strlen(firstLine)+strlen(tmp_host_2)+4*strlen("\r\n")+strlen("Connection: close")+50;
    char* new_request = (char*)malloc(sizeof(char)*size_request);
    if(new_request==NULL){
        perror("malloc\n");
        exit(1);
    }
    memset(new_request,'\0',strlen(firstLine)+strlen(tmp_host_2)+4*strlen("\r\n")+strlen("Connection: close")+50);
    new_request[0]='\0';
    strncat(new_request,firstLine,strlen(firstLine));
    strncat(new_request,"\r\n",strlen("\r\n"));
    strncat(new_request,tmp_host_2,strlen(tmp_host_2));
    strncat(new_request,"\r\n",strlen("\r\n"));
    strncat(new_request,"Connection: close",strlen("Connection: close"));
    strncat(new_request,"\r\n",strlen("\r\n"));
    strncat(new_request,"\r\n",strlen("\r\n"));
    return new_request;
}

/*
first:
The function that basically every process gets and does is a few things:
Accepts the request from the client and dynamically assigns the request to any value
Then sends a request-building function that extracts the host and checks that the
 host is not in a file that does not allow access to certain addresses
Checks that the request contains arguments in the first row otherwise 
issues an error accordingly checks the host row that is correct and in 
addition checks if the host we received is otherwise correct it issues 
an error message accordingly
then:
After all, it connects to the server and makes the request when it is correct
 on any other case it will send the
 server the error message that will display to the
  client and show the client the answer of the server

*/

int  proxy_management(void * fd){
    
    int newsoketfd = *((int *)fd);
    int check;
    char* read_request = (char*)malloc(sizeof(char));
    if(read_request==NULL){
        perror("malloc\n");
        exit(1);
    }
    int i=0;
    int total_size=0;
    char buf[BUFLEN];
    //read request from client 
    while(1){
        memset(buf,'\0',BUFLEN);
        check = read(newsoketfd,buf,BUFLEN-1);
        total_size+= check;
        read_request= (char*)realloc(read_request,sizeof(char)*(total_size+1));
        if(read_request==NULL){
            perror("realloc\n");
            exit(1);
        }
        memset(read_request+i,'\0',check);
        strcat(read_request,buf);
        i=total_size;
        if(strstr(read_request,"\r\n\r\n")!=NULL){
            break;
        }
        if(check==0){
            break;
        }
    }
    char* respons_err =NULL;
    //get line with host
    char*  k = find_host(read_request);
    if(k==NULL){
        respons_err = build_error_string(A400,length_400,BODY_400);
        write(newsoketfd,respons_err,strlen(respons_err));
        free(read_request);
        free(respons_err);
        close(newsoketfd);
        return 0;
    }
    //get the host
    char * host=get_host(k);
    if(check_if_host_in_filter(host)==-1){// host on filter file 
        respons_err = build_error_string(A403,length_403,BODY_403);
        write(newsoketfd,respons_err,strlen(respons_err));

        close(newsoketfd);
        free(host);
        free(read_request);
        return 0;
    } 
    int port=get_port(k);
    char* request = build_request(read_request);
    // if request==null then have no request 
    if(request==NULL){
        respons_err = build_error_string(A400,length_400,BODY_400);
        write(newsoketfd,respons_err,strlen(respons_err));
        free(respons_err);
        free(host);
        free(request);
        free(read_request);
        close(newsoketfd);
        return 0;
    }
    struct sockaddr_in serv_addr; 
    struct hostent *server;
    int sockfd_thread = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd_thread==-1){
        perror("socket filed\n");
        free(host);
        free(request);
        free(read_request);
        return 1;
    }
    // for check the first line 
    char* tmp =build_request(read_request);
    char*  firstline  = strtok(tmp,"\r");
    if(check_first_line_request(firstline)==1){// have no GET on request
        respons_err = build_error_string(A501,length_501,BODY_501);
        write(newsoketfd,respons_err,strlen(respons_err));
        free(host);
        free(request);
        free(tmp);
        free(respons_err);
        free(read_request);
        close(sockfd_thread);
        close(newsoketfd);
        return 0;
    }
    else if(check_first_line_request(firstline)==2){// bed  request
        respons_err = build_error_string(A400,length_400,BODY_400);
        write(newsoketfd,respons_err,strlen(respons_err));
        close(sockfd_thread);
        close(newsoketfd);
        free(respons_err);
        free(host);
        free(request);
        free(tmp);
        free(read_request);
        return 0;
    }
    server = gethostbyname(host);// get ip for address on server side
    if (server == NULL) { // have no addreses
        fprintf(stderr,"ERROR, no such host\n");
        respons_err = build_error_string(A404,length_404,BODY_404);
        write(newsoketfd,respons_err,strlen(respons_err));
        free(host);
        free(request);
        free(tmp);
        free(respons_err);
        free(read_request);
        close(sockfd_thread);
        close(newsoketfd);
        //exit(0);
        return 0; 
    } 
    
    memset(&serv_addr,0,sizeof(serv_addr));   
    serv_addr.sin_family = AF_INET; //
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);
    int rc = connect(sockfd_thread, (const struct sockaddr*) &serv_addr, sizeof(serv_addr));
    if(rc<0){
        perror("error conencting\n");
        free(host);
        free(request);
        free(tmp);
        free(read_request);
        return 1;
    }
    rc = write(sockfd_thread,request,strlen(request));
        if(rc<0){
            perror("write  is faild");
            free(host);
            free(request);
            free(tmp);
            free(read_request);
            exit(1);
            return 1;
    }
    check=0;
    unsigned char buffer[18];
    // read and write to client i take 18 the small to give also  the
    // client the css in website he want 
    while(1){
        memset(buffer,'\0',18);
        check = read(sockfd_thread,buffer,18-1);
        if(check==0){
            break;
        }
        else if (check>0){
            buffer[check]='\0';
            write(newsoketfd,buffer,check);
        }
        else{
            perror("read \n");
            free(host);
            free(request);
            free(tmp);
            free(read_request);
            return 1;
        }
    }
    free(host);
    free(request);
    free(tmp);
    free(read_request);
    close(sockfd_thread);
    close(newsoketfd);
    return 0;
}
void free_filter(){

    for(int i=0;i<size_row+1;i++){
        free(filter[i]);
    }
    free(filter);
}
int check_argv_arg(char* arg){
    int fleg=0;
    for(int i = 0; i < strlen(arg); i++)
    {
        if(arg[i]>57 || arg[i]<48){
            fleg=-1;
        }
    }
    return fleg;
}