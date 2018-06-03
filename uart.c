#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common.h"

#define DEV_UART_PATH_0  "/dev/ttyS0"
#define DEV_UART_PATH_1  "/dev/ttyS1"
#define DEV_UART_PATH_2  "/dev/ttyS2"
#define DEV_UART_PATH_3  "/dev/ttyS3"
#define DEV_UART_PATH_4  "/dev/ttyS4"
#define DEV_UART_PATH_5  "/dev/ttyS5"
#define DEV_UART_PATH_6  "/dev/ttyS6"
#define DEV_UART_PATH_7  "/dev/ttyS7"
#define DEV_UART_PATH_8  "/dev/ttyS8"


#define BUF_LEN		256
#define DEFAULT_TEST_STRING	"0123456789abcdefghijklmnopqrstuvwxyZABCDEFGHIJKLMNOPQRSTUVWXYZ123456789abcd"

#define PARITY_NONE	0
#define PARITY_EVEN	1
#define PARITY_ODD	2


#if 0
static void usage(const char * app)
{
	printf("	USART TEST TOOL\n");
	printf("USAGE:\n");
	printf(" %s <dev> <bau> r	- USART Revceive\n", app);
	printf(" %s <dev> <bau> t [str]	- USART Send String(def 0~z)\n", app);
	printf(" %s <dev> <bau> a	- USART Auto Loop Back\n", app);
	printf("dev:\n");
	printf("	1 - ttyS1\n");
	printf("	2 - ttyS2\n");
	printf("	...");
	printf("bau:\n");
	printf("	b600\n");
	printf("	b2400\n");
	printf("	b4800\n");
	printf("	b9600\n");
	printf("	b19200\n");
	printf("	b38400\n");
	printf("	b57600\n");
	printf("	b115200\n");
}
#endif

#define MAX_COM_NUM	8

struct tty_elem {
	unsigned char stop;
	unsigned char parity;
	unsigned char size;
	unsigned char cmd;
	unsigned char len;  // rlen or tlen
	unsigned char times;
	unsigned char quiet;
	unsigned char timeout;
	char str[BUF_LEN];
	char com[DEVICE_NAME_LEN];
	char file[FILE_NAME_LEN];
	speed_t speed;
};

enum opt_index {
	OPT_C = 0,
	OPT_B,
	OPT_L,
	OPT_T,
	OPT_R,
	OPT_LOOP,
	OPT_ACTIVE,
	OPT_PASSIVE,
	OPT_TIMES,
	OPT_F,
	OPT_QUITE,
	OPT_S,
	OPT_TR,
	OPT_TIMEOUT,
};

enum cmd_index {
	CMD_SEND = 1,
	CMD_RECV,
	CMD_LOOP,
	CMD_ACTIVE,
	CMD_PASSIVE,
	CMD_SEND_FILE,
	CMD_TR,
};

static const char cmd_desc[][MAX_OPTION_LEN] = {"NULL", "send", "recv", "loop", "active", "passive", "send file", "tx&rx"};
static const char options[][MAX_OPTION_LEN] = {"-c", "-b", "-l", "-t", "-r", "-loop", "-active", "-passive", "-ts", "-f", "-q", "-s", "-tr", "-tmo"};

