
// This is Ido Nir's P2P shell chat.

enum error_codes { // Fill in this shit later.
	EXIT_SUCCESS,			// Finished successfully.
	SOCK_FAILED,			// Socket failed.
	BIND_FAILED,			// Bind failed.
	CONNECTION_FAILED,		// Connection to remote user has failed.
	WSA_FAILED, 			// WSA module initialize failed.
	MEMORY_ALLOC_FAILED,	// Memory allocation has failed. 
	HOST_NAME_ERROR,		// Host name could not be retrieved.
	HOST_INFO_ERROR,		// Host information could not be retrieved.
	IPIFY_ERROR,			// Connection to ipify API has failed.
	LOCAL_SERVER_FAILED,	// Local server has failed.
	SEND_FAILED,			// 'Send' function has failed.
};

#include <stdio.h>
#include <Winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <windows.h>
#include <process.h>

#define USERNAME_MAX_LENGTH 20
#define IP_ADDR_BUFF_SIZE 16
#define MESSAGE_BUFF_MAX 1024 
#define TIME_SIZE 8
#define KEY NULL
#define IPIFY_URL "api.ipify.org"

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

	SOCKET server_socket; // Is used only for accepting connections. Is used only to create one connection at a time.
	SOCKET* active_sockets; // Array of sockets used for sending and recieving data to multiple peers.
	int amount_active; // The number of clients currently connected to local user.

	struct sockaddr_in* active_addresses; // A pointer to the list of addresses that the local user is connected to.

	int server_flag;	// A flag used to tell if server is now handling an incoming connection or not.
						// If a remote user is attempting connection to local server, the local client thread
						// will stop until it's done, and then resume the local client thread.
						// If server_flag equals FALSE, it means it's inactive.
						// If server flag equals TRUE, it's active.

	// Key for future encryption feature.
	char* key;
}User;


typedef struct sockaddr_in sockaddr_in;

// Forward declaration of 'complex' functions: (And by complex I mean sh*tty functions that use other functions and make me declare them as pitty so I won't get errors)
void init_user(User* self);
void print_bind_error(int bind_code);
void init_sockaddr_in(struct sockaddr_in* sa_in, char* address, unsigned short port);
void get_time(char* buf, size_t buf_size);
char* get_username();
char* get_private_ip();
char* get_public_ip();
int connection_choice(char* ip, unsigned short port);
void connect_to_remote_user(User* local, sockaddr_in remote_client);
void send_message(User* local, char* buff, fd_set* psend_set);
void print_socket_error();

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

	// Initialize Server flag to 0, meaning currently inactive.
	local->server_flag = FALSE;

	// Initialize client and server sockets.
	local->active_sockets = NULL;							// Here we initialize the sockets array to null. 
															// We'll later use those sockets to send and 
															// recieve data to other users. We'll create
															// sockets in this array using the 'invite' function,
															// or if invited- the 'accept' function will return a
															// new client socket.

	local->amount_active = 0;									// Here we initialize the size of the array of client sockets to 0 since its null
															// thus contains no items.

	local->active_addresses = NULL;							// Here we initialize the list of addresses that the
															// local user is connected to, to NULL - because when
															// the user is initialized, it's not connected to anyone.

	local->server_socket = socket(AF_INET, SOCK_STREAM, 0); // Here we initialize the socket that we use only 
															// to accept connections from other users.

	//	Socket error check:
	if (local->server_socket == INVALID_SOCKET)
	{
		printf("The server socket could not be created!\n");
		print_socket_error();
		exit(SOCK_FAILED);
	}

	// Bind sockets
	int bind_server = bind(local->server_socket, (struct sockaddr*) &local->server_addr, sizeof(local->server_addr));

	// Bind error check:
	if (bind_server != 0)
	{
		printf("Bind of user's server has failed.\n");
		print_bind_error(bind_server);
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

	sa_in->sin_family = AF_INET;		// We want our server to connect over an IPV4 Internet connection. 
										// The type of our address, AKA the address' family is AF_INET, 
										// which represents the IPV4 Internet connection. 

	sa_in->sin_port = htons(port);		// We set the port which we want the server to send data from. 
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
	sockaddr_in remote_user; // Remote server's address
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

	// Save values into 'remote_user':
	init_sockaddr_in(&remote_user, remote_ip, remote_port);

	// Create connection to remote user:
	connect_to_remote_user(local, remote_user);

	printf("Connection with %s:%hu was created!\n", remote_ip, remote_port);

}

