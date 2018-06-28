#ifndef PAINT_H_
#define PAINT_H_

#include "videotalk.h"

typedef struct {
    int error;
    enum CommandKind {
        cmdPOINT,
        cmdLINE,
        cmdRECT,
        cmdRECTFILL,
        cmdCIRCLE,
        cmdCIRCLEFILL,
        cmdOVAL,
        cmdOVALFILL,
        cmdSETCOLOR,
        cmdEXPORT,
        cmdLOAD,
        cmdSAVE,
        cmdUNDO,
        cmdQUIT
    } kind;
    union {
        int intargs[4];
        char strarg[100];
    } args;
} Command;

void init_canvas(void);

typedef void (*responder)(const char *);
void interpret_command(Command *result, const char *command, responder respond);
void execute_command(Command *command, responder respond);
void push_back_history(const char *str);
int get_history_num(void);
void overlay_canvas(image_t base);

#endif /* PAINT_H_ */
