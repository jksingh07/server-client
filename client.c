// ASP - PROJECT --> SERVER-CLIENT APPLICATION
// Team Member - Jaskaran Singh Luthra (110090236), Harjot Singh Saggu (110093636)


// CLIENT FILE


// IMPORTING LIBRARIES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <utime.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <time.h>

// DEFINING MACROS
#define PORT 8000    // PORT TO CONNECT WITH SERVER
#define MAX_ARGUMENTS 10
#define BUFFER_SIZE 1024
#define MIRROR_IP "192.168.2.33" // IP TO CONNECT WITH MIRROR IF SERVER REDIRECTS LOAD/CONNECTIONS

// funtion to removing line breaks from command
// The remove_linebreak function removes newline characters from each string in an array of strings.
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

// converting string to date format
// The convertStringToDate function converts a string in "YYYY-MM-DD" format to a time_t data type representing the same date and returns it.
time_t convertStringToDate(char *date)
{
	struct tm tm = {0};
	if (strptime(date, "%Y-%m-%d", &tm) == NULL)
	{
		return -1;
	}
	tm.tm_isdst = -1;
	time_t temp;
	temp = mktime(&tm); // The mktime function takes a pointer to a struct tm and 
						//returns a time_t value that represents the number of seconds since January 1, 1970.
	return temp;
}

// validating input command
// The input_validation function takes a string input_command as input and validates it based on certain criteria. 
// The function uses strtok to tokenize the input command and count the number of tokens.

