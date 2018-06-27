#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "paint.h"
#include "videotalk.h"

#define BUFSIZE 1000

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795028841971
#endif /* M_PI */

typedef struct string_node {
    char *str;
    struct string_node *prev;
    struct string_node *next;
} StringNode;

static pixelval canvas[IMG_WIDTH][IMG_HEIGHT];
static bool canvas_drawn[IMG_WIDTH][IMG_HEIGHT];
static StringNode *history_begin = NULL;
static StringNode *history_end = NULL;
static int history_size = 0;
static pixelval currentColor = {0, 0, 0};

static const char *default_history_file = "history.txt";

static void dumb(const char *str) { (void)str; }

void push_back_history(const char *str) {
    char *copied_str = (char *)malloc(sizeof(char) * (strlen(str) + 1));
    StringNode *node = (StringNode *)malloc(sizeof(StringNode));
    strcpy(copied_str, str);
    node->str = copied_str;
    node->prev = history_end;
    node->next = NULL;
    if (history_end == NULL) {
        history_begin = history_end = node;
    } else {
        history_end->next = node;
        history_end = node;
    }
    history_size++;
}

static void pop_back_history() {
    StringNode *second;
    assert(history_end != NULL);
    second = history_end->prev;
    if (second != NULL) {
        second->next = NULL;
    }
    free(history_end->str);
    free(history_end);
    history_end = second;
    if (second == NULL) {
        history_begin = NULL;
    }
    history_size--;
}

int get_history_num() { return history_size; }

void init_canvas() {
    currentColor.r = currentColor.g = currentColor.b = 0;
    memset(canvas, '\xff', sizeof(canvas));
    memset(canvas_drawn, 0, sizeof(canvas_drawn));
}

static void draw_point(const int x, const int y) {
    if (x < 0 || x >= IMG_WIDTH || y < 0 || y >= IMG_HEIGHT) {
        return;
    }
    canvas[x][y].r = currentColor.r;
    canvas[x][y].g = currentColor.g;
    canvas[x][y].b = currentColor.b;
    canvas_drawn[x][y] = true;
}

static int max(const int a, const int b) { return (a > b) ? a : b; }
static int min(const int a, const int b) { return (a < b) ? a : b; }

static void draw_line(const int x0, const int y0, const int x1, const int y1) {
    int i;
    const int n = max(abs(x1 - x0), abs(y1 - y0));
    if (n == 0) {
        draw_point(x0, y0);
    } else {
        for (i = 0; i <= n; i++) {
            const int x = x0 + i * (x1 - x0) / n;
            const int y = y0 + i * (y1 - y0) / n;
            draw_point(x, y);
        }
    }
}

static void draw_rect(const int x, const int y, const int w, const int h) {
    int i;
    if (0 <= y && y < IMG_HEIGHT) {
        int end = min(IMG_WIDTH, x + w);
        for (i = max(0, x); i <= end; i++) {
            draw_point(i, y);
        }
    }
    if (0 <= y + h && y + h < IMG_HEIGHT) {
        int end = min(IMG_WIDTH, x + w);
        for (i = max(0, x); i <= end; i++) {
            draw_point(i, y + h);
        }
    }
    if (0 <= x && x < IMG_WIDTH) {
        int end = min(IMG_HEIGHT - 1, y + h);
        for (i = max(0, y); i <= end; i++) {
            draw_point(x, i);
        }
    }
    if (0 <= x + h && x + h < IMG_WIDTH) {
        int end = min(IMG_HEIGHT - 1, y + h);
        for (i = max(0, y); i <= end; i++) {
            draw_point(x + w, i);
        }
    }
}

static void draw_rect_fill(const int x0, const int y0, const int w,
                           const int h) {
    int endx = min(IMG_WIDTH - 1, x0 + w);
    int endy = min(IMG_HEIGHT - 1, y0 + h);
    int x, y;
    for (x = max(x0, 0); x <= endx; x++) {
        for (y = max(y0, 0); y <= endy; y++) {
            draw_point(x, y);
        }
    }
}

