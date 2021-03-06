/*
	Sabin Raj Tiwari
	CMSC 621
	Project 3
*/

/* Include header files. */
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

/* Define the global constants. */
#define MAXCONNECTIONS 16
#define MAXDATASIZE 1024
#define MAXTHREADS 16

/* Import namespaces. */
using namespace std;

/* Structures for the program. */
struct server_data
{
	int fd, port;
    std::string host;
	struct sockaddr_in address, client;
};
struct client_data
{
    int fd;
    struct sockaddr_in address;
    server_data server;
};
struct lock_data
{
    int id, locked;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

/* Global variables. */
int used_threads = 0, message_count = 0;
int sfds[] = {-1, -1, -1};
int alive[] = {1, 1, 1};
pthread_t threads[MAXTHREADS];
lock_data lock;

/* Creates a lock_data object. */
lock_data construct_lock()
{
    lock_data data;
    data.locked = 0;
    pthread_cond_init(&data.cond, NULL);
    pthread_mutex_init(&data.lock, NULL);
    return data;
}

/* Get the string value from an int. */
std::string itos(int value)
{
	std::stringstream str;
	str << value;
	return str.str();
}

/* Get the string value for money. */
std::string mtos(double value)
{
	std::ostringstream moneystream;
	moneystream << fixed << std::setprecision(2) << value;
	return moneystream.str();
}

/* Get the string value from an double. */
std::string dtos(double value)
{
	std::stringstream str;
	str << value;
	return str.str();
}

/* Splits a string using a delimeter. */
template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) 
    {
        *(result++) = item;
    }
}

/* Uses split() to get the vector of string elements. */
std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

/* Method to handle the server backend requests. */
int vote_request(int fd, int index, string query)
{
    /* Get the socket data from the args. */
    int vote = -1;

    /* Send the vote request using the query. */
    int status = write(fd, query.c_str(), query.size());
    if(status < 0)
    {
        /* Set the server to failed. */
        cout << "Failed to write to the backend Server " << index << ".\n";
        alive[index] = 0;
    }
    else
    {
        /* If the write was successful, wait for the response. */
        int vote;
        status = read(fd, &vote, sizeof(int));
        if(status < 0)
        {
            /* Set the server to failed. */
            cout << "Failed to read from the backend Server " << index << ".\n";
            alive[index] = 0;
        }
        else
        {
            return vote;
        }
    }

    return -1;
}

/* Method to send the vote result and get the data. */
double commit_abort(int fd, int index, int commit)
{
    /* Send the commit command. */
    int status = write(fd, &commit, sizeof(int));
    if(status < 0)
    {
         /* Set the server to failed. */
        cout << "Failed to write to the backend Server " << index << ".\n";
        alive[index] = 0;
    }
    else
    {
        /* If the write was successful, wait for the response. */
        double response;
        status = read(fd, &response, sizeof(double));
        if(status < 0)
        {
            /* Set the server to failed. */
            cout << "Failed to read from the backend Server " << index << ".\n";
            alive[index] = 0;
        }
        else
        {
            return response;
        }
    }
    return -1;
}

/* Method that performs the two phase commit. */
double two_phase(string query)
{
    /* Start the threads for the backend servers. */
    int commit = 1;
    for(int i = 0; i < 3; i++)
    {
        if(alive[i] == 1)
        {
            int vote = vote_request(sfds[i], i, query);
            if(vote < 1) 
            {
                commit = 0;
            }  
        }
    }

    /* Send the commit or abort request using threads if votes was for commit. */
    double response = -1;
    for(int i = 0; i < 3; i++)
    {
        if(alive[i] == 1)
        {
            response = commit_abort(sfds[i], i, commit);
        }
    }

    return response;
}

/* Performs the function of creating an account. */
string create(string query)
{
    /* Perform the two phase commit and send response. */
    int response = two_phase(query);
    if(response > -1) 
    {
        return "OK " + itos(response);  
    }
    return "ERR Creating account";
}

/* Performs the function of updating an account. */
string update(string query)
{
    /* Perform the two phase commit and send response. */
    double response = two_phase(query);
    if(response > -1) 
    {
        return "OK " + mtos(response);  
    }
    return "ERR Account to update does not exist";
}

/* Performs the function of querying an account. */
string query(string query)
{
    /* Perform the two phase commit and send response. */
    double response = two_phase(query);
    if(response > -1) 
    {
        return "OK " + mtos(response);  
    }
    return "ERR Account to quert does not exist";
}

