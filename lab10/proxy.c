/*
 * proxy.c - CS:APP Web proxy
 *
 * IMPORTANT: Include your name, student number and email here
 * JI TING, 5140379049, 1456908370@qq.com
 *
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */

#include "csapp.h"

/*
 * Function prototypes
 */
void* doit(int fd,struct sockaddr_in clientaddr);
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
int PResponse(int serverfd,int fd,char *filename);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
int connect_server(char *hostname,int port,char *path);
void WriteLog(char *logstring,char *clientaddr,char *uri,int clength);
FILE *logfile;
FILE *cachefile;
void *thread(void *arg);
int Open_clientfd_ts(char *hostname, int port);
char *cachedata;
static sem_t logmutex;
static sem_t opencfdmutex;
/*
 * main - Main routine for the proxy program
 */

struct args{
    int connfd;
    struct sockaddr_in clientaddr;
};

int main(int argc, char **argv){
    
    int listenfd,connfd,port,clientlen,i;
    struct sockaddr_in clientaddr;
    pthread_t tid;
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(1);
    }
    Signal(SIGPIPE, SIG_IGN);
    port=atoi(argv[1]);
    listenfd=Open_listenfd(port);
    
    /* Initialize mutex */
    Sem_init(&logmutex,0,1);
    Sem_init(&opencfdmutex,0,1);
    
    while(1){
        struct args *p=(struct args*)Malloc(sizeof(struct args));
        clientlen=sizeof(int);
        p->connfd=Accept(listenfd,(SA*)&(p->clientaddr),&clientlen);
        Pthread_create(&tid, NULL, thread, p);
    }
    exit(0);
}

void *thread(void *arg){
    Pthread_detach(Pthread_self());
    
    struct args* tmp = (struct args*)arg;
    int connfd =  tmp->connfd;
    struct sockaddr_in clientaddr = tmp->clientaddr;
    free(tmp);
    
    return doit(connfd,clientaddr);
}

void* doit(int fd,struct sockaddr_in clientaddr){
    char buf[MAXLINE],method[10],uri[100],version[20];
    char filename[20],hostname[50],logstring[100];
    char path[50];
    int port,serverfd,clength=0,n;
    rio_t rio;
    sem_t mutex;
    int cachefd;
    path[0]='\0';
    
    /* Read request line and headers */
    Rio_readinitb(&rio,fd);
    Rio_readlineb(&rio,buf,MAXLINE);
    sscanf(buf,"%s %s %s\n",method,uri,version);
    
    /* Parse URI from GET request */
    parse_uri(uri,hostname,filename,&port);
    strcat(path,"./cache/");
    strcat(path,filename);
    
    /* If have cached */
    if((cachefile=fopen(path,"rb"))){
        while(n=Fread(buf,sizeof(char),MAXLINE,cachefile)){
            if(strstr(buf, "Content-length:"))
                sscanf(buf, "Content-length: %d\r\n", &clength);
            Rio_writen(fd,buf,n);
        }
        
        /* Write to log */
        WriteLog(logstring,&clientaddr,uri,clength);
        
        return NULL;
    }
    
    /* Connect to server */
    if((serverfd=connect_server(hostname,port,filename))<0)
        return NULL;
    
    clength=PResponse(serverfd,fd,filename);
    
    /* Write to log */
    WriteLog(logstring,&clientaddr,uri,clength);
    return NULL;
}

/* Use lock to protect logfile */
void WriteLog(char *logstring,char *clientaddr,char *uri,int clength){
    P(&logmutex);
    logfile=Fopen("./proxy.log","a");
    format_log_entry(logstring,&clientaddr,uri,clength);
    fprintf(logfile,logstring);
    fflush(logfile);
    Fclose(logfile);
    V(&logmutex);
}

/* Proxy get response from server and send to client */
int PResponse(int serverfd,int fd,char *filename){
    
    rio_t rio;
    char buf[MAXLINE];
    int n,clength=0;
    char path[50];
    path[0]='\0';
    
    strcat(path,"./cache/");
    strcat(path,filename);
    cachefile=Fopen(path,"wb");
    Rio_readinitb(&rio, serverfd);
    
    while(strcmp(buf,"\r\n")){
        if(strstr(buf, "Content-length:"))
            sscanf(buf, "Content-length: %d\r\n", &clength);
        n=Rio_readlineb(&rio, buf, MAXLINE);
        
        /* Write to cache */
        Fwrite(buf,sizeof(char),n,cachefile);
        Rio_writen(fd, buf, n);
    }
    
    while((n=Rio_readlineb(&rio,buf,MAXLINE))){
        Fwrite(buf,sizeof(char),n,cachefile);
        Rio_writen(fd, buf, n);
    }
    Fclose(cachefile);
    return clength;
}

/* Connect to server */
int connect_server(char *hostname,int port,char *path){
    char buf[MAXLINE];
    int clientfd;
    clientfd=Open_clientfd_ts(hostname,port);
    
    sprintf(buf,"GET /%s HTTP/1.1\r\n",path);
    sprintf(buf,"%sHost: %s\r\n\r\n",buf,hostname);
    Rio_writen(clientfd,buf,strlen(buf));
    return clientfd;
}


void clienterror(int fd,char *cause,char* errnum,char *shortmsg,char *longmsg){
    char buf[MAXLINE],body[MAXBUF];
    
    /* Build the HTTP response body */
    sprintf(body,"<html><title>Tiny Error</title>");
    sprintf(body,"%s<body bgcolor=""ffffff"">\r\n",body);
    sprintf(body,"%s%s: %s\r\n",body,errnum,shortmsg);
    sprintf(body,"%s<p>%s: %s\r\n",body,longmsg,cause);
    sprintf(body,"%s<hr><em>The Tiny Web server</em>\r\n",body);
    
    /* Print the HTTP response */
    sprintf(buf,"HTTP/1.0 %s %s\r\n",errnum,shortmsg);
    Rio_writen(fd,buf,strlen(buf));
    sprintf(buf,"Content-type: text/html\r\n");
    Rio_writen(fd,buf,strlen(buf));
    sprintf(buf,"Content-length: %d\r\n\r\n",(int)strlen(body));
    Rio_writen(fd,buf,strlen(buf));
    Rio_writen(fd,body,strlen(body));
}


int open_clientfd_ts(char *hostname, int port)
{
    int clientfd;
    sem_t mutex;
    struct hostent *hp=Malloc(sizeof(struct hostent));
    struct hostent *hq=Malloc(sizeof(struct hostent));
    struct sockaddr_in serveraddr;
    //printf("entered open_clientfd_ts!\n\n");
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1; /* check errno for cause of error */
    
    /* Fill in the server's IP address and port */
    P(&opencfdmutex);
    hq=gethostbyname(hostname);
    *hp=*hq;
    V(&opencfdmutex);
    if (hp == NULL)
        return -2; /* check h_errno for cause of error */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0],
          (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);
    
    /* Establish a connection with the server */
    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    return clientfd;
}

int Open_clientfd_ts(char *hostname, int port)
{
    int rc;
    
    if ((rc = open_clientfd_ts(hostname, port)) < 0) {
        if (rc == -1)
            unix_error("Open_clientfd_ts Unix error");
        else
            dns_error("Open_clientfd_ts DNS error");
    }
    return rc;
}


/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;
    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }
    
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')
        *port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        pathname[0] = '\0';
    }
    else {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }
    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;
    
    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));
    
    /*
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;
    
    
    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", time_str, a, b, c, d, uri,size);
}