void print_bind_error(int error_code) // The function recieves the error code from the 'bind()' function and prints the relevant error message. 
{
	printf("An error of a socket bind no. %d has occurred: ", error_code); // Print the error code so the user could know it.

	switch (error_code)
	{
		// The bind() function shall fail if:
	case EADDRINUSE:
		printf("The specified address is already in use.\n");
		break;

	case EADDRNOTAVAIL:
		printf("The specified address is not available from the local machine.\n");
		break;

	case EAFNOSUPPORT:
		printf("The specified address is not a valid address for the address family of the specified socket.\n");
		break;

	case EBADF:
		printf("The socket argument is not a valid file descriptor.\n");
		break;

	case EINVAL:
		printf("The socket is already bound to an address, and the protocol does not support binding to a new address; or the socket has been shut down.\n");
		break;

	case ENOTSOCK:
		printf("The socket argument does not refer to a socket.\n");
		break;

	case EOPNOTSUPP:
		printf("The socket type of the specified socket does not support binding to an address.\n");
		break;

	case EACCES:
		printf("The specified address is protectedand the current user does not have permission to bind to it.\n");
		break;

	case EISCONN:
		printf("The socket is already connected.");
		break;

	case ELOOP:
		printf("More than{ SYMLOOP_MAX } symbolic links were encountered during resolution of the pathname in address.\n");
		break;

	case ENAMETOOLONG:
		printf("Pathname resolution of a symbolic link produced an intermediate result whose length exceeds{ PATH_MAX }.\n");
		break;

	case ENOBUFS:
		printf("Insufficient resources were available to complete the call.\n");
		break;

		// If the address family of the socket is AF_UNIX, then bind() shall fail if:
	case EDESTADDRREQ:
		printf("The address argument is a null pointer.\n");
		break;

	case EISDIR:
		printf("The address argument is a null pointer.\n");
		break;

	case EIO:
		printf("An I / O error occurred.\n");
		break;

	case ENOENT:
		printf("A component of the pathname does not name an existing file or the pathname is an empty string.\n");
		break;

	case ENOTDIR:
		printf("A component of the path prefix of the pathname in address is not a directory.\n");
		break;

	case EROFS:
		printf("The name would reside on a read - only file system.");
		break;

	case 0: // In case no error has happened and the function was still called.
		printf("The bind was successful. No error has occurred\n");
		break;

	default:
		printf("An unknown error has occured.\n");
		break;
	}
}