static void usage(const char *app)
{
	printf("\n\t串口调试助手\n");
	printf("使用方法:\n");
	printf("%s -c 端口 [选项] [命令] [数据]\n", app);
	printf("-c, 端口\n");
	printf("	0 - ttyS0\n");
	printf("	1 - ttyS1\n");
	printf("	...\n");
	printf("\n[选项描述]：\n");
	printf("-b, 波特率, b600 ~ b115200之间(默认b9600)\n");
	printf("-l, 链路选项, 数据位(5/6/7/8)校验位(n/e/o)停止位(1/2),如-l 8n1\n");
	printf("\n[命令描述]：\n");
	printf("-t, 发送数据命令\n");
	printf("-r, 接收数据命令，线程方式\n");
	printf("-tr, 单端口收发同是进行,可实时输入发送值，并观察返回值\n");
	printf("-loop, 同一端口的还回测试，用于全双工测试\n");
	printf("-active, 主动测试端口\n");
	printf("-passive, 被动测试端口, 主、被动端口之间还回测试\n");
	printf("\n[数据描述]：\n");
	printf("-s, 通信的字符串, 如:-s abcdefg...\n");
	printf("-f, 发送文件, 如-f /tmp/filename.txt\n");
	printf("\n[其他选项]:\n");
	printf("-ts, 测试次数(默认10次)\n");
	printf("-tmo, 读超时时间(s),默认一直阻塞\n");
	printf("-q, 不打印日志信息\n");
	printf("-h, 帮助\n");
}

//static int msg_lev = PRINT_LEVE_INFO;

char *speed_str(speed_t speed, char *buf)
{
	switch (speed) {
		case B600:	{ sprintf(buf, "B600");      break; }
		case B2400:	{ sprintf(buf, "B2400");     break; }
		case B4800:	{ sprintf(buf, "B4800");     break; }
		case B9600:	{ sprintf(buf, "B9600");     break; }
		case B19200:	{ sprintf(buf, "B19200");    break; }
		case B38400:	{ sprintf(buf, "B38400");    break; }
		case B57600:	{ sprintf(buf, "B57600");    break; }
		case B115200:	{ sprintf(buf, "B115200");   break; }
		default:	{ break; }
	}
	return buf;
}

void print_speed(speed_t speed)
{
	switch (speed) {
		case B600:	{ printf("B600\n");      break; }
		case B2400:	{ printf("B2400\n");     break; }
		case B4800:	{ printf("B4800\n");     break; }
		case B9600:	{ printf("B9600\n");     break; }
		case B19200:	{ printf("B19200\n");    break; }
		case B38400:	{ printf("B38400\n");    break; }
		case B57600:	{ printf("B57600\n");    break; }
		case B115200:	{ printf("B115200\n");   break; }
		default:	{ printf("unsupport");   break; }
	}
}

speed_t get_speed(const char *str)
{
	if 	(0==strcmp(str, "b600"))	return B600;
	else if	(0==strcmp(str, "b2400"))	return B2400;
	else if (0==strcmp(str, "b4800"))	return B4800;
	else if (0==strcmp(str, "b9600"))	return B9600;
	else if (0==strcmp(str, "b19200"))	return B19200;
	else if (0==strcmp(str, "b38400"))	return B38400;
	else if (0==strcmp(str, "b57600"))	return B57600;
	else if (0==strcmp(str, "b115200"))	return B115200;
	else return B9600;
}

int get_parity(const char *str)
{
	if 	(0==strcmp(str, "none"))	return PARITY_NONE;
	else if	(0==strcmp(str, "even"))	return PARITY_EVEN;
	else if (0==strcmp(str, "odd"))		return PARITY_ODD;
	else					return PARITY_NONE;
}

inline void get_line_ctrl(const char *str, struct tty_elem *opt)
{
	opt->size = str[0] - '0';
	if(opt->size < 5 || opt->size > 8)
		opt->size = 8;
	if('e' == str[1]) opt->parity = PARITY_EVEN;
	else if	('o' == str[1]) opt->parity = PARITY_ODD;
	else opt->parity = PARITY_NONE;
	opt->stop = str[2] == '2' ? 2 : 1;
}

int get_dev_name(char *dev, char *name)
{
	switch (dev[0]) {
		case '1':
			strcpy(name, DEV_UART_PATH_1);
			return 0;
		case '2':
			strcpy(name, DEV_UART_PATH_2);
			return 0;
		case '3':
			strcpy(name, DEV_UART_PATH_3);
			return 0;
		case '4':
			strcpy(name, DEV_UART_PATH_4);
			return 0;
		case '5':
			strcpy(name, DEV_UART_PATH_5);
			return 0;
		case '6':
			strcpy(name, DEV_UART_PATH_6);
			return 0;
		case '7':
			strcpy(name, DEV_UART_PATH_7);
			return 0;
		case '8':
			strcpy(name, DEV_UART_PATH_8);
			return 0;
		case '0':
			strcpy(name, DEV_UART_PATH_0);
			return 0;
		default:
			return -1;
	}
	return 0;
}

