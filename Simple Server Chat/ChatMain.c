
// This is Ido Nir's P2P shell chat.

enum error_codes { // Fill in this shit later.
	EXIT_SUCCESS,			// Finished successfully.
	SOCK_FAILED,			// Socket failed.
	BIND_FAILED,			// Bind failed.
	CONNECTION_FAILED,		// Connection to destination's server has failed.
	WSA_FAILED, 			// WSA module initialize failed.
	MEMORY_ALLOC_FAILED,	// Memory allocation has failed. 
	HOST_NAME_ERROR,		// Host name could not be retrieved.
	HOST_INFO_ERROR,		// Host information could not be retrieved.
	IPIFY_ERROR,			// Connection to ipify API has failed.
	LOCAL_SERVER_FAILED,	// Local server has failed.
	SEND_FAILED,			// 'Send' function has failed.
};

#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <windows.h>

#define USERNAME_MAX_LENGTH 20
#define IP_ADDR_BUFF_SIZE 16
#define MESSAGE_BUFF_MAX 1024 
#define TIME_SIZE 8
#define KEY NULL

#pragma comment(lib,"ws2_32.lib") // Winsock library
#pragma warning(disable:4996) // In order to use inet_addr() in Visual Studio, we must disable this warning.

// All structures:
typedef struct User
{
	// User will input a username that will be shows in-chat.
	char username[USERNAME_MAX_LENGTH];

	// The local user's server's address, which includes the address'
	// family (AF_INET in this case), IP address, and port number.
	struct sockaddr_in server_addr;

	SOCKET server_socket; // Is used only for creating connections. Is used only to create one connection at a time.
	SOCKET* active_sockets; // Array of sockets used for sending and recieving data to multiple peers.
	int num_clients; // The number of clients currently connected to local user.

	struct sockaddr_in* active_addresses; // A pointer to the list of addresses that the local user is connected to.

	// Key for future encryption feature.
	char* key;
}User;

typedef struct sockaddr_in sockaddr_in;

// Forward declaration of 'complex' functions: (And by complex I mean sh*tty functions that use other functions and make me declare them as pitty so I won't get errors)
void init_user(User* self);
void bind_error_code(int bind_code);
void init_sockaddr_in(struct sockaddr_in* sa_in, char* address, unsigned short port);
void get_time(char* buf, size_t buf_size);
char* get_username();
char* get_private_ip();
char* get_public_ip();
int connection_choice(char* ip, unsigned short port);

// All functions:
void init_WSA(WSADATA* wsaData) // Function that recieves a pointer to the WSADATA struct and initialises it. Exits on 4 if failed.
{
	// Initialize Windows Socket module
	printf("Initialising Windows Socket module: \n");
	if (WSAStartup(MAKEWORD(1, 1), wsaData) != 0)
	{
		printf("WSA Startup failed.\n");
		exit(4);
	}
	printf("WSA Initialised.\n\n");
}

void init_user(User* local)
{
	// Initialize the username to be the user's input.
	char* username = get_username();
	strcpy(local->username, username);
	free(username);

	// Create a 'sockaddr_in' for the servers address:
	char* listen_ip = get_private_ip();
	unsigned short accept_port = 0; // To choose a randomly available port.
	
	// Initialize the local server's address.
	init_sockaddr_in(&local->server_addr, listen_ip, accept_port);
															// Here we create the local server's address:
															// we input the init_sockaddr_in function a
															// pointer to the address we want to change 
															// (to change it by reference), the ipv4 address
															// that the local user wants other users to connect
															// to, and the port he wants to open for the accept
															// to happen.

	// Initialize Encryption key.
	local->key = KEY; // We copy the key given in the parameters field by reference.

	// Initialize client and server sockets.
	local->active_sockets = NULL;							// Here we initialize the sockets array to null. 
															// We'll later use those sockets to send and 
															// recieve data to other users. We'll create
															// sockets in this array using the 'invite' function,
															// or if invited- the 'accept' function will return a
															// new client socket.

	local->num_clients = 0;									// Here we initialize the size of the array of client sockets to 0 since its null
															// thus contains no items.

	local->active_addresses = NULL;							// Here we initialize the list of addresses that the
															// local user is connected to, to NULL - because when
															// the user is initialized, it's not connected to anyone.

	local->server_socket = socket(AF_INET, SOCK_STREAM, 0); // Here we initialize the socket that we use only 
															// to accept connections from other users.

	//	Socket error check:
	if (local->server_socket == -1)
	{
		printf("The server socket could not be created!\n");
		exit(SOCK_FAILED);
	}

	// Bind sockets
	int bind_server = bind(local->server_socket, (struct sockaddr*) &local->server_addr, sizeof(local->server_addr));

	// Bind error check:
	if (bind_server != 0)
	{
		printf("Bind of user's server has failed.\n");
		bind_error_code(bind_server);
		closesocket(local->server_socket);
		exit(BIND_FAILED);
	}
	
	int size = sizeof(local->server_addr);
	getsockname(local->server_socket, (struct sockaddr*)&local->server_addr, &size);
	printf("\nLocal Server is open! Your address is %s:%hu\n", inet_ntoa(local->server_addr.sin_addr), local->server_addr.sin_port);
	printf("To invite users out of your network, you must get invited by your public ip: %s. \n\n", get_public_ip());
}

