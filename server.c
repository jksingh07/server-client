// ASP - PROJECT --> SERVER-CLIENT APPLICATION
// Team Member - Jaskaran Singh Luthra (110090236), Harjot Singh Saggu (110093636)


// SERVER FILE

// IMPORTING LIBRARIES
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

#define PORT 8000   // PORT FOR SERVER
#define MAX_RESPONSE_SIZE 1024
#define MAX_ARGUMENTS 10
#define CHUNK_SIZE 16384
#define MAX_BUFFER_SIZE 1024
#define MAX_EXTENSION_COUNT 6  // GET ARGUMENTS MAXIMUM LIMIT

// Terminal commands to start the project
// cd ASP-Project
// gcc -o server server.c
// ./server
// gcc -o client client.c
// ./client 127.0.0.1 8000

char files[MAX_BUFFER_SIZE];

// Function to transfer tar file which is temp.tar.gz to the client side
int transfer_file(int new_socket, const char *filename)
{
	// Open file to be transferred
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		perror("open failed");
		exit(EXIT_FAILURE);
	}

	char buffer[1024];
	ssize_t valread;

	// Read file contents and send to client
	while ((valread = read(fd, buffer, 1024)) > 0)
	{
		send(new_socket, buffer, valread, 0);
	}

	// Close file and socket
	memset(buffer, 0, sizeof(buffer));
	close(fd);

	return 0;
}