static void draw_circle(const int cx, const int cy, const int r) {
    const int r2 = (int)(ceil(r / sqrt(2.0)));
    int i;
    for (i = -r2; i <= r2; i++) {
        int j = (int)(sqrt(r * r - i * i));
        draw_point(cx + i, cy + j);
        draw_point(cx + i, cy - j);
        draw_point(cx + j, cy + i);
        draw_point(cx - j, cy + i);
    }
}

static void draw_oval(const int cx, const int cy, const int rx, const int ry) {
    double n = 2 * M_PI * max(rx, ry);
    for (int i = 0; i < n; i++) {
        draw_point((int)(cx + rx * cos(2 * i * M_PI / n)),
                   (int)(cy + ry * sin(2 * i * M_PI / n)));
    }
}

static void draw_oval_fill(const int cx, const int cy, const int rx,
                           const int ry) {
    double n = 2 * M_PI * max(rx, ry);
    for (int i = 0; i <= n / 2 + 1; i++) {
        draw_line((int)(cx + rx * cos(2 * i * M_PI / n)),
                  (int)(cy + ry * sin(2 * i * M_PI / n)),
                  (int)(cx + rx * cos(2 * i * M_PI / n)),
                  (int)(cy - ry * sin(2 * i * M_PI / n)));
    }
}

static void draw_circle_fill(const int cx, const int cy, const int r) {
    const int r2 = (int)(ceil(r / sqrt(2.0)));
    int i;
    for (i = -r2; i <= r2; i++) {
        int j = (int)(sqrt(r * r - i * i));
        if (0 <= cx + i && cx + i < IMG_WIDTH) {
            int endy = min(IMG_HEIGHT - 1, cy + j);
            int y;
            for (y = max(0, cy - j); y <= endy; y++) {
                draw_point(cx + i, y);
            }
        }
        if (0 <= cy + i && cy + i < IMG_HEIGHT) {
            int endx = min(IMG_WIDTH - 1, cx + j);
            int x;
            for (x = max(0, cx - j); x <= endx; x++) {
                draw_point(x, cy + i);
            }
        }
    }
}

static void export_bmp(const char *filename) {
    FILE *fp;
    unsigned char bitmapfileheader[14] = {'B', 'M', 0, 0,  0, 0, 0,
                                          0,   0,   0, 54, 0, 0, 0};
    unsigned char bitmapinfoheader[40] = {
        40, 0, 0, 0, 0,  0,  0, 0, 0,  0,  0, 0, 1, 0, 24, 0, 0, 0, 0, 0,
        0,  0, 0, 0, 18, 11, 0, 0, 18, 11, 0, 0, 2, 0, 0,  0, 2, 0, 0, 0};
    const int padperrow = (4 - (3 * IMG_WIDTH) % 4) % 4;
    const int bitmapsize = (3 * IMG_WIDTH + padperrow) * IMG_HEIGHT;
    int x, y;

    const int filesize = bitmapsize + 54;
    bitmapfileheader[2] = (unsigned char)filesize;
    bitmapfileheader[3] = (unsigned char)(filesize >> 8);
    bitmapfileheader[4] = (unsigned char)(filesize >> 16);
    bitmapfileheader[5] = (unsigned char)(filesize >> 24);

    bitmapinfoheader[4] = (unsigned char)IMG_WIDTH;
    bitmapinfoheader[5] = (unsigned char)(IMG_WIDTH >> 8);
    bitmapinfoheader[6] = (unsigned char)(IMG_WIDTH >> 16);
    bitmapinfoheader[7] = (unsigned char)(IMG_WIDTH >> 24);

    bitmapinfoheader[8] = (unsigned char)IMG_HEIGHT;
    bitmapinfoheader[9] = (unsigned char)(IMG_HEIGHT >> 8);
    bitmapinfoheader[10] = (unsigned char)(IMG_HEIGHT >> 16);
    bitmapinfoheader[11] = (unsigned char)(IMG_HEIGHT >> 24);

    bitmapinfoheader[20] = (unsigned char)bitmapsize;
    bitmapinfoheader[21] = (unsigned char)(bitmapsize >> 8);
    bitmapinfoheader[22] = (unsigned char)(bitmapsize >> 16);
    bitmapinfoheader[23] = (unsigned char)(bitmapsize >> 24);

    if ((fp = fopen(filename, "w")) == NULL) {
        fprintf(stderr, "error: cannot open %s.\n", filename);
        return;
    }
    fwrite(bitmapfileheader, 1, 14, fp);
    fwrite(bitmapinfoheader, 1, 40, fp);

    for (y = IMG_HEIGHT - 1; y >= 0; y--) {
        for (x = 0; x < IMG_WIDTH; x++) {
            fputc(canvas[x][y].b, fp);
            fputc(canvas[x][y].g, fp);
            fputc(canvas[x][y].r, fp);
        }
        for (x = 0; x < padperrow; x++) {
            fputc(0, fp);
        }
    }
    fclose(fp);

    fprintf(stderr, "exported to \"%s\"\n", filename);
}