static void show_configuration(struct tty_elem opt)
{
	char buf[100] = {0};
	char baud[10] = {0};
	msg_print(PLV_INFO, 
		  "端口: %s 波特率:%s 属性:%d%c%d 模式:%s 通信帧长:%d(B)\n",
		  &opt.com[5],
		  speed_str(opt.speed, baud),
		  opt.size,
		  opt.parity==PARITY_ODD?'O':(opt.parity==PARITY_EVEN?'E':'N'),
		  opt.stop,
		  cmd_desc[opt.cmd],
		  opt.len);
}

static int analysis_options(char **argv, char argc, struct tty_elem *opt)
{

	unsigned char len = sizeof(options) / MAX_OPTION_LEN;
	unsigned char port;
	int i = 1, j;
	// analysis commond line
	for(i = 1; i < argc; i++) {
		for(j = 0; j < len; j++) {
			if(strcmp(options[j], argv[i]))
				continue;
			else
				break;
		}
		if(j == len) {
			printf("未知选项 : %s\n", argv[i]);
			return -1;
		}
		switch(j) {
		case OPT_C :
			i++;
			if(i == argc || argv[i][0] == '-') {
				printf("请指定端口号, 如(-c 3)\n");
				return -1;
			}
			if(get_dev_name(argv[i], opt->com)) {
				printf("无效的端口号\n");
				return -1;
			}
			break;
		case OPT_B :
			i++;
			if(i == argc || argv[i][0] == '-') {
				printf("无效的波特率设置，如(-b b9600)\n");
				return -1;
			}
			opt->speed = get_speed(argv[i]);
			break;
		case OPT_L :
			i++;
			if(i == argc || argv[i][0] == '-' || strlen(argv[i]) != 3)  {
				printf("无效的链路设置，如(-l 8n1)\n");
				return -1;
			}
			get_line_ctrl(argv[i], opt);
			break;
#if 0
		case OPT_S :
			i++;
			if(i == argc || argv[i][0] == '-') {
				printf("无效的停止位设置, 如(-s 1)\n");
				return -1;
			}
			opt->stop =  argv[i][0] == '2' ? 2 : 1;
			break;
		case OPT_P :
			i++;
			if(i == argc || argv[i][0] == '-') {
				printf("无效的校验位设置, 如(-p odd)\n");
				return -1;
			}
			opt->parity = get_parity(argv[i]);
			break;
		case OPT_D :
			i++;
			if(i == argc || argv[i][0] == '-') {
				printf("无效的数据位设置, 如(-d 7)\n");
				return -1;
			}
			opt->size = int_str_to_int(argv[i]);
			if(opt->size < 5 || opt->size > 8) { //5 6 7 8
				//printf("无效的数据位长度(%d)\n", elem->num);
				opt->size = 8;
			}
			break;
#endif
		case OPT_T :
			opt->cmd = CMD_SEND;
			break;
		case OPT_R :
			opt->cmd = CMD_RECV;
			break;
			break;
		case OPT_LOOP :
			opt->cmd = CMD_LOOP;
			break;
		case OPT_ACTIVE :
			opt->cmd = CMD_ACTIVE;
			break;
		case OPT_PASSIVE :
			opt->cmd = CMD_PASSIVE;
			break;
		case OPT_TIMES :
			i++;
			if(i == argc || argv[i][0] == '-') {
				printf("无效的测试次数设置, 如(-t 100)\n");
				return -1;
			}
			opt->times = int_str_to_int(argv[i]);
			if(opt->times <= 0)
				opt->times = 10;
			break;
		case OPT_F :
			i++;
			if(i == argc || argv[i][0] == '-') {
				printf("无效的文件名设置, 如(-f /tmp/xxx.txt)\n");
				return -1;
			}
			strncpy(opt->file, argv[i], FILE_NAME_LEN);
			break;
		case OPT_QUITE :
			msg_level = PLV_WARING;
			break;
		case OPT_S:
			i++;
			if(i == argc || argv[i][0] == '-') {
				printf("无效的数据指定, 如(-s abcdefg...)\n");
				return -1;
			}
			strncpy(opt->str, argv[i], BUF_LEN);
			break;
		case OPT_TR:
			opt->cmd = CMD_TR;
			break;
		case OPT_TIMEOUT:
			i++;
			if(i == argc || argv[i][0] == '-') {
				printf("无效的超时时间设置, 如(-tmo 20)\n");
				return -1;
			}
			opt->timeout = int_str_to_int(argv[i]);
			break;
		default:
			break;
		}
	}
	// fill the default value
	if(0 == opt->com[0]) {
		printf("请指定测试端口, 如(-c 5)\n");
		return -1;
	}
	if(0 == opt->cmd) {
		printf("帅锅, 你要干哈子??\n");
		return -1;
	}
	if(0 == opt->stop)
		opt->stop = 1;
	if(0 == opt->parity)
		opt->parity = PARITY_NONE;
	if(0 == opt->size)
		opt->size = 8;
	if(0 == opt->times)
		opt->times = 1;
	if(0 == opt->quiet) {
		opt->quiet = DEFAULT_PRINT_LEVEL;
		msg_level = DEFAULT_PRINT_LEVEL;
	}
	if(0 == opt->str[0])
		strncpy(opt->str, DEFAULT_TEST_STRING, BUF_LEN);
	opt->len = strlen(opt->str);
		//opt->str = DEFAULT_TEST_STRING;
	//if(0 == opt->file)
	//	opt->
	if(0 == opt->speed)
		opt->speed = B9600;
	if(0 == opt->timeout)
		opt->timeout = 0;
	//show_configuration(opt);
	return 0;
}


