#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include "forceio.h"
#include "paint.h"
#include "videotalk.h"
#include "ttyio.h"

#define BUFSIZE 1000

static video_args *v_arg;

static const char dev_name[] = "/dev/video0";
static int fd = -1;

struct buffer {
    void *start;
    size_t length;
};

static struct buffer *buffers;
static unsigned int n_buffers;

static void errno_exit(const char *s) __attribute__((noreturn));
static void errno_exit(const char *s) {
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

static int xioctl(int fh, unsigned long request, void *arg) {
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

static void open_device(void) {
    struct stat st;

    if (stat(dev_name, &st) == -1) {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n", dev_name, errno,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "%s is no device\n", dev_name);
        exit(EXIT_FAILURE);
    }

    fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (fd == -1) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n", dev_name, errno,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void init_mmap(void) {
    struct v4l2_requestbuffers req;

    memset(&req, 0, sizeof(req));

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support memory mapping\n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s\n", dev_name);
        exit(EXIT_FAILURE);
    }

    buffers = calloc(req.count, sizeof(*buffers));

    if (!buffers) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
            errno_exit("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
            mmap(NULL /* start anywhere */, buf.length,
                 PROT_READ | PROT_WRITE /* required */,
                 MAP_SHARED /* recommended */, fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start) errno_exit("mmap");
    }
}

static void init_device(void) {
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s is no V4L2 device\n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is no video capture device\n", dev_name);
        exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming i/o\n", dev_name);
        exit(EXIT_FAILURE);
    }

    /* Select video input, video standard and tune here. */

    memset(&cropcap, 0, sizeof(cropcap));

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
            }
        }
    } else {
        /* Errors ignored. */
    }

    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fprintf(stderr, "Set format\r\n");
    fmt.fmt.pix.width = 160;                      // replace
    fmt.fmt.pix.height = 120;                     // replace
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  // replace
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)) errno_exit("VIDIOC_S_FMT");

    /* Note VIDIOC_S_FMT may change width and height. */
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV ||
        fmt.fmt.pix.width != 160 || fmt.fmt.pix.height != 120) {
        fprintf(stderr, "unsupported format\n");
        exit(EXIT_FAILURE);
    }

    struct v4l2_streamparm parm;
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = 10;
    if (-1 == xioctl(fd, VIDIOC_S_PARM, &parm)) errno_exit("VIDIOC_S_PARM");

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min) fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min) fmt.fmt.pix.sizeimage = min;

    init_mmap();
}

static void start_capturing(void) {
    unsigned int i;
    enum v4l2_buf_type type;

    for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) errno_exit("VIDIOC_QBUF");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)) errno_exit("VIDIOC_STREAMON");
}

static void print_image(int outfd, const image_t img) {
    char buffer[12 + IMG_HEIGHT * (20 * IMG_WIDTH + 6) + 10];
    int idx = 0;
    idx += sprintf(buffer + idx, "\033[3;J\033[H\033[2J");
    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            idx += sprintf(buffer + idx, "\033[48;2;%d;%d;%dm ", img[y][x].r,
                           img[y][x].g, img[y][x].b);
            // int rj = ((int)(img[y][x].r) + 26) * 5 / 255;
            // int gj = ((int)(img[y][x].g) + 26) * 5 / 255;
            // int bj = ((int)(img[y][x].b) + 26) * 5 / 255;
            // assert(0 <= rj <= 5 && 0 <= gj <= 5 && 0 <= bj <= 5);
            // idx += sprintf(buffer + idx, "\033[48;5;%dm ",
            //                16 + (rj * 36 + gj * 6 + bj));
        }
        idx += sprintf(buffer + idx, "\033[0m\n");
    }
    write_force(outfd, buffer, (size_t)idx);
}

static image_t self_img;
static pthread_mutex_t canvas_lock = PTHREAD_MUTEX_INITIALIZER;

