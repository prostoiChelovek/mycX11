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

typedef uint32_t X_id;
typedef uint32_t X_Window;
typedef uint32_t X_Colormap;
typedef uint8_t X_Keycode;
typedef uint32_t X_Set_of_Event;

enum X_Bool {
    True,
    False
};

enum X_Visual_type_Class {
    X_VISUAL_CLASS_STATIC_GRAY,
    X_VISUAL_CLASS_STATIC_COLOR,
    X_VISUAL_CLASS_TRUE_COLOR,
    X_VISUAL_CLASS_GRAY_SCALE,
    X_VISUAL_CLASS_PSEUDO_COLOR,
    X_VISUAL_CLASS_DIRECT_COLOR
};

enum X_Backing_stores {
    X_BACKING_STORES_NEVER,
    X_BACKING_STORES_WHEN_MAPPED,
    X_BACKING_STORES_ALWAYS
};

enum X_Event {
    X_EVENT_KeyPress = 0x00000001,
    X_EVENT_KeyRelease = 0x00000002,
    X_EVENT_ButtonPress = 0x00000004,
    X_EVENT_ButtonRelease = 0x00000008,
    X_EVENT_EnterWindow = 0x00000010,
    X_EVENT_LeaveWindow = 0x00000020,
    X_EVENT_PointerMotion = 0x00000040,
    X_EVENT_PointerMotionHint = 0x00000080,
    X_EVENT_Button1Motion = 0x00000100,
    X_EVENT_Button2Motion = 0x00000200,
    X_EVENT_Button3Motion = 0x00000400,
    X_EVENT_Button4Motion = 0x00000800,
    X_EVENT_Button5Motion = 0x00001000,
    X_EVENT_ButtonMotion = 0x00002000,
    X_EVENT_KeymapState = 0x00004000,
    X_EVENT_Exposure = 0x00008000,
    X_EVENT_VisibilityChange = 0x00010000,
    X_EVENT_StructureNotify = 0x00020000,
    X_EVENT_ResizeRedirect = 0x00040000,
    X_EVENT_SubstructureNotify = 0x00080000,
    X_EVENT_SubstructureRedirect = 0x00100000,
    X_EVENT_FocusChange = 0x00200000,
    X_EVENT_PropertyChange = 0x00400000,
    X_EVENT_ColormapChange = 0x00800000,
    X_EVENT_OwnerGrabButton = 0x01000000
};

enum X_Window_Class {
    X_WIN_CLASS_INPUT_OUTPUT,
    X_WIN_CLASS_INPUT_ONLY,
    X_WIN_CLASS_COPY_FROM_PARENT
};

struct X_Visual_type {
    X_id visual_id;
    enum X_Visual_type_Class class;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint8_t bits_per_rgb_val;
    uint16_t colormap_entries;
};

struct X_Depth {
    uint8_t depth;

    size_t visuals_len;
    struct X_Visual_type * visuals;
};

struct X_Screen {
    X_Window root;
    uint16_t width_px;
    uint16_t height_px;
    uint16_t width_mm;
    uint16_t height_mm;

    size_t allowed_depths_len;
    struct X_Depth * allowed_depths;

    uint8_t root_depth;
    X_id root_visual;
    X_Colormap default_colormap;
    uint32_t white_px;
    uint32_t black_px;
    uint16_t min_installed_maps;
    uint16_t max_installed_maps;
    enum X_Backing_stores backing_stores;
    enum X_Bool save_unders;
    X_Set_of_Event current_input_masks;
};

enum X_Img_byte_order {
    X_IMG_BYTE_LSB_FIRST,
    X_IMG_BYTE_MSB_FIRST
};

enum X_Bitmap_bit_order {
    X_BITMAP_BIT_LEAST_SIGNIFICANT,
    X_BITMAP_BIT_MOST_SIGNIFICANT
};

struct X_Pixmap_format {
    uint8_t depth;
    uint8_t bits_per_px;
    uint8_t scanline_pad;
};