int init_uart(struct tty_elem tty)
{
	int fd;
	struct termios st_newtio;

	// open device
	fd = open(tty.com, O_RDWR|O_NOCTTY/*|O_NDELAY*/);
	if (fd < 0) {
		printf("打开 %s 失败\n", tty.com);
		return fd;
	}

	/*
	 * FIXME: 当打开文件描述符时，设置了flag=O_RDWR|O_NOCTTY
	 *        然后这里fcntl(fd, F_SETFL, 0) clear了flag?? 为什么？
	 * SOLVED: from man page
	 *	F_SETFL (int)
	 *	Set the file status flags to the value specified by arg.  File access mode (O_RDONLY,  O_WRONLY,  O_RDWR)  and  file
	 *	creation flags (i.e., O_CREAT, O_EXCL, O_NOCTTY, O_TRUNC) in arg are ignored.  On Linux this command can change only
	 *	the O_APPEND, O_ASYNC, O_DIRECT, O_NOATIME, and O_NONBLOCK flags.
	 *	也就是说fcntl(fd, F_SETFL, 0)只是清除了了O_APPEND, O_ASYNC, O_DIRECT, O_NOATIME, and O_NONBLOCK 位。
	 *	通过F_GETFL也能证实Flag其实是没有变的。
	 */
	// clear O_NONBLOCK set Block.
	fcntl(fd, F_SETFL, 0);

	// clear struct for new port settings
	bzero(&st_newtio, sizeof(st_newtio));
	// configure baudrate;
	cfsetispeed(&st_newtio, tty.speed); // set input baudrate
	cfsetospeed(&st_newtio, tty.speed); // set output baudrate

	// ignore modem control lines and enable receiver.
	st_newtio.c_cflag |=CLOCAL|CREAD;

	// configure data bits
	st_newtio.c_cflag &= ~CSIZE;	// Mask the character size bits
	switch(tty.size) {
		case 5 : st_newtio.c_cflag |= CS5; break; // BUG
		case 6 : st_newtio.c_cflag |= CS6; break; // BUG
		case 7 : st_newtio.c_cflag |= CS7; break;
		case 8 : st_newtio.c_cflag |= CS8; break;
		default: st_newtio.c_cflag |= CS8; break;
	}
	
	/*
	 * NOTE:
	 * PARENB - 校验位使能开关，0表示无校验，1表示有校验
	 * PARODD - 奇校验开关, PARENB=0时，无效，PARENB = 1时：
	 *          0表示偶校验，1表示奇校验
	 */

	// configure parity
	if(PARITY_EVEN  == tty.parity) {
		st_newtio.c_cflag &= ~PARODD; // even
		st_newtio.c_cflag |= PARENB;
	} else if(PARITY_ODD  == tty.parity) {
		st_newtio.c_cflag |= (PARODD | PARENB); // odd
	} else {
		st_newtio.c_cflag &= ~PARENB; // none
	}
	/*
	 * NOTE: CSTOPB - 0表示1bit停止位，1表示2bits停止位
	 */
	// configure stop bits
	if(2 == tty.stop)
		st_newtio.c_cflag |= CSTOPB;	// 2 stop bit
	else
		st_newtio.c_cflag &= ~CSTOPB;	// 1 stop bit

	// disable hardware flow control;
	st_newtio.c_cflag &= ~CRTSCTS;
	// raw input
	st_newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	// raw output
	st_newtio.c_oflag &= ~OPOST;
	// clear input buffer
	tcflush(fd, TCIFLUSH);

	// configure timeout strategy
	// blockin
	st_newtio.c_cc[VTIME] = 0;
	// blocking read until 1 character arrives
	st_newtio.c_cc[VMIN] = 1;

	tcsetattr(fd, TCSANOW, &st_newtio);

	show_configuration(tty);

	return fd;
}

