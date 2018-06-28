#ifndef TTYIO_H_
#define TTYIO_H_

void ttyputs(const char *s);
void set_prompt(const char *prompt);
void print_prompt(void);
void ttyreader(void (*handler)(const char *input)) __attribute__((noreturn));
void ttyout_init(void);

#endif /* TTYIO_H_ */