static void process_image(const void *p, size_t size) {
    (void)size;
    for (int y = 0; y < 120; y += 2) {
        for (int x = 0; x < 160; x += 1) {
            int value = ((const unsigned char *)p)[((y * 160) + x) * 2];
            int cb = ((const unsigned char *)p)[((y * 160) + x) / 2 * 4 + 1];
            int cr = ((const unsigned char *)p)[((y * 160) + x) / 2 * 4 + 3];
            cb -= 128;
            cr -= 128;
            double r = value + 1.402 * cr;
            double g = value - 0.344 * cb - 0.714 * cr;
            double b = value + 1.772 * cb;
            self_img[y / 2][x].r =
                r > 255 ? 255 : r < 0 ? 0 : (unsigned char)(double)round(r);
            self_img[y / 2][x].g =
                g > 255 ? 255 : g < 0 ? 0 : (unsigned char)(double)round(g);
            self_img[y / 2][x].b =
                b > 255 ? 255 : b < 0 ? 0 : (unsigned char)(double)round(b);
        }
    }
    pthread_mutex_lock(&canvas_lock);
    overlay_canvas(self_img);
    pthread_mutex_unlock(&canvas_lock);
    print_image(v_arg->videoout1fd, self_img);
    static char sendbuf[3 + sizeof(image_t)];
    sendbuf[0] = 'V';
    sendbuf[1] = 'I';
    sendbuf[2] = 'D';
    memcpy(sendbuf + 3, &self_img, sizeof(self_img));
    send(v_arg->sockfd, sendbuf, sizeof(self_img), 0);
}

static int read_frame(void) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
            case EAGAIN:
                return 0;

            case EIO:
                /* Could ignore EIO, see spec. */

                /* fall through */

            default:
                errno_exit("VIDIOC_DQBUF");
        }
    }

    assert(buf.index < n_buffers);

    process_image(buffers[buf.index].start, buf.bytesused);

    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) errno_exit("VIDIOC_QBUF");

    return 1;
}

// static void stop_capturing(void) {
//     enum v4l2_buf_type type;
//     type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
//     if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
//         errno_exit("VIDIOC_STREAMOFF");
// }

// static void uninit_device(void) {
//     unsigned int i;
//     for (i = 0; i < n_buffers; ++i)
//         if (-1 == munmap(buffers[i].start, buffers[i].length))
//             errno_exit("munmap");
// }

// static void close_device(void) {
//     if (close(fd) == -1) errno_exit("close");

//     fd = -1;
// }

static void video_send(void) __attribute__((noreturn));
static void video_send(void) {
    open_device();
    init_device();
    start_capturing();
    while (1) {
        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select(fd + 1, &fds, NULL, NULL, &tv);

            if (-1 == r) {
                if (EINTR == errno) continue;
                errno_exit("select");
            }

            if (0 == r) {
                fprintf(stderr, "select timeout\n");
                exit(EXIT_FAILURE);
            }

            if (read_frame()) break;
            /* EAGAIN - continue select loop. */
        }
    }
    // stop_capturing();
    // uninit_device();
    // close_device();
}

static size_t min_sizet(size_t a, size_t b) { return a < b ? a : b; }

static void respond(const char *s) {
    char sendbuf[BUFSIZE + 3];
    sendbuf[0] = 'P';
    sendbuf[1] = 'N';
    sendbuf[2] = 'R';
    strncpy(sendbuf + 3, s, BUFSIZE);
    send(v_arg->sockfd, sendbuf, 3 + min_sizet(strlen(s) + 1, BUFSIZE), 0);
}

static void execute_received_paint_command(const char *buf) {
    Command command;
    interpret_command(&command, buf, respond);
    if (command.error != 0) {
        return;
    }
    if (command.kind == cmdQUIT || command.kind == cmdSAVE ||
        command.kind == cmdLOAD || command.kind == cmdEXPORT) {
        respond("error: that command is disabled.\n");
        return;
    }
    pthread_mutex_lock(&canvas_lock);
    execute_command(&command, respond);
    pthread_mutex_unlock(&canvas_lock);
    if (command.kind == cmdSAVE || command.kind == cmdLOAD ||
        command.kind == cmdEXPORT || command.kind == cmdUNDO) {
    } else {
        push_back_history(buf);
    }
}

