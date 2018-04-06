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

// json string compare
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

int main() {
    // initial initialization
    char message[1000], JSON_STRING[2000];
    memset(JSON_STRING, '\0', sizeof(JSON_STRING));
    memset(message, '\0', sizeof(message));

    // socket setup
    unsigned short port;       /* port client will connect to         */
    struct hostent *hostnm;    /* server host name information        */
    struct sockaddr_in server; /* server address                      */
    int s;                     /* client socket                       */

    // manually specify for now
    // is only internal so should be *safe*
    hostnm = gethostbyname("128.113.17.41");
    if (hostnm == (struct hostent *)0) {
        fprintf(stderr, "Gethostbyname failed\n");
        exit(2);
    }
    port = 7171;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr =
        *((unsigned long *)hostnm
              ->h_addr); // linter gets mad as it doesnt understand the struct?

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket()");
        exit(3);
    }
    puts("socket");

    // old timeout stuff try to move to tcp timeout
    /* struct timeval tv; */
    /* tv.tv_sec = 10; */
    /* tv.tv_usec = 0; */
    /* setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv); */
    /* setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof tv); */

    // connect to server
    if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connect()");
        exit(4);
    }

    puts("Connected\n");

    // this can go on github right?
    // to gen another
    // use python for now
    //
    // from uuid import uuid4
    // device_id = str(uuid4())
    char *uuid = "585f9168-1b9a-4bee-9a78-89fb5714aca7";
    int aaaa = 0;
    if ((aaaa = send(s, uuid, 36, 0)) < 0) {
        perror("Send failed");
        return 1;
    }
    // keep communicating with server
    while (1) {
        int count;
        // Receive a reply from the server
    recv:
        printf("Listening\n");
        if ((count = recv(s, JSON_STRING, 2000, 0)) < 0) {
            printf("COUNT INSIDE%d\n", count);
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                break;
            }
            // old code to bail out on read timeout to check connection
            /* printf("errno: %d\n", errno); */
            /* printf("EAGAIN,%d EWOUDLBLOCK,%d\n", EAGAIN, EWOULDBLOCK); */
            /* perror("recv failed"); */
            /* int j; */
            /* errno = 0; */
            /* printf("SENDING\n"); */
            /* if ((j = send(s, uuid, 36, 0)) < 0) { */
            /*     perror("interwebs broke"); */
            /*     exit(-6); */
            /* } */
            /* printf("JJJJJJ: %d\n", j); */
            /* perror("SEND"); */
            /* printf("%d ERROR\n", errno); */
            /* int error_code; */
            /* int error_code_size = sizeof(error_code); */
            /* printf("%d CODE\n", getsockopt(s, SOL_SOCKET, SO_ERROR,
             * &error_code, */
            /*                                &error_code_size)); */
            /* perror("FUCK"); */
            /* printf("J: %d\n", j); */

            /* goto recv; */
        }

        puts(JSON_STRING);

        // check magic bytes
        if (JSON_STRING[0] != 0x2) {
            puts("bad start byte");
            exit(5);
        }
        if (JSON_STRING[count - 1] != 0x3) {
            puts("bad end byte");
            exit(5);
        }

        // remove magic bytes
        // probably can be cleaned up
        memcpy(JSON_STRING, &JSON_STRING[1], count - 1);
        JSON_STRING[count - 2] = 0;
        puts(JSON_STRING);

        // json setup
        int i;
        int r;
        jsmn_parser p;
        jsmntok_t t[128]; /* We expect no more than 128 tokens */

        jsmn_init(&p);
        r = jsmn_parse(&p, JSON_STRING, strlen(JSON_STRING), t,
                       sizeof(t) / sizeof(t[0]));
        if (r < 0) {
            printf("Failed to parse JSON: %d\n", r);
            return 1;
        }

        /* Assume the top-level element is an object */
        if (r < 1 || t[0].type != JSMN_OBJECT) {
            printf("Object expected\n");
            return 1;
        }

        /* Loop over all keys of the root object */
        for (i = 1; i < r; i++) {
            if (jsoneq(JSON_STRING, &t[i], "user") == 0) {
                /* We may use strndup() to fetch string value */
                printf("- User: %.*s\n", t[i + 1].end - t[i + 1].start,
                       JSON_STRING + t[i + 1].start);
                i++;
            } else if (jsoneq(JSON_STRING, &t[i], "message") == 0) {
                /* We may additionally check if the value is either "true" or
                 * "false" */
                printf("- message: %.*s\n", t[i + 1].end - t[i + 1].start,
                       JSON_STRING + t[i + 1].start);
                i++;
            } else if (jsoneq(JSON_STRING, &t[i], "status") == 0) {
                /* We may additionally check if the value is either "true" or
                 * "false" */
                printf("- status: %.*s\n", t[i + 1].end - t[i + 1].start,
                       JSON_STRING + t[i + 1].start);
                if (strncmp("ok", JSON_STRING + t[i + 1].start,
                            t[i + 1].end - t[i + 1].start) == 0) {
                } else {
                    dprintf(2, "%.*s\n", t[i + 1].end - t[i + 1].start,
                            JSON_STRING + t[i + 1].start);
                    printf("status not ok\n");
                    return -1;
                }
                i++;
            } else if (jsoneq(JSON_STRING, &t[i], "title") == 0) {
                /* We may want to do strtol() here to get numeric value */
                printf("- Title: %.*s\n", t[i + 1].end - t[i + 1].start,
                       JSON_STRING + t[i + 1].start);
                i++;
            } else if (jsoneq(JSON_STRING, &t[i], "groups") == 0) {
                int j;
                printf("- Groups:\n");
                if (t[i + 1].type != JSMN_ARRAY) {
                    continue; /* We expect groups to be an array of strings */
                }
                for (j = 0; j < t[i + 1].size; j++) {
                    jsmntok_t *g = &t[i + j + 2];
                    printf("  * %.*s\n", g->end - g->start,
                           JSON_STRING + g->start);
                }
                i += t[i + 1].size + 1;
            } else {
                printf("Unexpected key: %.*s\n", t[i].end - t[i].start,
                       JSON_STRING + t[i].start);
            }
        }
    }

    printf("closeing");
    close(s);
    return 0;
}
