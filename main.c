#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <arpa/inet.h> /* for htonl */
#include <sys/socket.h>
#include <sys/un.h>

#define X_DEFAULT_COOKIE_FILE_NAME ".Xauthority"
#define X_SOCKET_PATH "/tmp/.X11-unix/X0"

/* number of bytes needed to round x up to a multiple of four.*/
#define X_NET_PAD(x) (4 - (x % 4)) % 4

int main(void) {
    struct passwd * pwd;
    char * cookie_file_path;
    FILE * cookie_file;

    int sock;
    struct sockaddr_un sock_addr;

    uint16_t proto_major_ver = 11;
    uint16_t proto_minor_ver = 0;

    unsigned char auth_proto_name[] = "MIT-MAGIC-COOKIE-1";
    size_t auth_proto_name_len = sizeof(auth_proto_name) - 1; /* without terminator */

    unsigned char * auth_proto_data;
    size_t auth_proto_data_len; /* without terminator */

    unsigned char * setup_req;
    size_t setup_req_len;
    size_t setup_req_offset;

    uint8_t setup_resp_success;
    unsigned char * setup_resp;
    size_t setup_resp_len;

    ssize_t sent_len = -1;
    ssize_t recv_len = -1;

    if (strlen(X_SOCKET_PATH) > sizeof(sock_addr.sun_path)) {
        fputs("X_SOCKET_PATH too long\n", stderr);
        return 1;
    }

    cookie_file_path = (char *)malloc(strlen(getenv("XAUTHORITY") + 1));
    strcpy(cookie_file_path, getenv("XAUTHORITY"));
    if (cookie_file_path == NULL) {
        pwd = getpwuid(getuid());
        if (pwd == NULL) {
            perror("getpwuid");
            return 1;
        }
        cookie_file_path = (char *)malloc(strlen(pwd->pw_dir) + strlen(X_DEFAULT_COOKIE_FILE_NAME) + 1);
        if (cookie_file_path == NULL) {
            perror("malloc cookie_file_path");
            return 1;
        }
        strcpy(cookie_file_path, pwd->pw_dir);
        cookie_file_path[strlen(pwd->pw_dir)] = '/';
        strcpy(cookie_file_path + strlen(pwd->pw_dir) + 1, X_DEFAULT_COOKIE_FILE_NAME);
    }

    if (access(cookie_file_path, R_OK) != 0) {
        free(cookie_file_path);
        fputs("no access to Xauthority\n", stderr);
        return 1;
    }
    cookie_file = fopen(cookie_file_path, "r");
    free(cookie_file_path);
    if (cookie_file == NULL) {
        perror("open");
        return 1;
    }

    fseek(cookie_file, 0, SEEK_END);
    auth_proto_data_len = 16;
    fseek(cookie_file, 0, SEEK_SET);
    auth_proto_data = (unsigned char *)malloc(auth_proto_data_len);
    if (auth_proto_data == NULL) {
        fclose(cookie_file);
        perror("malloc auth_proto_data");
        return 1;
    }
    if (fread(auth_proto_data, 1, auth_proto_data_len, cookie_file) != auth_proto_data_len) {
        fclose(cookie_file);
        free(auth_proto_data);
        fputs("fread wrong size for auth_proto_data\n", stderr);
        return 1;
    }
    fclose(cookie_file);

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sock_addr.sun_family = AF_UNIX;
    strcpy(sock_addr.sun_path, X_SOCKET_PATH);
    if (connect(sock, (struct sockaddr *)&sock_addr, sizeof(struct sockaddr_un)) != 0) {
        close(sock);
        free(auth_proto_data);
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
        free(auth_proto_data);
        perror("malloc setup_req");
        return 1;
    }

    setup_req_offset = 0;
    if (htonl(0x10203040) == 0x10203040) {
        setup_req[setup_req_offset] = 0x42; /* MSB first */
    } else {
        setup_req[setup_req_offset] = 0x6c; /* LSB first */
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
    printf("'%.*s'\n", (int)auth_proto_data_len, auth_proto_data);
    free(auth_proto_data);
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

    recv_len = recv(sock, (void *)&setup_resp_success, 1, 0);
    if (recv_len != 1) {
        close(sock);
        perror("recv setup_resp_success");
        return 1;
    }

    switch (setup_resp_success) {
    case 0: /* Failed */
        setup_resp = (unsigned char *)malloc(7);
        if (setup_resp == NULL) {
            close(sock);
            perror("malloc setup_resp in Failed (7)");
            return 1;
        }
        recv_len = recv(sock, (void *)setup_resp, 7, 0);
        if (recv_len != 7) {
            free(setup_resp);
            close(sock);
            perror("recv setup_resp in Failed (7)");
            return 1;
        }
        setup_resp_len = (*(uint16_t*)(setup_resp + 5)) * 4;
        free(setup_resp);
        setup_resp = (unsigned char *)malloc(setup_resp_len);
        if (setup_resp == NULL) {
            close(sock);
            perror("malloc setup_resp in Failed");
            return 1;
        }
        recv_len = recv(sock, (void *)setup_resp, setup_resp_len, 0);
        if (recv_len != setup_resp_len) {
            close(sock);
            free(setup_resp);
            perror("recv setup_resp in Failed");
            return 1;
        }

        printf("Failed: '%s'\n", (char*)(setup_resp));

        free(setup_resp);

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

