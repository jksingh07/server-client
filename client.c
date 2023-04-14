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
#define PORT 8000
#define MAX_ARGUMENTS 10
#define BUFFER_SIZE 1024

int validate_input(char *buffer)
{

	// tokenize input
	char *arguments[MAX_ARGUMENTS];
	int num_arguments = 0;

	// Parse the input commands
	char *token = strtok(buffer, " "); // Tokenize command using space as delimiter

	while (token != NULL)
	{
		arguments[num_arguments++] = token; // Store the token in the array
		token = strtok(NULL, " ");			// Get the next token
	}
	arguments[num_arguments] = NULL; // Set the last element of the array to NULL

	if (num_arguments < 1)
	{
		printf("Invalid command, try again\n");
		return 0;
	}
	// todo validate input here further

	// return 0 if any error

	return 1;
}

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

int main(int argc, char *argv[])
{
	int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char buffer[1024] = {0};
	char valbuf[1024];
	char server_ip[16];
	char *filename = "out.tar.gz";

	if (argc < 2)
	{
		printf("Usage: %s <server_ip>\n", argv[0]);
		return 1;
	}
	strcpy(server_ip, argv[1]);

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Socket creation error\n");
		return 1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
	{
		printf("Invalid address or Address not supported\n");
		return 1;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Connection failed\n");
		return 1;
	}

	printf("Connected to the server! \n");
	read(sock, buffer, 1024);
	printf("Message from server: %s \n\n", buffer);

	while (1)
	{
		printf("Enter a command: ");
		fgets(buffer, 1024, stdin);
		// buffer[strcspn(buffer, "\n")] = 0; // remove newline character

		strcpy(valbuf, buffer);
		if (!validate_input(valbuf))
			continue;

		// send command to server
		send(sock, buffer, strlen(buffer), 0);

		/*  **************************************************************** Tanmay File transfer Code **************************************************************** */
		/*  **********************************************************File Transfer, File Size adjusment, Loop breaking and Unzip File on "-u"  *****************************************************************/

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

		// Compare buffer with pattern
		if (strcmp(buffcmd, "gettargz") == 0 || strcmp(buffcmd, "getfiles") == 0 || strcmp(buffcmd, "dgetfiles") == 0 || strcmp(buffcmd, "sgetfiles") == 0)
		{
			printf("Buffer matches the pattern\n");

			send(sock, filename, strlen(filename), 0);

			// Receive file from server
			FILE *fp = fopen(filename, "wb");
			if (fp == NULL)
			{
				printf("Error opening file");
				return -1;
			}

			// Get file size from server
			long file_size;
			recv(sock, &file_size, sizeof(file_size), 0);

			printf(" File Size %ld\n", file_size);

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
		else
		{
			printf("Buffer does not match the pattern so no need for file transfer\n");
		}

		/*  **************************************************************** Code Modification Ends - tanmay  *****************************************************************/

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
	}

	close(sock);
	return 0;
}
