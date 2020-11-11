#pragma once

#include "chatlib.h"

// When this function is called it asks the user for an IP address which he wants to invite.
void invite(User* local)
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
	printf("Insert the port of the user you want to invite: ");
	scanf_s("%hu", &remote_port);

	while (!port_validation(remote_port))
	{
		printf("\nPort is invalid! Please enter a VALID port: ");
		scanf_s("%hu", &remote_port);
	}

	// Save values into 'remote_user':
	init_sockaddr_in(&remote_user, remote_ip, remote_port);

	// Create connection to remote user:
	connect_to_remote_user(local, remote_user);

}

// Main function of local client thread.
void local_client(User* local)
{
	while (local->server_flag == FALSE) // Runs constantly unless client thread is suspended or server is running.
	{
		// Print hour and minute of message
		char time[TIME_SIZE];
		get_time(time, sizeof(time));

		// Scan for user's message
		printf("| %s | %s: ", time, local->username);
		char buff[MESSAGE_BUFF_MAX];
		gets_s(buff, MESSAGE_BUFF_MAX);

		if (!stricmp(buff, "/exit")) // To exit the chat.
		{
			terminate_all_connections(local); // Close all connections.
			WSACleanup(); // Close the Winsock module.
			exit(EXIT_SUCCESS);
		}

		if (!stricmp(buff, "/Invite")) // We use stricmp to compare strings while ignoring case.
			invite(local);

		if (!stricmp(buff, "/help")) // We use stricmp to compare strings while ignoring case.
			help_menu();

		if (!stricmp(buff, "/active")) // We use stricmp to compare strings while ignoring case.
			print_connected_peers(local);

		else
			send_message(local, buff); // Function recieves local user, a buffer, and a set which contains array of sockets connected to remote users, and distributes the buffer to each remote client.
	}
}