#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>
#include <assert.h>

#include <tram_tracking.h>

// gcc tram_dashboard.c tram_tracking.c -I`pwd`

/* 
    The Tram data server (server.py) publishes messages over a custom protocol. 
    
    These messages are either:

    1. Tram Passenger Count updates (MSGTYPE=PASSENGER_COUNT)
    2. Tram Location updates (MSGTYPE=LOCATION)

    It publishes these messages over a continuous byte stream, over TCP.

    Each message begins with a 'MSGTYPE' content, and all messages are made up in the format of [CONTENT_LENGTH][CONTENT]:

    For example, a raw Location update message looks like this:

        7MSGTYPE8LOCATION7TRAM_ID7TRAMABC5VALUE4CITY

        The first byte, '7', is the length of the content 'MSGTYPE'. 
        After the last byte of 'MSGTYPE', you will find another byte, '8'.
        '8' is the length of the next content, 'LOCATION'. 
        After the last byte of 'LOCATION', you will find another byte, '7', the length of the next content 'TRAM_ID', and so on.

        Parsing the stream in this way will yield a message of:

        MSGTYPE => LOCATION
        TRAM_ID => TRAMABC
        VALUE => CITY

        Meaning, this is a location message that tells us TRAMABC is in the CITY.

        Once you encounter a content of 'MSGTYPE' again, this means we are in a new message, and finished parsing the current message

    The task is to read from the TCP socket, and display a realtime updating dashboard all trams (as you will get messages for multiple trams, indicated by TRAM_ID), their current location and passenger count.

    E.g:

        Tram 1:
            Location: Williams Street
            Passenger Count: 50

        Tram 2:
            Location: Flinders Street
            Passenger Count: 22

    To start the server to consume from, please install python, and run python3 server.py 8081

    Feel free to modify the code below, which already implements a TCP socket consumer and dumps the content to a string & byte array
*/

#define BUFFER_SIZE 256

void error(char* msg) {
    perror(msg);
    exit(1);
}

typedef enum {
    LOCATION,
    PASSENGER_COUNT,
    UNKNOWN
} msg_type;

typedef struct {
    char tram_id[BUFFER_SIZE];
    char value[BUFFER_SIZE];
    msg_type type;
} tram_info_update;

#define TRAM_INFO_INITIALIZER { .tram_id = {}, .value = {}, .type = UNKNOWN }

// Update location basing on `tram_info_update` struct
typedef void (*handle_info_update_t)(const tram_info_update* update);

void update_location(const tram_info_update* update) {
    tram_tracking_update_location(update->tram_id, update->value);
}

void update_passenger_count(const tram_info_update* update) {
    tram_tracking_update_passenger_count(update->tram_id, atoi(update->value));
}

static handle_info_update_t message_type_to_handle[] = {
    [LOCATION]        = update_location,
    [PASSENGER_COUNT] = update_passenger_count,
};

void tram_info_update_report(const tram_info_update* update_info) {
    message_type_to_handle[update_info->type](update_info);
};

typedef void (*set_info_update_value_t)(tram_info_update* to_update, const char* value, char value_len);

typedef struct {
    const char* key;
    set_info_update_value_t set_value;
} expected_key;

void set_msg_type(tram_info_update* to_update, const char* value, char value_len) {
    char tmp_buffer[BUFFER_SIZE];

    assert(value_len < BUFFER_SIZE);
    strncpy(tmp_buffer, value, value_len);
    tmp_buffer[value_len] = '\0';

    if (strcmp(tmp_buffer, "LOCATION") == 0) {
        to_update->type = LOCATION;
    } else if (strcmp(tmp_buffer, "PASSENGER_COUNT") == 0) {
        to_update->type = PASSENGER_COUNT;
    } else {
        error("Unknown message type");
    }
}

void set_tram_id(tram_info_update* to_update, const char* value, char value_len) {
    assert(value_len < BUFFER_SIZE);
    strncpy(to_update->tram_id, value, value_len);
    to_update->tram_id[value_len] = '\0';
}

void set_value(tram_info_update* to_update, const char* value, char value_len) {
    assert(value_len < BUFFER_SIZE);
    strncpy(to_update->value, value, value_len);
    to_update->value[value_len] = '\0';
}

static expected_key expected_keys[] = {
    {"MSGTYPE", set_msg_type}, 
    {"TRAM_ID", set_tram_id},
    {"VALUE", set_value},
    {NULL, NULL}
};

void set_key_value(const char* input, int* pos, expected_key* exp_key, tram_info_update* update_info) {
    int dest_pos = 0;
    int i = *pos;
    char input_key_len = input[i++];
    char input_value_len;
    int input_offset;

    for (int key_pos = 0; exp_key->key[key_pos] != '\0'; ++key_pos, ++i) {
        if (input[i] != exp_key->key[key_pos]) {
            error("Cannot read next key - unexpected key");
        }
    }
    if (i - *pos - 1 != input_key_len) {
        error("Cannot read next key - wrong length");
    }

    input_value_len = input[i++];
    // Discuss adding check for enough place in input for input_value_len
    // and custom check (like isdigit of all entries for passengers_count value)
    exp_key->set_value(update_info, input + i, input_value_len);

    *pos = i + input_value_len;
}

void report_update(const char* input) {
    tram_info_update update_info = TRAM_INFO_INITIALIZER;
    int pos = 0;
    int ret;
    
    for (expected_key* exp_key = expected_keys; exp_key->key != NULL; ++exp_key) {
        set_key_value(input, &pos, exp_key, &update_info);
    }

    tram_info_update_report(&update_info);
}

int main(int argc, char *argv[]){
	if(argc < 2) {
        fprintf(stderr,"No port provided\n");
        exit(1);
	}	
	int sockfd, portno, n;
	char buffer[BUFFER_SIZE];
	
	struct sockaddr_in serv_addr;
	struct hostent* server;
	
	portno = atoi(argv[1]); // check that variable represents valid port ???
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		error("Socket failed \n");
	}
	
	server = gethostbyname("127.0.0.1");
	if(server == NULL) {
		error("No such host\n");
	}
	
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char*) server->h_addr, (char*) &serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);
	
	if(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		error("Connection failed\n");

	tram_tracking_setup();

	while(1) {
		bzero(buffer, BUFFER_SIZE);
		n = read(sockfd, buffer, BUFFER_SIZE - 1);
		if(n < 0)
			error("Error reading from Server");
		report_update(buffer);
        tram_tracking_print_current_status();
	}
	
    tram_tracking_destroy();
	return 0;
}
