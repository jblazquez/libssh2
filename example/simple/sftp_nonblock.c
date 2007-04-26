/*
 * $Id: sftp_nonblock.c,v 1.6 2007/04/26 22:59:29 gknauf Exp $
 *
 * Sample showing how to do SFTP non-blocking transfers.
 *
 * The sample code has default values for host name, user name, password
 * and path to copy, but you can specify them on the command line like:
 *
 * "sftp 192.168.0.1 user password /tmp/secrets"
 */

#include <libssh2.h>
#include <libssh2_sftp.h>

#ifdef HAVE_WINSOCK2_H
# include <winsock2.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
# ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

int main(int argc, char *argv[])
{
	unsigned long hostaddr;
	int sock, i, auth_pw = 1;
	struct sockaddr_in sin;
	const char *fingerprint;
	LIBSSH2_SESSION *session;
	char *username=(char *)"username";
	char *password=(char *)"password";
	char *sftppath=(char *)"/tmp/TEST";
	int rc;
	LIBSSH2_SFTP *sftp_session;
	LIBSSH2_SFTP_HANDLE *sftp_handle;

#ifdef WIN32
	WSADATA wsadata;

	WSAStartup(WINSOCK_VERSION, &wsadata);
#endif

	if (argc > 1) {
		hostaddr = inet_addr(argv[1]);
	} else {
		hostaddr = htonl(0x7F000001);
	}

	if(argc > 2) {
		username = argv[2];
	}
	if(argc > 3) {
		password = argv[3];
	}
	if(argc > 4) {
		sftppath = argv[4];
	}
	/*
	 * The application code is responsible for creating the socket
	 * and establishing the connection
	 */
	sock = socket(AF_INET, SOCK_STREAM, 0);

	sin.sin_family = AF_INET;
	sin.sin_port = htons(22);
	sin.sin_addr.s_addr = hostaddr;
	if (connect(sock, (struct sockaddr*)(&sin),
		    sizeof(struct sockaddr_in)) != 0) {
		fprintf(stderr, "failed to connect!\n");
		return -1;
	}

	/* We set the socket non-blocking. We do it after the connect just to
	   simplify the example code. */
#ifdef F_SETFL
	/* FIXME: this can/should be done in a more portable manner */
	rc = fcntl(sock, F_GETFL, 0);
	fcntl(sock, F_SETFL, rc | O_NONBLOCK);
#else
#error "add support for setting the socket non-blocking here"
#endif

	/* Create a session instance
	 */
	session = libssh2_session_init();
	if(!session)
		return -1;

	/* ... start it up. This will trade welcome banners, exchange keys,
	 * and setup crypto, compression, and MAC layers
	 */
	rc = libssh2_session_startup(session, sock);
	if(rc) {
		fprintf(stderr, "Failure establishing SSH session: %d\n", rc);
		return -1;
	}

	/* At this point we havn't yet authenticated.  The first thing to do
	 * is check the hostkey's fingerprint against our known hosts Your app
	 * may have it hard coded, may go to a file, may present it to the
	 * user, that's your call
	 */
	fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_MD5);
	printf("Fingerprint: ");
	for(i = 0; i < 16; i++) {
		printf("%02X ", (unsigned char)fingerprint[i]);
	}
	printf("\n");

	if (auth_pw) {
		/* We could authenticate via password */
		if (libssh2_userauth_password(session, username, password)) {
			printf("Authentication by password failed.\n");
			goto shutdown;
		}
	} else {
		/* Or by public key */
		if (libssh2_userauth_publickey_fromfile(session, username,
							"/home/username/.ssh/id_rsa.pub",
							"/home/username/.ssh/id_rsa",
							password)) {
			printf("\tAuthentication by public key failed\n");
			goto shutdown;
		}
	}

	fprintf(stderr, "libssh2_sftp_init()!\n");
	sftp_session = libssh2_sftp_init(session);

	if (!sftp_session) {
		fprintf(stderr, "Unable to init SFTP session\n");
		goto shutdown;
	}

	/* Since we have set non-blocking, tell libssh2 we are non-blocking */
	libssh2_sftp_set_blocking(sftp_session, 0);
	
	fprintf(stderr, "libssh2_sftp_open()!\n");
	/* Request a file via SFTP */
	sftp_handle =
		libssh2_sftp_open(sftp_session, sftppath, LIBSSH2_FXF_READ, 0);

	if (!sftp_handle) {
		fprintf(stderr, "Unable to open file with SFTP\n");
		goto shutdown;
	}
	fprintf(stderr, "libssh2_sftp_open() is done, now receive data!\n");
	do {
		char mem[1024];

		/* loop until we fail */
		fprintf(stderr, "libssh2_sftp_readnb()!\n");
        while ((rc = libssh2_sftp_readnb(sftp_handle, mem, sizeof(mem))) == LIBSSH2SFTP_EAGAIN) {
            ;
        }
		if (rc > 0) {
			write(2, mem, rc);
		} else {
			break;
        }
	} while (1);

	libssh2_sftp_close(sftp_handle);
	libssh2_sftp_shutdown(sftp_session);

 shutdown:

	libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
	libssh2_session_free(session);

#ifdef WIN32
	Sleep(1000);
	closesocket(sock);
#else
	sleep(1);
	close(sock);
#endif
printf("all done\n");
	return 0;
}