struct X_Setup_success_reply {
    size_t vendor_len;
    unsigned char * vendor;

    uint32_t release_num;
    X_id resource_id_base;
    X_id resource_id_mask;
    enum X_Img_byte_order img_byte_order;
    uint8_t bitmap_scanline_unit;
    uint8_t bitmap_scanline_pad;
    enum X_Bitmap_bit_order bitmap_bit_order;

    size_t pixmap_formats_len;
    struct X_Pixmap_format * pixmap_formats;

    size_t roots_len;
    struct X_Screen * roots;

    uint32_t motion_buffer_size;
    uint16_t max_req_len;
    X_Keycode min_keycode;
    X_Keycode max_keycode;
};

struct X {
    int sock;

    X_id root_wid;
    struct X_Visual_type root_visual;

    /*
    The resource_id_mask contains a single contiguous set of bits (at least 18).

    The client allocates resource IDs for types WINDOW, PIXMAP, CURSOR, FONT, GCONTEXT, and COLORMAP
    by choosing a value with only some subset of these bits set and ORing it with resource_id_base.
    Only values constructed in this way can be used to name newly created resources over this con-
    nection. Resource IDs never hav e the top three bits set. The client is not restricted to linear or
    contiguous allocation of resource IDs. Once an ID has been freed, it can be reused. An ID must
    be unique with respect to the IDs of all other resources, not just other resources of the same type.
    However, note that the value spaces of resource identifiers, atoms, visualids, and keysyms are dis-
    tinguished by context, and as such, are not required to be disjoint; for example, a given numeric
    value might be both a valid window ID, a valid atom, and a valid keysym.
    */
    X_id resource_id_base;
    X_id resource_id_mask;

    X_id allocated_ids_num;
};

void X_destroy(struct X * x) {
    if (x == NULL) {
        return;
    }

    close(x->sock);

    free(x);
}

struct X * make_X() {
    /* TODO: report error reasons; do not print anything */ 

    size_t i;
    size_t j;
    size_t k;

    int sock;
    struct sockaddr_un sock_addr;

    uint16_t proto_major_ver = 11;
    uint16_t proto_minor_ver = 0;

    unsigned char auth_proto_name[] = "MIT-MAGIC-COOKIE-1";
    size_t auth_proto_name_len = sizeof(auth_proto_name) - 1; /* without terminator */

    unsigned char auth_proto_data[] = "";
    size_t auth_proto_data_len = sizeof(auth_proto_data) - 1; /* without terminator */

    unsigned char * setup_req;
    size_t setup_req_len;
    size_t setup_req_offset;

    uint8_t setup_resp_success;
    unsigned char * setup_resp;
    unsigned char * setup_resp_field;
    size_t setup_resp_len;

    struct X_Setup_success_reply setup_reply;

    ssize_t sent_len = -1;
    ssize_t recv_len = -1;

    struct X_Screen * root;
    struct X_Depth * root_allowed_depth;
    struct X_Visual_type * root_visual;

    struct X * x;

