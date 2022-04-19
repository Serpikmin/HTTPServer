#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/select.h>

// ----------------------- GLOBAL VARIABLES ---------------------------------
char * http_root_path;
int PORT;
struct sockaddr_in *address;
int addrlen;
int server_fd;


const int QUEUE_LENGTH= 32; // number of clients we will support
const char * MONTH_LIST[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char * WEEKDAY_LIST[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};



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

  void * server_func(void *);

  /*
  * HELPER FUNCTION: server_func
  *
  */
  void * server_func(void * client_socket) {
    int new_socket = client_socket; // file descriptor/ socket of a client that connected
    long valread; // amount of bytes read from client's message
    int connection_flag = 0; // 0 if no new TCP connection is open, 1 if there is a TCP connection currently open
    while(1){
      if (connection_flag == 0) { // if connection_flag = 0, this thread has no actve TCP conn. So go ahead and listen for any incoming connections
        printf("\n+++++++ Waiting for new connection ++++++++\n\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *)address, (socklen_t*)&addrlen))<0) // listening for a new connection
        {
          perror("In accept");
          exit(EXIT_FAILURE);
        }
        connection_flag = 1; // TCP connection has been established with a client on this thread, set this to 1 so that we don't open a new TCP connection
      }
      printf("client socket value: %d\n", new_socket);


      char buffer[30000] = {0}; // this buffer will hold client's requests
      char client_request_cpy[30000]; // copy of client's request

      // using select() to set timeout
      fd_set set;
      struct timeval timeout;
      int rv;

      FD_ZERO(&set);
      FD_SET(new_socket, &set);

      timeout.tv_sec = 0;
      timeout.tv_usec = 10000000; // this is in microseconds. Reference: 1,000,000 microseconds = 1 second. so this is 10 seconds
      rv = select(new_socket + 1, &set, NULL, NULL, &timeout);
      if (rv == -1) {
        perror("Error with select");
      }
      else if (rv == 0) { // it means timeout occured, which means read has blocked for exactly 10 seconds
        printf("No new request recieved from socket %d\n", new_socket);
        close(new_socket); // close the socket because no new request has been recieved from the client
        printf("Closed socket %d\n", new_socket);
        connection_flag = 0; // since the socket/TCP connection on this thread has been closed, set this flag to 0.
        continue; // go back up to the top of the while loop so that we can listen for a new connection on this thread
      }
      else { // timeout has not occured, because client is writing to the server socket
        printf("Reading client request...\n");
        valread = read(new_socket, buffer, 30000); // read in client request
      }

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
      //char buffer[30000] = {0}; // this buffer will hold client's requests
      //char client_request_cpy[30000];
      //printf("blocking on read\n");
      //valread = read( new_socket , buffer, 30000);
      printf("client request: %s\n", buffer );
      printf("amt read: %lu\n", valread);
      if (valread == 0) {

          close(new_socket);
          connection_flag = 0; // Because the socket has been closed, there is no active TCP connection on this thread. set to 0.
          printf("Closed socket %d\n", new_socket);
          continue;
      }

      strncpy(client_request_cpy, buffer, strlen(buffer));

      if (strstr(client_request_cpy, "\r\n")== NULL){
        printf("Carriage Return Failure\n");
        char* response = "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\n";
        write(new_socket, response, strlen(response));
        close(new_socket);
        printf("Error with client syntax, 400 Response sent\n");
        connection_flag = 0;
        continue;
      }



      // Extract name of the requested file from the GET request
      const char s[2] = " "; // splitting on white spaces
      char *requested_http_path;
      char *requested_ptr = NULL;
      requested_http_path = strtok_r(buffer, s, &requested_ptr);
      if (strncmp(requested_http_path, "GET", 3) != 0)
      {
        requested_http_path = "/null";
        printf("Request not understood, proceeding with dummy string\n");
        char* response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
        write(new_socket, response, strlen(response));
        close(new_socket);
        printf("Error opening file, 400 Response sent\n");
        connection_flag = 0;
        continue;

      }
      else
      {
        int i;
        for (i = 0; i < 1; i++)
        {
          requested_http_path= strtok_r(NULL, s, &requested_ptr);
        }
      }
      if (requested_http_path == NULL)
      {
        requested_http_path = "/null";
        printf("Request not understood, proceeding with dummy string\n");
        char* response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
        write(new_socket, response, strlen(response));
        close(new_socket);
        printf("Error opening file, 400 Response sent\n");
        connection_flag = 0;
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
      char *sliced_ptr = NULL;
      token = strtok_r(requested_http_path_sliced, "%", &sliced_ptr);
      strncat(cleaned_path, token, strlen(token) + 1);
      while (token != NULL) {
        token = strtok_r(NULL, "%", &sliced_ptr);
        char space[102] = " ";
        if (token != NULL) {
          strncat(space, token, strlen(token) + 1);
          strncat(space, "\0", strlen("\0"));
          strncat(cleaned_path, space, strlen(token) +1);
        }
      }

      // Deals with token after %
      char *cleaned_ptr = NULL;
      char *token_spaces;
      token_spaces = strtok_r(cleaned_path, " ", &cleaned_ptr);
      strncat(html_path_decoded, token_spaces, strlen(token_spaces) +1);
      while (token_spaces != NULL) {
        token_spaces = strtok_r(NULL, " ", &cleaned_ptr);
        char space_dummy[102] = " ";
        if (token_spaces != NULL) {
          char *sliced_token = token_spaces + 2;
          strncat(space_dummy, sliced_token, strlen(sliced_token) + 1);
          strncat(html_path_decoded, space_dummy,strlen(space_dummy)+1);
        }
      }


      // Add folder to opening path for requests
      // Concatenate the decoded html path with the http root path
      char final_path[strlen(html_path_decoded) + strlen(http_root_path) + 2];
      strcpy(final_path, http_root_path);
      strncat(final_path, html_path_decoded, strlen(html_path_decoded) + 1);
      printf("file to open: %s\n", final_path);


      // find and open the requested path/file in the directory
      if (strncmp(html_path_decoded,"favicon.ico", 11) != 0){
        if ((fp=fopen(final_path,"rb")) == NULL) { // opening file
          // TODO: Send back 404 response to the client when the file is not found
          if (strncmp(requested_http_path, "/null", 5) != 0)
          {
            char* response = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
            write(new_socket, response, strlen(response));
            close(new_socket);
            printf("Error opening file, 404 Response sent\n");
            connection_flag = 0;
            continue;
          }
          else
          {
            char* response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
            write(new_socket, response, strlen(response));
            close(new_socket);
            printf("Error with client syntax, 400 Response sent\n");
            connection_flag = 0;
            continue;
          }
          memset(html_path_decoded, 0, strlen(html_path_decoded));
          memset(cleaned_path, 0, strlen(cleaned_path)+1);
        } else {
          printf("File opened successfully.\n");

          //Error check reading the file
          //Send response if fails - 500 error

          if (fread(&dest, sizeof(dest), 1, fp) < 0){
            char* read_response_err = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            write(new_socket, read_response_err, strlen(read_response_err));
            close(new_socket);
            connection_flag = 0;

            perror("Unable to read file");
            //exit(EXIT_FAILURE);
            continue;
          }

          fclose(fp);
          memset(fp, '\0', sizeof(fp));

          // status THIS HAS BEEN MOVED TO IF-RANGE

          // content-length
          char content_length[30];
          char content_length_header[30] = "Content-Length: ";
          sprintf(content_length, "%d", strlen(dest));
          strncat(content_length_header, content_length, strlen(content_length) + 1);
          strncat(content_length_header, "\r\n", strlen("\r\n"));

          // connection
          char *connection_header = "Connection: keep-alive\r\n";

          // etag header (only added if etag_bool is true)
          int etag_bool = 0;
          char temp[30] = {0};
          char etag_header[100] = "Etag: ";
          int offset = 1;
          generate_etag(final_path, temp, offset);
          strncat(etag_header,temp,strlen(temp));
          strncat(etag_header,"\r\n", strlen("\r\n"));

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
            char *accept_ptr = NULL;
            accept_ret = strstr(client_request_cpy, "Accept");
            content_type = strtok_r(accept_ret, ",", &accept_ptr) + 8;
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

          //If-Match Header
          /*
          The system our group has chosen for etags is a simple Caesar cipher of
          the file name. We figured that path + filename must be unique for every
          request, therefore, we are using that to create unique etags for every
          file requests from the client.
          */
          char* ifmatchresult = strstr(client_request_cpy, "If-Match");

          if(ifmatchresult != NULL){
            etag_bool = 1;
            printf("------------------If-Match Header Exists------------------\n");
            char etag[100] = {0};
            int offset = 1;
            generate_etag(final_path, etag, offset);
            printf("ETAG: %s\n", etag);

            //The case where the etag does not match expected
            if(strstr(ifmatchresult, etag) == NULL){
              //TODO: Look into adding more to the response?
              char rns[36] = "HTTP/1.1 416 Range Not Satisfiable\r\n\r\n\0";
              write(new_socket, rns, 36);
              close(new_socket);
              connection_flag = 0;
              continue;
            }

          }

          //If-None-Match Header
          /*
          If-None-Match operates similarly to If-Match with in regards to etags.
          */
          char* ifnonematchresult = strstr(client_request_cpy, "If-None-Match");

          if(ifnonematchresult != NULL){
            etag_bool = 1;
            printf("------------------If-None-Match Header Exists------------------\n");
            char etag[100] = {0};
            int offset = 1;
            generate_etag(final_path, etag, offset);
            printf("ETAG: %s\n", etag);

            //The case where the etag does not match expected
            if(strstr(ifnonematchresult, etag) != NULL){
              //TODO: Look into adding more to the response?
              char rns[27] = "HTTP/1.1 304 Not Modified\r\n\r\n\0";
              write(new_socket, rns, 27);
              close(new_socket);
              connection_flag = 0;
              continue;
            }

          }


          // If-Modified-Since
          char* ifmodifiedsinceresult = strstr(client_request_cpy, "If-Modified-Since");

          if(ifmodifiedsinceresult != NULL){
            printf("------------------If-Modified-Since Header Exists------------------\n");

            struct tm tm;
            char* modified_ptr = NULL;
            memset(&tm, 0, sizeof(tm));
            char* token = strtok_r(ifmodifiedsinceresult, " ", &modified_ptr);  // Requst
            char day[2];
            char month[2];
            int monthDigit;
            char year[4];
            char time[8];
            char timestamp[20];
            time_t epoch;

            token = strtok_r(NULL, " ", &modified_ptr);  // Weekday
            token = strtok_r(NULL, " ", &modified_ptr);  // Day
            strcpy(day, token);
            token = strtok_r(NULL, " ", &modified_ptr);  // Month
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
            token = strtok_r(NULL, " ", &modified_ptr);  // Year
            strcpy(year, token);
            token = strtok_r(NULL, " ", &modified_ptr);  // Time
            strcpy(time, token);
            snprintf(timestamp, 20, "%.4s-%.2s-%.2s %.8s", year, month, day, time);

            if (strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm) != NULL)
            {
              epoch = mktime(&tm);
            }

            if (epoch > attr.st_mtime) // TODO: Fix properly
            {
              char rns[27] = "HTTP/1.1 304 Not Modified\r\n\r\n\0";
              write(new_socket, rns, 27);
              close(new_socket);
              connection_flag = 0;
              continue;

            }
          }



          //TODO: If-Unmodified-Since
          char* ifunmodifiedsinceresult = strstr(client_request_cpy, "If-Unmodified-Since");

          if(ifunmodifiedsinceresult != NULL){
            printf("------------------If-Unmodified-Since Header Exists------------------\n");

            struct tm tm;
            char *unmodified_ptr = NULL;
            memset(&tm, 0, sizeof(tm));
            char* token = strtok_r(ifunmodifiedsinceresult, " ", &unmodified_ptr);  // Requst
            char day[2];
            char month[2];
            int monthDigit;
            char year[4];
            char time[8];
            char timestamp[20];
            time_t epoch;

            token = strtok_r(NULL, " ", &unmodified_ptr);  // Weekday
            token = strtok_r(NULL, " ", &unmodified_ptr);  // Day
            strcpy(day, token);
            token = strtok_r(NULL, " ", &unmodified_ptr);  // Month
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
            token = strtok_r(NULL, " ", &unmodified_ptr);  // Year
            strcpy(year, token);
            token = strtok_r(NULL, " ", &unmodified_ptr);  // Time
            strcpy(time, token);
            snprintf(timestamp, 20, "%.4s-%.2s-%.2s %.8s", year, month, day, time);

            if (strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm) != NULL)
            {
              epoch = mktime(&tm);
            }

            if (epoch < attr.st_mtime) // TODO: Fix properly
            {
              char rns[34] = "HTTP/1.1 412 Precondition Failed\r\n\r\n\0";
              write(new_socket, rns, 34);
              close(new_socket);
              connection_flag = 0;
              continue;

            }
          }


          //TODO: If-Range
          char *status_header;
          char* ifrangeresult = strstr(client_request_cpy, "If-Range");

          if(ifrangeresult != NULL){
            printf("------------------If-Range Header Exists------------------\n");

            int is_date = 0;

            for (int i = 0; i < 7; i++){
              if (strstr(ifrangeresult,WEEKDAY_LIST[i]) != NULL){
                is_date = 1;
              }
              printf("Is it a date?: %d\n", is_date);
            }
            if (is_date == 1){ //if-range is a date
              //Copy and Paste If-Modified
              struct tm tm;
              char *range_ptr = NULL;
              memset(&tm, 0, sizeof(tm));
              char* token = strtok_r(ifrangeresult, " ", &range_ptr);  // Requst
              char day[2];
              char month[2];
              int monthDigit;
              char year[4];
              char time[8];
              char timestamp[20];
              time_t epoch;

              token = strtok_r(NULL, " ", &range_ptr);  // Weekday
              token = strtok_r(NULL, " ", &range_ptr);  // Day
              strcpy(day, token);
              token = strtok_r(NULL, " ", &range_ptr);  // Month
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
              token = strtok_r(NULL, " ", &range_ptr);  // Year
              strcpy(year, token);
              token = strtok_r(NULL, " ", &range_ptr);  // Time
              strcpy(time, token);
              snprintf(timestamp, 20, "%.4s-%.2s-%.2s %.8s", year, month, day, time);

              if (strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm) != NULL)
              {
                epoch = mktime(&tm);
              }

              if (epoch > attr.st_mtime) // TODO: Fix properly
              {
                status_header = "HTTP/1.1 206 Partial Content\r\n\r\n\0";
              }
              else{
                status_header = "HTTP/1.1 200 OK\r\n";
              }
            }
            else{ //If-Range is an etag
              //Copy and paste If-Match
              char etag[100] = {0};
              int offset = 1;
              generate_etag(final_path, etag, offset);
              printf("ETAG: %s\n", etag);

              //The case where the etag does not match expected
              if(strstr(ifrangeresult, etag) == NULL){
                status_header = "HTTP/1.1 206 Partial Content\r\n\r\n\0";
              }
              else{
                status_header = "HTTP/1.1 200 OK\r\n";
              }
            }
          }
          else{
            status_header = "HTTP/1.1 200 OK\r\n";
          }

          // concatenate all the headers to http_response_header
          strncat(http_response_header, status_header, strlen(status_header) + 1);
          strncat(http_response_header, connection_header, strlen(connection_header) + 1);
          strncat(http_response_header, content_length_header, strlen(content_length_header)+1);
          strncat(http_response_header, content_type_header, strlen(content_type_header)+1);
          strncat(http_response_header, last_modified_header, strlen(last_modified_header)+1);
          if (etag_bool == 1){
            strncat(http_response_header, etag_header,strlen(etag_header)+1);
          }
          strncat(http_response_header, "\r\n", strlen("\r\n") );
          printf("http_response_header w/o data:\n%s", http_response_header);

          // appending data to header
          memcpy(dest + strlen(dest), "\r\n", strlen("\r\n"));
          memcpy(http_response_header + strlen(http_response_header), dest, strlen(dest));

          // send this to the client (should just be the code below)
          write(new_socket, http_response_header, strlen(http_response_header));

          printf("------------------Message sent to client-------------------\n");

        }} // making sure the below vars get memset even if an error occurs
        memset(buffer, '\0', strlen(buffer));
      }

      //return 0;
    }


    //---------------------- MAIN FUNCTION ----------------------------------------
    int main(int argc, char *argv[]) {

      // Deals with no arguements/incorrect # of args here
      printf("Arguments passed: %d\n", argc-1 );
      if (argc != 3){
        printf("Insufficient # of arguements passed.\n");
        return -1;
      }

      PORT = atoi(argv[1]); // set PORT to the second argument passed in
      printf("port: %d\n", PORT);


      http_root_path = argv[2]; // extract the http root path (which is third argument passed in)
      printf("http root path: %s\n", http_root_path);

      // set up server socket
      address = init_server_addr(PORT);
      addrlen = sizeof(address);
      server_fd = setup_server_socket(address, QUEUE_LENGTH); //has been binded and listened


      int client_cnt; // iterator for the for loops
      pthread_t thread_list[QUEUE_LENGTH]; // list of threads
      int client_soc = -2;
      printf("Creating threads...\n");
      // Creating QUEUE_LENGTH number of threads to handle QUEUE_LENGTH number of clients
      for (client_cnt = 0; client_cnt < QUEUE_LENGTH; client_cnt++) {
        pthread_create(&thread_list[client_cnt], NULL, &server_func, (void *) client_soc);
      }

      printf("Created %d new threads\n", client_cnt);

      printf("Joining threads...\n");
      // Running threads
      for (client_cnt = 0; client_cnt < QUEUE_LENGTH; client_cnt++) {
        pthread_join(thread_list[client_cnt], NULL);
      }

      return 0;
    }
