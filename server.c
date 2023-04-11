#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/ip.h>
#include <fnmatch.h>
#include <ftw.h>


#define PORT 49152
#define MAX_RESPONSE_SIZE 1024
#define MAX_ARGUMENTS 10
#define CHUNK_SIZE 16384
#define MAX_BUFFER_SIZE 1024
#define MAX_EXTENSION_COUNT 6

// cd ASP-Project
// gcc -o server server.c
// ./server
// gcc -o client client.c
// ./client 127.0.0.1

//char *files;
char files[MAX_BUFFER_SIZE];

/*  **************************************************************** File Transfer Function - tanmay  **************************************************************** */

int transfer_file(int new_socket, const char* filename) {
    // Open file to be transferred
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    ssize_t valread;

    // Read file contents and send to client
    while ((valread = read(fd, buffer, 1024)) > 0) {
        send(new_socket, buffer, valread, 0);
    }

    // Close file and socket
    close(fd);

    return 0;
}

/*  **************************************************************** Code Modification Ends - tanmay  *****************************************************************/


int recursive_search(char *dir_name, char **file_types, int extension_count, char *files, int *files_len) {
    DIR *dir;
    struct dirent *ent;
    char buffer[MAX_BUFFER_SIZE];
    int status;
    int i;

    if ((dir = opendir(dir_name)) == NULL) {
        perror("opendir failed");
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
            sprintf(buffer, "%s/%s", dir_name, ent->d_name);
            recursive_search(buffer, file_types, extension_count, files, files_len);
        } else {
            for (i = 0; i < extension_count; i++) {
                if (fnmatch(file_types[i], ent->d_name, 0) == 0) {
                    if (*files_len + strlen(dir_name) + strlen(ent->d_name) + 2 > MAX_BUFFER_SIZE) {
                        fprintf(stderr, "Too many files found, limit is %d\n", MAX_BUFFER_SIZE);
                        closedir(dir);
                        return -1;
                    }
                    sprintf(buffer, "%s/%s", dir_name, ent->d_name);
                    strcat(files, buffer);
                    strcat(files, " ");
                    *files_len += strlen(buffer) + 1;
                    break;
                }
            }
        }
    }
    closedir(dir);

    return 0;
}

void replace(char *str) {
    char *ptr = str;
    while ((ptr = strstr(ptr, "//")) != NULL) {
        memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
    }
}

void remove_linebreak(char** tokens, int num_tokens) {
    for (int i = 0; i < num_tokens; i++) {
        char* token = tokens[i];
        int length = strcspn(token, "\n");
        char* new_token = (char*) malloc(length + 1);
        strncpy(new_token, token, length);
        new_token[length] = '\0';
        tokens[i] = new_token;
    }
}


