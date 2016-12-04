/*
 * tinychat.c - [Starting code for] a web-based chat server.
 *
 * Based on:
 *  tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *      GET method to serve static and dynamic content.
 *   Tiny Web server
 *   Dave O'Hallaron
 *   Carnegie Mellon University
 */
#include "csapp.h"
#include "dictionary.h"
#include "more_string.h"

void doit(int fd);
dictionary_t *read_requesthdrs(rio_t *rp);
void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *d);
void parse_query(const char *uri, dictionary_t *d);
void serve_form(int fd, const char *pre_cont_header, const char *conversation, const char *name, const char *topic);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
static void print_stringdictionary(dictionary_t *d);

dictionary_t *conversations; //define the conversations dictionary

int main(int argc, char **argv) 
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  
  conversations = make_dictionary(COMPARE_CASE_SENS, free);

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  /* Don't kill the server if there's an error, because
     we want to survive errors due to a client. But we
     do want to report errors. */
  exit_on_error(0);

  /* Also, don't stop on broken connections: */
  Signal(SIGPIPE, SIG_IGN);

  while (1) {
  //TODO: make threads here. Lock global dictionaries.
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    if (connfd >= 0) {
      Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                  port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);
      doit(connfd);
      Close(connfd);
    }
  }
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd) 
{
  char buf[MAXLINE], *method, *uri, *version;
  rio_t rio;
  dictionary_t *headers, *query;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (Rio_readlineb(&rio, buf, MAXLINE) <= 0)
    return;
  printf("%s", buf);
  
  if (!parse_request_line(buf, &method, &uri, &version)) {
    clienterror(fd, method, "400", "Bad Request",
                "TinyChat did not recognize the request");
  } else {
    if (strcasecmp(version, "HTTP/1.0")
        && strcasecmp(version, "HTTP/1.1")) {
      clienterror(fd, version, "501", "Not Implemented",
                  "TinyChat does not implement that version");
    } else if (strcasecmp(method, "GET")
               && strcasecmp(method, "POST")) {
      clienterror(fd, method, "501", "Not Implemented",
                  "TinyChat does not implement that method");
    } else {
      headers = read_requesthdrs(&rio);

      /* Parse all query arguments into a dictionary */
      query = make_dictionary(COMPARE_CASE_SENS, free);
      parse_uriquery(uri, query);

      
      if (!strcasecmp(method, "POST")) {
        read_postquery(&rio, headers, query);

		  /* For debugging, print the dictionary */
		  print_stringdictionary(query);
		  
		  //check if it's coming from the first entry screen or not
		  const char *entry = dictionary_get(query, "entry");
		  const char *name = dictionary_get(query, "name");
		  const char *topic = dictionary_get(query, "topic");
		  const char *chat_header = append_strings("Tinychat - ", topic, NULL);
		  if (entry != NULL) { /* this is the entry screen if entry exists*/
			  if (NULL == dictionary_get(conversations, topic)) { /*if conversation doesn't exist, add to dictionary*/
			  	  char *init = append_strings("", NULL);
				  dictionary_set(conversations, topic, init);
			  }
			  else { /*if it already exists, we need to simply join that conversation...somehow*/
			  	char *conv = dictionary_get(conversations, topic);
			  	serve_form(fd, chat_header, conv, name, topic);
			  }

			  //Print conversations dictionary for debugging
			  print_stringdictionary(conversations);
			  
			  const char *conversation = dictionary_get(conversations, topic);
			  serve_form(fd, chat_header, conversation, name, topic);	
		  }
		  else {
		  	const char *new_message = dictionary_get(query, "message");
		  	const char *conv = dictionary_get(conversations, topic);
		  	if (strcmp(new_message, "") == 0) {
		  		serve_form(fd, chat_header, conv, name, topic);
		  	}
		  	else {
			  	const char *new_conv = append_strings(conv, "\r\n", name, ": ", new_message, NULL);
			  	dictionary_set(conversations, topic, (void *) new_conv);
			  	serve_form(fd, chat_header, new_conv, name, topic);
		  	}
		  }
		  print_stringdictionary(conversations);
      }
      
      /* The start code sends back a text-field form: */
      else if (!strcasecmp(method, "GET")) {
		
      	if (starts_with("/conversation", uri)) {
      		parse_uriquery(uri, query);
      		const char *topic = dictionary_get(query, "topic");
      		const char *conv = dictionary_get(conversations, topic);
      		if (conv == NULL) {
      			char *init = append_strings("", NULL);
      			dictionary_set(conversations, topic, init);
      		}
      		else {
		  		printf(conv);
		  		serve_form(fd, "", conv, "", topic);
      		}      		
      	}
      	else if (starts_with("/say", uri)) {
      		parse_uriquery(uri, query);
      		const char *topic = dictionary_get(query, "topic");
      		const char *user = dictionary_get(query, "user");
      		const char *content = dictionary_get(query, "content");
      		const char *conv = dictionary_get(conversations, topic);
      		const char *new_conv = append_strings(conv, "\r\n", user, ": ", content, NULL);
      		dictionary_set(conversations, topic, (void *) new_conv);
      		serve_form(fd, "", "", "", "");
      	}
      	else if (starts_with("/import", uri)) {
      	
      	}
     	else {
     		serve_form(fd, "Welcome to TinyChat", NULL, NULL, NULL);
     	}
      }

      /* Clean up */
      free_dictionary(query);
      free_dictionary(headers);
    }

    /* Clean up status line */
    free(method);
    free(uri);
    free(version);
  }
}

