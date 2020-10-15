
// This is Ido Nir's P2P shell chat.

//	Program rational:
//	Design comments :The program should start by opening the client's side. 
//	One user will start the chat and send an invitation to a certain IP(using the client side),
//	and the second user should choose to wait('listen()') for a connection(using the server side).
//	After the connection will be created, each user will choose a nickname for the chat screen.
//	After a connection will be created, the serverand the client will run simultaniously - The users
//	will send a message using the client side, and recieve messages using the server side.

enum error_codes { // Fill in this shit later.
	EXIT_SUCCESS,		// Finished successfully.
	SOCK_FAILED,		// Socket failed.
	BIND_FAILED,		// Bind failed.
	CONNECTION_FAILED,	// Connection to destination's server has failed.
	WSA_FAILED 			// WSA module initialize failed.
};
// Error code meaning for each value the program can return:
// Value:	0	| Finished successfully.
// Value:	1	| Socket failed.
// Value:	2	| Bind failed.
// Value:	3	| Connection to destination's server has failed.
// Value:	4	| WSA module initialize failed.
// Value:	5	| 

// Do Not mind this
// Value:	1	| Meaning: User called for help.
// Value:	2	| Meaning: IP address that user has input was invalid.
// Value:	3	| Meaning: Bind of this PC's server has failed.
// Value:	4	| Meaning: Connection to other PC's server has failed.

#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <windows.h>

#define PORT_IN 6666
#define PORT_OUT 6667
#define IP_SELF "127.0.0.1"
#define IP_DEST "127.0.0.1"

#define NAME_LENGTH (20)
#define IP_ADDR_BUF_SIZE 16

#pragma comment(lib,"ws2_32.lib") // Winsock library
#pragma warning(disable:4996) // In order to use inet_addr() in Visual Studio, we must disable this warning.

// All structures:
typedef struct user
{
	// User will input a nickname that will be shows in-chat.
	char nickname[NAME_LENGTH];

	// The user's Instance for the 'sockaddr_in' struct which includes the 
	// address' family (AF_INET in this case), IP address, and port number.
	struct sockaddr_in* client;
	struct sockaddr_in* server;

	// Create a TCP socket for client and server side:
	SOCKET server_socket;
	SOCKET client_socket;

	// Key for future encryption feature.
	char* key;
}user;
typedef struct sockaddr_in sockaddr_in;

// Forward declaration of 'complex' functions: (And by complex I mean sh*tty functions that use other functions and make me declare them as pitty so I won't get errors)
void init_user(user* self, char* ip, int port_in, int port_out, char* key);
void bind_error_code(int bind_code);
void init_sockaddr_in(struct sockaddr_in* sa_in, char* address, int port);

// All functions:
void init_WSA(WSADATA* wsaData) // Function that recieves a pointer to the WSADATA struct and initialises it. Exits on 4 if failed.
{
	// Initialize Windows Socket module
	printf("Initialising Windows Socket module: \n");
	if (WSAStartup(MAKEWORD(1, 1), wsaData) != 0)
	{
		printf(stderr, "WSA Startup failed.\n");
		exit(4);
	}
	printf("WSA Initialised.\n");
}

char intro()
{ 
	printf("Welcome to Ido Nir's P2P Secure Chat!\n");
	printf("Please enter 'h' to be the host, or 'g' to be the guest: ");
	char choice;
	do
	{
		scanf_s(" %c", &choice, 1);
	} while (choice != 'g' && choice != 'G' && choice != 'h' && choice != 'H');

	return choice;
}