/*  **************************************************************** New Gettargz function - tanmay modified   **************************************************************** */
/*  *************************************************************** File transfer implementaion, file size for buffer adjustment   **************************************************************** */
int gettargz(int client_sockfd, char **extensions, int extension_count, int unzip) {
    DIR *dir;
    struct dirent *ent;
    char *home_dir = "/Users/tanmayverma/ASP/";
    char buffer[MAX_BUFFER_SIZE];
    char *temp_tar_name = "temp.tar.gz";
    char *tar_command_format = "tar -cf %s %s";
    int status;
    int i;
    char *file_types[extension_count];
    memset(files, 0, sizeof(files));
    
    printf("%s\n", home_dir);
    printf("%d\n", extension_count-1);

    for (i = 1; i < extension_count; i++) {
        printf("%s\n", extensions[i]);
    }
    if ((dir = opendir(home_dir)) == NULL) {
        perror("opendir failed");
        return -1;
    }
    for (i = 0; i < extension_count; i++) {
        file_types[i] = malloc(strlen(extensions[i]) + 2);
        sprintf(file_types[i], "*.%s", extensions[i]);
    }
    int *files_len = 0;
    if (recursive_search(home_dir, file_types, extension_count, files, &files_len) != 0) {
        send(client_sockfd, "Error finding files", strlen("Error finding files"), 0);
        return -1;
    }
    int num_file_types = extension_count-1;
    char abs_path[MAX_BUFFER_SIZE];
    sprintf(abs_path, "%s", home_dir);
    if (files_len == 0) {
        send(client_sockfd, "No file found", strlen("No file found"), 0);
        return 0;
    }
    replace(files);
    printf("Files: %s\n", files);
    sprintf(buffer, tar_command_format, temp_tar_name, files);
    printf("%s\n", buffer);
    status = system(buffer);
    if (status != 0) {
        fprintf(stderr, "Error creating tar file\n");
        return -1;
    }
    // Get the size of the tar file
    FILE *fp;
    long int file_size;
    fp = fopen(temp_tar_name, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error opening file\n");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    // Send the size of the file as a long int
    send(client_sockfd, &file_size, sizeof(file_size), 0);
    // Transfer the file
    int file_trans_result = transfer_file(client_sockfd, temp_tar_name);
    if (file_trans_result != 0) {
        fprintf(stderr, "Error transferring file\n");
    }
    else {
        fprintf(stderr, "File transferred successfully\n");
    }
    fclose(fp);
    return 0;
}
 /*  **************************************************************** Code Modification Ends - tanmay  *****************************************************************/

void findfile(int client_sockfd, char** arguments){

	char* filename = arguments[1];
	
	
// implement todo
			printf("inside findfile\n");
			char response[1024];
			
			printf("filename: %s\n", filename);
			
		
    char* home_dir = getenv("HOME"); // Get the home directory path
    char* command = (char*) malloc(strlen(home_dir) + strlen(filename) + 27); // Allocate memory for the command string
    sprintf(command, "find %s -name '%s' -print -quit", home_dir, filename); // Construct the find command
	printf("executing command: %s\n", command);
    FILE* pipe = popen(command, "r"); // Open a pipe to the command
    if (pipe != NULL) {
        char line[256];
        if (fgets(line, sizeof(line), pipe) != NULL) { // Read the first line of output
            line[strcspn(line, "\n")] = '\0'; // Remove the newline character from the end of the line
            struct stat sb;
            if (stat(line, &sb) == 0) { // Get the file information using stat()
                sprintf(response, "%s (%lld bytes, created %s)\n", line, (long long) sb.st_size, ctime(&sb.st_ctime)); // Print the file information
            } else {
                sprintf(response,"Unable to get file information for %s\n", line);
            }
        } else {
            sprintf(response,"File not found\n");
        }
        pclose(pipe); // Close the pipe
    } else {
        printf("Error opening pipe to command\n");
    }
    free(command); // Free the memory allocated for the command string
	
	write(client_sockfd, response, strlen(response));
			
}

/*  ************************************* Full Code Modified by Tanmay- Added return value, transfer file to client ************************************* */
/*  ************************************** and added recursive call with file extension specification, ************************************* */
/*  ************************************** without use of absolute path for the folder in arguments *************************************  */
int getfiles(int client_fd, char **args, int argLen)
{
    printf("%d\n", argLen);
    char *file_list = "";
    char *tar_cmd = malloc(sizeof(char) * 1024);
    char *temp_tar_name = "temp.tar.gz";
    char *home_dir = "/Users/tanmayverma/ASP/";

    // Append files to the file_list string
    for (int i = 1; i < argLen; i++)
    {
        if (args[i] != NULL)
        {
            // char *dir_name = args[i];
            printf("%s Home Directory : \n", home_dir);

            // Recursively search for files in the directory
            char *file_types[] = {"*.txt", "*.pdf", "*.doc"};
            int extension_count = 3;
            int files_len = 0;
            char files[MAX_BUFFER_SIZE] = "";
            int status = recursive_search(home_dir, file_types, extension_count, files, &files_len);

            if (status != 0)
            {
                printf("Error in recursive_search\n");
                return -1;
            }

            if (strlen(files) > 0)
            {
                char *temp = malloc(strlen(file_list) + strlen(files) + 1);
                strcpy(temp, file_list);
                strcat(temp, files);
                file_list = temp;
            }
        }
    }

    if (strlen(file_list) == 0)
    {
        // None of the files exist
        send(client_fd, "No file found", strlen("No file found"), 0);
        return -1;
    }

    // Create tar command to compress files into temp.tar.gz
    snprintf(tar_cmd, 1024, "tar -czf temp.tar.gz %s", file_list);

    // Execute tar command
    system(tar_cmd);

    // Send compressed file to client
    FILE *fileptr;
    char *buffer;
    long filelen;
    long int file_size_getfiles;

    fileptr = fopen(temp_tar_name, "rb"); // Open the file in binary mode

    if (fileptr == NULL)
    {
        fprintf(stderr, "Error opening file\n");
        return -1;
    }

    fseek(fileptr, 0, SEEK_END);
    file_size_getfiles = ftell(fileptr);
    fseek(fileptr, 0, SEEK_SET);

    buffer = (char *)malloc((filelen + 1) * sizeof(char)); // Allocate memory for the file data
    fread(buffer, filelen, 1, fileptr);                   // Read the file data into the buffer

    // Send the size of the file as a long int
    send(client_fd, &file_size_getfiles, sizeof(file_size_getfiles), 0);
    // Transfer the file - tanmay
    int file_trans_result = transfer_file(client_fd, temp_tar_name);
    if (file_trans_result != 0)
    {
        fprintf(stderr, "Error transferring file\n");
    }
    else
    {
        fprintf(stderr, "File transferred successfully\n");
    }

    fclose(fileptr); // Close the file

    return 0;
}
/*  **************************************************************** Code Modification Ends - tanmay  *****************************************************************/


void sgetfiles(int client_sockfd, char** arguments) {
    
	//char* dirname = getenv("HOME");
	char* dirname = "/Users/tanmayverma/";
    char* filename = "temp.tar.gz";
    size_t size1 = atoi(arguments[0]);
    size_t size2 = atoi(arguments[1]);
	
    char buf[MAX_RESPONSE_SIZE];
    DIR *dir;
    struct dirent *ent;
    int fd[2], nbytes;
    pid_t pid;

    // Create a temporary file to store the list of files
    char tmp_file[] = "tempXXXXXX";
    int tmp_fd = mkstemp(tmp_file);
    if (tmp_fd < 0) {
        perror("Error creating temporary file");
        return;
    }

    // Open the current directory
    dir = opendir(".");
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    // Iterate through the files in the directory and write the filenames
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG) {
            char path[MAX_RESPONSE_SIZE];
            struct stat st;
            sprintf(path, "%s/%s", ".", ent->d_name);
            if (stat(path, &st) == 0 && st.st_size >= size1 && st.st_size <= size2) {
                write(tmp_fd, ent->d_name, strlen(ent->d_name));
                write(tmp_fd, "\n", 1);
            }
        }
    }
    closedir(dir);

    // Fork a child process to handle the tar and zip operation
    if (pipe(fd) < 0) {
        perror("Error creating pipe");
        return;
    }
    pid = fork();
    if (pid < 0) {
        perror("Error forking child process");
        return;
    } else if (pid == 0) {
        // Child process
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        execlp("tar", "tar", "-czf", "-", "-T", tmp_file, NULL);
        perror("Error executing tar command");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        close(fd[1]);
        while ((nbytes = read(fd[0], buf, MAX_RESPONSE_SIZE)) > 0) {
            write(client_sockfd, buf, nbytes);
        }
        close(fd[0]);

        // Delete the temporary file
        unlink(tmp_file);

    }
}

