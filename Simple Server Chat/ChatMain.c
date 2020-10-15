
// This is Ido Nir's P2P shell chat.

//Program rational:
//	Design comments : The program should start by opening the client's side. 
//	One user will start the chat and send an invitation to a certain IP(using the client side),
//	and the second user should choose to wait('listen()') for a connection(using the server side).
//	After the connection will be created, each user will choose a nickname for the chat screen.
//	After a connection will be created, the serverand the client will run simultaniously - The users
//	will send a message using the client side, and recieve messages using the server side.

// Error code meaning for each value the program can return:
// Value:	0	| Meaning: finished successfully.
// Value:	1	| Meaning: User called for help.
// Value:	2	| Meaning: IP address that user has input was invalid.
// Value:	3	| Meaning: Bind of this PC's server has failed.
// Value:	4	| Meaning: Connection to other PC's server has failed.

#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>

#define PORT 6666
#define IP "127.0.0.1"

#pragma comment(lib,"ws2_32.lib") //winsock library
#pragma warning(disable:4996) // In order to use inet_addr() in Visual Studio, we must disable this warning.

char intro()
{ 
	printf("Welcome to Ido Nir's P2P Secure Chat!\n");
	printf("Please enter 'h' to be the hosst, or 'g' to be the guest: ");
	char choice;
	do
	{
		scanf_s("%c", &choice);
	} while (choice != 'g' && choice != 'G' && choice != 'h' && choice != 'H');

	return choice;
}

void bind_error_code(int bind_code) // The function recieves the error code from the 'bind()' function and prints the relevant error message. 
{
	printf("Error code is %d. ", bind_code); // Print the error code so the user could know it.

	switch (bind_code)
	{
		// The bind() function shall fail if:
	case EADDRINUSE:
		printf("The specified address is already in use.\n");

	case EADDRNOTAVAIL:
		printf("The specified address is not available from the local machine.\n");

	case EAFNOSUPPORT:
		printf("The specified address is not a valid address for the address family of the specified socket.\n");

	case EBADF:
		printf("The socket argument is not a valid file descriptor.\n");

	case EINVAL:
		printf("The socket is already bound to an address, and the protocol does not support binding to a new address; or the socket has been shut down.\n");

	case ENOTSOCK:
		printf("The socket argument does not refer to a socket.\n");

	case EOPNOTSUPP:
		printf("The socket type of the specified socket does not support binding to an address.\n");

	case EACCES:
		printf("The specified address is protectedand the current user does not have permission to bind to it.\n");

	case EISCONN:
		printf("The socket is already connected.");

	case ELOOP:
		printf("More than{ SYMLOOP_MAX } symbolic links were encountered during resolution of the pathname in address.\n");

	case ENAMETOOLONG:
		printf("Pathname resolution of a symbolic link produced an intermediate result whose length exceeds{ PATH_MAX }.\n");

	case ENOBUFS:
		printf("Insufficient resources were available to complete the call.\n");

		// If the address family of the socket is AF_UNIX, then bind() shall fail if:
	case EDESTADDRREQ:
		printf("The address argument is a null pointer.\n");

	case EISDIR:
		printf("The address argument is a null pointer.\n");

	case EIO:
		printf("An I / O error occurred.\n");

	case ENOENT:
		printf("A component of the pathname does not name an existing file or the pathname is an empty string.\n");

	case ENOTDIR:
		printf("A component of the path prefix of the pathname in address is not a directory.\n");

	case EROFS:
		printf("The name would reside on a read - only file system.");

	case 0: // In case no error has happened and the function was still called.
		printf("The bind was successful. No error has occurred\n");

	default:
		printf("An unknown error has occured.\n");
	}
}