    if (strlen(X_SOCKET_PATH) > sizeof(sock_addr.sun_path)) {
        fputs("X_SOCKET_PATH too long\n", stderr);
        return NULL;
    }

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return NULL;
    }

    sock_addr.sun_family = AF_UNIX;
    strcpy(sock_addr.sun_path, X_SOCKET_PATH);
    if (connect(sock, (struct sockaddr *)&sock_addr, sizeof(struct sockaddr_un)) != 0) {
        close(sock);
        perror("connect");
        return NULL;
    }

    setup_req_len =
        12 \
        + auth_proto_name_len + X_NET_PAD(auth_proto_name_len) \
        + auth_proto_data_len + X_NET_PAD(auth_proto_data_len);
    setup_req = (unsigned char *)malloc(setup_req_len);
    if (setup_req == NULL) {
        close(sock);
        perror("malloc setup_req");
        return NULL;
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
    setup_req_offset += auth_proto_data_len;

    setup_req_offset += X_NET_PAD(auth_proto_data_len); /* unused */

    sent_len = send(sock, (void *)setup_req, setup_req_len, 0);
    if (sent_len != setup_req_len) {
        close(sock);
        free(setup_req);
        perror("send setup_req");
        return NULL;
    }

    free(setup_req);

    recv_len = recv(sock, (void *)&setup_resp_success, 1, 0);
    if (recv_len != 1) {
        close(sock);
        perror("recv setup_resp_success");
        return NULL;
    }

    switch (setup_resp_success) {
    case 1: /* Success */
        break;
    case 0: /* Failed */
        setup_resp = (unsigned char *)malloc(7);
        if (setup_resp == NULL) {
            close(sock);
            perror("malloc setup_resp in Failed (7)");
            return NULL;
        }
        recv_len = recv(sock, (void *)setup_resp, 7, 0);
        if (recv_len != 7) {
            free(setup_resp);
            close(sock);
            perror("recv setup_resp in Failed (7)");
            return NULL;
        }
        setup_resp_len = (*(uint16_t*)(setup_resp + 5)) * 4;
        free(setup_resp);
        setup_resp = (unsigned char *)malloc(setup_resp_len);
        if (setup_resp == NULL) {
            close(sock);
            perror("malloc setup_resp in Failed");
            return NULL;
        }
        recv_len = recv(sock, (void *)setup_resp, setup_resp_len, 0);
        if (recv_len != setup_resp_len) {
            close(sock);
            free(setup_resp);
            perror("recv setup_resp in Failed");
            return NULL;
        }

        printf("Failed: '%s'\n", (char*)(setup_resp));

        free(setup_resp);

        close(sock);
        return NULL;
    case 2: /* Authenticate */
        puts("Authenticate\n");
        close(sock);
        return NULL;
    default:
        fprintf(stderr, "Unexpected setup_resp_success: '%u'\n", setup_resp_success);
        close(sock);
        return NULL;
    }

    /* will only get this far if Success */

    setup_resp_len = 7;
    setup_resp = (unsigned char *)malloc(setup_resp_len);
    if (setup_resp == NULL) {
        close(sock);
        perror("malloc setup_resp (7)");
        return NULL;
    }
    recv_len = recv(sock, (void *)setup_resp, setup_resp_len, 0);
    if (recv_len != setup_resp_len) {
        close(sock);
        perror("recv setup_resp");
        return NULL;
    }
    setup_resp_len = (*(uint16_t *)(setup_resp + 5)) * 4;
    free(setup_resp);
    setup_resp = (unsigned char *)malloc(setup_resp_len);
    if (setup_resp == NULL) {
        close(sock);
        perror("malloc setup_resp (big)");
        return NULL;
    }
    setup_resp_field = setup_resp;
    recv_len = recv(sock, (void *)setup_resp, setup_resp_len, 0);
    if (recv_len != setup_resp_len) {
        close(sock);
        perror("recv setup_resp");
        return NULL;
    }

    memset((void*)&setup_reply, 0, sizeof(setup_reply));

    setup_reply.release_num = *(uint32_t*)setup_resp_field;
    setup_resp_field += 4;
    setup_reply.resource_id_base = *(uint32_t *)setup_resp_field;
    setup_resp_field += 4;
    setup_reply.resource_id_mask = *(uint32_t *)setup_resp_field;
    setup_resp_field += 4;
    setup_reply.motion_buffer_size = *(uint32_t *)setup_resp_field;
    setup_resp_field += 4;
    setup_reply.vendor_len = *(uint16_t *)setup_resp_field;
    setup_resp_field += 2;
    setup_reply.max_req_len = *(uint16_t *)setup_resp_field;
    setup_resp_field += 2;
    setup_reply.roots_len = *(uint8_t *)setup_resp_field;
    setup_resp_field += 1;
    setup_reply.pixmap_formats_len = *(uint8_t *)setup_resp_field;
    setup_resp_field += 1;
    if (*(uint8_t *)setup_resp_field == 0) {
        setup_reply.img_byte_order = X_IMG_BYTE_LSB_FIRST;
    } else {
        setup_reply.img_byte_order = X_IMG_BYTE_MSB_FIRST;
    }
    setup_resp_field += 1;
    if (*(uint8_t *)setup_resp_field == 0) {
        setup_reply.bitmap_bit_order = X_BITMAP_BIT_LEAST_SIGNIFICANT;
    } else {
        setup_reply.img_byte_order = X_BITMAP_BIT_MOST_SIGNIFICANT;
    }
    setup_resp_field += 1;
    setup_reply.bitmap_scanline_unit = *(uint8_t *)setup_resp_field;
    setup_resp_field += 1;
    setup_reply.bitmap_scanline_pad = *(uint8_t *)setup_resp_field;
    setup_resp_field += 1;
    setup_reply.min_keycode = *(uint8_t *)setup_resp_field;
    setup_resp_field += 1;
    setup_reply.max_keycode = *(uint8_t *)setup_resp_field;
    setup_resp_field += 1;
    setup_resp_field += 4; /* unused */

    setup_reply.vendor = (unsigned char *)malloc(setup_reply.vendor_len);
    if (setup_reply.vendor == NULL) {
        free(setup_resp);
        close(sock);
        perror("malloc setup_reply.vendor");
        return NULL;
    }
    memcpy((void *)setup_reply.vendor, (void *)setup_resp_field, setup_reply.vendor_len);
    setup_resp_field += setup_reply.vendor_len;
    setup_resp_field += X_NET_PAD(setup_reply.vendor_len);

    setup_reply.pixmap_formats = 
        (struct X_Pixmap_format *)calloc(
            setup_reply.pixmap_formats_len, sizeof(struct X_Pixmap_format));
    if (setup_reply.pixmap_formats == NULL) {
        free(setup_reply.vendor);
        free(setup_resp);
        close(sock);
        perror("calloc pixmap_formats");
        return NULL;
    }
    for (i = 0; i < setup_reply.pixmap_formats_len; i++) {
        setup_reply.pixmap_formats[i].depth = *(uint8_t *)setup_resp_field;
        setup_resp_field++;
        setup_reply.pixmap_formats[i].bits_per_px = *(uint8_t *)setup_resp_field;
        setup_resp_field++;
        setup_reply.pixmap_formats[i].scanline_pad = *(uint8_t *)setup_resp_field;
        setup_resp_field++;
        setup_resp_field += 5; /* unused */
    }

    setup_reply.roots = (struct X_Screen *)calloc(setup_reply.roots_len, sizeof(struct X_Screen));
    if (setup_reply.roots == NULL) {
        free(setup_reply.pixmap_formats);
        free(setup_reply.vendor);
        free(setup_resp);
        close(sock);
        perror("calloc setup_reply.roots");
        return NULL;
    }
    for (i = 0; i < setup_reply.roots_len; i++) {
        setup_reply.roots[i].root = *(uint32_t *)setup_resp_field;
        setup_resp_field += 4;
        setup_reply.roots[i].default_colormap = *(uint32_t *)setup_resp_field;
        setup_resp_field += 4;
        setup_reply.roots[i].white_px = *(uint32_t *)setup_resp_field;
        setup_resp_field += 4;
        setup_reply.roots[i].black_px = *(uint32_t *)setup_resp_field;
        setup_resp_field += 4;
        setup_reply.roots[i].current_input_masks = *(uint32_t *)setup_resp_field;
        setup_resp_field += 4;
        setup_reply.roots[i].width_px = *(uint16_t *)setup_resp_field;
        setup_resp_field += 2;
        setup_reply.roots[i].height_px = *(uint16_t *)setup_resp_field;
        setup_resp_field += 2;
        setup_reply.roots[i].width_mm = *(uint16_t *)setup_resp_field;
        setup_resp_field += 2;
        setup_reply.roots[i].height_mm = *(uint16_t *)setup_resp_field;
        setup_resp_field += 2;
        setup_reply.roots[i].min_installed_maps = *(uint16_t *)setup_resp_field;
        setup_resp_field += 2;
        setup_reply.roots[i].max_installed_maps = *(uint16_t *)setup_resp_field;
        setup_resp_field += 2;
        setup_reply.roots[i].root_visual = *(uint16_t *)setup_resp_field;
        setup_resp_field += 4;
        switch (*(uint8_t *)setup_resp_field) {
        case 0:
            setup_reply.roots[i].backing_stores = X_BACKING_STORES_NEVER;
            break;
        case 1:
            setup_reply.roots[i].backing_stores = X_BACKING_STORES_WHEN_MAPPED;
            break;
        case 2:
            setup_reply.roots[i].backing_stores = X_BACKING_STORES_ALWAYS;
            break;
        };
        setup_resp_field += 1;
        if (*(uint8_t *)setup_resp_field == 0)  {
            setup_reply.roots[i].save_unders = False;
        } else {
            setup_reply.roots[i].save_unders = True;
        }
        setup_resp_field += 1;
        setup_reply.roots[i].root_depth = *(uint8_t *)setup_resp_field;
        setup_resp_field += 1;
        setup_reply.roots[i].allowed_depths_len = *(uint8_t *)setup_resp_field;
        setup_resp_field += 1;
        setup_reply.roots[i].allowed_depths =
            (struct X_Depth *)calloc(
                setup_reply.roots[i].allowed_depths_len, sizeof(struct X_Depth));
        if (setup_reply.roots[i].allowed_depths == NULL) {
            for (j = 0; j < i; j++) {
                for (k = 0; k < setup_reply.roots[j].allowed_depths_len; k++) {
                    free(setup_reply.roots[j].allowed_depths[k].visuals);
                }
                free(setup_reply.roots[j].allowed_depths);
            }
            free(setup_reply.roots);
            free(setup_reply.pixmap_formats);
            free(setup_reply.vendor);
            free(setup_resp);
            close(sock);
            perror("calloc allowed_depths");
            return NULL;
        }
        for (j = 0; j < setup_reply.roots[i].allowed_depths_len; j++) {
            setup_reply.roots[i].allowed_depths[j].depth = *(uint8_t *)setup_resp_field;
            setup_resp_field += 1;
            setup_resp_field += 1; /* unused */
            setup_reply.roots[i].allowed_depths[j].visuals_len = *(uint16_t *)setup_resp_field;
            setup_resp_field += 2;
            setup_resp_field += 4; /* unused */
            setup_reply.roots[i].allowed_depths[j].visuals =
                (struct X_Visual_type *)calloc(
                    setup_reply.roots[i].allowed_depths[j].visuals_len,
                    sizeof(struct X_Visual_type));
            if (setup_reply.roots[i].allowed_depths[j].visuals == NULL) {
                /* TODO: cleanup. I give up */
                return NULL;
            }
            for (k = 0; k < setup_reply.roots[i].allowed_depths[j].visuals_len; k++) {
                setup_reply.roots[i].allowed_depths[j].visuals[k].visual_id = *(uint32_t *)setup_resp_field;
                setup_resp_field += 4;
                switch (*(uint8_t *)setup_resp_field) {
                case 0:
                    setup_reply.roots[i].allowed_depths[j].visuals[k].class = X_VISUAL_CLASS_STATIC_GRAY;
                case 1:
                    setup_reply.roots[i].allowed_depths[j].visuals[k].class = X_VISUAL_CLASS_GRAY_SCALE;
                case 2:
                    setup_reply.roots[i].allowed_depths[j].visuals[k].class = X_VISUAL_CLASS_STATIC_COLOR;
                case 3:
                    setup_reply.roots[i].allowed_depths[j].visuals[k].class = X_VISUAL_CLASS_PSEUDO_COLOR;
                case 4:
                    setup_reply.roots[i].allowed_depths[j].visuals[k].class = X_VISUAL_CLASS_TRUE_COLOR;
                case 5:
                    setup_reply.roots[i].allowed_depths[j].visuals[k].class = X_VISUAL_CLASS_DIRECT_COLOR;
                };
                setup_resp_field += 1;
                setup_reply.roots[i].allowed_depths[j].visuals[k].bits_per_rgb_val = *(uint8_t *)setup_resp_field;
                setup_resp_field += 1;
                setup_reply.roots[i].allowed_depths[j].visuals[k].colormap_entries = *(uint16_t *)setup_resp_field;
                setup_resp_field += 2;
                setup_reply.roots[i].allowed_depths[j].visuals[k].red_mask = *(uint32_t *)setup_resp_field;
                setup_resp_field += 4;
                setup_reply.roots[i].allowed_depths[j].visuals[k].green_mask = *(uint32_t *)setup_resp_field;
                setup_resp_field += 4;
                setup_reply.roots[i].allowed_depths[j].visuals[k].blue_mask = *(uint32_t *)setup_resp_field;
                setup_resp_field += 4;
                setup_resp_field += 4; /* unused */
            }
        }
    }

    x = (struct X *)malloc(sizeof(struct X));
    if (x == NULL) {
        /* TODO: cleanup */
        close(sock);
        perror("malloc X");
        return NULL;
    }

    x->sock = sock;
    x->resource_id_base = setup_reply.resource_id_base;
    x->resource_id_mask = setup_reply.resource_id_mask;
    x->allocated_ids_num = 0;

    /* TODO: should not be picking the first root */

    if (setup_reply.roots_len == 0) {
        /* TODO: cleanup */
        free(x);
        close(sock);
        return NULL;
    }

    root = &setup_reply.roots[0];
    root_allowed_depth = NULL;
    root_visual = NULL;

    x->root_wid = root->root;

    for (i = 0; i < root->allowed_depths_len; i++) {
        if (root->allowed_depths[i].depth == root->root_depth) {
            root_allowed_depth = &root->allowed_depths[i];
            break;
        }
    }
    if (root_allowed_depth == NULL) {
        /* TODO: cleanup */
        fputs("root_allowed_depth not found\n", stderr);
        return NULL;
    }
    for (i = 0; i < root_allowed_depth->visuals_len; i++) {
        if (root_allowed_depth->visuals[i].visual_id == root->root_visual) {
            root_visual = &root_allowed_depth->visuals[i];
            break;
        }
    }
    if (root_visual == NULL) {
        /* TODO: cleanup */
        fputs("root_visual not found\n", stderr);
        return NULL;
    }
    memcpy((void *)&x->root_visual, (void *)root_visual, sizeof(struct X_Visual_type));

    for (i = 0; i < setup_reply.roots_len; i++) {
        for (j = 0; j < setup_reply.roots[i].allowed_depths_len; j++) {
            free(setup_reply.roots[i].allowed_depths[j].visuals);
        }
        free(setup_reply.roots[i].allowed_depths);
    }
    free(setup_reply.roots);
    free(setup_reply.pixmap_formats);
    free(setup_reply.vendor);
    free(setup_resp);

    return x;
}