/* *********************** Harjot's Code *********************** */
time_t strtotime(const char *time_str)
{
    struct tm tm;
    if (strptime(time_str, "%Y-%m-%d", &tm) == NULL)
    {
        return (time_t)-1;
    }
    time_t t = mktime(&tm);
    if (t == (time_t)-1)
    {
        return (time_t)-1;
    }
    return t;
}

	
/* *********************** Harjot's Code *********************** */
void dget(char *root_path, time_t date1, time_t date2, char *command, char *files)
{
    // open output file

    int files_added = 0;
    char buffer[MAX_BUFFER_SIZE];

    // open root directory
    DIR *dir = opendir(root_path);

    if (dir == NULL)
    {
        printf("Error opening directory.\n");
        return;
    }

    // read directory entries
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // construct full path of file/directory
        char path[256];

        sprintf(path, "%s/%s", root_path, entry->d_name);

        // get file/directory stats
        struct stat statbuf;
        if (stat(path, &statbuf) == -1)
        {
            continue;
        }

        // if file, check if it's between dates and print name
        if (S_ISREG(statbuf.st_mode))
        {
            if (statbuf.st_ctime >= date1 && statbuf.st_ctime <= date2)
            {
                // sprintf(command + strlen(command), "%s ", path);
                sprintf(buffer, "%s/%s", root_path, entry->d_name);
                strcat(files, buffer);
                strcat(files, " ");
                files_added = 1;
                // printf("%s\n", entry->d_name);
                // printf("%s ", path);
            }
        }
        // if directory, recursively search it
        else if (S_ISDIR(statbuf.st_mode))
        {
            dget(path, date1, date2, command, files);
        }
    }
    closedir(dir);

    // close directory
}

