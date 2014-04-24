#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define SOCKET_NAME "/tmp/socket"

#define SPECIAL_FILE "/tmp/magic"

void
server(void)
{
	struct sockaddr_un sun = { 0 };
	socklen_t sunlen;
	int l, fd, magic;

	if ((magic = open(SPECIAL_FILE, O_RDWR|O_CREAT|O_TRUNC, 0600)) == -1)
		err(1, "open");
 
	if ((l = socket(PF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "server socket");

	unlink(SOCKET_NAME);

	sun.sun_family = AF_UNIX;
	snprintf(sun.sun_path, sizeof(sun.sun_path), SOCKET_NAME);

	if (bind(l, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "bind");

	if (listen(l, 5) == -1)
		err(1, "listen");

	sunlen = sizeof(sun);

	while ((fd = accept(l, (struct sockaddr *)&sun, &sunlen)) != -1) {
		struct msghdr msg = { 0 };
		struct cmsghdr *cmsg;
		struct iovec iov;
		char buf[CMSG_SPACE(sizeof(magic))];

		iov.iov_base = (void *)"ok";
		iov.iov_len = 2;

		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = buf;
		msg.msg_controllen = sizeof(buf);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(magic));

		*((int *)(void *)CMSG_DATA(cmsg)) = magic;

		if (sendmsg(fd, &msg, 0) == -1)
			err(1, "sendmsg");
		close(fd);	
	}

	close(l);
	unlink(SOCKET_NAME);
}

void
client(void)
{
	struct sockaddr_un sun = { 0 };
	struct msghdr msg = { 0 };
	struct cmsghdr *cmsg;
	struct iovec iov;
	int fd, magic;
	char cbuf[CMSG_SPACE(sizeof(int))];
	char buf[16];

	if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1) {
		err(1, "socket");
	}
 
	sun.sun_family = AF_UNIX;
	snprintf(sun.sun_path, sizeof(sun.sun_path), SOCKET_NAME);

	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		err(1, "connect");
	}

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);

	if (recvmsg(fd, &msg, 0) == -1)
		err(1, "recvmsg");

	cmsg = CMSG_FIRSTHDR(&msg);
	assert(cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS);
	magic = *(int *)(void *)CMSG_DATA(cmsg);

	if (write(magic, "hello", strlen("hello")) != strlen("hello"))
		err(1, "write(magic)");

	close(magic);
	close(fd);
}

int
main(int argc, char **argv)
{
	switch (fork()) {
	case 0:
		sleep(2);		/* poor mans synchronization */
		printf("starting client\n");
		client();
		_exit(0);
		break;
	case -1:
		err(1, "fork");
	default:
		printf("starting server\n");
		server();
		exit(1);
	}
	return 1;
}
