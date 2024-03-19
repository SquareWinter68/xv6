// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

void history_buff(char current_char, int eol_flag);
extern int shift_flag;
void write_to_buffer(char* string);
void arrow_handling(int keycode);
int color_flag = 0;

static void consputc(int);

static int panicked = 0;

static struct {
	struct spinlock lock;
	int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
	static char digits[] = "0123456789abcdef";
	char buf[16];
	int i;
	uint x;

	if(sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	i = 0;
	do{
		buf[i++] = digits[x % base];
	}while((x /= base) != 0);

	if(sign)
		buf[i++] = '-';

	while(--i >= 0)
		consputc(buf[i]);
}

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
	int i, c, locking;
	uint *argp;
	char *s;

	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

	if (fmt == 0)
		panic("null fmt");

	argp = (uint*)(void*)(&fmt + 1);
	for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
		if(c != '%'){
			consputc(c);
			continue;
		}
		c = fmt[++i] & 0xff;
		if(c == 0)
			break;
		switch(c){
		case 'd':
			printint(*argp++, 10, 1);
			break;
		case 'x':
		case 'p':
			printint(*argp++, 16, 0);
			break;
		case 's':
			if((s = (char*)*argp++) == 0)
				s = "(null)";
			for(; *s; s++)
				consputc(*s);
			break;
		case '%':
			consputc('%');
			break;
		default:
			// Print unknown % sequence to draw attention.
			consputc('%');
			consputc(c);
			break;
		}
	}

	if(locking)
		release(&cons.lock);
}