static void export_svg(const char *filename) {
    FILE *fp;
    StringNode *node;
    char svgColorString[8] = "#000000";
    if ((fp = fopen(filename, "w")) == NULL) {
        fprintf(stderr, "error: cannot open %s.\n", filename);
        return;
    }

    fprintf(fp,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            "viewBox=\"0 0 %d %d\" width=\"%d\" height=\"%d\">",
            IMG_WIDTH, IMG_HEIGHT, IMG_WIDTH, IMG_HEIGHT);

    for (node = history_begin; node != NULL; node = node->next) {
        Command command;
        int *args;
        interpret_command(&command, node->str, dumb);
        args = command.args.intargs;
        if (command.kind == cmdPOINT) {
            fprintf(fp,
                    "<path d=\"M %d %d %d %d\" "
                    "stroke=\"%s\" fill=\"none\" />",
                    args[0], args[1], args[0], args[1], svgColorString);
        } else if (command.kind == cmdLINE) {
            fprintf(fp,
                    "<path d=\"M %d %d %d %d\" "
                    "stroke=\"%s\" fill=\"none\" />",
                    args[0], args[1], args[2], args[3], svgColorString);
        } else if (command.kind == cmdRECTFILL) {
            fprintf(fp,
                    "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
                    "fill=\"%s\" stroke=\"none\" />",
                    args[0], args[1], args[2], args[3], svgColorString);
        } else if (command.kind == cmdRECT) {
            fprintf(fp,
                    "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
                    "stroke=\"%s\" fill=\"none\" />",
                    args[0], args[1], args[2], args[3], svgColorString);
        } else if (command.kind == cmdCIRCLEFILL) {
            fprintf(fp,
                    "<circle cx=\"%d\" cy=\"%d\" r=\"%d\" "
                    "fill=\"%s\" stroke=\"none\" />",
                    args[0], args[1], args[2], svgColorString);
        } else if (command.kind == cmdCIRCLE) {
            fprintf(fp,
                    "<circle cx=\"%d\" cy=\"%d\" r=\"%d\" "
                    "stroke=\"%s\" fill=\"none\" />",
                    args[0], args[1], args[2], svgColorString);
        } else if (command.kind == cmdOVALFILL) {
            fprintf(fp,
                    "<ellipse cx=\"%d\" cy=\"%d\" rx=\"%d\" ry=\"%d\" "
                    "fill=\"%s\" stroke=\"none\" />",
                    args[0], args[1], args[2], args[3], svgColorString);
        } else if (command.kind == cmdOVAL) {
            fprintf(fp,
                    "<ellipse cx=\"%d\" cy=\"%d\" rx=\"%d\" ry=\"%d\" "
                    "stroke=\"%s\" fill=\"none\" />",
                    args[0], args[1], args[2], args[3], svgColorString);
        } else if (command.kind == cmdSETCOLOR) {
            sprintf(svgColorString, "#%02x%02x%02x", args[0], args[1], args[2]);
        }
    }
    fputs("</svg>\n", fp);
    fclose(fp);

    fprintf(stderr, "exported to \"%s\"\n", filename);
}

static void load_history(const char *filename, responder respond) {
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
        interpret_command(&command, buf, respond);
        if (command.error != 0) {
            /* an error occurred */
            break;
        }
        execute_command(&command, respond);
        push_back_history(buf);
    }

    fprintf(stderr, "loaded from \"%s\"\n", filename);

    fclose(fp);
}

