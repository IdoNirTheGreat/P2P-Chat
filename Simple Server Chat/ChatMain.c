
// This is Ido Nir's P2P shell chat.

enum error_codes {
	EXIT_SUCCESS,			// Finished successfully.
	SOCK_FAILED,			// Socket failed.
	BIND_FAILED,			// Bind failed.
	CONNECTION_FAILED,		// Connection to remote user has failed.
	WSA_FAILED, 			// WSA module initialize failed.
	MALLOC_FAILED,			// Memory allocation has failed. 
	HOST_NAME_ERROR,		// Host name could not be retrieved.
	HOST_INFO_ERROR,		// Host information could not be retrieved.
	IPIFY_ERROR,			// Connection to ipify API has failed.
	LOCAL_SERVER_FAILED,	// Local server has failed.
	SEND_FAILED,			// 'Send' function has failed.
	THREAD_FAILED,			// Thread creation has failed.
	UNKNOWN_ERROR,			// An unknown error has occurred.
};

// Import Libraries:
#include <process.h>
#include "chatlib.h"
#include "client.h"
#include "server.h"


// Ignore Warnings:
#pragma comment(lib,"ws2_32.lib")	// Winsock library
#pragma warning(disable:4996)		// In order to use inet_addr() in Visual Studio, we must disable this warning.

// Main function:
int main(int argc, char* argv[])
{
	// Enable use of escape codes:
	enable_vt();

	/*char* header[6];
	header[0] = " _____    _         _   _ _      _       _____  _____  _____   _____ _           _   ";
	header[1] = "|_   _|  | |       | \ | (_)    ( )     | ___ \/ __  \| ___ \ /  __ \ |         | |  ";
	header[2] = "  | |  __| | ___   |  \| |_ _ __|/ ___  | |_/ /`' / /'| |_/ / | /  \/ |__   __ _| |_ ";
	header[3] = "  | | / _` |/ _ \  | . ` | | '__| / __| |  __/   / /  |  __/  | |   | '_ \ / _` | __|";
	header[4] = " _| || (_| | (_) | | |\  | | |    \__ \ | |    ./ /___| |     | \__/\ | | | (_| | |_ ";
	header[5] = "    \___/\__,_|\___/  \_| \_/_|_|    |___/ \_|    \_____/\_|      \____/_| |_|\__,_|\__|";
                                                                                                                                                    
                                                                                                                                                    

	printf("%s\n%s\n%s\n%s\n%s\n%s\n", header[0], header[1], header[2], header[3], header[4], header[5]);*/
	
	printf("\nWelcome to Ido Nir's P2P Chat!\n\n");

	// Initiate Windows socket module:
	WSADATA wsaData;
	init_WSA(&wsaData);

	// Create local user:
	User* local = (User*)calloc(1, sizeof(User));
	if (local == NULL)
		exit(MALLOC_FAILED);
	init_user(local);
	
	help_menu();
	printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n                       Chat started!\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

	DWORD client_thread_id = 0;
	HANDLE client_thread = CreateThread(
		NULL,									// Default security attributes
		0,										// Use default stack size  
		(LPTHREAD_START_ROUTINE) local_client,	// Thread function name
		local,									// Argument to thread function 
		0,										// Create thread and run it instantly
		&client_thread_id						// Returns the thread identifier 
	);

	// Thread creation error check:
	if (client_thread == NULL)
		exit(THREAD_FAILED);

	// We must 'malloc' the variables to pass them as parameters to other threads.
	New_Connection* connection = (New_Connection*)calloc(1, sizeof(New_Connection));
	if (connection == NULL)
	{
		exit(MALLOC_FAILED);
	}
	connection->local_user = local;
			
	int sockaddr_in_size = sizeof(sockaddr_in);

	// Create fd_sets: 
	fd_set read_set; // Will contain at the first place the local server's socket, then all client sockets (only to recieve data from them).
	fd_set write_set;	// Will contain all client sockets (only to send data to them).

	while (TRUE) // Main loop:
	{
		// Reset the sets at the beginning of every iteration: 
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);

		// Insert existing sockets into the sets:
		FD_SET(local->server_socket, &read_set); // The server socket will be inserted first into 'read_set'.
		for (int i = 0; i < local->amount_active; i++) // Insert all other active sockets. 
		{
			FD_SET(local->active_sockets[i], &read_set);
			FD_SET(local->active_sockets[i], &write_set);
		} 
		
		// Call and error check the 'select' method: 
		if (select(0, &read_set, &write_set, NULL, NULL) == SOCKET_ERROR) 
		{
			print_socket_error();
			exit(CONNECTION_FAILED); 
		} 
		
		// Check for Incoming connections to local server: 
		if (FD_ISSET(local->server_socket, &read_set)) 
		{ 
			// If the server socket accepts a connection: 
			connection->s = accept(local->server_socket, (struct sockaddr*)&(connection->remote_user), &sockaddr_in_size);
			if (connection->s == SOCKET_ERROR) 
			{ 
				exit(SOCK_FAILED);
			} 
			
			else
			{ 
				// Terminate client:
				TerminateThread(client_thread, 0);

				// Call the local server: 
				local_server(connection);
				
				// Reset 'connection': 
				New_Connection* connection = (New_Connection*)calloc(1, sizeof(New_Connection));
				if (connection == NULL) 
				{ 
					exit(MALLOC_FAILED);
				} 

				connection->local_user = local;

				// Restart client:
				HANDLE client_thread = CreateThread(
					NULL,									// Default security attributes
					0,										// Use default stack size  
					(LPTHREAD_START_ROUTINE)local_client,	// Thread function name
					local,									// Argument to thread function 
					0,										// Create thread and run it instantly
					&client_thread_id						// Returns the thread identifier 
				);
			} 
		} 
		
		// Check for incoming messages: 
		for (int i = 0; i < local->amount_active; i++) 
		{
			if (FD_ISSET(local->active_sockets[i], &read_set)) // If socket is ready to be read from: 
			{ 
				recieve_message(local->active_sockets[i]);
			} 

		} 
	
	}

	return 0;
}