int do_uart_send(int fd, const char *tx)
{
	int ret;
	tcflush(fd, TCOFLUSH);
	ret = write(fd, tx, strlen(tx));
	printf("发送字符串(%d):%s\n", ret, tx);
	return ret;
}

#if 1
int do_uart_recv(int fd, unsigned char rlen, unsigned char tmo)
{
	char ch;
	int len, cnt;
	fd_set fds;
	struct timeval timeout;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tcflush(fd, TCIFLUSH); // clear receive
	tcflush(fd, TCOFLUSH);
	while(1) {
		if(tmo) {
			timeout.tv_sec = tmo; // 30s time out.
			timeout.tv_usec = 0;
			// 这里的退出阻塞的条件是达到vmin的条件。
			// 也就是说，如果vmin = 2时，当收到1个字节，这个
			// 函数同样不会退出阻塞状态。
			if(select(fd+1, &fds, NULL, NULL, &timeout) <= 0) {
				printf("接收超时\n");
				// 如果VMIN条件为达到，这里超时后，
				// 不break，执行read时，同样会被阻塞在read。
				break; 
			}
		}
		read(fd, &ch, 1);
		printf("%c", ch);
		fflush(stdout);
	}
	// do not return, use ctrl + c to cut it,
	// data recover by systerm
	return 0;
}
#endif

#if 0  // for test...
int do_uart_recv(int fd, unsigned char rlen, unsigned char tmo)
{
	char ch[10] = {0};
	int len, cnt;
	fd_set fds;
	struct timeval timeout;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tcflush(fd, TCIFLUSH); // clear receive
	tcflush(fd, TCOFLUSH);
	while(1) {
		if(tmo) {
			timeout.tv_sec = tmo; // 30s time out.
			timeout.tv_usec = 0;
			if(select(fd+1, &fds, NULL, NULL, &timeout) <= 0) {
				printf("接收超时\n");
				break;
			}
		}
		len = read(fd, ch, 5);
		ch[len] = 0;
		printf("recv(%d): %s\n",len, ch);
		fflush(stdout);
	}
	// do not return, use ctrl + c to cut it,
	// data recover by systerm
	return 0;
}
#endif
	