X_id X_alloc_id(struct X * x) {
    X_id id;
    uint32_t mask_shift = 0;

    while (! ((x->resource_id_mask >> mask_shift) & 0x1)) {
        mask_shift++;
    }
    id = x->resource_id_base | (x->allocated_ids_num << mask_shift);
    x->allocated_ids_num++;

    return id;
}

void X_free_id(struct X * x, X_id id) {
    (void) id;
    /* TODO */
}

uint32_t X_rgb(struct X * x, uint32_t r, uint32_t g, uint32_t b) {
    uint32_t res = 0;
    size_t shift = 0;

    if (x->root_visual.class != X_VISUAL_CLASS_DIRECT_COLOR) {
        /* TODO: this is so wrong */
        fputs("Unsupported root_visual.class. TODO\n", stderr);
        exit(1);
    }

    while (! ((x->root_visual.red_mask >> shift) & 0x1)) {
        shift++;
    }
    res |= (r << shift) & x->root_visual.red_mask;
    shift = 0;

    while (! ((x->root_visual.green_mask >> shift) & 0x1)) {
        shift++;
    }
    res |= (g << shift) & x->root_visual.green_mask;
    shift = 0;

    while (! ((x->root_visual.blue_mask >> shift) & 0x1)) {
        shift++;
    }
    res |= (b << shift) & x->root_visual.blue_mask;

    return res;
}