int is_alnum_string(char* string) {
	while (*string) {
		if (!(('a' <= *string && *string <= 'z') || ('A' <= *string && *string <= 'Z') || ('0' <= *string && *string <= '9')))
			return FALSE;
		++string;
	}

	return TRUE;
}

char* get_username()
{
	char *username = (char*)calloc(USERNAME_MAX_LENGTH, sizeof(char));
	if (!username)
		exit(MEMORY_ALLOC_FAILED);

	printf("Please enter a username that is 3-%d characters long and alphanumeric: ", USERNAME_MAX_LENGTH);
	gets_s(username, USERNAME_MAX_LENGTH);

	while (strlen(username) < 3 || !is_alnum_string(username))
	{
		printf("Please enter a username again: ");
		gets_s(username, USERNAME_MAX_LENGTH);
	}

	return username;
}

void init_sockaddr_in(sockaddr_in* sa_in, char* ip, unsigned short port) // This function recieves a pointer to the 'sockaddr_in' instance, an IP address, and a port, and initialises the server's address.
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

	// We need to set the server's IP address in 'server_addr'. 
	sa_in->sin_addr.s_addr = inet_addr(ip);
	// The '.sin_addr', AKA the server's 'actual' address, could be different
	// types of IPV4 addresses- you can see the different types by clicking
	// on it; but this is irrelevant for now. For a typical connection,
	// we'll set it as '.s_addr'. We set the value of the address to the ip
	// by inputting the ip as a string into the function 'inet_addr', which
	// makes the string an unsigned long in the 'Network Byte Order', so
	// there's no need to use the 'htons' function.
}

int ipv4_validation(char* ip) // Function checks if the given ip address valid or not. Returns 1 for valid and 0 for invalid.
{
	if (inet_addr(ip) == INADDR_NONE)
	{
		printf("Error! Given IP address is invalid! \n");
		return FALSE;
	}

	return TRUE;
}

int port_validation(unsigned short port) // Function checks if given port is valid. If valid return TRUE, else return FALSE.
{
	if (port > 0 && port < 65536)
		return TRUE;

	return FALSE;
}