void *do_rx_thread(void *arg)
{
	int fd = *(int *)arg;
	char ch;
	//fd_set fds;
	//struct timeval timeout;
	//FD_ZERO(&fds);
	//FD_SET(fd, &fds);

	tcflush(fd, TCIFLUSH); // clear receive
	tcflush(fd, TCOFLUSH);
	printf("接收数据中...\n");
	while(1) {
		//timeout.tv_sec = 30; // 20s time out.
		//timeout.tv_usec = 0;
		//if(select(fd+1, &fds, NULL, NULL, &timeout) <= 0) {
		//	printf("接收超时\n");
		//	break;
		//}
		read(fd, &ch, 1);
		printf("%c", ch);
		fflush(stdout);
	}
}

int do_uart_txrx(int fd)
{
	int ret, i;
	char inch;
	char buf[BUF_LEN] = {0};
	pthread_t rxthread;
	pthread_create(&rxthread, NULL, (void *)&do_rx_thread, &fd);
	tcflush(fd, TCOFLUSH);
	sleep(2); // 确保线程中 “接收数据中...”完整的打印出来， 和线程准备好
	while(1) {
		printf("\n输入:");
		scanf("%s", buf);
		fflush(stdout);
		printf("收到:");
		write(fd, buf, strlen(buf)); // fixme: 第一组数据尾巴会掉几个字节。

		sleep(2); // 确保线程收到数据已经打印完成。！
		printf("\n敲回车继续发送，敲Esc退出程序\n");
		scanf("%c", &inch);
		if(27 == inch) // Esc
			break;
	}
	return 0;
}

int do_uart_loop(int fd, const char *tx, unsigned char tms)
{

	int i, l, cnt = tms;
	int len = strlen(tx);
	char rx_buf[BUF_LEN] = {0};
	char ch;
	fd_set fds;
	struct timeval timeout;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tcflush(fd, TCIFLUSH); // clear receive
	tcflush(fd, TCOFLUSH);

	while(cnt--) {
		write(fd, tx, len);
		msg_print(PLV_INFO, "发送(%d):%s\n", cnt, tx);

		for(i = 0; i < len; i++) {
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
			if(select(fd+1, &fds, NULL, NULL, &timeout) <= 0) {
				printf("接收超时\n");
				goto error_out;
			}
			read(fd, &ch, 1);
			rx_buf[i] = ch;
		}
		rx_buf[i+1] = 0;
		msg_print(PLV_INFO, "接收(%d):%s\n", cnt, rx_buf);
		if(0 != memcmp(rx_buf, tx, len))
			goto error_out;
	}
	msg_print(PLV_RESULT, "\n还回测试	[成功]\n");
	return 0;
error_out:
	msg_print(PLV_RESULT, "\n还回测试	[失败]\n");
	return -1;
}

int do_uart_passive(int fd, unsigned char len)
{
	int i;
	fd_set fds;
	struct timeval timeout;
	char rx_buf[BUF_LEN] = {0};
	char ch;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	//timeout.tv_sec = 0;
	//timeout.tv_usec = 0;

	tcflush(fd, TCIFLUSH); // clear receive
	tcflush(fd, TCOFLUSH);

	while(1) {
		printf("接收:");
		fflush(stdout);
		for(i = 0; i < len; i++) {
			//if(select(fd+1, &fds, NULL, NULL, &timeout) <= 0) {
			//	//printf("接收超时\n");
			//	break;
			//}
			read(fd, &ch, 1); //如果程序阻塞在这儿，下面那一句printf是打印不出来的。
			rx_buf[i] = ch;
			printf("%c", ch);
			fflush(stdout);
		}
		printf("\n");
		rx_buf[i+1] = 0;
		//printf("接收:%s\n", rx_buf);
		write(fd, rx_buf, i);
		//sleep(1);
	}
	// as a server, do not return.
	return 0;
}

