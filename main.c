#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> /* for htonl */
#include <sys/socket.h>
#include <sys/un.h>

#define X_SOCKET_PATH "/tmp/.X11-unix/X0"

/* number of bytes needed to round x up to a multiple of four.*/
#define X_NET_PAD(x) (4 - (x % 4)) % 4

int main(void) {
    int sock;
    struct sockaddr_un sock_addr;

    uint16_t proto_major_ver = 11;
    uint16_t proto_minor_ver = 11;

    unsigned char auth_proto_name[] = "";
    size_t auth_proto_name_len = sizeof(auth_proto_name) - 1; /* without terminator */

    unsigned char auth_proto_data[] = "";
    size_t auth_proto_data_len = sizeof(auth_proto_data) - 1; /* without terminator */

    unsigned char * setup_req;
    size_t setup_req_len;
    size_t setup_req_offset;

    uint8_t setup_resp_success;
    /*
    unsigned char * setup_resp;
    size_t setup_resp_len;
    size_t setup_resp_offset;
    */

    ssize_t sent_len = -1;
    ssize_t recv_len = -1;

    if (strlen(X_SOCKET_PATH) > sizeof(sock_addr.sun_path)) {
        fputs("X_SOCKET_PATH too long\n", stderr);
        return 1;
    }

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sock_addr.sun_family = AF_UNIX;
    strcpy(sock_addr.sun_path, X_SOCKET_PATH);
    if (connect(sock, (struct sockaddr *)&sock_addr, sizeof(struct sockaddr_un)) != 0) {
        close(sock);
        perror("connect");
        return 1;
    }

    setup_req_len =
        12 \
        + auth_proto_name_len + X_NET_PAD(auth_proto_name_len) \
        + auth_proto_data_len + X_NET_PAD(auth_proto_data_len);
    setup_req = (unsigned char *)malloc(setup_req_len);
    if (setup_req == NULL) {
        close(sock);
        perror("malloc setup_req");
        return 1;
    }

    setup_req_offset = 0;
    if (htonl(0x10203040) == 0x10203040) {
        setup_req[setup_req_offset] = 0x42; /* MSB first */
    } else {
        setup_req[setup_req_offset] = 0x5c; /* LSB first */
    }
    setup_req_offset++;

    setup_req_offset++; /* unused */

    *(uint16_t *)(setup_req + setup_req_offset) = proto_major_ver;
    setup_req_offset += 2;

    *(uint16_t *)(setup_req + setup_req_offset) = proto_minor_ver;
    setup_req_offset += 2;

    *(uint16_t *)(setup_req + setup_req_offset) = auth_proto_name_len;
    setup_req_offset += 2;

    *(uint16_t *)(setup_req + setup_req_offset) = auth_proto_data_len;
    setup_req_offset += 2;

    setup_req_offset += 2; /* unused */

    memcpy((void *)(setup_req + setup_req_offset), (void *)auth_proto_name, auth_proto_name_len);
    setup_req_offset += auth_proto_name_len;

    setup_req_offset += X_NET_PAD(auth_proto_name_len); /* unused */

    memcpy((void *)(setup_req + setup_req_offset), (void *)auth_proto_data, auth_proto_data_len);
    setup_req_offset += auth_proto_data_len;

    setup_req_offset += X_NET_PAD(auth_proto_data_len); /* unused */

    sent_len = send(sock, (void *)setup_req, setup_req_len, 0);
    if (sent_len != setup_req_len) {
        close(sock);
        free(setup_req);
        perror("send setup_req");
        return 1;
    }

    free(setup_req);

    recv_len = recv(sock, (void *)(&setup_resp_success), 1, MSG_WAITALL);
    printf("recv_len: %li\n", recv_len);
    if (recv_len != 1) {
        close(sock);
        perror("recv setup_resp_success");
        return 1;
    }

    switch (setup_resp_success) {
    case 0: /* Failed */
        puts("Failed\n");
        break;
    case 1: /* Success */
        puts("Success\n");
        break;
    case 2: /* Authenticate */
        puts("Authenticate\n");
        break;
    default:
        fprintf(stderr, "Unexpected setup_resp_success: '%u'\n", setup_resp_success);
        close(sock);
        return 1;
        break;
    }

    close(sock);

    return 0;
}

