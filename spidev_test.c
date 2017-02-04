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

static const char *device = "/dev/spidev0.0";
static uint8_t mode = 0x00;
static uint8_t bits = 8;
static uint32_t speed = 50000;
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

static void setRegister(int fd, uint8_t reg, uint32_t length, uint8_t * src) {
	uint8_t byteLength = 0;
	if(length>=255) {
		byteLength = 0xff;
	} else {
		byteLength = length;
	}
	
	sendCommand(fd,reg | 0b10000000,byteLength);
	
	sendData(fd,length,src);
	
	printf("Register write end length=%d \n",length);
}

static void initSPI(int fd) {
	int ret = 0;
	if (fd < 0)
		pabort("can't open device");

	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");
}

int main(int argc, char *argv[])
{
	
	int fd;
	uint8_t glflag = 1;

	fd = open(device, O_RDWR);
	initSPI(fd);

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

	sendCommand(fd,0x00,0x01);
	uint8_t zeroes = 0;
	recvData(fd,1,&zeroes);
	if(zeroes!=0x00)
		glflag = 0;
	printf("Reading 0x00 register: 0x%02x\n",zeroes);
	
	sendCommand(fd,0x01,0x01);
	uint8_t ones = 0;
	recvData(fd,1,&ones);
	if(ones!=0xFF)
		glflag = 0;
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
	for(int i=0; i < 3; i++) {
	getRegister(fd,0x7d,800,out);
	printf("Cycle #%d\n",i);
	}
	usleep(1000);
	getRegister(fd,0x7c,128,out);
	
	
	sendCommand(fd,0x01,0x01);
	ones = 0;
	recvData(fd,1,&ones);
	if(ones!=0xFF)
		glflag = 0;
	printf("Reading 0x01 register: 0x%02x\n",ones);
	
	sendCommand(fd,0x00,0x01);
	zeroes = 0;
	recvData(fd,1,&zeroes);
	if(zeroes!=0x00)
		glflag = 0;
	printf("Reading 0x00 register: 0x%02x\n",zeroes);
	
	sendCommand(fd,0x00,0x01);
	zeroes = 0;
	recvData(fd,1,&zeroes);
	if(zeroes!=0x00)
		glflag = 0;
	printf("Reading 0x00 register: 0x%02x\n",zeroes);
	
	sendCommand(fd,0x00,0x01);
	zeroes = 0;
	recvData(fd,1,&zeroes);
	if(zeroes!=0x00)
		glflag = 0;
	printf("Reading 0x00 register: 0x%02x\n",zeroes);
	
	for(int y=0; y<100; y++)
	for(int j=0; j<8; j++) {
		uint8_t tvg[150];
		for(int i=0; i<150; i++) {
			tvg[i] = i + j;
		}
		setRegister(fd,0x40 + j,150,tvg);
		uint8_t read_tvg[150];
		getRegister(fd,0x40 + j,150,read_tvg);
		uint8_t flag = 1;
		for(int k=0; k<150; k++) {
			if(tvg[k] != read_tvg[k]) {
				flag = 0;
				glflag = 0;
			} 
		}
		if(flag) {
			printf("\nRead OK!     \n");
		} else {
			printf("\n\n\nRead not OK	!\n");
		}
	}
	
	if(glflag) {
		printf("\nGL Read OK!     \n");
	} else {
		printf("\n\n\nGL Read not OK	!\n");
	}
	
	close(fd);

	return 0;
}