X_id X_create_window(struct X * x) {
    X_id wid;
    X_id parent_wid;
    enum X_Window_Class class;
    uint8_t depth;
    uint16_t req_len;
    X_id visual;
    int16_t pos_x;
    int16_t pos_y;
    uint16_t width;
    uint16_t height;
    uint16_t border_width;
    uint32_t value_mask;
    uint32_t values[15];

    size_t values_len;

    unsigned char req[(8 + 15) * 4];
    unsigned char * req_field;

    unsigned char err_resp[32];
    uint8_t err_code;

    ssize_t sent_len = -1;
    ssize_t recv_len = -1;

    wid = X_alloc_id(x);
    parent_wid = x->root_wid;
    class = X_WIN_CLASS_COPY_FROM_PARENT;
    depth = 0;  /* copy from parent */
    visual = 0; /* copy from parent */
    pos_x = 10;
    pos_y = 10;
    width = 100;
    height = 100;
    border_width = 1;

    value_mask = 0x02; /* background-pixel */
    values_len = 2; /* because background-pixel is the second value */
    values[1] = X_rgb(x, 255, 128, 64);

    req_len = 8 + values_len;

    req_field = req;

    *(uint8_t *)req_field = 1; /* CreateWindow */
    req_field += 1;
    *(uint8_t *)req_field = depth;
    req_field += 1;
    *(uint16_t *)req_field = req_len;
    req_field += 2;
    *(uint32_t *)req_field = wid;
    req_field += 4;
    *(uint32_t *)req_field = parent_wid;
    req_field += 4;
    *(int16_t *)req_field = pos_x;
    req_field += 2;
    *(int16_t *)req_field = pos_y;
    req_field += 2;
    *(uint16_t *)req_field = width;
    req_field += 2;
    *(uint16_t *)req_field = height;
    req_field += 2;
    *(uint16_t *)req_field = border_width;
    req_field += 2;
    *(uint16_t *)req_field = class;
    req_field += 2;
    *(uint32_t *)req_field = visual;
    req_field += 4;
    *(uint32_t *)req_field = value_mask;
    req_field += 4;
    memcpy((void *)req_field, (void *)values, values_len * 4);
    req_field += values_len * 4;

    sent_len = send(x->sock, (void *)req, (req_field - req), 0);
    if (sent_len != (req_field - req)) {
        perror("send");
        return 0;
    }

    recv_len = recv(x->sock, (void *)err_resp, 32, 0);
    if (recv_len < 0) {
        perror("recv");
        return 0;
    } else if (recv_len == 32) {
        err_code = err_resp[1];
        printf("Err: %i\n", err_code);
        return 0;
    }

    req_field = req;

    *(uint8_t *)req_field = 8; /* MapWindow */
    req_field += 1;
    req_field += 1; /* unused */
    *(uint16_t *)req_field = 2;
    req_field += 2;
    *(uint32_t *)req_field = wid;
    req_field += 2;

    sent_len = send(x->sock, (void *)req, (req_field - req), 0);
    if (sent_len != (req_field - req)) {
        perror("send");
        return 0;
    }

    return wid;
}

void X_destroy_window(struct X * x, X_id id) {
    if (x == NULL) {
        return;
    }
    (void)id;
    /* TODO */
}

int main(void) {
    struct X * x = make_X();

    X_id win = X_create_window(x);

    X_destroy_window(x, win);
    X_destroy(x);

    return 0;
}