void print_socket_error()
{ // Function prints out the socket error code meaning.

	int error_code = WSAGetLastError();

	printf("Socket error number %d has occurred: ", error_code); // Print error code number:

	// Print error meaning:
	switch (error_code)
	{
	case WSA_INVALID_HANDLE:
			printf("Specified event object handle is invalid. An application attempts to use an event object, but the specified handle is not valid.\n");
			break;
		
	case WSA_NOT_ENOUGH_MEMORY:
		printf("Insufficient memory available. An application used a Windows Sockets function that directly maps to a Windows function.The Windows function is indicating a lack of required memory resources.\n");
		break;

	case WSA_INVALID_PARAMETER:
		printf("One or more parameters are invalid.	An application used a Windows Sockets function which directly maps to a Windows function.The Windows function is indicating a problem with one or more parameters.\n");
		break;

	case WSA_OPERATION_ABORTED:
		printf("Overlapped operation aborted. An overlapped operation was canceled due to the closure of the socket, or the execution of the SIO_FLUSH command in WSAIoctl.\n");
		break;

	case WSA_IO_INCOMPLETE:
		printf("Overlapped I / O event object not in signaled state. The application has tried to determine the status of an overlapped operation which is not yet completed.Applications that use WSAGetOverlappedResult(with the fWait flag set to FALSE) in a polling mode to determine when an overlapped operation has completed, get this error code until the operation is complete.\n");
		break;
	
	case WSA_IO_PENDING:
		printf("Overlapped operations will complete later. The application has initiated an overlapped operation that cannot be completed immediately.A completion indication will be given later when the operation has been completed.\n");
		break;

	case WSAEINTR:
		printf("Interrupted function call. A blocking operation was interrupted by a call to WSACancelBlockingCall.\n");
		break;

	case WSAEBADF:
		printf("File handle is not valid. The file handle supplied is not valid.\n");
		break;

	case WSAEACCES:
		printf("Permission denied. An attempt was made to access a socket in a way forbidden by its access permissions.An example is using a broadcast address for sendto without broadcast permission being set using setsockopt(SO_BROADCAST). Another possible reason for the WSAEACCES error is that when the bind function is called(on Windows NT 4.0 with SP4 and later), another application, service, or kernel mode driver is bound to the same address with exclusive access.Such exclusive access is a new feature of Windows NT 4.0 with SP4 and later, and is implemented by using the SO_EXCLUSIVEADDRUSE option.\n");
		break;

	case WSAEFAULT:
		printf("Bad address. The system detected an invalid pointer address in attempting to use a pointer argument of a call.This error occurs if an application passes an invalid pointer value, or if the length of the buffer is too small.For instance, if the length of an argument, which is a sockaddr structure, is smaller than the sizeof(sockaddr).\n");
		break;

	case WSAEINVAL:
		printf("Invalid argument. Some invalid argument was supplied(for example, specifying an invalid level to the setsockopt function).In some instances, it also refers to the current state of the socket—for instance, calling accept on a socket that is not listening.\n");
		break;

	case WSAEMFILE:
		printf("Too many open files. Too many open sockets.Each implementation may have a maximum number of socket handles available, either globally, per process, or per thread.\n");
		break;

	case WSAEWOULDBLOCK:
		printf("Resource temporarily unavailable. This error is returned from operations on nonblocking sockets that cannot be completed immediately, for example recv when no data is queued to be read from the socket.It is a nonfatal error, and the operation should be retried later.It is normal for WSAEWOULDBLOCK to be reported as the result from calling connect on a nonblocking SOCK_STREAM socket, since some time must elapse for the connection to be established.\n");
		break;

	case WSAEINPROGRESS:
		printf("Operation now in progress. A blocking operation is currently executing.Windows Sockets only allows a single blocking operation—per - task or thread—to be outstanding, and if any other function call is made(whether or not it references that or any other socket) the function fails with the WSAEINPROGRESS error.\n");
		break;

	case WSAEALREADY:
		printf("Operation already in progress. An operation was attempted on a nonblocking socket with an operation already in progress—that is, calling connect a second time on a nonblocking socket that is already connecting, or canceling an asynchronous request(WSAAsyncGetXbyY) that has already been canceled or completed.\n");
		break;

	case WSAENOTSOCK:
		printf("Socket operation on nonsocket. An operation was attempted on something that is not a socket.Either the socket handle parameter did not reference a valid socket, or for select, a member of an fd_set was not valid.\n");
		break;

	case WSAEDESTADDRREQ:
		printf("Destination address required. A required address was omitted from an operation on a socket.For example, this error is returned if sendto is called with the remote address of ADDR_ANY.\n");
		break;

	case WSAEMSGSIZE:
		printf("Message too long. A message sent on a datagram socket was larger than the internal message buffer or some other network limit, or the buffer used to receive a datagram was smaller than the datagram itself.\n");
		break;

	case WSAEPROTOTYPE:
		printf("Protocol wrong type for socket.	A protocol was specified in the socket function call that does not support the semantics of the socket type requested. For example, the ARPA Internet UDP protocol cannot be specified with a socket type of SOCK_STREAM.\n");
		break;

	case WSAENOPROTOOPT:
		printf("Bad protocol option. An unknown, invalid or unsupported option or level was specified in a getsockopt or setsockopt call.\n");
		break;

	case WSAEPROTONOSUPPORT:
		printf("Protocol not supported.	The requested protocol has not been configured into the system, or no implementation for it exists.For example, a socket call requests a SOCK_DGRAM socket, but specifies a stream protocol.\n");
		break;

	case WSAESOCKTNOSUPPORT:
		printf("Socket type not supported. The support for the specified socket type does not exist in this address family.For example, the optional type SOCK_RAW might be selected in a socket call, and the implementation does not support SOCK_RAW sockets at all.\n");
		break;

	case WSAEOPNOTSUPP:
		printf("Operation not supported. The attempted operation is not supported for the type of object referenced.Usually this occurs when a socket descriptor to a socket that cannot support this operation is trying to accept a connection on a datagram socket.\n");
		break;

	case WSAEPFNOSUPPORT:
		printf("Protocol family not supported. The protocol family has not been configured into the system or no implementation for it exists.This message has a slightly different meaning from WSAEAFNOSUPPORT.However, it is interchangeable in most cases, and all Windows Sockets functions that return one of these messages also specify WSAEAFNOSUPPORT.\n");
		break;

	case WSAEAFNOSUPPORT:
		printf("Address family not supported by protocol family. An address incompatible with the requested protocol was used.All sockets are created with an associated address family(that is, AF_INET for Internet Protocols) and a generic protocol type(that is, SOCK_STREAM).This error is returned if an incorrect protocol is explicitly requested in the socket call, or if an address of the wrong family is used for a socket, for example, in sendto.\n");
		break;

	case WSAEADDRINUSE:
		printf("Address already in use.	Typically, only one usage of each socket address(protocol / IP address / port) is permitted.This error occurs if an application attempts to bind a socket to an IP address / port that has already been used for an existing socket, or a socket that was not closed properly, or one that is still in the process of closing.For server applications that need to bind multiple sockets to the same port number, consider using setsockopt(SO_REUSEADDR).Client applications usually need not call bind at all—connect chooses an unused port automatically.When bind is called with a wildcard address(involving ADDR_ANY), a WSAEADDRINUSE error could be delayed until the specific address is committed.This could happen with a call to another function later, including connect, listen, WSAConnect, or WSAJoinLeaf.\n");
		break;

	case WSAEADDRNOTAVAIL:
		printf("Cannot assign requested address. The requested address is not valid in its context.This normally results from an attempt to bind to an address that is not valid for the local computer.This can also result from connect, sendto, WSAConnect, WSAJoinLeaf, or WSASendTo when the remote address or port is not valid for a remote computer(for example, address or port 0).\n");
		break;

	case WSAENETDOWN:
		printf("Network is down. A socket operation encountered a dead network.This could indicate a serious failure of the network system(that is, the protocol stack that the Windows Sockets DLL runs over), the network interface, or the local network itself.\n");
		break;

	case WSAENETUNREACH:
		printf("Network is unreachable.	A socket operation was attempted to an unreachable network.This usually means the local software knows no route to reach the remote host.\n");
		break;

	case WSAENETRESET:
		printf("Network dropped connection on reset. The connection has been broken due to keep - alive activity detecting a failure while the operation was in progress.It can also be returned by setsockopt if an attempt is made to set SO_KEEPALIVE on a connection that has already failed.\n");
		break;

	case WSAECONNABORTED:
		printf("Software caused connection abort. An established connection was aborted by the software in your host computer, possibly due to a data transmission time - out or protocol error.\n");
		break;

	case WSAECONNRESET:
		printf("Connection reset by peer. An existing connection was forcibly closed by the remote host.This normally results if the peer application on the remote host is suddenly stopped, the host is rebooted, the host or remote network interface is disabled, or the remote host uses a hard close(see setsockopt for more information on the SO_LINGER option on the remote socket).This error may also result if a connection was broken due to keep - alive activity detecting a failure while one or more operations are in progress.Operations that were in progress fail with WSAENETRESET.Subsequent operations fail with WSAECONNRESET.\n");
		break;

	case WSAENOBUFS:
		printf("No buffer space available. An operation on a socket could not be performed because the system lacked sufficient buffer space or because a queue was full.\n");
		break;

	case WSAEISCONN:
		printf("Socket is already connected. A connect request was made on an already - connected socket.Some implementations also return this error if sendto is called on a connected SOCK_DGRAM socket(for SOCK_STREAM sockets, the to parameter in sendto is ignored) although other implementations treat this as a legal occurrence.\n");
		break;

	case WSAENOTCONN:
		printf("Socket is not connected. A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using sendto) no address was supplied.Any other type of operation might also return this error—for example, setsockopt setting SO_KEEPALIVE if the connection has been reset.\n");
		break;

	case WSAESHUTDOWN:
		printf("Cannot send after socket shutdown. A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call.By calling shutdown a partial close of a socket is requested, which is a signal that sending or receiving, or both have been discontinued.\n");
		break;

	case WSAETOOMANYREFS:
		printf("Too many references. Too many references to some kernel object.\n");
		break;

	case WSAETIMEDOUT:
		printf("Connection timed out. A connection attempt failed because the connected party did not properly respond after a period of time, or the established connection failed because the connected host has failed to respond.\n");
		break;

	case WSAECONNREFUSED:
		printf("Connection refused. No connection could be made because the target computer actively refused it.This usually results from trying to connect to a service that is inactive on the foreign host—that is, one with no server application running.\n");
		break;

	case WSAELOOP:
		printf("Cannot translate name. Cannot translate a name.\n");
		break;
	
	case WSAENAMETOOLONG:
		printf("Name too long. A name component or a name was too long.\n");
		break;

	case WSAEHOSTDOWN:
		printf("Host is down. A socket operation failed because the destination host is down.A socket operation encountered a dead host.Networking activity on the local host has not been initiated.These conditions are more likely to be indicated by the error WSAETIMEDOUT.\n");
		break;

	case WSAEHOSTUNREACH:
		printf("No route to host. A socket operation was attempted to an unreachable host.See WSAENETUNREACH.\n");
		break;

	case WSAENOTEMPTY:
		printf("Directory not empty. Cannot remove a directory that is not empty.\n");
		break;

	case WSAEPROCLIM:
		printf("Too many processes. A Windows Sockets implementation may have a limit on the number of applications that can use it simultaneously.WSAStartup may fail with this error if the limit has been reached.\n");
		break;

	case WSAEUSERS:
		printf("User quota exceeded. Ran out of user quota.\n");
		break;

	case WSAEDQUOT:
		printf("Disk quota exceeded. Ran out of disk quota.\n");
		break;

	case WSAESTALE:
		printf("Stale file handle reference. The file handle reference is no longer available.\n");
		break;

	case WSAEREMOTE:
		printf("Item is remote.	The item is not available locally.\n");
		break;

	case WSASYSNOTREADY:
		printf("Network subsystem is unavailable. This error is returned by WSAStartup if the Windows Sockets implementation cannot function at this time because the underlying system it uses to provide network services is currently unavailable.Users should check That the appropriate Windows Sockets DLL file is in the current path. That they are not trying to use more than one Windows Sockets implementation simultaneously.If there is more than one Winsock DLL on your system, be sure the first one in the path is appropriate for the network subsystem currently loaded. The Windows Sockets implementation documentation to be sure all necessary components are currently installedand configured correctly.\n");
		break;

	case WSAVERNOTSUPPORTED:
		printf("Winsock.dll version out of range. The current Windows Sockets implementation does not support the Windows Sockets specification version requested by the application.Check that no old Windows Sockets DLL files are being accessed.\n");
		break;

	case WSANOTINITIALISED:
		printf("Successful WSAStartup not yet performed. Either the application has not called WSAStartup or WSAStartup failed.The application may be accessing a socket that the current active task does not own(that is, trying to share a socket between tasks), or WSACleanup has been called too many times.\n");
		break;

	case WSAEDISCON:
		printf("Graceful shutdown in progress. Returned by WSARecv and WSARecvFrom to indicate that the remote party has initiated a graceful shutdown sequence.\n");
		break;

	case WSAENOMORE:
		printf("No more results. No more results can be returned by the WSALookupServiceNext function.\n");
		break;

	case WSAECANCELLED:
		printf("Call has been canceled. A call to the WSALookupServiceEnd function was made while this call was still processing.The call has been canceled.\n");
		break;

	case WSAEINVALIDPROCTABLE:
		printf("Procedure call table is invalid. The service provider procedure call table is invalid.A service provider returned a bogus procedure table to Ws2_32.dll.This is usually caused by one or more of the function pointers being NULL.\n");
		break;

	case WSAEINVALIDPROVIDER:
		printf("Service provider is invalid. The requested service provider is invalid.This error is returned by the WSCGetProviderInfo and WSCGetProviderInfo32 functions if the protocol entry specified could not be found.This error is also returned if the service provider returned a version number other than 2.0.\n");
		break;

	case WSAEPROVIDERFAILEDINIT:
		printf("Service provider failed to initialize. The requested service provider could not be loaded or initialized.This error is returned if either a service provider's DLL could not be loaded (LoadLibrary failed) or the provider's WSPStartup or NSPStartup function failed.\n");
		break;

	case WSASYSCALLFAILURE:
		printf("System call failure. A system call that should never fail has failed.This is a generic error code, returned under various conditions. Returned when a system call that should never fail does fail.For example, if a call to WaitForMultipleEvents fails or one of the registry functions fails trying to manipulate the protocol / namespace catalogs.	Returned when a provider does not return SUCCESS and does not provide an extended error code.Can indicate a service provider implementation error.\n");
		break;

	case WSASERVICE_NOT_FOUND:
		printf("Service not found. No such service is known.The service cannot be found in the specified name space.\n");
		break;

	case WSATYPE_NOT_FOUND:
		printf("Class type not found. The specified class was not found.\n");
		break;

	case WSA_E_NO_MORE:
		printf("No more results. No more results can be returned by the WSALookupServiceNext function.\n");
		break;

	case WSA_E_CANCELLED:
		printf("Call was canceled.	A call to the WSALookupServiceEnd function was made while this call was still processing.The call has been canceled.\n");
		break;

	case WSAEREFUSED:
		printf("Database query was refused.	A database query failed because it was actively refused.\n");
		break;

	case WSAHOST_NOT_FOUND:
		printf("Host not found.	No such host is known.The name is not an official host name or alias, or it cannot be found in the database(s) being queried.This error may also be returned for protocoland service queries, and means that the specified name could not be found in the relevant database.\n");
		break;

	case WSATRY_AGAIN:
		printf("Nonauthoritative host not found. This is usually a temporary error during host name resolution and means that the local server did not receive a response from an authoritative server.A retry at some time later may be successful.\n");
		break;

	case WSANO_RECOVERY:
		printf("This is a nonrecoverable error.	This indicates that some sort of nonrecoverable error occurred during a database lookup.This may be because the database files(for example, BSD - compatible HOSTS, SERVICES, or PROTOCOLS files) could not be found, or a DNS request was returned by the server with a severe error.\n");
		break;

	case WSANO_DATA:
		printf("Valid name, no data record of requested type. The requested name is validand was found in the database, but it does not have the correct associated data being resolved for.The usual example for this is a host name - to - address translation attempt(using gethostbyname or WSAAsyncGetHostByName) which uses the DNS(Domain Name Server).An MX record is returned but no A record—indicating the host itself exists, but is not directly reachable.\n");
		break;

	case WSA_QOS_RECEIVERS:
		printf("QoS receivers. At least one QoS reserve has arrived.\n");
		break;

	case WSA_QOS_SENDERS:
		printf("QoS senders. At least one QoS send path has arrived.\n");
		break;

	case WSA_QOS_NO_SENDERS:
		printf("No QoS senders.	There are no QoS senders.\n");
		break;

	case WSA_QOS_NO_RECEIVERS:
		printf("QoS no receivers. There are no QoS receivers.\n");
		break;

	case WSA_QOS_REQUEST_CONFIRMED:
		printf("QoS request confirmed. The QoS reserve request has been confirmed.\n");
		break;

	case WSA_QOS_ADMISSION_FAILURE:
		printf("QoS admission error. A QoS error occurred due to lack of resources.\n");
		break;

	case WSA_QOS_POLICY_FAILURE:
		printf("QoS policy failure.	The QoS request was rejected because the policy system couldn't allocate the requested resource within the existing policy.\n");
		break;

	case WSA_QOS_BAD_STYLE:
		printf("QoS bad style. An unknown or conflicting QoS style was encountered.\n");
		break;

	case WSA_QOS_BAD_OBJECT:
		printf("QoS bad object.	A problem was encountered with some part of the filterspec or the provider - specific buffer in general.\n");
		break;

	case WSA_QOS_TRAFFIC_CTRL_ERROR:
		printf("QoS traffic control error. An error with the underlying traffic control(TC) API as the generic QoS request was converted for local enforcement by the TC API.This could be due to an out of memory error or to an internal QoS provider error.\n");
		break;

	case WSA_QOS_GENERIC_ERROR:
		printf("QoS generic error. A general QoS error.\n");
		break;

	case WSA_QOS_ESERVICETYPE:
		printf("QoS service type error.	An invalid or unrecognized service type was found in the QoS flowspec.\n");
		break;

	case WSA_QOS_EFLOWSPEC:
		printf("QoS flowspec error.	An invalid or inconsistent flowspec was found in the QOS structure.\n");
		break;

	case WSA_QOS_EPROVSPECBUF:
		printf("Invalid QoS provider buffer. An invalid QoS provider - specific buffer.\n");
		break;

	case WSA_QOS_EFILTERSTYLE:
		printf("Invalid QoS filter style. An invalid QoS filter style was used.\n");
		break;

	case WSA_QOS_EFILTERTYPE:
		printf("Invalid QoS filter type. An invalid QoS filter type was used.\n");
		break;

	case WSA_QOS_EFILTERCOUNT:
		printf("Incorrect QoS filter count.	An incorrect number of QoS FILTERSPECs were specified in the FLOWDESCRIPTOR.\n");
		break;

	case WSA_QOS_EOBJLENGTH:
		printf("Invalid QoS object length. An object with an invalid ObjectLength field was specified in the QoS provider - specific buffer.\n");
		break;

	case WSA_QOS_EFLOWCOUNT:
		printf("Incorrect QoS flow count. An incorrect number of flow descriptors was specified in the QoS structure.\n");
		break;

	case WSA_QOS_EUNKOWNPSOBJ:
		printf("Unrecognized QoS object. An unrecognized object was found in the QoS provider - specific buffer.\n");
		break;

	case WSA_QOS_EPOLICYOBJ:
		printf("Invalid QoS policy object. An invalid policy object was found in the QoS provider - specific buffer.\n");
		break;

	case WSA_QOS_EPSFLOWSPEC:
		printf("Invalid QoS provider - specific flowspec. An invalid or inconsistent flowspec was found in the QoS provider - specific buffer.\n");
		break;

	case WSA_QOS_EPSFILTERSPEC:
		printf("Invalid QoS provider - specific filterspec.	An invalid FILTERSPEC was found in the QoS provider - specific buffer.\n");
		break;

	case WSA_QOS_ESDMODEOBJ:
		printf("Invalid QoS shape discard mode object. An invalid shape discard mode object was found in the QoS provider - specific buffer.\n");
		break;

	case WSA_QOS_ESHAPERATEOBJ:
		printf("Invalid QoS shaping rate object. An invalid shaping rate object was found in the QoS provider - specific buffer.\n");
		break;

	case WSA_QOS_RESERVED_PETYPE:
		printf("Reserved policy QoS element type. A reserved policy element was found in the QoS provider - specific buffer.\n");
	
	default:
		printf("An unknown socket error has occurred. ");
		break;
	}

}

