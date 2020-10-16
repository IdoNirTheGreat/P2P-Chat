
// This is Ido Nir's P2P shell chat.

// Main documentation:
//	Design comments :The program should start by opening the client's side. 
//	One user will start the chat and send an invitation to a certain IP(using the client side),
//	and the second user should choose to wait('listen()') for a connection(using the server side).
//	After the connection will be created, each user will choose a nickname for the chat screen.
//	After a connection will be created, the serverand the client will run simultaniously - The users
//	will send a message using the client side, and recieve messages using the server side.

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

enum error_codes { // Fill in this shit later.
	EXIT_SUCCESS,		// Finished successfully.
	SOCK_FAILED,		// Socket failed.
	BIND_FAILED,		// Bind failed.
	CONNECTION_FAILED,	// Connection to destination's server has failed.
	WSA_FAILED, 		// WSA module initialize failed.
	MEMORY_ALLOC_FAILED // Memory allocation has failed. 
};

#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <windows.h>

#define PORT_IN 6666
#define PORT_OUT 6667
#define IP_LOCAL "127.0.0.1"
#define IP_REMOTE "127.0.0.1"
#define NAME_LENGTH (20)
#define IP_ADDR_BUF_SIZE 16

#pragma comment(lib,"ws2_32.lib") // Winsock library
#pragma warning(disable:4996) // In order to use inet_addr() in Visual Studio, we must disable this warning.

// All structures:
typedef struct User
{
	// User will input a nickname that will be shows in-chat.
	char nickname[NAME_LENGTH];

	// The local user's server's address, which includes the address'
	// family (AF_INET in this case), IP address, and port number.
	struct sockaddr_in server;

	SOCKET server_socket; // Is used only for creating connections. Is used only to create one connection at a time.
	SOCKET* client_sockets; // Array of sockets used for sending and recieving data to multiple peers.
	int num_clients; // The number of clients currently connected to local user.

	// Key for future encryption feature.
	char* key;
}User;

typedef struct sockaddr_in sockaddr_in;

// Forward declaration of 'complex' functions: (And by complex I mean sh*tty functions that use other functions and make me declare them as pitty so I won't get errors)
void init_user(User* self, char* ip, int port_in, int port_out, char* key);
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

void init_user(User* local, char* self_ip, int accept_port, char* key)
{
	// Initialize the nickname to be the user's input.
	printf("Please enter a nickname that is up to %d characters: ", NAME_LENGTH);
	gets_s(local->nickname, NAME_LENGTH);

	// Initialize the user's end server 'sockaddr_in' instance 'self'.
	init_sockaddr_in(&(local->server), self_ip, accept_port);	// Here we create the local server's address:
																// We input the init_sockaddr_in function a
																// pointer to the address we want to change 
																// (to change it by reference), the IPV4 address
																// that the local user wants other users to connect
																// to, and the port he wants to open for the accept
																// to happen.

	// Initialize Encryption key.
	local->key = key; // We copy the key given in the parameters field by refference.

	// Initialize client and server sockets.
	local->client_sockets = NULL;							// Here we initialize the sockets array to null. 
															// We'll later use those sockets to send and 
															// recieve data to other users. We'll create
															// sockets in this array using the 'invite' function,
															// or if invited- the 'accept' function will return a
															// new client socket.

	local->num_clients = 0;									// Here we initialize the size of the array of client sockets to 0 since its null
															// thus contains no items.

	local->server_socket = socket(AF_INET, SOCK_STREAM, 0); // Here we initialize the socket that we use only 
															// to accept connections from other users.

	// If socket could not be created:
	if (local->server_socket == -1)
	{
		printf("The server socket could not be created!\n");
		exit(SOCK_FAILED);
	}

	// Bind sockets
	int bind_server = bind(local->server_socket, &(local->server), sizeof(local->server));

	// Bind error check:
	if (bind_server != 0)
	{
		printf("Bind of user's server has failed.\n");
		bind_error_code(bind_server);
		closesocket(local->server_socket);
		exit(BIND_FAILED);
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
	sockaddr_in remote_server;

	char remote_ip[IP_ADDR_BUF_SIZE]; // Max Length of IPV4 address. This is the IP of the destination.
	int remote_port; // Port of Destination.

	// Recieve IP address of destination from user.
	do
	{
		printf("Insert an IP address you want to invite: ");
		gets_s(&remote_ip, sizeof(remote_ip));
	} while (!ip_valid(remote_ip));

	// Recieve port of destination from user.
	do
	{
		printf("Insert the port of the user you want to invite: ");
		scanf_s("%d", &remote_port);
	} while (!port_valid(remote_port));

	init_sockaddr_in(&remote_server, remote_ip, remote_port);
	return remote_server;
}

void start_host(User* local)
{
	// Create a 'sockaddr_in' instance for the server of the destination.
	sockaddr_in remote = invite();

	// Connect the local client socket to the remote server socket.
	if (connect(local->client_socket, (struct sockaddr*)&remote, sizeof(remote)) != 0) {
		printf("Connection with the server of destination has failed!\n");
		exit(CONNECTION_FAILED);
	}
	printf("Connected to destination successfully.\n");
	// At this point, user->client_socket is connected to 'server_socket' at the destination.
}

void accept_new_connection()
{

}

void host(User* local)
{
	start_host(local);
}

void start_guest(User* local)
{
	SOCKET new_socket;
	struct sockaddr_in remote;
	int size_of_sockaddr = sizeof(struct sockaddr_in);
	if (listen(local->server_socket, 0) == SOCKET_ERROR)
	{
		printf("Listen function failed with error: %d\n", WSAGetLastError());
		return NULL; // should we return null?
	}

	// 'new_socket' is the socket that we use to connect to the user that we have just accepted.
	new_socket = accept(local->server_socket, (struct sockaddr*)&remote, &size_of_sockaddr);

	// TODO:  let the user choose if to accept or deny the connection.
	// choose_acceptance();

	// TODO: error check if the acceptance did not work.

	// Insert the new socket to the array of sockets inside the local user's struct, an increment the size of the array by 1.
	(local->num_clients)++; // Increment the size of the client-sockets array by one.
	local->client_sockets = (SOCKET*)realloc(local->client_sockets, (local->num_clients) * sizeof(SOCKET)); // Change the pointer of the 'client_sockets' array to a new pointer which points to a block of memory with the new size of the array. The 'realloc' function also copies the values of the array to the new block of memory.

	// Memory allocation check:
	if (local->client_sockets == NULL)
	{
		printf("Error! Memory could not be reallocated!\n");
		exit(MEMORY_ALLOC_FAILED);
	}

	// We insert the new socket created by the 'accept' function to the last place of the 'client_sockets' array.
	local->client_sockets[local->num_clients - 1] = new_socket;

	// TODO: make an introduction AKA multiple_handshake.
	//		 add a struct called 'known_peers', which will contain their nickname and their 'sockaddr_in'.
	//		when a new connection at one of the peers is created, it will distribute to all known peers the 
	//		
}

void guest(User* local)
{
	start_guest(local);
}

void terminate_connection(User* me) // This function terminates the connection of a user by closing sockets.
{
	closesocket(me->server_socket);
	// closesocket(me->client_socket); run a loop to close all client sockets.
}

// Main function:
int main(int argc, char* argv[])
{
	char choice = intro();

	WSADATA wsaData;
	init_WSA(&wsaData);

	User local;

	// Here we need to start running host and guest simultaneously


	return 0;
}