/* Thread to handle the client request. */
void *client_request(void *args)
{
    /* Get the socket data from the args. */
    client_data data = *((client_data*)args);

    /* Values to store the responses. */
    char buffer[MAXDATASIZE];
    string buffer_str, client_addr;
    bool end = false;
    int status;

    /* Loop that handles the client request. */
    while(!end)
    {
        /* Read in the request from the client. */
        client_addr = inet_ntoa(data.address.sin_addr);
        status = read(data.fd, &buffer, MAXDATASIZE);
        if(status < 0)
        {
            cerr << "Error receiving dat afrom the client.\n";
            continue;
        }

        /* Get the data from the buffer and store it in the string. */
        buffer[status] = '\0';
        buffer_str = buffer;
        cout << "Received data from the client: [" << buffer_str << "]\n";
        if(buffer_str == "") break;
		message_count++;

        /* Split the token from the client and perform the transaction. */
        vector<string> tokens = split(buffer_str, ':');
        /* Check the query and perform the respective function. */

        /* Create the lock point. */
        pthread_mutex_lock(&(lock.lock));
        if(lock.locked == 1)
        {
            /* Wait for the lock to release if there is one. */
		    pthread_cond_wait(&(lock.cond), &(lock.lock));
        }

        lock.locked = 1;
        if(tokens[0] == "CREATE") 
			buffer_str = create(buffer_str);
        else if(tokens[0] == "UPDATE") 
			buffer_str = update(buffer_str);
        else if(tokens[0] == "QUERY") 
			buffer_str = query(buffer_str);
        else
		{
			buffer_str = "OK";
			end = true;
		}

        /* Unlock the records after the transaction. */
        lock.locked = 0;
        pthread_cond_signal(&(lock.cond));
        pthread_mutex_unlock(&(lock.lock));

        /* Send the response to the client. */
        buffer_str = buffer_str + "\r\n";
        status = write(data.fd, buffer_str.c_str(), buffer_str.length());
        if(status < 0)
        {
            cerr << "Error writing data to client.\n";
        }
    }

    close(data.fd);
    used_threads--;
    cout << "Used threads (-): " + itos(used_threads) + "\n";
}

/* Connects to the backend server using the port. */
int connect_to_backend(string host, int port)
{
    /* Setup the connection information to the server. */
    struct hostent *server;
    server = gethostbyname(host.c_str());
    if(server == NULL)
    {
        cerr << "No host exists with the address: " << host << "\n";
        return -1;
    }

    /* Get the address to the server. */
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    bcopy((char *)server -> h_addr, (char *)&address.sin_addr.s_addr, server -> h_length);
	address.sin_port = htons(port);

    /* Setup the socket. */
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sfd < 0)
	{
		/* Show error if the socket descriptor fails. */
		cerr << "Socket is not formed.\n";
		return -1;
	}

	/* Try to connect to the server using the address that was created. */
	int c = connect(sfd, (struct sockaddr *) &address, sizeof(address));
	if(c < 0)
	{
		/* Show error and exit if the connection fails. */
		cerr << "Error connecting to the server. No connection could be made using the provided address and port.\n";
		return -1;
	}

    return sfd;
}

/* Main function login for the server program. */
int main(int argc, char **argv)
{
    if(argc < 4)
    {
        /* Show error if the correct number of arguments were not passed. */
		cerr << "Usage: server <port_number> <backend_host> <backend_port_1> <backend_port_2> <backend_port_3>\n";
		exit(1);
    }

    /* Connect to the backend servers. */
    server_data server;
    lock = construct_lock();
    for(int i = 3; i < argc; i++)
    {
        sfds[i - 3] = connect_to_backend(argv[2], atoi(argv[i]));
    }

    /* Create the stream socket for the server. */
    server.fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server.fd < 0)
    {
        cerr << "Error creating socket.\n";
        exit(1);
    }

    /* Declare the socket data and server address. */
    server.port = atoi(argv[1]);
    server.address.sin_family = AF_INET;
    server.address.sin_addr.s_addr = INADDR_ANY;
    server.address.sin_port = htons(server.port);

    /* Bind the server address to the listen file descriptor. */
    if(bind(server.fd, (struct sockaddr *)&(server.address), sizeof(server.address)) < 0)
    {
        cerr << "Error binding socket.\n";
        exit(1);
    }

    /* Listen for the incoming requests. */
    cout << "Server waiting for client requests...\n";
    listen(server.fd, MAXCONNECTIONS);
    while(1)
    {
        /* Accept a client request. */
        client_data client;
        client.server = server;
        socklen_t addrlen = sizeof(client.address);
        client.fd = accept(server.fd, (struct sockaddr *)&(client.address), &addrlen);
        if(client.fd < 0)
        {
            cerr << "Error accepting client request.\n";
            exit(1);
        }

        /* Create a thread call the client request function. */
        pthread_create(&threads[used_threads], NULL, client_request, &client);
        used_threads++;
        cout << "Used threads (+): " + itos(used_threads) + "\n";
    }

    return 0;
}