/* *********************** Harjot's Code *********************** */

// int dgetfiles(int client_sockfd, char **arguments)
// {

//     time_t date1 = strtotime(arguments[0]);
//     time_t date2 = strtotime(arguments[1]);

//     // get home directory path
//     char *root_path = "/Users/tanmayverma/ASP/";

//     // get files

//     char buffer[MAX_BUFFER_SIZE];
//     char *archive_name = "temp.tar.gz";
//     char *command = "tar -cf temp.tar.gz %s";
//     char *files = malloc(MAX_BUFFER_SIZE);

//     dget(root_path, date1, date2, command, files);
//     replace(files);
//     sprintf(buffer, command, files);
//     // printf("%s", buffer);
//     // if (strlen(files) != 0)
//     //     system(buffer);
//     // else
//     //     printf("No files found");
//     if (strlen(files) != 0) {
//     int status = system(buffer);
//     if (status == 0) {
//         printf("Command executed successfully\n");
//         } else {
//         fprintf(stderr, "Error executing command\n");
//      }
//     } else {
//     printf("No files found\n");
//     }


    
//     free(files);
//     return 0;
// }

/*  ************************************* Code Modified by Tanmay- Added return value, transfer file to client for dgetfiles command ************************************* */
int dgetfiles(int client_sockfd, char **arguments)
{
    time_t date1 = strtotime(arguments[0]);
    time_t date2 = strtotime(arguments[1]);

    // get home directory path
    char *root_path = "/Users/tanmayverma/ASP/";

    // get files
    char buffer[MAX_BUFFER_SIZE];
    char *archive_name = "temp.tar.gz";
    char *command = "tar -czf temp.tar.gz %s";
    char *files = malloc(MAX_BUFFER_SIZE);

    dget(root_path, date1, date2, command, files);
    replace(files);
    sprintf(buffer, command, files);

    if (strlen(files) != 0) {
        int status = system(buffer);
        if (status == 0) {
            printf("Command executed successfully\n");
            // Send the compressed file to the client
            FILE *fileptr;
            char *buffer;
            long file_size;
            long int file_size_getfiles;

            fileptr = fopen(archive_name, "rb"); // Open the file in binary mode

            if (fileptr == NULL)
            {
                fprintf(stderr, "Error opening file\n");
                return -1;
            }

            fseek(fileptr, 0, SEEK_END);
            file_size = ftell(fileptr);
            fseek(fileptr, 0, SEEK_SET);

            buffer = (char *)malloc((file_size + 1) * sizeof(char)); // Allocate memory for the file data
            fread(buffer, file_size, 1, fileptr);                   // Read the file data into the buffer

            // Send the size of the file as a long int
            send(client_sockfd, &file_size, sizeof(file_size), 0);
            // Transfer the file
            int file_trans_result = transfer_file(client_sockfd, archive_name);
            if (file_trans_result != 0)
            {
                fprintf(stderr, "Error transferring file\n");
            }
            else
            {
                fprintf(stderr, "File transferred successfully\n");
            }

            fclose(fileptr); // Close the file
        } else {
            fprintf(stderr, "Error executing command\n");
        }
    } else {
        printf("No files found\n");
    }

    free(files);
    return 0;
}

