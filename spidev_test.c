#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static const char *device = "/dev/spidev0.1";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 70000;
static uint16_t delay = 100;

static void sendCommand(int fd, uint8_t commandByte, uint8_t length) {
	uint8_t tx[2] = { commandByte, length };
	uint8_t rx[2] = { 0, 0 };
	
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
		.cs_change = 1,
	};

	printf("[tx: 0x%02x len: 0x%02x]\n",tx[0],tx[1]);
	int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	printf("[rx: 0x%02x rx+: 0x%02x]\n",rx[0],rx[1]);
	


	if (ret < 1)
		pabort("can't send command message");	
}

static void sendData(int fd, uint8_t length,const uint8_t * bufPtr) {
	uint8_t rx[4096];
	
	memset(rx,0x00,4096);
	
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)bufPtr,
		.rx_buf = (unsigned long)rx,
		.len = length,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
		.cs_change = 1,
	};
	
	int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

	if (ret < 1)
		pabort("can't send command message");	
}

static void recvData(int fd, uint32_t length, uint8_t * bufPtr) {
	uint8_t tx[4096];
	
	memset(tx,0x00,4096);
	
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)bufPtr,
		.len = length,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
		.cs_change = 1,
	};
	
	int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

	if (ret < 1)
		pabort("can't send command message");	
}

static void getRegister(int fd, uint8_t reg, uint32_t length, uint8_t * dest) {
	uint8_t byteLength = 0;
	if(length>=255) {
		byteLength = 0xff;
	} else {
		byteLength = length;
	}
	sendCommand(fd,reg,byteLength);
	
	memset(dest,0x00,length);
	
	recvData(fd,length,dest);
	
	printf("Printing register length=%d :",length);
	printf("\n==============================");
	
	for (int ret = 0; ret < length; ret++) {
		if (!(ret % 32))
			puts("");
		printf("%.2X ", dest[ret]);
	}
	
	printf("\n==============================\n");
}



static void print_usage(const char *prog)
{
	printf("Usage: %s [-DsbdlHOLC3]\n", prog);
	puts("  -D --device   device to use (default /dev/spidev1.1)\n"
	     "  -s --speed    max speed (Hz)\n"
	     "  -d --delay    delay (usec)\n"
	     "  -b --bpw      bits per word \n"
	     "  -l --loop     loopback\n"
	     "  -H --cpha     clock phase\n"
	     "  -O --cpol     clock polarity\n"
	     "  -L --lsb      least significant bit first\n"
	     "  -C --cs-high  chip select active high\n"
	     "  -3 --3wire    SI/SO signals shared\n");
	exit(1);
}

static void parse_opts(int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "device",  1, 0, 'D' },
			{ "speed",   1, 0, 's' },
			{ "delay",   1, 0, 'd' },
			{ "bpw",     1, 0, 'b' },
			{ "loop",    0, 0, 'l' },
			{ "cpha",    0, 0, 'H' },
			{ "cpol",    0, 0, 'O' },
			{ "lsb",     0, 0, 'L' },
			{ "cs-high", 0, 0, 'C' },
			{ "3wire",   0, 0, '3' },
			{ "no-cs",   0, 0, 'N' },
			{ "ready",   0, 0, 'R' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:s:d:b:lHOLC3NR", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'b':
			bits = atoi(optarg);
			break;
		case 'l':
			mode |= SPI_LOOP;
			break;
		case 'H':
			mode |= SPI_CPHA;
			break;
		case 'O':
			mode |= SPI_CPOL;
			break;
		case 'L':
			mode |= SPI_LSB_FIRST;
			break;
		case 'C':
			mode |= SPI_CS_HIGH;
			break;
		case '3':
			mode |= SPI_3WIRE;
			break;
		case 'N':
			mode |= SPI_NO_CS;
			break;
		case 'R':
			mode |= SPI_READY;
			break;
		default:
			print_usage(argv[0]);
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd;

	parse_opts(argc, argv);

	fd = open(device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

	//transfer(fd);
	
	sendCommand(fd,0x00,0x01);
	uint8_t zeroes = 0;
	recvData(fd,1,&zeroes);
	printf("Reading 0x00 register: 0x%02x\n",zeroes);
	
	sendCommand(fd,0x01,0x01);
	uint8_t ones = 0;
	recvData(fd,1,&ones);
	printf("Reading 0x01 register: 0x%02x\n",ones);
	
	uint8_t out[4096];
	usleep(1000);
	getRegister(fd,0x7c,32,out);
	usleep(1000);
	getRegister(fd,0x7d,64,out);
	usleep(1000);
	getRegister(fd,0x7c,32,out);
	usleep(1000);
	getRegister(fd,0x7d,128,out);
	usleep(1000);
	getRegister(fd,0x7c,600,out);
	usleep(1000);
	for(int i=0; i < 50; i++) {
	getRegister(fd,0x7d,800,out);
	printf("Cycle #%d\n",i);
	}
	usleep(1000);
	getRegister(fd,0x7c,128,out);
	
	
	sendCommand(fd,0x01,0x01);
	ones = 0;
	recvData(fd,1,&ones);
	printf("Reading 0x01 register: 0x%02x\n",ones);
	
	sendCommand(fd,0x00,0x01);
	zeroes = 0;
	recvData(fd,1,&zeroes);
	printf("Reading 0x00 register: 0x%02x\n",zeroes);
	
	sendCommand(fd,0x00,0x01);
	zeroes = 0;
	recvData(fd,1,&zeroes);
	printf("Reading 0x00 register: 0x%02x\n",zeroes);
	
	sendCommand(fd,0x00,0x01);
	zeroes = 0;
	recvData(fd,1,&zeroes);
	printf("Reading 0x00 register: 0x%02x\n",zeroes);
	

	close(fd);

	return ret;
}