/*
 * read_requesthdrs - read HTTP request headers
 */
dictionary_t *read_requesthdrs(rio_t *rp) 
{
  char buf[MAXLINE];
  dictionary_t *d = make_dictionary(COMPARE_CASE_INSENS, free);

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    parse_header_line(buf, d);
  }
  
  return d;
}

void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *dest)
{
  char *len_str, *type, *buffer;
  int len;
  
  len_str = dictionary_get(headers, "Content-Length");
  len = (len_str ? atoi(len_str) : 0);

  type = dictionary_get(headers, "Content-Type");
  
  
  buffer = malloc(len+1);
  Rio_readnb(rp, buffer, len);
  buffer[len] = 0;

  if (!strcasecmp(type, "application/x-www-form-urlencoded")) {
    parse_query(buffer, dest);
  }

  free(buffer);
}

static char *ok_header(size_t len, const char *content_type) {
  char *len_str, *header;
  
  header = append_strings("HTTP/1.0 200 OK\r\n",
                          "Server: TinyChat Web Server\r\n",
                          "Connection: close\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n",
                          "Content-type: ", content_type, "\r\n\r\n",
                          NULL);
  free(len_str);

  return header;
}

/*
 * serve_form - sends a form to a client
 */
void serve_form(int fd, const char *pre_cont_header, const char *conversation, const char *name, const char *topic)
{
  size_t len;
  char *body, *header;
  //if this is for the automated request
  if (!strcmp(pre_cont_header, "")) {
  	body = append_strings(conversation, "\r\n", NULL);
  }
  //if it's the first screen, serve the join form.
  else if (strcmp(pre_cont_header, "Welcome to TinyChat") == 0) {
  	body = append_strings("<html><body>\r\n",
                        "<p>",
                        pre_cont_header,
                        "</p>",
                        "\r\n<form action=\"reply\" method=\"post\"",
                        " enctype=\"application/x-www-form-urlencoded\"",
                        " accept-charset=\"UTF-8\">\r\n",
                        "Name: <input type=\"text\" name=\"name\"><br>\r\n",
                        "Topic: <input type=\"text\" name=\"topic\"><br>\r\n",
                        "<input type=\"hidden\" name=\"entry\" value=\"yes\">\r\n",
                        "<input type=\"submit\" value=\"Join Conversation\">\r\n",
                        "</form></body></html>\r\n",
                        NULL);
  	
  }
  //else serve the conversation form.
  else {
  	body = append_strings("<html><body>\r\n",
                        "<p>",
                        pre_cont_header,
                        "</p>",
                        "\r\n<form action=\"reply\" method=\"post\"",
                        " enctype=\"application/x-www-form-urlencoded\"",
                        " accept-charset=\"UTF-8\">\r\n",
                        conversation,
                        "<br>\r\n",
                        name,
                        ":",
                        "<input type=\"hidden\" name=\"name\" value=",
                        name,
                        ">\r\n",
                        "<input type=\"hidden\" name=\"topic\" value=",
                       	topic,
                        ">\r\n",          
                        "<input type=\"text\" name=\"message\"><br>\r\n",
                        "<input type=\"submit\" value=\"Add Message\">\r\n",
                        "</form></body></html>\r\n",
                        NULL);
  }
  
  len = strlen(body);

  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  free(body);
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
  size_t len;
  char *header, *body, *len_str;

  body = append_strings("<html><title>Tiny Error</title>",
                        "<body bgcolor=""ffffff"">\r\n",
                        errnum, " ", shortmsg,
                        "<p>", longmsg, ": ", cause,
                        "<hr><em>The Tiny Web server</em>\r\n",
                        NULL);
  len = strlen(body);

  /* Print the HTTP response */
  header = append_strings("HTTP/1.0 ", errnum, " ", shortmsg,
                          "Content-type: text/html; charset=utf-8\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n\r\n",
                          NULL);
  free(len_str);
  
  Rio_writen(fd, header, strlen(header));
  Rio_writen(fd, body, len);

  free(header);
  free(body);
}

static void print_stringdictionary(dictionary_t *d)
{
  int i, count;

  count = dictionary_count(d);
  for (i = 0; i < count; i++) {
    printf("%s=%s\n",
           dictionary_key(d, i),
           (const char *)dictionary_value(d, i));
  }
  printf("\n");
}