/*  ************************************* Code Modification Ends - Tanmay ************************************* */
void processClient(int client_sockfd)
{
	char command[255];
	char response[1024];
	int n, pid;
	write(client_sockfd, "Send commands", 14);

	if (pid = fork()) /*reading csd messages */
	{
		while (1)
		{
			n = read(client_sockfd, command, 255);
			
			if (n <= 0) { // Check if no data received (client closed connection)
            printf("Client disconnected.\n");
            break; // Exit loop and terminate processclient() function
			}
			
			printf("\ngot input: %s\n",command);
			
			
			char *arguments[MAX_ARGUMENTS];
			int num_arguments = 0;
			
			// Parse the command received from client
			char* token = strtok(command, " "); // Tokenize command using space as delimiter
			
			while (token != NULL) {
				arguments[num_arguments++] = token; // Store the token in the array
				token = strtok(NULL, " "); // Get the next token
			}
			arguments[num_arguments] = NULL; // Set the last element of the array to NULL
			
			// Remove line breaks from tokens
			remove_linebreak(arguments, num_arguments);
			
			char* cmd = arguments[0]; // Extract first token as the command
			
			printf("got command: %s\n",cmd);
			
			// Process the command and generate response
			if (strcmp(cmd, "findfile") == 0)
			{
				
				findfile(client_sockfd, arguments);

			}
			else if (strcmp(cmd, "sgetfiles") == 0)
			{
				
				sgetfiles(client_sockfd, arguments);

			}
			else if (strcmp(cmd, "dgetfiles") == 0)
            {
                printf("IN dgetfiles ");
                int resultdgetfiles = dgetfiles(client_sockfd, arguments);

                if (resultdgetfiles == -1) {
					fprintf(stderr, "An error occurred while creating or sending the tar archive.\n");
				
			}
            }
			else if (strcmp(cmd, "getfiles") == 0)
			{
	
				int resultgetfiles = getfiles(client_sockfd, arguments, num_arguments);

            if (resultgetfiles == -1) {
					fprintf(stderr, "An error occurred while creating or sending the tar archive.\n");
				
			}
            }
			else if (strcmp(cmd, "gettargz") == 0)
			{
				int unzip = 0;  // set to 1 to unzip the tar archive after creating it
				// int num_extensions = num_arguments;

				// printf("Number of extensions: %d\n", num_extensions);
   				// initialize the client socket and connect to the server

    			// call the gettargz function
				int result = gettargz(client_sockfd, arguments, num_arguments, unzip);

				// check the result of the function call
				if (result == -1) {
					fprintf(stderr, "An error occurred while creating or sending the tar archive.\n");
					exit(EXIT_FAILURE);
				}

			}

            /* Modified Code for terminating the client connection on Quit Command - Tanmay */
			else if (strcmp(cmd, "quit") == 0)
			{
				printf("Client requested to quit.\n");
                snprintf(response, MAX_RESPONSE_SIZE, "Server has closed the connection \n"); // Generate response for valid command
                write(client_sockfd, response, strlen(response));
                close(client_sockfd); // Close the client socket
                printf("Client socket closed.\n");
                break;
                exit(0); 
			}
            /* ******************************************************************************* */
            
			else
			{
				snprintf(response, MAX_RESPONSE_SIZE, "Invalid command\n");	// Generate response for invalid command
				write(client_sockfd, response, strlen(response));
				continue;	// Continue to next iteration of loop to wait for new command
			}

			// Send response to client
			//send(client_sockfd, response, strlen(response), 0);
			
			
		}
	}
}	


int main(int argc, char *argv[])
{
	int sd, csd, portNumber, status;
	socklen_t len;
	struct sockaddr_in servAdd;	//ipv4

	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Cannot create socket\n");
		exit(1);
	}

	//Add information to the servAdd struct variable of type sockaddr_in
	servAdd.sin_family = AF_INET;
	//When INADDR_ANY is specified in the bind call, the socket will be bound to all local interfaces.
	//The htonl function takes a 32-bit number in host byte order and returns a 32-bit number in the network byte order used in TCP/IP networks
	servAdd.sin_addr.s_addr = htonl(INADDR_ANY);	//Host to network long 

	//htonos: Host to network short-byte order
	servAdd.sin_port = htons(PORT);

	//struct sockaddr is the generic structure to store socket addresses
	//The procedure it to typecast the specific socket addreses of type sockaddr_in, sockaddr_in6, sockaddr_un into sockaddr

	bind(sd, (struct sockaddr *) &servAdd, sizeof(servAdd));
	listen(sd, 5);

	while (1)
	{
		csd = accept(sd, (struct sockaddr *) NULL, NULL);
		printf("Got a client\n");
		if (!fork())	//Child process 
			processClient(csd);
		close(csd);
		waitpid(0, &status, WNOHANG);	// waitpid? 
	}
}	//End main