void terminate_all_connections(User* local) // This function terminates all connection of a user by closing sockets.
{
	closesocket(local->server_socket);
	for (int i = 0; i < local->amount_active; i++)
		closesocket(local->active_sockets[i]);
	WSACleanup();
}

void get_time(char* buff, size_t buff_size) {
	SYSTEMTIME time;
	
	GetLocalTime(&time);

	sprintf_s(buff, buff_size, "%02d:%02d", time.wHour, time.wMinute);
}

void help_menu()
{
	printf("Help Menu:\n");
	printf("To send a message in the chat room, just type it!\n");
	printf("To invite someone, type '/invite'. Then, insert their IP address and port.\n");
	printf("Don't forget to insert the user's public IP if they're not in the same network as you.\n");
	printf("FYI, The chat currently supports only IPV4 addresses.\n");
	printf("To exit the chat, type '/exit'.\n\n");
}

void local_client(User* local, fd_set* psend_set)
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
		terminate_all_connections(local); // Close all connections.
		WSACleanup(); // Close the Winsock module.
		exit(EXIT_SUCCESS);
	}
	
	if (!stricmp(buff, "/Invite")) // We use stricmp to compare strings while ignoring case.
		to_invite(local);

	if (!stricmp(buff, "/help")) // We use stricmp to compare strings while ignoring case.
		help_menu();

	else
		send_message(local, buff, psend_set); // Function recieves local user, a buffer, and a set which contains array of sockets connected to remote users, and distributes the buffer to each remote client.

}

