/************************************
Copyright 2012 Analog Devices, Inc. (ADI)
Permission to freely use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies: THIS SOFTWARE
IS PROVIDED “AS IS” AND ADI DISCLAIMS ALL WARRANTIES INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL ADI BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM ANY USE OF SAME, INCLUDING ANY LOSS OF USE OR DATA OR
PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTUOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
***************************************/
#include <corecrt.h>
#include <stdint.h>

uint16_t pec15Table[256];
uint16_t CRC15_POLY = 0x4599;
void	init_PEC15_Table()
{
	for (int i = 0; i < 256; i++) {
		uint16_t remainder = i << 7;
		for (int bit = 8; bit > 0; --bit) {
			if (remainder & 0x4000) {
				remainder = ((remainder << 1));
				remainder = (remainder ^ CRC15_POLY);
			} else {
				remainder = ((remainder << 1));
			}
		}
		pec15Table[i] = remainder & 0xFFFF;
	}
}

uint16_t pec15(char *data, int len)
{
	uint16_t remainder, address;
	remainder = 16; // PEC seed
	for (int i = 0; i < len; i++) {
		address = ((remainder >> 7) ^ data[i]) & 0xff; // calculate PEC
							       // table address
		remainder = (remainder << 8) ^ pec15Table[address];
	}
	return (remainder * 2); // The CRC15 has a 0 in the LSB so the final
				// value must be multiplied by 2
}

/* MAIN */
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s <FFFF..FF> (plain hex)\n", argv[0]);
		return 1;
	}

	uint8_t hex_data[256];
	char   *data = argv[1];
	size_t	len  = strlen(data) / 2;

	for (int i = 0; i < len; i++) {
		sscanf(&data[i * 2], "%02hhx", &hex_data[i]);
	}

	init_PEC15_Table();
	uint16_t crc = pec15((char *)hex_data, len);
	printf("CRC: 0x%04x\n", crc);

	return 0;
}
