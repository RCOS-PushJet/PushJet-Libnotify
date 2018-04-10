#include "./jsmn.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MSG_SIZE 2000
#define CON_PORT 7171

// json string compare
static int jsoneq(const char *json, jsmntok_t *tok, const char *str) {
    if (tok->type == JSMN_STRING && (int)strlen(str) == tok->end - tok->start &&
        strncmp(json + tok->start, str, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

int main() {
    // initial initialization
    char* message = (char*)calloc(MSG_SIZE, 1);
    char* JSON_STRING = (char*)calloc(MSG_SIZE, 1);

    // socket setup
    struct hostent *hostnm;    /* server host name information        */
    struct sockaddr_in server; /* server address                      */
    int sockfd;                     /* client socket                       */

    // manually specify for now
    // is only internal so should be *safe*
    hostnm = gethostbyname("128.113.17.41");
    if (hostnm == (struct hostent *)0) {
        fprintf(stderr, "Gethostbyname failed\n");
        exit(2);
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(CON_PORT);
    server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr); // linter gets mad as it doesnt understand the struct?

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket()");
        exit(3);
    }
    printf("Socket call sucessful!\n");
    printf("Socket file descriptor: %d\n", sockfd);
    char addrString[50];
    inet_ntop(AF_INET, &server.sin_addr, addrString, sizeof(struct sockaddr_in));
    printf("Preparing to connect to target address - %s:%d\n", addrString, ntohs(server.sin_port));

    // old timeout stuff try to move to tcp timeout
    /* struct timeval tv; */
    /* tv.tv_sec = 10; */
    /* tv.tv_usec = 0; */
    /* setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv); */
    /* setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof tv); */

    // connect to server
    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connect()");
        exit(4);
    }

    puts("Connection successful\n");

    // this can go on github right?
    // to gen another
    // use python for now
    //
    // from uuid import uuid4
    // device_id = str(uuid4())
    char *uuid = "585f9168-1b9a-4bee-9a78-89fb5714aca7";
    if (send(sockfd, uuid, 36, 0) < 0) {
        perror("Send failed");
        return 1;
    }

    // keep communicating with server
    while (1) {
        int bytes_recvd;
        // Receive a reply from the server
    recv:
        printf("Listening\n");
        if ((bytes_recvd = recv(sockfd, JSON_STRING, 2000, 0)) < 0) {
            printf("bytes_recvd INSIDE%d\n", bytes_recvd);
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                break;
            }
        }

        puts(JSON_STRING);

        // check magic bytes
        if (JSON_STRING[0] != 0x2) {
            puts("bad start byte");
            exit(5);
        }

        if (JSON_STRING[bytes_recvd - 1] != 0x3) {
            puts("bad end byte");
            exit(5);
        }

        // remove magic bytes
        // probably can be cleaned up
        JSON_STRING += 1;
        JSON_STRING[bytes_recvd - 2] = 0;
        puts(JSON_STRING);

        // json setup
        int index;
        int toks;
        jsmn_parser p;
        jsmntok_t t[128]; /* We expect no more than 128 tokens */

        jsmn_init(&p);
        toks = jsmn_parse(&p, JSON_STRING, strlen(JSON_STRING), t,
                       sizeof(t) / sizeof(t[0]));
        if (toks < 0) {
            printf("Failed to parse JSON: %d\n", toks);
            return 1;
        }

        /* Assume the top-level element is an object */
        if (toks < 1 || t[0].type != JSMN_OBJECT) {
            printf("Object expected\n");
            return 1;
        }

        /* Loop over all keys of the root object */
        for (index = 1; index < toks; index++) {
            if (jsoneq(JSON_STRING, &t[index], "user") == 0) {
                /* We may use strndup() to fetch string value */
                printf("- User: %.*s\n", t[index + 1].end - t[index + 1].start,
                       JSON_STRING + t[index + 1].start);
                index++;
            } else if (jsoneq(JSON_STRING, &t[index], "message") == 0) {
                /* We may additionally check if the value is either "true" or
                 * "false" */
                printf("- message: %.*s\n", t[index + 1].end - t[index + 1].start,
                       JSON_STRING + t[index + 1].start);
                index++;
            } else if (jsoneq(JSON_STRING, &t[index], "status") == 0) {
                /* We may additionally check if the value is either "true" or
                 * "false" */
                printf("- status: %.*s\n", t[index + 1].end - t[index + 1].start,
                       JSON_STRING + t[index + 1].start);
                if (strncmp("ok", JSON_STRING + t[index + 1].start,
                            t[index + 1].end - t[index + 1].start) == 0) {
                } else {
                    dprintf(2, "%.*s\n", t[index + 1].end - t[index + 1].start,
                            JSON_STRING + t[index + 1].start);
                    printf("status not ok\n");
                    return -1;
                }
                index++;
            } else if (jsoneq(JSON_STRING, &t[index], "title") == 0) {
                /* We may want to do strtol() here to get numeric value */
                printf("- Title: %.*s\n", t[index + 1].end - t[index + 1].start,
                       JSON_STRING + t[index + 1].start);
                index++;
            } else if (jsoneq(JSON_STRING, &t[index], "groups") == 0) {
                int j;
                printf("- Groups:\n");
                if (t[index + 1].type != JSMN_ARRAY) {
                    continue; /* We expect groups to be an array of strings */
                }
                for (j = 0; j < t[index + 1].size; j++) {
                    jsmntok_t *g = &t[index + j + 2];
                    printf("  * %.*s\n", g->end - g->start,
                           JSON_STRING + g->start);
                }
                index += t[index + 1].size + 1;
            } else {
                printf("Unexpected key: %.*s\n", t[index].end - t[index].start,
                       JSON_STRING + t[index].start);
            }
        }
    }

    printf("Exiting...\n");
    close(sockfd);
    return 0;
}