void init_sockaddr(struct sockaddr_in* sa_in, char* address) // This function recieves a pointer to the 'sockaddr_in' instance and an IP address and initialises the sever's address.
{
	
	sa_in->sin_family = AF_INET;	// We want our server to connect over an IPV4 Internet connection. 
										// The type of our address, AKA the address' family is AF_INET, 
										// which represents the IPV4 Internet connection. 

	sa_in->sin_port = htons(PORT); // We set the port which we want the server to send data from. 
										// The function 'htons()': casts a short from Host to Network Byte Order-
										// Host-TO-Network-Short .The usual way that we read a number in binary,
										// which is that the first (most left) digit represents the biggest/most
										// significant value. This way to read a binary number is called 
										// 'Host Byte Order'/'Little-Endian Byte Order'. We use this function
										// to ensure the port is in Network Byte Order because that is the 
										// standard way to set a port value.

	char* ip = address;						// We need to set the ip address as a string.

	// We need to set the server's IP address in 'server_addr'. 
	sa_in->sin_addr.s_addr = inet_addr(address);
	// The '.sin_addr', AKA the server's 'actual' address, could be different
	// types of IPV4 addresses- you can see the different types by clicking
	// on it; but this is irrelevant for now. For a typical connection,
	// we'll set it as '.s_addr'. We set the value of the address to the ip
	// by inputting the ip as a string into the function 'inet_addr', which
	// makes the string an unsigned long in the 'Network Byte Order', so
	// there's no need to use the 'htons' function.
}

int main(int argc, char* argv[])
{
	// Help function for user:
	/*if (strcmp(argv[1], "help"))
	{
		help();
		return 1;
	}*/

	// Check if given IP address is valid:
	char* ip = IP; // After finishing testing, change to argv[1].
	if (inet_addr(ip) == -1)			// Error check to see if the given ip address is invalid.
	{
		printf("Error! Given IP address is invalid! \n");
		return 2;
	}

	// Initialize Windows Socket module
	printf("Initialising Windows Socket module: \n");
	WSADATA wsaData; // if this doesn’t work
	if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
	{
		printf(stderr, "WSA Startup failed.\n");
		return 1;
	}
	printf("WSA Initialised.\n");
	
	char side = intro();
	// User chose to be the host:
	if (side == 'h' || side == 'H') 
	{
		// Create a TCP socket for client and server side:
		SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
		SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);

		// Socket error check:
		if (server_socket == INVALID_SOCKET || client_socket == INVALID_SOCKET)
		{
			if (server_socket == INVALID_SOCKET)
				printf("Error! The socket 'server_socket' could not be created!\n");
			if (client_socket == INVALID_SOCKET)
				printf("Error! The socket 'client_socket' could not be created!\n");
			else
				printf("Error! An unknown error has occurred in the socket error check.\n");

			return 1;
		}
		else
			printf("Sockets created successfully.\n");

		// Define the server address' properties:
		struct sockaddr_in server_address;
		init_sockaddr(&server_address, ip); // Initialize 'server_address'.

		// Bind between the socket and the server, and then check for errors:
		int bind_code = bind(server_socket, &server_address, sizeof(server_address));
		if (bind_code != 0) // The 'bind()' function returns zero if the bind was successful.
		{
			printf("The bind of the socket and the server failed.\n");
			bind_error_code(bind_code);
			return 3;
		}

		// Define the client address' properties:
		struct sockaddr_in client_address;
		init_sockaddr(&client_address, ip); // Initialize 'client_address'.

		// Connect to server of other peer.
		if (connect(client_socket, &client_address, sizeof(client_address)) != 0)
		{
			printf("Connection with server of other user has failed.\n");
			return 4;
		}

		// Close all sockets.
		closesocket(server_socket);
		closesocket(client_socket);
	}

	// User chose to be the guest:
	if (side == 'g' || side == 'G')
	{

	}

	/*

	What else needs to be done:

	0. Recieve the IP address and the port which they want to communicate on. Probably should happen on the client side.
	1. Use the listen() function to wait for a connection to happen. Print a relevant message to let the user know it.
	2. Use the accept() function to accept the connection. I recommend to check if the IP trying to make the connection is authorized.
		Print a relevant message to let the user what's happenning.
	3. Recieve a message using the recieve() function, and print the message.
	4. Make the user choose whether to leave the chat, or send a message back.
	5. If the user chose to send a message back, call the client side to send back a message.
	6. Future option: create a .txt file that a log of all the actions that happened in the chat.

	*/

	return 0;
}