static void send_prompt(int count) {
    char sendbuf[100];
    sendbuf[0] = 'P';
    sendbuf[1] = 'N';
    sendbuf[2] = 'C';
    snprintf(sendbuf + 3, sizeof(sendbuf) - 3, "\033[2K\033[1G%3d > ",
             count + 1);
    send(v_arg->sockfd, sendbuf, strlen(sendbuf) + 1, 0);
}

static void *video_recv(void *arg) {
    static char recvbuf[sizeof(image_t) + 10];
    (void)arg;
    send_prompt(get_history_num() + 1);
    while (1) {
        ssize_t n = recv(v_arg->sockfd, recvbuf, sizeof(recvbuf) - 1, 0);
        if (n == -1) {
            errno_exit("recv");
        }
        // fprintf(stderr, "received command size: %d\n", n);
        if (n < 3) {
            continue;
        }
        if (recvbuf[0] == 'V' && recvbuf[1] == 'I' && recvbuf[2] == 'D') {
            print_image(v_arg->videoout2fd, (pixelval(*)[])(recvbuf + 3));
        } else if (recvbuf[0] == 'P' && recvbuf[1] == 'N' &&
                   recvbuf[2] == 'T') {
            recvbuf[n] = '\0';
            // fprintf(stderr, "paint command received: %s\n", recvbuf + 3);
            execute_received_paint_command(recvbuf + 3);
            send_prompt(get_history_num() + 1);
        } else if (recvbuf[0] == 'P' && recvbuf[1] == 'N' &&
                   recvbuf[2] == 'R') {
            ttyputs(recvbuf + 3);
        } else if (recvbuf[0] == 'P' && recvbuf[1] == 'N' &&
                   recvbuf[2] == 'C') {
            set_prompt(recvbuf + 3);
            print_prompt();
        } else {
            fprintf(
                stderr,
                "unknown command received: %c%c%c(\"\\x%02x\\x%02x\\x%02x\")\n",
                recvbuf[0], recvbuf[1], recvbuf[2], recvbuf[0], recvbuf[1],
                recvbuf[2]);
        }
    }
    return NULL;
}

static const char *default_history_file = "history.txt";

static void load_history_file(const char *filename) {
    FILE *fp;
    char buf[BUFSIZE];

    if (filename == NULL || strcmp(filename, "") == 0) {
        filename = default_history_file;
    }
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "error: cannot open %s.\n", filename);
        return;
    }

    while (fgets(buf, BUFSIZE, fp) != NULL) {
        Command command;
        interpret_command(&command, buf, ttyputs);
        if (command.error != 0) {
            /* an error occurred */
            break;
        }
        if (command.kind == cmdLOAD) {
            load_history_file(command.args.strarg);
        }
        char sendbuf[BUFSIZE + 3];
        sendbuf[0] = 'P';
        sendbuf[1] = 'N';
        sendbuf[2] = 'T';
        strcpy(sendbuf + 3, buf);
        send(v_arg->sockfd, sendbuf, 3 + strlen(buf) + 1, 0);
    }

    ttyputs("loaded from \"");
    ttyputs(filename);
    ttyputs("\"\n");

    fclose(fp);
}

void paint_input_handler(const char *input) {
    Command command;

    interpret_command(&command, input, ttyputs);
    if (command.error != 0) {
        print_prompt();
        return;
    }
    if (command.kind == cmdLOAD) {
        load_history_file(command.args.strarg);
    } else {
        char sendbuf[BUFSIZE + 3];
        sendbuf[0] = 'P';
        sendbuf[1] = 'N';
        sendbuf[2] = 'T';
        strcpy(sendbuf + 3, input);
        send(v_arg->sockfd, sendbuf, 3 + strlen(input) + 1, 0);
    }
}

void *videotalk(void *arg) {
    v_arg = arg;
    init_canvas();

    pthread_t threadid;
    pthread_create(&threadid, NULL, video_recv, NULL);

    video_send();
    return NULL;
}