static void save_history(const char *filename) {
    FILE *fp;
    StringNode *node;

    if (filename == NULL || strcmp(filename, "") == 0) {
        filename = default_history_file;
    }
    if ((fp = fopen(filename, "w")) == NULL) {
        fprintf(stderr, "error: cannot open %s.\n", filename);
        return;
    }

    for (node = history_begin; node != NULL; node = node->next) {
        fprintf(fp, "%s", node->str);
    }

    fprintf(stderr, "saved as \"%s\"\n", filename);

    fclose(fp);
}

static int interpret_number(const char *str, int *nump) {
    char *endp;
    if (str == NULL) {
        return 1;
    }
    *nump = (int)strtol(str, &endp, 10);
    if (endp != str + strlen(str)) {
        return 1;
    }
    return 0;
}

/* Interpret a command */
void interpret_command(Command *result, const char *command,
                       responder respond) {
    char buf[BUFSIZE];
    char *s;
    strncpy(buf, command, BUFSIZE);
    buf[strlen(command) - 1] = 0; /* remove the newline character at the end */

    if (strlen(buf) == 0) {
        /* empty line */
        result->error = 2;
        return;
    }
    char *saveptr;
    s = strtok_r(buf, " ", &saveptr);

    result->error = 0;
    if (strcmp(s, "point") == 0) {
        int x, y;
        if (interpret_number(strtok_r(NULL, " ", &saveptr), &x) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &y)) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        result->kind = cmdPOINT;
        result->args.intargs[0] = x;
        result->args.intargs[1] = y;
        return;
    }

    if (strcmp(s, "line") == 0) {
        int x0, y0, x1, y1;
        if (interpret_number(strtok_r(NULL, " ", &saveptr), &x0) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &y0) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &x1) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &y1)) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        result->kind = cmdLINE;
        result->args.intargs[0] = x0;
        result->args.intargs[1] = y0;
        result->args.intargs[2] = x1;
        result->args.intargs[3] = y1;
        return;
    }

    if (strcmp(s, "rect") == 0 || strcmp(s, "rectfill") == 0) {
        int x, y, w, h;
        if (interpret_number(strtok_r(NULL, " ", &saveptr), &x) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &y) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &w) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &h)) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        if (w < 0 || h < 0) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        if (strcmp(s, "rectfill") == 0) {
            result->kind = cmdRECTFILL;
        } else {
            result->kind = cmdRECT;
        }
        result->args.intargs[0] = x;
        result->args.intargs[1] = y;
        result->args.intargs[2] = w;
        result->args.intargs[3] = h;
        return;
    }

    if (strcmp(s, "circle") == 0 || strcmp(s, "circlefill") == 0) {
        int cx, cy, r;
        if (interpret_number(strtok_r(NULL, " ", &saveptr), &cx) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &cy) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &r)) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        if (r < 0) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        if (strcmp(s, "circlefill") == 0) {
            result->kind = cmdCIRCLEFILL;
        } else {
            result->kind = cmdCIRCLE;
        }
        result->args.intargs[0] = cx;
        result->args.intargs[1] = cy;
        result->args.intargs[2] = r;
        return;
    }
    if (strcmp(s, "oval") == 0 || strcmp(s, "ovalfill") == 0) {
        int cx, cy, rx, ry;
        if (interpret_number(strtok_r(NULL, " ", &saveptr), &cx) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &cy) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &rx) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &ry)) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        if (rx < 0 || ry < 0) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        if (strcmp(s, "ovalfill") == 0) {
            result->kind = cmdOVALFILL;
        } else {
            result->kind = cmdOVAL;
        }
        result->args.intargs[0] = cx;
        result->args.intargs[1] = cy;
        result->args.intargs[2] = rx;
        result->args.intargs[3] = ry;
        return;
    }

    if (strcmp(s, "setcolor") == 0) {
        int r, g, b;
        if (interpret_number(strtok_r(NULL, " ", &saveptr), &r) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &g) ||
            interpret_number(strtok_r(NULL, " ", &saveptr), &b)) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        result->kind = cmdSETCOLOR;
        result->args.intargs[0] = r;
        result->args.intargs[1] = g;
        result->args.intargs[2] = b;
        return;
    }

    if (strcmp(s, "save") == 0) {
        s = strtok_r(NULL, " ", &saveptr);
        result->kind = cmdSAVE;
        strcpy(result->args.strarg, s == NULL ? "" : s);
        return;
    }

    if (strcmp(s, "load") == 0) {
        s = strtok_r(NULL, " ", &saveptr);
        result->kind = cmdLOAD;
        strcpy(result->args.strarg, s == NULL ? "" : s);
        return;
    }

    if (strcmp(s, "export") == 0) {
        char *ext = "";
        s = strtok_r(NULL, " ", &saveptr);
        if (s == NULL) {
            respond("error: please specify output file name.\n");
            result->error = 1;
            return;
        }
        result->kind = cmdEXPORT;
        strcpy(result->args.strarg, s == NULL ? "" : s);
        if (strlen(s) >= 4) {
            ext = s + strlen(s) - 4;
        }
        if (strcmp(ext, ".bmp") == 0 || strcmp(ext, ".svg") == 0) {
        } else {
            respond("error: unknown file type.\n");
            result->error = 1;
            return;
        }
        return;
    }

    if (strcmp(s, "undo") == 0) {
        int n = 1;
        result->kind = cmdUNDO;
        s = strtok_r(NULL, " ", &saveptr);
        if (s != NULL && interpret_number(s, &n)) {
            respond("error: invalid argument.\n");
            result->error = 1;
            return;
        }
        if (n < 0) {
            respond("error: invalid argument.\n");
        }
        result->args.intargs[0] = n;
        return;
    }

    if (strcmp(s, "quit") == 0) {
        result->kind = cmdQUIT;
        return;
    }

    respond("error: unknown command.\n");
    result->error = 1;
    return;
}