void init_user(user* self, char* ip, int port_in, int port_out, char* key)
{
	// Initialize the nickname to be the user's input.
	printf("Please enter a nickname that is up to %d characters: ", NAME_LENGTH);
	gets_s(self->nickname, NAME_LENGTH);

	// Initialize the user's client end 'sockaddr_in' instance 'self'.
	init_sockaddr_in(self->client, ip, port_out);
	// Initialize the user's end server 'sockaddr_in' instance 'self'.
	init_sockaddr_in(self->server, ip, port_in);
	
	// Initialize Encryption key.
	self->key = key;

	// Initialize client and server sockets.
	self->client_socket= socket(AF_INET, SOCK_STREAM, 0);
	self->server_socket = socket(AF_INET, SOCK_STREAM, 0);

	// Sockets error check:
	if (self->client_socket == -1) // If socket could not be created:
	{
		printf("The client socket could not be created!\n");
		exit(SOCK_FAILED);
	}
	if (self->server_socket == -1) // If socket could not be created:
	{
		printf("The server socket could not be created!\n");
		exit(SOCK_FAILED);
	}

	// Bind sockets
	int bind_client = bind(self->client_socket, self->client, sizeof(self->client));
	int bind_server = bind(self->client_socket, self->server, sizeof(self->server));

	// Sockets error check:
	if (bind_client != 0)
	{
		printf("Bind of user's client has failed.\n");
		bind_error_code(bind_client);
		exit(2);

	}
	if (bind_server != 0)
	{
		printf("Bind of user's server has failed.\n");
		bind_error_code(bind_server);
		exit(2);

	}
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

void init_sockaddr_in(sockaddr_in* sa_in, char* address, int port) // This function recieves a pointer to the 'sockaddr_in' instance, an IP address, and a port, and initialises the server's address.
{
	
	sa_in->sin_family = AF_INET;	// We want our server to connect over an IPV4 Internet connection. 
										// The type of our address, AKA the address' family is AF_INET, 
										// which represents the IPV4 Internet connection. 

	sa_in->sin_port = htons(port); // We set the port which we want the server to send data from. 
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

int ip_valid(char* ip) // Function checks if the given ip address valid or not. Returns 1 for valid and 0 for invalid.
{
	if (inet_addr(ip) == -1)			
	{
		printf("Error! Given IP address is invalid! \n");
		return 0;
	}

	return 1;
}

int port_valid(int port) // Function checks if given port is valid. If valid return 1, else return 0.
{
	if (port >= 0 && port <= 65536)
		return 1;

	return 0;
}

sockaddr_in invite() // When this function is called it asks the user for an IP address which he wants to invite and returns it.
{
	sockaddr_in dest_server;
	
	char ip[IP_ADDR_BUF_SIZE]; // Max Length of IPV4 address. This is the IP of the destination.
	int port; // Port of Destination.

	// Recieve IP address of destination from user.
	do
	{
		printf("Insert an IP address you want to invite: ");
		gets_s(&ip, sizeof(ip));
	} while (!ip_valid(ip));

	// Recieve port of destination from user.
	do
	{
		printf("Insert the port of the user you want to invite: ");
		scanf_s("%d", &port);
	} while (!port_valid(port));

	init_sockaddr_in(&dest_server, ip, port);
	return dest_server;
}

void start_host(user* me)
{	
	// Create a 'sockaddr_in' instance for the server of the destination.
	sockaddr_in dest = invite();	

	// Connect the local client socket to the remote server socket.
	if (connect(me->client_socket, (struct sockaddr*) &dest, sizeof(dest)) != 0) {
		printf("Connection with the server of destination has failed!\n");
		exit(3);
	}
	printf("Connected to destination successfully.\n");
	
	// At this point, user->client_socket is connected to 'server_socket' at the destination.
}

SOCKET* start_guest(user* chat_user)
{
	SOCKET new_socket;
	struct sockaddr_in client;
	int size_of_sockaddr = sizeof(struct sockaddr_in);
	if (listen(chat_user->server_socket, 1) == SOCKET_ERROR)
        printf(L"listen function failed with error: %d\n", WSAGetLastError());
	new_socket = accept(chat_user->server_socket, (struct sockaddr *)&client, &size_of_sockaddr);
}

void terminate_connection(user* me) // This function terminates the connection of a user by closing sockets.
{
	closesocket(me->server_socket);
	closesocket(me->client_socket);
}

// Main function:
int main(int argc, char* argv[])
{
	char choice = intro();

	WSADATA wsaData; // Create an instance of the WSADATA struct.
	init_WSA(&wsaData); //  Initialize the WSADATA instance.

	user chat_user; // Create an instance for the user running the program.

	if (choice == 'h' || choice == 'H') // Start host side:
	{
		start_host();
	}

	else // Start guest side:
	{
		init_user(&chat_user, IP_SELF, PORT_IN, PORT_OUT, NULL);
		//start_guest();
	}


	return 0;
}