void
panic(char *s)
{
	int i;
	uint pcs[10];

	cli();
	cons.locking = 0;
	// use lapiccpunum so that we can call panic from mycpu()
	cprintf("lapicid %d: panic: ", lapicid());
	cprintf(s);
	cprintf("\n");
	getcallerpcs(&s, pcs);
	for(i=0; i<10; i++)
		cprintf(" %p", pcs[i]);
	panicked = 1; // freeze other CPU
	for(;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
	int pos;

	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	if(c == '\n')
		pos += 80 - pos%80;
	else if(c == BACKSPACE){
		if(pos > 0) --pos;
	} else if (color_flag){
		crt[pos++] = (c&0xff) | 0x0200;  // black on white
		color_flag = 0;
	 }else
	 	crt[pos++] = (c&0xff) | 0x0700;  // black on white

	if(pos < 0 || pos > 25*80)
		panic("pos under/overflow");

	if((pos/80) >= 24){  // Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*23*80);
		pos -= 80;
		memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
	if(panicked){
		cli();
		for(;;)
			;
	}

	if(c == BACKSPACE){
		uartputc('\b'); uartputc(' '); uartputc('\b');
	} else
		uartputc(c);
	cgaputc(c);
}

#define INPUT_BUF 128
struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

int char_test;
void
consoleintr(int (*getc)(void))
{
	static int flagh;
	int c, doprocdump = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		char_test = c;
		switch(c){
		case C('P'):  // Process listing.
			// procdump() locks cons.lock indirectly; invoke later
			doprocdump = 1;
			break;
		case C('U'):  // Kill line.
			while(input.e != input.w &&
			      input.buf[(input.e-1) % INPUT_BUF] != '\n'){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':  // Backspace
			if(input.e != input.w){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		default:
			if(c != 0 && input.e-input.r < INPUT_BUF){
				c = (c == '\r') ? '\n' : c;
				
				if (c != 226 && c != 227){
					input.buf[input.e++ % INPUT_BUF] = c;
					consputc(c);
				}
				
				if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
					input.w = input.e;
					wakeup(&input.r);
				}
			}
			break;
		}
	}
	release(&cons.lock);
	arrow_handling(char_test);
	if(doprocdump) {
		procdump();  // now call procdump() wo. cons.lock held
	}
}

typedef struct tuple{
	char string[INPUT_BUF];
	int edited;
} tuple;

tuple tp1 = {.edited = 0, .string = {[127] = '\0'}};
tuple tp2 = {.edited = 0, .string = {[127] = '\0'}};
tuple tp3 = {.edited = 0, .string = {[127] = '\0'}};

typedef struct history{
	tuple* tuples[3];
	int read, write, current_pos;
	
} history;
history hist = {.read = 0, .write = 0, .current_pos = 0 , .tuples = {&tp1, &tp2, &tp3}};


int
consoleread(struct inode *ip, char *dst, int n)
{
	uint target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){
		while(input.r == input.w){
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if(c == C('D')){  // EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
				history_buff(c, 1);
			}
			break;
		}

		*dst++ = c;
		--n;
		if(c == '\n'){
			history_buff(c, 1);
			break;
		}
		history_buff(c,0);
			
	}
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for(i = 0; i < n; i++)
		consputc(buf[i] & 0xff);
	release(&cons.lock);
	ilock(ip);

	return n;
}

void
consoleinit(void)
{
	initlock(&cons.lock, "console");

	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	cons.locking = 1;

	ioapicenable(IRQ_KBD, 0);
}



void history_buff(char current_char, int eol_flag){
	if (eol_flag){
		hist.tuples[hist.write]->edited = 1;
		hist.write = (++hist.write % 3);
		hist.current_pos = 0;
		return;
	}
	if (hist.tuples[hist.write]->edited){
		for (int i = 0; i < INPUT_BUF; i++){
			hist.tuples[hist.write]->string[i] = '\0';
		}
		hist.tuples[hist.write]->edited = 0;
	}
	hist.tuples[hist.write]->string[hist.current_pos++] = current_char;
	
}
// helper to print only even keyboard interups (get rid of key released)
static int once = 0;
//helper for up down problem
void arrow_handling(int keycode){
	
	// if (shift_flag && keycode == 226 && !(once&1)){
	// 	// ARROW UP CODE
	// 	hist.read = (hist.read - 1 + 3) % 3; // Move to the previous entry in a cyclic manner
	// 	write_to_buffer(hist.tuples[hist.read]->string);
	// }
	// else if (shift_flag && keycode == 227 && !(once&1)){
	// 	// ARROW DN CODE
	// 	hist.read = (hist.read + 1) % 3; // Move to the next entry in a cyclic manner
	// 	write_to_buffer(hist.tuples[hist.read]->string);
	// }
	if (shift_flag && keycode == 226 && !(once & 1)) {
    // ARROW UP CODE
    do {
        hist.read = (hist.read - 1 + 3) % 3; // Move to the previous entry in a cyclic manner
    } while (hist.tuples[hist.read]->string[0] == '\0'); // Skip empty strings
    	write_to_buffer(hist.tuples[hist.read]->string);
	} 	
	else if (shift_flag && keycode == 227 && !(once & 1)) {
    // ARROW DN CODE
    do {
        hist.read = (hist.read + 1) % 3; // Move to the next entry in a cyclic manner
    } while (hist.tuples[hist.read]->string[0] == '\0'); // Skip empty strings
    	write_to_buffer(hist.tuples[hist.read]->string);
	}
	once ++;
	

	//     input.buf[input.e++ % INPUT_BUF] = 'l';
    // consputc('l');
    // input.buf[input.e++ % INPUT_BUF] = 's';
    // consputc('s');
    // input.buf[input.e++ % INPUT_BUF] = '\n';
    // consputc('\n');

    // // Wake up any process waiting for input
    // input.w = input.e;
    // wakeup(&input.r);

}

void write_to_buffer(char* string){
	int counter = 0;
	int i,c,count;
	count = 1;
		while(input.e > input.r ){
			cgaputc(BACKSPACE);
			input.e--;
		}
	for(i = 0; (c = string[i] & 0xff) != 0; i++){
		input.buf[input.e++ % INPUT_BUF] = c;
		color_flag = 1;
		consputc(c);
		
	}
}