void to_invite(User* local) // When this function is called it asks the user for an IP address which he wants to invite
{
	sockaddr_in remote_server; // Remote server's address
	char remote_ip[IP_ADDR_BUFF_SIZE]; // Max Length of IPV4 address. This is the IP of the destination.
	unsigned short remote_port; // Port of Destination.

	// Recieve IP address of destination from user.
	printf("Insert an IP address you want to invite: ");
	gets_s(remote_ip, sizeof(remote_ip));
	
	// IP address error check:
	while (!ipv4_validation(remote_ip))
	{
		printf("\nIP is invalid! Please enter a VALID IP address: ");
		gets_s(remote_ip, sizeof(remote_ip));
	}

	// Recieve port of destination from user.
	do
	{
		printf("Insert the port of the user you want to invite: ");
		scanf_s("%hu", &remote_port);
	} while (!port_validation(remote_port));

	// Save fields into remote_server:
	init_sockaddr_in(&remote_server, remote_ip, remote_port);
	
	// Create a new socket at local->active_sockets
	++local->num_clients; // Increment the size of the client-sockets array by one.
	local->active_sockets = (SOCKET*)realloc(local->active_sockets, (local->num_clients) * sizeof(SOCKET)); // Change the pointer of the 'client_sockets' array to a new pointer which points to a block of memory with the new size of the array. The 'realloc' function also copies the values of the array to the new block of memory.

	// Memory allocation check:
	if (local->active_sockets == NULL)
	{
		printf("Error! Memory could not be reallocated!\n");
		exit(MEMORY_ALLOC_FAILED);
	}

	local->active_sockets[(local->num_clients - 1)] = socket(AF_INET, SOCK_STREAM, 0);
	printf("New socket created!\n");

	// Create connection to remote_server:
	if (connect(local->active_sockets[(local->num_clients - 1)], (struct sockaddr*)&remote_server, sizeof(remote_server)))
		printf("Connection to %s:%hu could not be created!\n", remote_ip, remote_port);
		
	else
		printf("Connection with %s:%hu was created!\n", remote_ip, remote_port);

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

void terminate_all_connections(User* local) // This function terminates all connection of a user by closing sockets.
{
	closesocket(local->server_socket);
	for (int i = 0; i < local->num_clients; i++)
		closesocket(local->active_sockets[i]);
}

void get_time(char* buff, size_t buff_size) {
	SYSTEMTIME time;
	
	GetLocalTime(&time);

	sprintf_s(buff, buff_size, "%02d:%02d", time.wHour, time.wMinute);
}

void help_menu()
{
	printf("\nIdo Nir's P2P help menu:\n\n");
}

void user_input(User* local)
{
	// Print hour and minute of message
	char time[TIME_SIZE];
	get_time(time, sizeof(time));
	
	// Scan for user's message
	printf("| %s | %s: ", time, local->username);
	char buff[MESSAGE_BUFF_MAX];
	gets_s( buff, MESSAGE_BUFF_MAX);
	
	if (!stricmp(buff, "/exit")) // To exit the chat.
	{
		WSACleanup(); // Close the Winsock module.
		exit(EXIT_SUCCESS);
	}
	
	if (!stricmp(buff, "/Invite")) // We use stricmp to compare strings while ignoring case.
		to_invite(local); // consider using "RegisterHotkey"

	if (!stricmp(buff, "/help")) // We use stricmp to compare strings while ignoring case.
		help_menu();
	
	else
		for (int i = 0; i < local->num_clients; i++) // Send to each remote user the message.
		{
			//if (local->active_sockets[i] == NULL) // If connection died or their are no connections active, continue to the next socket.
			//	continue;
			send(local->active_sockets[i], buff, strlen(buff), 0);
		}

}

char* get_private_ip()
{
	// 'Local' is the host
	char host_buffer[256];
	struct hostent* local_host_info;

	// Retrieve local host's name
	int local_host_name = gethostname(host_buffer, sizeof(host_buffer));

	// Host name error check:
	if (local_host_name == -1)
	{
		printf("Local host's name could not be retrieved.\n");
		exit(HOST_NAME_ERROR);
	}

	// Retrieve local host's info
	local_host_info = gethostbyname(host_buffer);

	// Local host info error check:
	if (local_host_info == NULL)
	{
		printf("Local host's information could not be retrieved. \n");
		exit(HOST_INFO_ERROR);
	}

	char *ip_str = inet_ntoa(*((struct in_addr*)local_host_info->h_addr_list[0]));

	return ip_str;
}

char* get_public_ip() // TODO: Not completed!
{


	return "0.0.0.0";
}

void introduction(User* local) // TODO: create the introduction fuction.
{	// This function makes an introduction, AKA multiple_handshake.
	// * To make things more clear, the remote client we need to make the introduction with is the last in the active_addresses and active_sockets array.

	// Step 1: Send the addresses to the remote client:

	char buff[MESSAGE_BUFF_MAX]; // The string that the list of addresses will be saved into.
	int index = 0; // An index to the last place that was written into 'buff'.

	// Add to buffer the addresses that the local client is connected to.
	for (int i = 0; i < (local->num_clients-1) ; i++) // The last client in the active_addresses array is the remote client we have just connected to, so we don't need to connect to it again.
	{
		// Add the IP address and port with a colon between them, and barriers between each address.
		snprintf(buff, "||%s:%hu", inet_ntoa(local->active_addresses[i].sin_addr), local->active_addresses[i].sin_port); 
	}
	snprintf(buff, "%c", '\0'); // Finish the buffer with an EOF.

	printf("The list of addresses sent to remote user: %s\n", buff); // For debugging.
	send(local->active_sockets[local->num_clients - 1], buff, (int)strlen(buff), 0); // Sending the buffer.

	// Step 2: The remote user connects to all addresses it hasn't connected to. Nothing actually happens in the local side.
	// Sweet nothing :)

	// Step 3: The local user recieves a list of addresses that the remote client is connected to.

	// Step 4: The local user makes connections to all addresses recieved to the remote client.
}

int connection_choice(char* ip, unsigned short port) // Function recieves the ip and port of the client trying to connect, asks user if accept or deny incoming connection. Returns TRUE if accept and FALSE if deny.
{
	printf("\nYou have an incoming connection from %s:%hu, do you want to accept connection? (y/n):\n", ip, port);
	char* choice[2];
	gets_s(choice, 2); 

	if (!stricmp(choice, "y")) // If user accepts connection:
		return TRUE;


	if (!stricmp(choice, "n")) // If user denies connection:
	{
		return FALSE; 
	}

	return connection_choice(ip, port); // Make it recursive just for the hell of it? :)
}

void insert_remote_client(User* local, SOCKET s, sockaddr_in remote_client)
{	// This function recieves the 'local' user and a socket 's' that was returned by the 'accept' function in the local server's thread.
	// The function re-allocates the array of active sockets in 'local' and inserts the new socket.
	// After that, the function re-allocates the array of active addresses in 'local' and inserts the address of the remote client we have just connected to.

	++(local->num_clients); // Increment the size of the client-sockets array by one.
	local->active_sockets = (SOCKET*)realloc(local->active_sockets, (local->num_clients) * sizeof(SOCKET)); // Change the pointer of the 'client_sockets' array to a new pointer which points to a block of memory with the new size of the array. The 'realloc' function also copies the values of the array to the new block of memory.
	// Memory allocation check:
	if (local->active_sockets == NULL)
	{
		printf("Error! Memory could not be reallocated!\n");
		exit(MEMORY_ALLOC_FAILED);
	}

	// We insert the new socket created by the 'accept' function to the last place of the 'client_sockets' array.
	local->active_sockets[local->num_clients - 1] = s;

	local->active_addresses = (SOCKET*)realloc(local->active_addresses, (local->num_clients) * sizeof(sockaddr_in)); // Change the pointer of the 'active_addresses' array to a new pointer which points to a block of memory with the new size of the array. The 'realloc' function also copies the addresses of the array to the new block of memory.
	// Memory allocation check:
	if (local->active_sockets == NULL)
	{
		printf("Error! Memory could not be reallocated!\n");
		exit(MEMORY_ALLOC_FAILED);
	}

	// We add the address of the remote client we have just connected to, to the list of active addresses in 'local'.
	local->active_addresses[local->num_clients - 1] = remote_client;

	// Function finishes successfully.
}

void server_thread(User* local)
{	// This function will run continuously, listening to incoming connections. If a remote client is trying to make a connection, the function will kick-off.

	if (listen(local->server_socket, 0) == SOCKET_ERROR)
	{
		printf("Listen function failed with error: %d\n", WSAGetLastError());
		exit(LOCAL_SERVER_FAILED);
	}

	struct sockaddr_in remote_client; // Address of the remote client.
	int size_of_sockaddr = sizeof(struct sockaddr_in); // the size fo the address, for the use of the 'getpeername' function.

	// We must save the socket that we use to connect to the user that we have just accepted.
	SOCKET temp_socket = accept(local->server_socket, (struct sockaddr*)&remote_client, &size_of_sockaddr);

	// Retrieve the address remote client which is trying to connect to local server, so the user could choose if to accept or deny connection.
	int error = getpeername(temp_socket, &remote_client, &size_of_sockaddr);
		// Error check for the use of getpeername function:
		if (error == SOCKET_ERROR)
		{
			printf("Remote client's address could not be retrieved.\n");
			exit(LOCAL_SERVER_FAILED);
		}

	char* remote_ip = inet_ntoa(remote_client.sin_addr); // Store the ip in a string;
	// We must accept the connection at the first place, and then if the user doesn't want to accept the connection we will close it.
	
	// If user DOES NOT want to connect to remote client:
	if (!connection_choice(remote_ip, remote_client.sin_port)) // If user chose to deny connection:
	{
		printf("Connection denied!\n");
		closesocket(temp_socket); // We don't need to use the new socket created by the local server's acceptance because the user doesn't want to communicate to the remote client.
	}

	// If user DOES want to connect to remote client:
	else
	{
		insert_remote_client(local, temp_socket, remote_client);
		printf("Connection with %s has been created successfully!\n", remote_ip);
		
		// Advance to the introduction:
		introduction(local);
	}
}

// Main function:
int main(int argc, char* argv[])
{
	printf("\nWelcome to Ido Nir's P2P Chat!\n\n");

	// Initiate Windows socket module:
	WSADATA wsaData;
	init_WSA(&wsaData);

	// Get user input to start the chat:
	User local;
	init_user(&local);

	printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n                       Chat started!\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	while (1)
		user_input(&local);

	return 0;
}