// Function to recursively iterate through the directories/ nested diretories and search for the file with that extension
// It starts by opening the directory and iterating through its contents using the readdir function. 
// If it encounters a subdirectory, it recursively calls itself with the subdirectory path. 
// If it encounters a file, it compares its extension with the list of extensions provided. 
// If it finds a match, it appends the file path to the buffer and increments the buffer length.
// The function returns 0 if it completes successfully, and -1 if there are too many files found or if there is an error opening the directory.
int recursive_search(char *dir_name, char **file_types, int extension_count, char *files, int *files_len)
{
	DIR *dir;
	struct dirent *ent;
	char buffer[MAX_BUFFER_SIZE];
	int status;
	int i;

	// Open Directory from where you want to start the search
	if ((dir = opendir(dir_name)) == NULL)
	{
		perror("opendir failed");
		return -1;
	}

	while ((ent = readdir(dir)) != NULL)
	{
		// if its directory
		if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
		{
			sprintf(buffer, "%s/%s", dir_name, ent->d_name);
			recursive_search(buffer, file_types, extension_count, files, files_len);
		}
		else
		{
			for (i = 0; i < extension_count; i++)
			{
				// matching the extension of the file
				if (fnmatch(file_types[i], ent->d_name, 0) == 0)
				{
					if (*files_len + strlen(dir_name) + strlen(ent->d_name) + 2 > MAX_BUFFER_SIZE)
					{
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
	memset(buffer, 0, sizeof(buffer));

	return 0;
}

// This function removes '//' with '/' in the path
void replace(char *str)
{
	char *ptr = str;
	while ((ptr = strstr(ptr, "//")) != NULL)
	{
		memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
	}
}

// This function will make the list of arguments like an array
void remove_linebreak(char **tokens, int num_tokens)
{
	for (int i = 0; i < num_tokens; i++)
	{
		char *token = tokens[i];
		int length = strcspn(token, "\n");
		char *new_token = (char *)malloc(length + 1);
		strncpy(new_token, token, length);
		new_token[length] = '\0';
		tokens[i] = new_token;
	}
}

// Function to get files with particular extension (Up to 6 exetnsions accepted)
int gettargz(int client_sockfd, char **extensions, int extension_count, int unzip)
{
	DIR *dir;
	struct dirent *ent;
	// char *home_dir = getenv("HOME");
	char *home_dir = "/Users/jaskaransingh/source"; // giving personal folder to reduce computation and for testing
	char buffer[MAX_BUFFER_SIZE];
	char *temp_tar_name = "temp.tar.gz"; // tar file will be generated with this name
	char *tar_command_format = "tar -cf %s %s";
	int status;
	int i;
	char *file_types[extension_count];
	memset(files, 0, sizeof(files));

	printf("%s\n", home_dir);
	printf("%d\n", extension_count - 1);

	for (i = 1; i < extension_count; i++)
	{
		printf("%s\n", extensions[i]);
	}
	if ((dir = opendir(home_dir)) == NULL)
	{
		perror("opendir failed");
		return -1;
	}
	for (i = 0; i < extension_count; i++)
	{
		file_types[i] = malloc(strlen(extensions[i]) + 2);
		sprintf(file_types[i], "*.%s", extensions[i]);
	}
	int *files_len = 0;
	if (recursive_search(home_dir, file_types, extension_count, files, &files_len) != 0)
	{
		send(client_sockfd, "Error finding files", strlen("Error finding files"), 0);
		return -1;
	}
	int num_file_types = extension_count - 1;
	char abs_path[MAX_BUFFER_SIZE];
	sprintf(abs_path, "%s", home_dir);
	if (files_len == 0)
	{
		send(client_sockfd, "No file found", strlen("No file found"), 0);
		return 0;
	}
	replace(files);
	// printf("Files: %s\n", files);
	sprintf(buffer, tar_command_format, temp_tar_name, files);
	// printf("%s\n", buffer);
	status = system(buffer);
	if (status != 0)
	{
		fprintf(stderr, "Error creating tar file\n");
		return -1;
	}
	// Get the size of the tar file
	FILE *fp;
	long int file_size;
	fp = fopen(temp_tar_name, "rb");
	if (fp == NULL)
	{
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
	if (file_trans_result != 0)
	{
		fprintf(stderr, "Error transferring file\n");
	}
	else
	{
		fprintf(stderr, "File transferred successfully\n");
	}
	fclose(fp);
	// memset(file_size, 0, sizeof(file_size));
	return 0;
}

// funtion to find files - find the first occurance of file in the system and returns path to the client
void findfile(int client_sockfd, char **arguments)
{

	char *filename = arguments[1];
	char response[1024];
	memset(response, 0, sizeof(response));
	printf("Filename: %s\n", filename);

	// Get the home directory path
	char *home_dir = getenv("HOME");
	// Allocate memory for the command string
	char *command = (char *)malloc(strlen(home_dir) + strlen(filename) + 27);
	// Construct the find command
	sprintf(command, "find %s -name '%s' -print -quit", home_dir, filename);
	printf("executing command: %s\n", command);
	FILE *pipe = popen(command, "r"); // Open a pipe to the command

	if (pipe != NULL)
	{
		char line[256];
		if (fgets(line, sizeof(line), pipe) != NULL)
		{									  // Read the first line of output
			line[strcspn(line, "\n")] = '\0'; // Remove the newline character from the end of the line
			struct stat sb;
			if (stat(line, &sb) == 0)
			{																										  // Get the file information using stat()
				sprintf(response, "%s (%lld bytes, created %s)\n", line, (long long)sb.st_size, ctime(&sb.st_ctime)); // Print the file information
			}
			else
			{
				sprintf(response, "Unable to get file information for %s\n", line);
			}
		}
		else
		{
			sprintf(response, "File not found\n");
		}
		pclose(pipe); // Close the pipe
	}
	else
	{
		printf("Error opening pipe to command\n");
	}

	write(client_sockfd, response, strlen(response));
	memset(response, 0, sizeof(response));
	free(command); // Free the memory allocated for the command string
}
// funtion to exectute getfiles command - return the mentioned files (search files in the system ) to the client in a zip
void getfiles(int client_sockfd, char **arguments, int len)
{
	char files[6][100]; // can accept 6 files with char limit of 100
	char cmd[500] = "find /Users/jaskaransingh -type f \\( -name ";
	char temp[100];
	int i;

	// get file names from arguments
	// max len should be 6
	if (len > 6)
	{
		printf("Error: More arguments.\n");
		return;
	}
	for (i = 0; i < len; i++)
	{
		strcpy(files[i], arguments[i]);
	}

	// build command string
	for (i = 0; i < len; i++)
	{
		strcat(cmd, "\"");
		strcat(cmd, files[i]);
		strcat(cmd, "\"");
		if (i < len - 1)
		{
			strcat(cmd, " -o -name ");
		}
	}
	strcat(cmd, " \\) -print0 | tar czvf temp.tar.gz --null -T -");

	char *temp_tar_name = "temp.tar.gz";
	// execute command
	int result = system(cmd);

	if (result == 0)
	{

		// Get the size of the tar file
		FILE *fp;
		long int file_size;
		fp = fopen(temp_tar_name, "rb");
		if (fp == NULL)
		{
			fprintf(stderr, "Error opening file\n");
			return;
		}
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		// Send the size of the file as a long int
		send(client_sockfd, &file_size, sizeof(file_size), 0);
		// Transfer the file
		int file_trans_result = transfer_file(client_sockfd, temp_tar_name);
		if (file_trans_result != 0)
		{
			fprintf(stderr, "Error transferring file\n");
		}
		else
		{
			fprintf(stderr, "File transferred successfully\n");
		}
		fclose(fp); // Close the file
					// memset(file_size,0,sizeof(file_size));
	}

	else
		printf("No files found");

	return;
}
// This function takes a string representation of a date and time in the format "YYYY-MM-DD"
// and converts it to a Unix timestamp, which is the number of seconds since January 1, 1970.
// It returns the Unix timestamp as a time_t value.
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
// Funtion to exectute dgetfiles - takes 2 dates date1 and date 2 and return all files including in range from date1 to date2.
void dgetfiles(int client_sockfd, char **arguments)
{

	char *date1 = arguments[1];
	char *date2 = arguments[2];
	// get home directory path
	char *root_path = getenv("HOME");
	char *temp_tar_name = "temp.tar.gz";
	char cmd[1024];
	// creating a compressed tar archive of selected files in a directory based on time limits specified by the arguments array.
	sprintf(cmd, "find /Users/jaskaransingh/source -type f -newermt \"%s 00:00:00\" ! -newermt \"%s 23:59:00\" -print0 | tar czvf temp.tar.gz --null -T -", date1, date2);
	int result = system(cmd);
	char *archive_name = "temp.tar.gz";

	if (result == 0)
	{

		// Get the size of the tar file
		FILE *fp;
		long int file_size;
		fp = fopen(temp_tar_name, "rb");
		if (fp == NULL)
		{
			fprintf(stderr, "Error opening file\n");
			return;
		}
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		// Send the size of the file as a long int
		send(client_sockfd, &file_size, sizeof(file_size), 0);
		// Transfer the file
		int file_trans_result = transfer_file(client_sockfd, temp_tar_name);
		if (file_trans_result != 0)
		{
			fprintf(stderr, "Error transferring file\n");
		}
		else
		{
			fprintf(stderr, "File transferred successfully\n");
		}
		fclose(fp); // Close the file
					// memset(file_size,0, sizeof(file_size));
	}

	else
		printf("No files found");

	return;
}
// Funtion to exectue sgetfiles command - takes 2 sizes size1 and size2 and return all files including in range from size1 to size2.
void sgetfiles(int client_sockfd, char **arguments)
{

	char *temp_tar_name = "temp.tar.gz";
	char cmd[1024];
	// creating a compressed tar archive of selected files in a directory based on size limits specified by the arguments array.
	sprintf(cmd, "find /Users/jaskaransingh/source -type f -size +%sc -size -%sc -print0 | tar czvf temp.tar.gz --null -T -", arguments[1], arguments[2]);

	int result = system(cmd);
	if (result == 0)
	{

		// Get the size of the tar file
		FILE *fp;
		long int file_size;
		fp = fopen(temp_tar_name, "rb");
		if (fp == NULL)
		{
			fprintf(stderr, "Error opening file\n");
			return;
		}
		fseek(fp, 0, SEEK_END); // pointer at end
		file_size = ftell(fp);  // using the pointer at end calcuates file size
		fseek(fp, 0, SEEK_SET); // reset pointer to start of file

		// Send the size of the file as a long int
		send(client_sockfd, &file_size, sizeof(file_size), 0);
		// Transfer the file
		int file_trans_result = transfer_file(client_sockfd, temp_tar_name);
		if (file_trans_result != 0)
		{
			fprintf(stderr, "Error transferring file\n");
		}
		else
		{
			fprintf(stderr, "File transferred successfully\n");
		}
		fclose(fp); // Close the file
	}

	else
		printf("No files found");
	// free(files);
	return;
}

// This function process all the commands coming from client
// Funtion to process client request
void processClient(int client_sockfd)
{
	char command[1024];
	char response[1024];
	int n, pid;
	while (1)
	{
		memset(files, 0, sizeof(files));
		memset(command, 0, sizeof(command));
		n = read(client_sockfd, command, 255);

		if (n <= 0)
		{ // Check if no data received (client closed connection)
			printf("Client disconnected.\n");
			break; // Exit loop and terminate processclient() function
		}

		printf("\ngot input: %s\n", command);

		char *arguments[MAX_ARGUMENTS];
		int num_arguments = 0;

		// Parse the command received from client
		char *token = strtok(command, " "); // Tokenize command using space as delimiter

		while (token != NULL)
		{
			arguments[num_arguments++] = token; // Store the token in the array
			token = strtok(NULL, " ");			// Get the next token
		}
		arguments[num_arguments] = NULL; // Set the last element of the array to NULL

		// Remove line breaks from tokens
		remove_linebreak(arguments, num_arguments);

		char *cmd = arguments[0]; // Extract first token as the command

		printf("got command: %s\n", cmd);

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
			dgetfiles(client_sockfd, arguments);
		}
		else if (strcmp(cmd, "getfiles") == 0)
		{

			getfiles(client_sockfd, arguments, num_arguments);
		}
		else if (strcmp(cmd, "gettargz") == 0)
		{
			int unzip = 0; // set to 1 to unzip the tar archive after creating it
			// int num_extensions = num_arguments;
			// initialize the client socket and connect to the server

			// call the gettargz function
			int result = gettargz(client_sockfd, arguments, num_arguments, unzip);

			// check the result of the function call
			if (result == -1)
			{
				fprintf(stderr, "An error occurred while creating or sending the tar archive.\n");
				exit(EXIT_FAILURE);
			}
		}
		else if (strcmp(cmd, "quit") == 0)
		{
			printf("Client requested to quit.\n");
			snprintf(response, MAX_RESPONSE_SIZE, "Server has closed the connection"); // Generate response for invalid command
			write(client_sockfd, response, strlen(response));
			snprintf(response, MAX_RESPONSE_SIZE, "quit"); // Generate response for invalid command
			write(client_sockfd, response, strlen(response));

			break; // Exit loop and terminate processclient() function
		}
		else
		{
			snprintf(response, MAX_RESPONSE_SIZE, "Invalid command\n"); // Generate response for invalid command
			write(client_sockfd, response, strlen(response));
			continue; // Continue to next iteration of loop to wait for new command
		}
		memset(files, 0, sizeof(files));
		memset(command, 0, sizeof(command));
	}
}

int main(int argc, char *argv[])
{
	int sd, csd, portNumber, status;
	socklen_t len;
	struct sockaddr_in servAdd; // ipv4

	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Cannot create socket\n");
		exit(1);
	}

	// Add information to the servAdd struct variable of type sockaddr_in
	servAdd.sin_family = AF_INET;
	// When INADDR_ANY is specified in the bind call, the socket will be bound to all local interfaces.
	// The htonl function takes a 32-bit number in host byte order and returns a 32-bit number in the network byte order used in TCP/IP networks
	servAdd.sin_addr.s_addr = htonl(INADDR_ANY); // Host to network long

	// htonos: Host to network short-byte order
	servAdd.sin_port = htons(PORT);

	// struct sockaddr is the generic structure to store socket addresses
	// The procedure it to typecast the specific socket addreses of type sockaddr_in, sockaddr_in6, sockaddr_un into sockaddr

	bind(sd, (struct sockaddr *)&servAdd, sizeof(servAdd));
	listen(sd, 5);
	int count = 0; // keep track of no. of connections

	// waiting for clients to connect
	while (1)
	{
		csd = accept(sd, (struct sockaddr *)NULL, NULL);
		printf("Got a client\n");
		count++;
		printf("Connections: %d\n", count);
		char response[1024];
		// First 4 client server will handle
		if (count < 4)
		{
			snprintf(response, MAX_RESPONSE_SIZE, "success");
		}
		// Next 4 clients sending for mirror
		else if (count < 8)
		{
			snprintf(response, MAX_RESPONSE_SIZE, "8001");
		}
		// Remaing  handled by the server and the mirror in an alternating manner
		else
		{
			if (count % 2 == 0)
				snprintf(response, MAX_RESPONSE_SIZE, "success");
			else
				snprintf(response, MAX_RESPONSE_SIZE, "8001");
		}
		write(csd, response, strlen(response));

		if (!fork())
		{ // Child process

			processClient(csd);

			close(csd);

			return 0;
		}

		waitpid(0, &status, WNOHANG);
	}
} // End main