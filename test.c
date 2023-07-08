#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
	uint16_t foo = 0b0011010001011111;

	printf("foo long is %ul bin %016b, hex %04x, foo left 4 is %u, right %uld\n",foo,foo,foo,(foo&0x000f),(foo)>>4);

	/* work in hex instead */
	uint8_t bar[2];
	bar[0] = (foo & 0x00ff);
	bar[1] = (foo & 0xfff0);
	printf("bar[0]=%02x, bar[1]=%02x\n",bar[0],bar[1]);

	uint16_t orig = foo>>4;
	//++orig;
	uint16_t to = orig<<4 | (foo&0x000f);
	printf("orig %u to %u\n",orig,to);

	/*  Now back to hex */
	bar[0] = (to & 0xff00) >> 8;
	bar[1] = (to & 0x00FF);
	printf("bytes [%02x,%02x]\n",bar[0],bar[1]);

	return 0;
}
