#include <lib/string.h>
#include <lib/types.h>
#include <lib/debug.h>
#include <lib/spinlock.h>

#include "video.h"
#include "console.h"
#include "serial.h"
#include "keyboard.h"

#define BUFLEN 1024
static char linebuf[BUFLEN];

static spinlock_t cons_lk;

struct {
    char buf[CONSOLE_BUFFER_SIZE];
    uint32_t rpos, wpos;
} cons;

void cons_init()
{
    memset(&cons, 0x0, sizeof(cons));
    serial_init();
    video_init();
    spinlock_init(&cons_lk);
}

void cons_intr(int (*proc)(void))
{
    int c;

    spinlock_acquire(&cons_lk);
    while ((c = (*proc)()) != -1) {
        if (c == 0)
            continue;
        cons.buf[cons.wpos++] = c;
        if (cons.wpos == CONSOLE_BUFFER_SIZE)
            cons.wpos = 0;
    }
    spinlock_release(&cons_lk);
}

char cons_getc(void)
{
    int c;

    // poll for any pending input characters,
    // so that this function works even when interrupts are disabled
    // (e.g., when called from the kernel monitor).
    serial_intr();
    keyboard_intr();
    spinlock_acquire(&cons_lk);

    // grab the next character from the input buffer.
    if (cons.rpos != cons.wpos) {
        c = cons.buf[cons.rpos++];
        if (cons.rpos == CONSOLE_BUFFER_SIZE)
            cons.rpos = 0;
        spinlock_release(&cons_lk);
        return c;
    }
    spinlock_release(&cons_lk);
    return 0;
}

void cons_putc(char c)
{
    serial_putc(c);
    video_putc(c);
}

char getchar(void)
{
    char c;

    while ((c = cons_getc()) == 0)
        /* do nothing */ ;
    return c;
}

void putchar(char c)
{
    cons_putc(c);
}

char *readline(const char *prompt)
{
    int i;
    char c;

    if (prompt != NULL)
        dprintf("%s", prompt);

    i = 0;
    while (1) {
        c = getchar();
        if (c < 0) {
            dprintf("read error: %e\n", c);
            return NULL;
        } else if ((c == '\b' || c == '\x7f') && i > 0) {
            putchar('\b');
            i--;
        } else if (c >= ' ' && i < BUFLEN - 1) {
            putchar(c);
            linebuf[i++] = c;
        } else if (c == '\n' || c == '\r') {
            putchar('\n');
            linebuf[i] = 0;
            return linebuf;
        }
    }
}