/*
 * Use to test half-duplex communications.
 */
int do_uart_active(int fd, const char *tx, unsigned char tms)
{
	int i, cnt = tms;
	char ch;
	fd_set fds;
	char tx_buf[BUF_LEN] = {0};
	char rx_buf[BUF_LEN] = {0};
	struct timeval timeout;
	tcflush(fd, TCIFLUSH); // clear receive
	tcflush(fd, TCOFLUSH);

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	//timeout.tv_sec = 5;
	//timeout.tv_usec = 0;

	while(cnt--) {
		write(fd, tx, strlen(tx));
		msg_print(PLV_INFO, "发送:%s\n", tx);
		for(i = 0; i < strlen(tx); i++) {
			timeout.tv_sec = 3; //NOTE：每次都需要重置时间片！不然时间会逐渐减少到0，然后超时。
			timeout.tv_usec = 0;
			if(select(fd+1, &fds, NULL, NULL, &timeout) <= 0) {
				printf("接收超时\n");
				if(i > 0)
					printf("接收:%s\n", rx_buf);
				goto error_out;
				//break;
			}
			read(fd, &ch, 1);
			rx_buf[i] = ch;
		}
		rx_buf[i+1] = 0;
		msg_print(PLV_INFO, "接收:%s\n", rx_buf);
		if(strncmp(rx_buf, tx, strlen(tx))) {
			goto error_out;
		}
	}
	msg_print(PLV_RESULT, "\n还回测试	[Success]\n");
	return 0;
error_out:
	msg_print(PLV_RESULT, "\n还回测试	[Fail]\n");
	return -1;
}

int do_uart_send_file()
{
	return 0;
}

int uart_send_recv(int fd, char *tx_str, char *rx_str)
{
	int ret, i;
	char ch;
	fd_set fds;
	struct timeval timeout;
	tcflush(fd, TCIFLUSH); // clear receive
	tcflush(fd, TCOFLUSH);
	
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	write(fd, tx_str, strlen(tx_str));
	for(i = 0; i < BUF_LEN; i++) {
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		if(select(fd+1, &fds, NULL, NULL, &timeout) <= 0) {
			if(i > 0)
				break;
			goto error_out;
		}
		read(fd, &ch, 1);
		rx_str[i] = ch;
	}
	rx_str[i + 1] = 0;
	return 0;
error_out:
	return -1;
}


#ifdef BUILD_WST_TEST_LIB
int uart_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	int i, cnt, fd;
	int ret = 0;
	struct tty_elem tty;
	//char buf_tx[BUF_LEN] = {0};
	//char buf_rx[BUF_LEN] = {0};
	//char buf[BUF_LEN] = {0};
	//fd_set fds;
	//struct timeval timeout;

	memset(&tty, 0, sizeof(struct tty_elem));

	if(analysis_options(argv, argc, &tty) < 0) {
		usage(argv[0]);
		return -1;
	}
	//show_configuration(tty);
	//return 0;
	fd = init_uart(tty);
	if(fd < 0) {
		return -1;
	}

	switch(tty.cmd) {
	case CMD_SEND :
		ret = do_uart_send(fd, tty.str);
		break;
	case CMD_RECV :
		ret = do_uart_recv(fd, 0, tty.timeout);
		break;
	case CMD_LOOP :
		ret = do_uart_loop(fd, tty.str, tty.times);
		break;
	case CMD_ACTIVE :
		ret = do_uart_active(fd, tty.str, tty.times);
		break;
	case CMD_PASSIVE :
		ret = do_uart_passive(fd, tty.len);
		break;
	case CMD_SEND_FILE :
		ret = do_uart_send_file(fd, tty.file);
		break;
	case CMD_TR :
		ret = do_uart_txrx(fd);
		break;
	default:
		break;
	}
	close(fd);
	return ret;
}
