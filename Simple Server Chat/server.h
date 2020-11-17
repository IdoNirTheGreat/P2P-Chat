#pragma once

#include "chatlib.h"

// Function will be called if there's a connection attempt to the local server, by a new thread ('server_thread').
void local_server(New_Connection* connection)
{
	printf("\n\n* Invitation process has commenced: *\n\n");

	connection->local_user->server_flag = TRUE;	// Server is now handling a connection, so the server flag is set to TRUE.
												// Server flag state will return to FALSE after the connection is either denied,
												// or after the introduction has ended after connection was granted.

	// TODO: Remove comments before release:
	//// If user invited it's own server (if ip address of remote user whom invited is the same as the local user's ip address):
	//if ( (strcmp(inet_ntoa(connection->remote_user.sin_addr), inet_ntoa(connection->local_user->server_addr.sin_addr)) == 0) && (ntohs(connection->local_user->server_addr.sin_port) == ntohs(connection->remote_user.sin_port)))
	//{
	//	printf("You cannot invite yourself to a chat!\n");
	//	printf("\n* Invitation process has ended. *\n\n");
	//	
	//	// Server flag state is returned to FALSE:
	//	connection->local_user->server_flag = FALSE;

	//	return; // To end run of function here and not continue;
	//}

	// Any valid case of invitation:
	// We must accept the connection at the first place, and then if the user doesn't want to accept the connection we will close it.

	char popup_message[MESSAGE_BUFF_MAX] = { '\0' };
	sprintf(popup_message, "You have an incoming connection from %s:%hu, do you want to accept the connection? ", inet_ntoa(connection->remote_user.sin_addr), ntohs(connection->remote_user.sin_port));
	int choice = MessageBoxA(NULL, popup_message, "Ido Nir's P2P Chat", MB_YESNO);

	// If user wants to accept connection:
	if (choice == IDYES)
	{
		// Send acception:
		char accept[MESSAGE_BUFF_MAX] = "accepted";
		encrypt(accept);
		int error = send(connection->s, accept, sizeof(accept), 0);
		if (error == SOCKET_ERROR) // Error check to see if connection has died:
		{
			printf("Connection has died.\n");
			return;
		}

		// Recieve acknowledgent:
		char ack[MESSAGE_BUFF_MAX] = { '\0' };
		error = recv(connection->s, ack, sizeof(ack), 0);
		if (error == SOCKET_ERROR) // Error check to see if connection has died:
		{
			printf("Connection has died.\n");
			return;
		}
		decrypt(ack);

		if (strcmp(ack, "recieved") != 0)
		{
			printf("Handshake failed!\n");
			return;
		}

		printf("Handshake completed. Connection accepted.\n");

		// Insert remote user to the 'active addresses' array and the socket to 'active sockets' array.
		insert_remote_user(connection->local_user, connection->s, connection->remote_user);

		printf("\n* Invitation process has ended. *\n\n");

		// Advance to the introduction:
		introduction(connection->local_user, FALSE);

		// Server flag state is returned to FALSE:
		connection->local_user->server_flag = FALSE;
	}

	// If user deny connection:
	else if (choice == IDNO)
	{
		printf("\nConnection denied!\n");

		// Send denial:
		char deny[MESSAGE_BUFF_MAX] = "denied";
		encrypt(deny);
		int error = send(connection->s, deny, sizeof(deny), 0);
		if (error == SOCKET_ERROR) // Error check to see if connection has died:
		{
			printf("Connection has died.\n");
			return;
		}

		// Recieve acknowledgment:
		char ack[MESSAGE_BUFF_MAX] = { '\0' };
		error = recv(connection->s, ack, sizeof(ack), 0);
		if (error == SOCKET_ERROR) // Error check to see if connection has died:
		{
			printf("Connection has died.\n");
			return;
		}
		decrypt(ack);

		if (strcmp(ack, "recieved") != 0)
		{
			printf("Handshake failed! Remote user's acknowledgment message was corrupt.\n");
		}

		printf("Handshake completed. Connection denied.\n");

		closesocket(connection->s); // We don't need to use the new socket created by the local server's acceptance because the user doesn't want to communicate to the remote client.
		connection->local_user->server_flag = FALSE; // Server is not active anymore.

		printf("\n* Invitation process has ended. *\n\n");
	}

	// For any other input other than yes/no:
	else
	{
		printf("Input error! Connection denied.\n");
		printf("\n* Invitation process has ended. *\n\n");
	}
}