void execute_command(Command *command, responder respond) {
    const int *args = command->args.intargs;
    if (command->kind == cmdPOINT) {
        draw_point(args[0], args[1]);
    } else if (command->kind == cmdLINE) {
        draw_line(args[0], args[1], args[2], args[3]);
    } else if (command->kind == cmdRECTFILL) {
        draw_rect_fill(args[0], args[1], args[2], args[3]);
    } else if (command->kind == cmdRECT) {
        draw_rect(args[0], args[1], args[2], args[3]);
    } else if (command->kind == cmdCIRCLEFILL) {
        draw_circle_fill(args[0], args[1], args[2]);
    } else if (command->kind == cmdCIRCLE) {
        draw_circle(args[0], args[1], args[2]);
    } else if (command->kind == cmdOVALFILL) {
        draw_oval_fill(args[0], args[1], args[2], args[3]);
    } else if (command->kind == cmdOVAL) {
        draw_oval(args[0], args[1], args[2], args[3]);
    } else if (command->kind == cmdSETCOLOR) {
        currentColor.r = (unsigned char)args[0];
        currentColor.g = (unsigned char)args[1];
        currentColor.b = (unsigned char)args[2];
    } else if (command->kind == cmdSAVE) {
        save_history(command->args.strarg);
    } else if (command->kind == cmdLOAD) {
        load_history(command->args.strarg, respond);
    } else if (command->kind == cmdEXPORT) {
        char *ext = "";
        char *s = command->args.strarg;
        if (strlen(s) >= 4) {
            ext = s + strlen(s) - 4;
        }
        if (strcmp(ext, ".bmp") == 0) {
            export_bmp(s);
        } else if (strcmp(ext, ".svg") == 0) {
            export_svg(s);
        }
    } else if (command->kind == cmdUNDO) {
        StringNode *node;
        int i;
        for (i = 0; i < args[0]; i++) {
            if (history_begin == NULL) {
                respond("error: no history.\n");
                break;
            }
            pop_back_history();
        }
        init_canvas();
        for (node = history_begin; node != NULL; node = node->next) {
            Command cmd;
            interpret_command(&cmd, node->str, respond);
            execute_command(&cmd, respond);
        }
    }
}

void overlay_canvas(image_t base) {
    int cnt = 0;
    int x, y;
    for (y = 0; y < IMG_HEIGHT; y++) {
        for (x = 0; x < IMG_WIDTH; x++) {
            if (canvas_drawn[x][y] != 0) {
                base[y][x].r = canvas[x][y].r;
                base[y][x].g = canvas[x][y].g;
                base[y][x].b = canvas[x][y].b;
                cnt++;
            }
        }
    }
}