// findfile command - returns 1
// sgetfiles command - returns 2
// dgetfiles command - returns 2
// getfiles command - returns 4
// gettargz command - returns 5
// quit command - returns 6
int input_validation(char *input_command)
{
	int i = 0;
	char *cmd = strtok(input_command, " ");
	int count = 0;
	char *local[50];
	int isUnzip = 0;
	while (1)
	{
		char *cmd1 = strtok(NULL, " ");
		if (cmd1 == NULL)
		{
			break;
		}
		if (strcmp(cmd1, "\n") == 0)
		{
			continue;
		}
		local[i] = cmd1;
		i++;
		count++;
	}
	if (count > 0 && strcmp(local[count - 1], "-u\n") == 0)
	{
		isUnzip = 1;
	}
	if (strcmp(cmd, "findfile") == 0)
	{
		if (count != 1)
		{
			fprintf(stderr, "Command Invalid - findfile `filename`\n");
			return -1;
		}
		return 1;
	}

	if (strcmp(cmd, "sgetfiles") == 0)
	{
		if (count < 2 || count > 3)
		{
			fprintf(stderr, "Command Invalid - sgetfiles size1 size2 <-u>\n");
			return -1;
		}

		int size1 = atoi(local[0]); // converts a string argument to an integer.
		int size2 = atoi(local[1]);
		if (size1 < 0 || size2 < 0)
		{
			fprintf(stderr, "Command Invalid - sgetfiles size1 size2 <-u>: [Size1, Size2] >= 0\n");
			return -1;
		}

		if (size2 < size1)
		{
			fprintf(stderr,
					"Command Invalid - sgetfiles size1 size2 <-u>: Size 1 should be less than equal to size 2\n");
			return -1;
		}

		return 2;
	}

	if (strcmp(cmd, "dgetfiles") == 0)
	{
		if (count < 2 || count > 3)
		{
			fprintf(stderr, "Command Invalid - dgetfiles date1 date2 <-u>\n");
			return -1;
		}

		time_t date1, date2;
		date1 = convertStringToDate(local[0]);
		date2 = convertStringToDate(local[1]);
		if (date1 == -1 || date2 == -1)
		{
			fprintf(stderr, "Invalid date format should YYYY-MM-DD\n");
			return -1;
		}

		if (date2 < date1)
		{
			fprintf(stderr, "date2 should be greater than equal to date1\n");
			return -1;
		}
		return 2;
	}

	if (strcmp(cmd, "getfiles") == 0)
	{

		if (isUnzip == 0 && count > 6) // without unzip args count can only be 6 but if count>6 without unzip then its invalid
		{
			fprintf(stderr,
					"Command Invalid - getfiles file1 file2 file3 file4 file5 file6(file 1 ..up to file6) <-u>\n");
			return -1;
		}

		if (count < 1 || count > 7)
		{
			fprintf(stderr,
					"Command Invalid - getfiles file1 file2 file3 file4 file5 file6(file 1 ..up to file6) <-u>\n");
			return -1;
		}

		return 4;
	}

	if (strcmp(cmd, "gettargz") == 0)
	{
		if (isUnzip == 0 && count > 6) // without unzip args count can only be 6 but if count>6 without unzip then its invalid
		{
			fprintf(stderr, "Command Invalid - gettargz <extension list> <-u> //up to 6 different file types\n");
			return -1;
		}

		if (count < 1 || count > 7)
		{
			fprintf(stderr, "Command Invalid - gettargz <extension list> <-u> //up to 6 different file types\n");
			return -1;
		}
		return 5;
	}

	if (strcmp(cmd, "quit\n") == 0)
	{
		if (count)
		{
			fprintf(stderr, "Command Invalid - quit\n");
			return -1;
		}
		return 6;
	}

	fprintf(stderr, "Command not supported!\n");
	return -1;
}
// main funtion
int main(int argc, char *argv[])
{
	// declaring variables
	int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char buffer[1024] = {0};
	char valbuf[1024];
	char server_ip[16];
	char *filename = "out.tar.gz";

	// check the terminal command if arguments are leass that 2, correct command contains IP address as arg.
	if (argc < 2)
	{
		printf("Usage: %s <server_ip>\n", argv[0]);
		return 1;
	}
	strcpy(server_ip, argv[1]);

	// creating client socket
	// AF_INET and SOCK_STREAM are constants used as parameters in the socket function to specify the type of communication protocol to be used by the socket.
	// AF_INET stands for Address Family Internet and is used for IPv4 protocol. It is the most common address family used for networking.
	// SOCK_STREAM is used for a reliable, stream-oriented connection between two sockets, which means that data is transmitted in a continuous stream and arrives at the receiver in the same order as it was sent by the sender.
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Socket creation error\n");
		return 1;
	}
	// Configure server address
	// configuring the sockaddr_in structure (serv_addr) to represent the server's address and port.
	serv_addr.sin_family = AF_INET; //  represents the address family of the socket (in this case, IPv4).
	serv_addr.sin_port = htons(PORT); // serv_addr.sin_port is being set to the port number that the server will listen on. htons() is a function that converts the port number from host byte order to network byte order, which is necessary for communicating over a network.

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
	{
		printf("Invalid address or Address not supported\n");
		return 1;
	}
	// connecting to server
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Connection failed\n");
		return 1;
	}
	// receving message from server
	read(sock, buffer, 1024);
	printf("Message from server: %s \n\n", buffer);

	// connecting to mirror
	if (strcmp(buffer, "success") != 0)
	{
		printf("Connecting to Mirror\n\n");
		close(sock);
		// int sock1=0;
		// close(sock);
		// server = connectToServer(message, argv[2]); // Connecting with Mirror , message contains IP of Mirror
		// server = connectToServer(argv[1], buffer);
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			printf("Socket creation error\n");
			return 1;
		}
		// configuring address
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(8001);

		// Convert IPv4 and IPv6 addresses from text to binary form
		if (inet_pton(AF_INET, MIRROR_IP, &serv_addr.sin_addr) <= 0)
		{
			printf("Invalid address or Address not supported\n");
			return 1;
		}
		// connecting to mirror
		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		{
			printf("Connection failed\n");
			return 1;
		}
	}
	else
		printf("Connected to the server! \n");
	while (1)
	{

		printf("\nC$ ");
		memset(buffer, 0, sizeof(buffer));
		// waiting for input command
		fgets(buffer, 1024, stdin);
		// buffer[strcspn(buffer, "\n")] = 0; // remove newline character
		// memset(buffer, 0, sizeof(buffer));
		strcpy(valbuf, buffer);
		// validating command
		if (input_validation(valbuf) == -1)
		{
			continue;
		}

		// send command to server
		send(sock, buffer, strlen(buffer), 0);

		char *arguments[MAX_ARGUMENTS];
		int num_arguments = 0;

		// Parse the command received from client
		char *token = strtok(buffer, " "); // Tokenize command using space as delimiter

		while (token != NULL)
		{
			arguments[num_arguments++] = token; // Store the token in the array
			token = strtok(NULL, " ");			// Get the next token
		}
		arguments[num_arguments] = NULL; // Set the last element of the array to NULL

		// Remove line breaks from tokens
		remove_linebreak(arguments, num_arguments);

		char *buffcmd = arguments[0]; // Extract first token as the command
		memset(buffer, 0, sizeof(buffer));
		// Compare buffer with pattern
		if (strcmp(buffcmd, "gettargz") == 0 || strcmp(buffcmd, "getfiles") == 0 || strcmp(buffcmd, "dgetfiles") == 0 || strcmp(buffcmd, "sgetfiles") == 0)
		{
			printf("Buffer matches the pattern\n");

			// send(sock, filename, strlen(filename), 0);

			// Receive file from server
			FILE *fp = fopen(filename, "wb");
			if (fp == NULL)
			{
				printf("Error opening file");
				return -1;
			}

			// Get file size from server
			long file_size;
			// memset(file_size, 0, sizeof(file_size));
			recv(sock, &file_size, sizeof(file_size), 0);

			printf("File Size %ld\n", file_size);

			// Receive file data from server
			long total_bytes_received = 0;
			while (total_bytes_received < file_size)
			{
				int bytes_to_receive = BUFFER_SIZE;
				if (total_bytes_received + BUFFER_SIZE > file_size)
				{
					bytes_to_receive = file_size - total_bytes_received;
				}
				int bytes_received = recv(sock, buffer, bytes_to_receive, 0);
				if (bytes_received < 0)
				{
					printf("Error receiving file data");
					return -1;
				}
				fwrite(buffer, sizeof(char), bytes_received, fp);
				total_bytes_received += bytes_received;
				if (total_bytes_received >= file_size)
				{
					break;
				}
			}

			fclose(fp);

			printf("File received successfully\n");

			// Check if the last argument is "-u" to unzip the file
			if (strcmp(arguments[num_arguments - 1], "-u") == 0)
			{
				char cmd[BUFFER_SIZE];
				snprintf(cmd, BUFFER_SIZE, "tar -xzvf %s", filename);
				system(cmd);
				printf("File unzipped successfully\n");
			}

			memset(buffer, 0, sizeof(buffer));
			continue;
		}
		// Exit client if got quit
		else if (strcmp(buffcmd, "quit") == 0)
		{

			printf("Response: quit ");

			exit(0);
		}
		// else
		//{
		//	printf("Buffer does not match the file trasfer or unzip pattern so no need for file transfer\n");
		// }

		// receive response from server
		memset(buffer, 0, sizeof(buffer));
		valread = read(sock, buffer, 1024);

		if (valread > 0)
		{
			printf("Bytes Received: %d\n", valread);
			printf("%s\n", buffer);
		}
		else
		{
			printf("Server disconnected\n");
			break;
		}
		fflush(stdout);
	}

	close(sock);
	return 0;
} // closing main funtion
