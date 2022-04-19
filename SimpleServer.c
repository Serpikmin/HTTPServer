#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

const int QUEUE_LENGTH= 32; // number of clients the socket can handle
const char * MONTH_LIST[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

//--------------- HELPER FUNCTIONS ---------------------------------------------

/*
* HELPER FUNCTION: sockaddr_in
* Initialize a server address associated with the given port.
*/
struct sockaddr_in *init_server_addr(int port) {
  struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));

  // Allow sockets across machines.
  addr->sin_family = PF_INET;
  // The port the process will listen on.
  addr->sin_port = htons(port);
  // Clear this field; sin_zero is used for padding for the struct.
  memset(&(addr->sin_zero), 0, 8);

  // Listen on all network interfaces.
  addr->sin_addr.s_addr = INADDR_ANY;

  return addr;
}

/*
* HELPER FUNCTION: setup_server_socket
* Create and setup a socket for a server to listen on.
*/
int setup_server_socket(struct sockaddr_in *self, int num_queue) {
  int soc = socket(PF_INET, SOCK_STREAM, 0);
  if (soc < 0) {
    perror("socket");
    exit(1);
  }

  // Make sure we can reuse the port immediately after the
  // server terminates. Avoids the "address in use" error
  int on = 1;
  int status = setsockopt(soc, SOL_SOCKET, SO_REUSEADDR,
    (const char *) &on, sizeof(on));
    if (status < 0) {
      perror("setsockopt");
      exit(1);
    }

    // Associate the process with the address and a port
    if (bind(soc, (struct sockaddr *)self, sizeof(*self)) < 0) {
      // bind failed; could be because port is in use.
      perror("bind");
      exit(1);
    }

    // Set up a queue in the kernel to hold pending connections.
    if (listen(soc, num_queue) < 0) {
      // listen failed
      perror("listen");
      exit(1);
    }

    return soc;
  }

  /*
  * HELPER FUNCTION: generate_etag
  *
  */
  const char* generate_etag(char* filename, char* etag, int offset){
    memset(etag, 0, 100);
    char ch;

    for (size_t i = 0; i < strlen(filename); i++) {
      ch = filename[i];
      if (ch >= 'a' && ch <= 'z'){
        if ((ch+offset) > 'z'){
          ch = ch - (26-offset);
        }
        else{
          ch = ch + offset;
        }
      }
      if (ch >= 'A' && ch <= 'Z'){
        if ((ch+offset) > 'Z'){
          ch = ch - (26-offset);
        }
        else{
          ch = ch + offset;
        }
      }

      etag[i] = ch;
    }
    return 0;
  }



  //---------------------- MAIN FUNCTION ----------------------------------------
  /*
  * Main Function
  */

  int main(int argc, char *argv[]) {

    // Deals with no arguements/incorrect # of args here
    printf("Arguments passed: %d\n", argc-1 );
    if (argc != 3){
      printf("Insufficient # of arguements passed.\n");
      return -1;
    }

    const int PORT = atoi(argv[1]); // set PORT to the second argument passed in


    char *http_root_path = argv[2]; // extract the http root path which is the third argument passed in
    printf("http root path: %s\n", http_root_path);

    // set up server socket
    struct sockaddr_in *address = init_server_addr(PORT);
    int addrlen = sizeof(address);
    int server_fd = setup_server_socket(address, QUEUE_LENGTH); // socket gets binded and starts listening


    int new_socket; // client's socket/file descriptor when they connect
    long valread; // will hold the amount of bytes read in from client socket.

    // need while(1) loop so that server is constantly running and is listening
    while(1) {

      printf("\n+++++++ Waiting for new connection ++++++++\n\n");
      if ((new_socket = accept(server_fd, (struct sockaddr *)address, (socklen_t*)&addrlen))<0) // listen for incoming clients wanting to connect to server
      {
        perror("accept");
        exit(EXIT_FAILURE);
      }

      // incoming client been successfully connected to server at this point.

      // Prepare and memset required variables
      char dest[500000]; // contains file content
      char http_response_header[600000];
      FILE *fp;
      char html_path_decoded[100]; // this will contain the cleaned requested path.
      char cleaned_path[100];
      memset(dest, '\0', strlen(dest));
      memset(http_response_header, '\0', strlen(http_response_header));
      memset(html_path_decoded, '\0', strlen(html_path_decoded));
      memset(cleaned_path, '\0', strlen(cleaned_path));

      // Read client's requests
      char buffer[30000] = {0}; // this buffer will hold client's requests
      char client_request_cpy[30000]; // copy of client's request so we can do different operations with it later
      valread = read( new_socket , buffer, 30000); // read in any messages sent from client
      printf("client request: %s\n", buffer );

      strncpy(client_request_cpy, buffer, strlen(buffer));

      if (strstr(client_request_cpy, "\r\n")== NULL){
        printf("Carriage Return Failure\n");
        char* response = "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\n";
        write(new_socket, response, strlen(response));
        close(new_socket);
        printf("Error with client syntax, 400 Response sent\n");
        continue;
      }




      // Extract name of the requested file from the GET request
      const char s[2] = " "; // splitting on white spaces
      char *requested_http_path;
      requested_http_path = strtok(buffer, s);
      if (strcmp(requested_http_path, "GET") != 0)
      {
        requested_http_path = "/null";
        printf("Request not understood, proceeding with dummy string\n");
        close(new_socket);
        continue;
      }
      else
      {
        int i;
        for (i = 0; i < 1; i++)
        {
          requested_http_path= strtok(NULL,s);
        }
      }
      if (requested_http_path == NULL)
      {
        requested_http_path = "/null";
        printf("Request not understood, proceeding with dummy string\n");
        close(new_socket);
        continue;
      }

      printf("Requested http path from client: %s\n", requested_http_path);

      // getting rid of the first / in requested http path name
      char requested_http_path_sliced[strlen(requested_http_path)-1];
      strncpy(requested_http_path_sliced, &requested_http_path[1], strlen(requested_http_path) - 1);
      requested_http_path_sliced[strlen(requested_http_path) - 1] = '\0';

      //----- Handles the case in which client is requesting a file with spaces in it
      // Handles HTML encoding %20
      char *token;
      token = strtok(requested_http_path_sliced, "%");
      strncat(cleaned_path, token, strlen(token) + 1);
      while (token != NULL) {
        token = strtok(NULL, "%"); // get whatever comes before the %
        char space[102] = " ";
        if (token != NULL) { // token must not be NULL if we want to concatenate it to another string
          strncat(space, token, strlen(token) + 1); // append whatever came before the % to the space
          strncat(space, "\0", strlen("\0"));
          strncat(cleaned_path, space, strlen(token) +1);
        }
      }

      // Deals with token after %
      char *token_spaces;
      token_spaces = strtok(cleaned_path, " ");
      strncat(html_path_decoded, token_spaces, strlen(token_spaces) +1);
      while (token_spaces != NULL) {
        token_spaces = strtok(NULL, " ");
        char space_dummy[102] = " ";
        if (token_spaces != NULL) {
          char *sliced_token = token_spaces + 2; // to get rid of the '20' in ''%20' move the pointer over by 2
          strncat(space_dummy, sliced_token, strlen(sliced_token) + 1);
          strncat(html_path_decoded, space_dummy,strlen(space_dummy)+1);
        }
      }


      // Add folder to opening path for requests
      // Concatenate the decoded html path with the http root path
      char final_path[strlen(html_path_decoded) + strlen(http_root_path) + 2];
      strcpy(final_path, http_root_path);
      strncat(final_path, html_path_decoded, strlen(html_path_decoded) + 1);

      // find and open the requested path/file in the directory
      if (strncmp(html_path_decoded,"favicon.ico", 11) != 0) {
        if ((fp=fopen(final_path,"rb")) == NULL) { // opening file
          // TODO: Send back 404 response to the client when the file is not found
          if (strcmp(requested_http_path, "/null") != 0)
          {
            char* response = "HTTP/1.0 404 Not Found\r\nConnection: close\r\n\r\n";
            write(new_socket, response, strlen(response));
            close(new_socket);
            printf("Error opening file, 404 Response sent\n");
            continue; // go back up to the top of the loop so we can begin listening for a new client request
          }
          else {
            char* response = "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\n";
            write(new_socket, response, strlen(response));
            close(new_socket);
            printf("Error with client syntax, 400 Response sent\n");
            continue;
          }
          memset(html_path_decoded, 0, strlen(html_path_decoded));
          memset(cleaned_path, 0, strlen(cleaned_path)+1);
        }
        printf("File opened successfully.\n");

        memset(dest, 0, 500000); // i had to do this because dest wasnt empty on declaration for some reason

        //Error check reading the file
        //Send response if fails - 500 error

        if (fread(&dest, sizeof(dest), 1, fp) < 0){
          char* read_response_err = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
          write(new_socket, read_response_err, strlen(read_response_err));
          close(new_socket);

          perror("Unable to read file");
          exit(EXIT_FAILURE);
        }

        fclose(fp);
        memset(fp, '\0', sizeof(fp));

        // status
        char *status_header = "HTTP/1.0 200 OK\r\n";

        // content-length
        char content_length[30];
        char content_length_header[30] = "Content-Length: ";
        sprintf(content_length, "%d", strlen(dest));
        strncat(content_length_header, content_length, strlen(content_length) + 1);
        strncat(content_length_header, "\r\n", strlen("\r\n"));

        // connection <- Remember to change on persistantserver.c
        char *connection_header = "Connection: close\r\n";

        // Content-Type
        char content_type_header[40] = "Content-Type: ";
        char *content_type;
        char *file_type;
        const char dot_ch = '.';
        file_type = strrchr(html_path_decoded, dot_ch);
        char file_type_txt[] = ".txt";
        char file_type_html[] = ".html";
        char file_type_js[] = ".js";
        char file_type_css[] = ".css";
        char file_type_jpg[] = ".jpg";
        char file_type_png[] = ".png";
        char file_type_jpeg[] = ".jpeg";
        if (strcmp(file_type, file_type_txt) == 0) {
          content_type = "text/plain";
        }
        else if (strcmp(file_type, file_type_html) == 0) {
          content_type = "text/html";
        }
        else if (strcmp(file_type, file_type_js) == 0) {
          content_type = "application/javascript";
        }
        else if (strcmp(file_type, file_type_css) == 0) {
          content_type = "text/css";
        }
        else if (strcmp(file_type, file_type_jpg) == 0) {
          content_type = "image/jpg";
        }
        else if (strcmp(file_type, file_type_png) == 0) {
          content_type = "image/png";
        }
        else if (strcmp(file_type, file_type_jpeg) == 0) {
          content_type = "image/jpeg";
        }
        else {
          //TODO: Would we bring an error here? Would we accept other things?
          //Leave it for the time being?
          char *accept_ret;
          accept_ret = strstr(client_request_cpy, "Accept");
          content_type = strtok(accept_ret, ",") + 8;
        }

        strncat(content_type_header, content_type, strlen(content_type) + 1);
        strncat(content_type_header, "\r\n", strlen("\r\n"));

        // TODO: Last-Modified
        char last_modified[30];
        char last_modified_header[60] = "Last-Modified: ";
        struct stat attr;
        stat(final_path, &attr);
        strcpy(last_modified, ctime(&attr.st_mtime));
        strncat(last_modified_header, last_modified, strlen(last_modified));
        strncat(last_modified_header, "\r\n", strlen("\r\n"));

        //If-Match Header
        /*
        The system our group has chosen for etags is a simple Caesar cipher of
        the file name. We figured that path + filename must be unique for every
        request, therefore, we are using that to create unique etags for every
        file requests from the client.
        */
        char* ifmatchresult = strstr(client_request_cpy, "If-Match");

        if(ifmatchresult != NULL){
          printf("------------------If-Match Header Exists------------------\n");
          char etag[100] = {0};
          int offset = 1;
          generate_etag(final_path, etag, offset);
          printf("ETAG: %s\n", etag);

          //The case where the etag does not match expected
          if(strstr(ifmatchresult, etag) == NULL){
            //TODO: Look into adding more to the response?
            char rns[36] = "HTTP/1.0 416 Range Not Satisfiable\n\0";
            write(new_socket, rns, 36);
            close(new_socket);
            continue;
            //TODO: goes through rest of server, consider making it not do that.
            // For sanity checking on the server print statements.
          }

        }

        //If-None-Match Header
        /*
        If-None-Match operates similarly to If-Match with in regards to etags.
        */
        char* ifnonematchresult = strstr(client_request_cpy, "If-None-Match");

        if(ifnonematchresult != NULL){
          printf("------------------If-None-Match Header Exists------------------\n");
          char etag[100] = {0};
          int offset = 1;
          generate_etag(final_path, etag, offset);
          printf("ETAG: %s\n", etag);

          //The case where the etag does not match expected
          if(strstr(ifnonematchresult, etag) != NULL){
            //TODO: Look into adding more to the response?
            char rns[27] = "HTTP/1.0 304 Not Modified\n\0";
            write(new_socket, rns, 27);
            close(new_socket);
            continue;
          }

        }


        // If-Modified-Since
        char* ifmodifiedsinceresult = strstr(client_request_cpy, "If-Modified-Since");

        if(ifmodifiedsinceresult != NULL){
          printf("------------------If-Modified-Since Header Exists------------------\n");

          struct tm tm;
          memset(&tm, 0, sizeof(tm));
          char* token = strtok(ifmodifiedsinceresult, " ");  // Requst
          char day[2];
          char month[2];
          int monthDigit;
          char year[4];
          char time[8];
          char timestamp[20];
          time_t epoch;

          token = strtok(NULL, " ");  // Weekday
          token = strtok(NULL, " ");  // Day
          strcpy(day, token);
          token = strtok(NULL, " ");  // Month
          for (int i = 0; i < 12; i++){
            if (strcmp(MONTH_LIST[i], token) == 0){
              monthDigit = i + 1;
            }
          }

          if (monthDigit < 10){
            snprintf(month, 3, "0%d", monthDigit);
          }
          else{
            snprintf(month, 3, "%d", monthDigit);
          }
          token = strtok(NULL, " ");  // Year
          strcpy(year, token);
          token = strtok(NULL, " ");  // Time
          strcpy(time, token);
          snprintf(timestamp, 20, "%.4s-%.2s-%.2s %.8s", year, month, day, time);

          if (strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm) != NULL)
          {
            epoch = mktime(&tm);
          }

          if (epoch > attr.st_mtime) // TODO: Fix properly
          {
            char rns[27] = "HTTP/1.0 304 Not Modified\n\0";
            write(new_socket, rns, 27);
            close(new_socket);
            continue;

          }
        }



        //TODO: If-Unmodified-Since
        char* ifunmodifiedsinceresult = strstr(client_request_cpy, "If-Unmodified-Since");

        if(ifunmodifiedsinceresult != NULL){
          printf("------------------If-Unmodified-Since Header Exists------------------\n");

          struct tm tm;
          memset(&tm, 0, sizeof(tm));
          char* token = strtok(ifunmodifiedsinceresult, " ");  // Requst
          char day[2];
          char month[2];
          int monthDigit;
          char year[4];
          char time[8];
          char timestamp[20];
          time_t epoch;

          token = strtok(NULL, " ");  // Weekday
          token = strtok(NULL, " ");  // Day
          strcpy(day, token);
          token = strtok(NULL, " ");  // Month
          for (int i = 0; i < 12; i++){
            if (strcmp(MONTH_LIST[i], token) == 0){
              monthDigit = i + 1;
            }
          }

          if (monthDigit < 10){
            snprintf(month, 3, "0%d", monthDigit);
          }
          else{
            snprintf(month, 3, "%d", monthDigit);
          }
          token = strtok(NULL, " ");  // Year
          strcpy(year, token);
          token = strtok(NULL, " ");  // Time
          strcpy(time, token);
          snprintf(timestamp, 20, "%.4s-%.2s-%.2s %.8s", year, month, day, time);

          if (strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm) != NULL)
          {
            epoch = mktime(&tm);
          }

          if (epoch < attr.st_mtime) // TODO: Fix properly
          {
            char rns[34] = "HTTP/1.0 412 Precondition Failed\n\0";
            write(new_socket, rns, 34);
            close(new_socket);
            continue;

          }
        }

        // concatenate all headers to http_response_header
        strncat(http_response_header, status_header, strlen(status_header) + 1);
        strncat(http_response_header, connection_header, strlen(connection_header) + 1);
        strncat(http_response_header, content_length_header, strlen(content_length_header)+1);
        strncat(http_response_header, content_type_header, strlen(content_type_header)+1);
        strncat(http_response_header, last_modified_header, strlen(last_modified_header)+1);
        strncat(http_response_header, "\r\n", strlen("\r\n") );
        printf("http_response_header w/o data:\n%s", http_response_header);

        // appending data to header
        memcpy(dest + strlen(dest), "\r\n", strlen("\r\n"));
        memcpy(http_response_header + strlen(http_response_header), dest, strlen(dest));

        // send this to the client
        write(new_socket, http_response_header, strlen(http_response_header));

        printf("------------------Message sent to client-------------------\n");
      } 
        close(new_socket);
    }
      return 0;
  }
