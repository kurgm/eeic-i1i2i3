#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "ttyio.h"

#define BUFSIZE 1000

static FILE *tty_out = NULL;
static char prompt_str[BUFSIZE] = "\033[2K\033[1G  1 > ";

static pthread_mutex_t ttyout_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t promptstr_lock = PTHREAD_MUTEX_INITIALIZER;

void ttyputs(const char *s) {
    pthread_mutex_lock(&ttyout_lock);
    fputs(s, tty_out);
    fflush(tty_out);
    pthread_mutex_unlock(&ttyout_lock);
}

void set_prompt(const char *prompt) {
    pthread_mutex_lock(&promptstr_lock);
    strncpy(prompt_str, prompt, sizeof(prompt_str));
    prompt_str[sizeof(prompt_str) - 1] = '\0';
    pthread_mutex_unlock(&promptstr_lock);
}

void print_prompt() {
    pthread_mutex_lock(&promptstr_lock);
    ttyputs(prompt_str);
    pthread_mutex_unlock(&promptstr_lock);
}

void ttyreader(void (*handler)(const char *input)) {
    FILE *tty_in = fopen("/dev/tty", "r");
    if (tty_in == NULL) {
        perror("fopen tty r");
        exit(1);
    }
    char buf[BUFSIZE];

    while (1) {
        fgets(buf, BUFSIZE, tty_in);
        handler(buf);
    }
}

void ttyout_init() {
    tty_out = fopen("/dev/tty", "w");
    if (tty_out == NULL) {
        perror("fopen tty w");
        exit(1);
    }
}