void send_message(User* local, char* buff, fd_set* psend_set)
{	// Function recieves local user and a buffer, and distributes the buffer to each remote client.

	char fbuff[MESSAGE_BUFF_MAX] = ""; // Final buffer that will be distributed.

	char time[TIME_SIZE];
	get_time(time, sizeof(time));

	// Final-buffer is made out of a time-stamp, username and original buffer:
	snprintf(fbuff, "| %s | %s: %s", time, local->username, buff);

	for (int i = 0; i < local->amount_active; i++) // Send to each remote user the message.
		if (FD_ISSET(local->active_sockets[i], psend_set)) // If socket isn't blocked
			send(local->active_sockets[i], fbuff, strlen(fbuff), 0); // Send message
		
}

void recieve_message(SOCKET sender)
{	// Function recieves the socket connected to user that sent a message, and prints the message.
	
	char buff[MESSAGE_BUFF_MAX] = ""; // The buffer used to recieve the incoming message
	recv(sender, buff, sizeof(buff), 0); // Recieve the incoming buffer into 'buff'.
	//decrypt(buff); // For when the decryption function will be implemented.
	printf("%s\n", buff);

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

char* get_public_ip()	// TODO: Not working yet, a solution for port mapping must be written so a machine inside
						//	the network could connect to a machine outside of the network.
{
	//// We'll use the Ipify API for this:
	//
	//// Retrieve Ipify IP:
	//struct hostent* ipify_info = gethostbyname(IPIFY_URL);
	//printf("Host name: %s\n", ipify_info->h_name);
	//printf("IP address: %s\n", inet_ntoa(*((struct in_addr*)ipify_info->h_addr)));
	//sockaddr_in ipify;
	//init_sockaddr_in(&ipify, ipify_info->h_addr, 443);

	//// Create the socket for the request:
	//SOCKET temp = socket(AF_INET, SOCK_STREAM, 0);

	//// Create connection:
	//if (!connect(temp, (struct sockaddr*)&ipify, sizeof(ipify)))
	//{
	//	printf("Connection with 'Ipify.org' has failed.\n");
	//	return "0.0.0.0";
	//}

	////// Send request:
	////char* request = "GET / HTTP/1.0\r\nHost: api.ipify.org \r\n User-Agent: " AGENT_NAME "\r\n\r\n";
	////send(temp, "GET / HTTP / 1.0\r\n", (int)strlen("GET / HTTP/1.0\r\n"), 0); // The request is an empty string.

	//// Recieve IP from Ipify server:
	//char public_ip[IP_ADDR_BUFF_SIZE] = "";
	//recv(temp, public_ip, IP_ADDR_BUFF_SIZE, 0);

	//// Close the socket to leave no open connections:
	//close(temp);

	//return public_ip;
	return "0.0.0.0";
}

void introduction(User* local) // TODO: create the introduction fuction.
{	// This function makes an introduction, AKA multiple_handshake.
	// * To make things more clear, the remote client we need to make the introduction with is the last in the active_addresses and active_sockets array.

	// Step 1: Send the addresses to the remote client:

	char buff[MESSAGE_BUFF_MAX]; // The string that the list of addresses will be saved into.
	int index = 0; // An index to the last place that was written into 'buff'.

	// Add to buffer the addresses that the local client is connected to.
	for (int i = 0; i < (local->amount_active-1) ; i++) // The last client in the active_addresses array is the remote client we have just connected to, so we don't need to connect to it again.
	{
		// Add the IP address and port with a colon between them, and padding between each address.
		snprintf(buff, "||%s:%hu", inet_ntoa(local->active_addresses[i].sin_addr), local->active_addresses[i].sin_port); 
	}
	snprintf(buff, "%c", '\0'); // Finish the buffer with an EOF.

	printf("The list of addresses sent to remote user: %s\n", buff); // For debugging.
	// encrypt(buff); TODO when the encryption function will be completed.
	send(local->active_sockets[local->amount_active - 1], buff, (int)strlen(buff), 0); // Sending the buffer.

	// Step 2: The remote user connects to all addresses it hasn't connected to. Nothing actually happens in the local side.
	// Sweet nothing :)

	// Step 3: The local user recieves a list of addresses that the remote client is connected to.
	*buff = ""; // Re-write 'buff'.
	recv(local->active_sockets[local->amount_active - 1], buff, (int)strlen(buff), 0); // Recieve the buffer. 
	// decrypt(buff); TODO when the decryption function will be completed.
	
	printf("The list of addresses the remote client has sent is: %s\n", buff); // For debugging.

	sockaddr_in* addresses = NULL; // A pointer to the list of addresses that the remote client has sent.
	int amount = 0; // The amount of addresses the remote client has sent.

	// The amount of addresses is determined by the amount of "||" paddings.
	for (int i = 0; buff[i] != '\0'; i++)
	{
		if (buff[i] == '|' && buff[i + 1] == '|') // If reached padding reallocate memory to handle one more address.
		{
			addresses = (sockaddr_in*)realloc(addresses, (++amount) * sizeof(sockaddr_in));
			// Memory allocation check:
			if (addresses == NULL)
			{
				printf("Error! Memory could not be reallocated!\n");
				exit(MEMORY_ALLOC_FAILED);
			}

		}
					
		char ip[IP_ADDR_BUFF_SIZE] = "";
		char port[5] = ""; // Port can be up to 5 digits.
		int j = 0;

		// Copy the values from the buffer into the string, while advancing i and j, until reached a colon.
		while (buff[i] != ':')
			ip[j++] = buff[i++]; 

		// Copy the values from the buffer into the string, while advancing i and j, until reached a padding.
		j = 0;
		while (buff[i + 1] != '|')
			port[j++] = buff[i++];

		init_sockaddr_in((addresses + amount - 1), ip, (unsigned short)atoi(port)); // Initialize the address with the given values.
	}

	// Step 4: The local user makes connections to all addresses recieved to the remote client.
	for (int i = 0; i < amount; i++)
		connect_to_remote_user(local, addresses[i]);

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

void insert_remote_user(User* local, SOCKET s, sockaddr_in remote_user)
{	// This function recieves the 'local' user and a socket 's' that was returned by the 'accept' function in the local server's thread.
	// The function re-allocates the array of active sockets in 'local' and inserts the new socket.
	// After that, the function re-allocates the array of active addresses in 'local' and inserts the address of the remote client we have just connected to.

	++(local->amount_active); // Increment the size of the client-sockets array by one.
	local->active_sockets = (SOCKET*)realloc(local->active_sockets, (local->amount_active) * sizeof(SOCKET)); // Change the pointer of the 'client_sockets' array to a new pointer which points to a block of memory with the new size of the array. The 'realloc' function also copies the values of the array to the new block of memory.
	// Memory allocation check:
	if (local->active_sockets == NULL)
	{
		printf("Error! Memory could not be reallocated!\n");
		exit(MEMORY_ALLOC_FAILED);
	}

	// We insert the new socket created by the 'accept' function to the last place of the 'client_sockets' array.
	local->active_sockets[local->amount_active - 1] = s;

	local->active_addresses = (sockaddr_in*)realloc(local->active_addresses, (local->amount_active) * sizeof(sockaddr_in)); // Change the pointer of the 'active_addresses' array to a new pointer which points to a block of memory with the new size of the array. The 'realloc' function also copies the addresses of the array to the new block of memory.
	// Memory allocation check:
	if (local->active_sockets == NULL)
	{
		printf("Error! Memory could not be reallocated!\n");
		exit(MEMORY_ALLOC_FAILED);
	}

	// We add the address of the remote client we have just connected to, to the list of active addresses in 'local'.
	local->active_addresses[local->amount_active - 1] = remote_user;

	// Function finishes successfully.
}

void connect_to_remote_user(User* local, sockaddr_in remote_user)
{ 
	// The function recieves the local user and an address of a remote user. The function creates a connection
	// to the remote user and inserts the new socket into 'local->active_sockets', the address of the remote user
	// into 'local->active_addresses.

	SOCKET temp = socket(AF_INET, SOCK_STREAM, 0); // A temporary socket to create the initial connection with the remote user.
	//	Socket error check:
	if (temp == INVALID_SOCKET)
	{
		printf("The server socket could not be created!\n");
		exit(SOCK_FAILED);
	}

	// Create the connection:
	if (!connect(temp, (struct sockaddr*)&remote_user, sizeof(remote_user)))
	{
		printf("Connection with %s:%hu has failed.\n", inet_ntoa(remote_user.sin_addr), remote_user.sin_port);
		exit(CONNECTION_FAILED);
	}

	// Insert the new socket and address of the connection to the remote user into local.
	insert_remote_user(local, temp, remote_user); 

}

void local_server(User* local)
{	// This function will be called if there's an incoming connection to the local server.
	// The function will be called by a new thread created when there's an incoming connection.
	
	struct sockaddr_in remote_client; // Address of the remote client.
	int size_of_sockaddr = sizeof(struct sockaddr_in); // the size fo the address, for the use of the 'getpeername' function.

	// We must save the socket that we use to connect to the user that we have just accepted.
	SOCKET temp_socket = accept(local->server_socket, (struct sockaddr*)&remote_client, &size_of_sockaddr);
	local->server_flag = TRUE; // Set the server flag to active.

	// Retrieve the address remote client which is trying to connect to local server, so the user could choose if to accept or deny connection.
	int error = getpeername(temp_socket, &remote_client, &size_of_sockaddr);
	// Error check for the use of getpeername function:
	if (error == SOCKET_ERROR)
	{
		printf("Remote client's address could not be retrieved.\n");
		exit(LOCAL_SERVER_FAILED);
	}

	char* remote_ip = inet_ntoa(remote_client.sin_addr); // Store the ip in a string;
	
	// We must accept the connection at the first place, and then if the user doesn't
	// want to accept the connection we will close it.

	// If user DOES NOT want to connect to remote client:
	if (!connection_choice(remote_ip, remote_client.sin_port)) // If user chose to deny connection:
	{
		printf("Connection denied!\n");
		closesocket(temp_socket); // We don't need to use the new socket created by the local server's acceptance because the user doesn't want to communicate to the remote client.
	}

	// If user DOES want to connect to remote client:
	else
	{
		insert_remote_user(local, temp_socket, remote_client);
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

	help_menu();
	printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n                       Chat started!\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

	// Create fd_sets:
	fd_set recieve_set;	// Will contain at the first place the local server's socket, then all client sockets (only to recieve data from them).
	fd_set send_set;	// Will contain all client sockets (only to send data to them).

	while (TRUE) // Main loop:
	{
		// Reset the sets at the beginning of every loop iteration:
		FD_ZERO(&recieve_set);
		FD_ZERO(&send_set);

		// Insert the server socket to 'recieve_set', so it will always listen for incoming connections.
		FD_SET(local.server_socket, &recieve_set);

		// Insert the client sockets to 'recieve_set', so it could recieve messages from the users connected to local user.
		for (int i = 0; i < local.amount_active; ++i)
			FD_SET(local.active_sockets[i], &recieve_set);

		// Implement the 'select' function in the error check:
		if (select(0, &recieve_set, &send_set, NULL, NULL) == SOCKET_ERROR)
			print_socket_error();

		// Check for incoming connections to the local server socket:
		if (FD_ISSET(local.server_socket, &recieve_set))
			local_server(&local); // Run the function that manages incoming connections

		// Check for incoming messages from any connected remote user:
		for (int i = 0; i < local.amount_active; ++i)
			if (FD_ISSET(local.active_sockets[i], &recieve_set)) // Returns true if there is a pending message recieved by the socket 'local.active_sockets[i]'.
				recieve_message(local.active_sockets[i]);

		// Check if remote users are ready to recieve messages from local user (the check is inside the function 'send_message'):
		local_client(&local, &send_set);

	}

	return 0;
}