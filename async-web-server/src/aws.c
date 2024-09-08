// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/eventfd.h>
#include <libaio.h>
#include <errno.h>

#include "aws.h"
#include "utils/util.h"
#include "utils/debug.h"
#include "utils/sock_util.h"
#include "utils/w_epoll.h"

/* server socket file descriptor */
static int listenfd;

/* epoll file descriptor */
static int epollfd;

static void connection_prepare_send_404(struct connection *conn)
{
	/* TODO: Prepare the connection buffer to send the 404 header. */
	// header ul pe care vreau sa l trimit in cazul in care nu pot deschide fisierul
	char *response = "HTTP/1.1 404 Not Found\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: O\r\n\r\n";

	snprintf(conn->send_buffer, BUFSIZ, "%s", response);

	conn->send_len = strlen(response);
	send(conn->sockfd, conn->send_buffer, conn->send_len, 0);
}

struct connection *connection_create(int sockfd)
{
	/* TODO: Initialize connection structure on given socket. */
	struct connection *conn = malloc(sizeof(*conn));

	conn->sockfd = sockfd;
	memset(conn->recv_buffer, 0, BUFSIZ);
	memset(conn->send_buffer, 0, BUFSIZ);
	conn->state = STATE_INITIAL;
	return conn;
}

void connection_start_async_io(struct connection *conn)
{
	/* TODO: Start asynchronous operation (read from file).
	 * Use io_submit(2) & friends for reading data asynchronously.
	 */
}

void connection_remove(struct connection *conn)
{
	// /* TODO: Remove connection handler. */
	if (conn->state != STATE_CONNECTION_CLOSED) { // verific daca a fost initiata conexiunea
		close(conn->sockfd);
		conn->state = STATE_CONNECTION_CLOSED;
		free(conn);
	}
}

void handle_new_connection(void)
{
	/* TODO: Handle a new connection request on the server socket. */
	struct sockaddr_in addr;
	struct connection *conn;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	int new_sockfd;
	int rc;

	/* TODO: Accept new connection. */
	new_sockfd = accept(listenfd, (struct sockaddr *)&addr, &addrlen);
	DIE(new_sockfd < 0, "accept");

	/* TODO: Set socket to be non-blocking. */
	int flags = fcntl(new_sockfd, F_GETFL, 0);

	fcntl(new_sockfd, F_SETFL, flags | O_NONBLOCK);

	/* TODO: Instantiate new connection handler. */
	conn = connection_create(new_sockfd);

	/* TODO: Add socket to epoll. */
	rc = w_epoll_add_ptr_in(epollfd, new_sockfd, conn);
	DIE(rc < 0, "w_epoll_add_in");
	/* TODO: Initialize HTTP_REQUEST parser. */
}

void connection_send_header(struct connection *conn)
{
	/* May be used as a helper function. */
	/* TODO: Send as much data as possible from the connection send buffer.
	 * Returns the number of bytes sent or -1 if an error occurred
	 */
	conn->file_size = lseek(conn->fd, 0, SEEK_END);
	lseek(conn->fd, 0, SEEK_SET);

	sprintf(conn->send_buffer, "HTTP/1.1 200 OK\r\n"
		"Content-Length: %ld\r\n"
		"Connection: close\r\n"
		"\r\n",
		conn->file_size);

	dlog(LOG_INFO, "Numarul de caractere trimise: %ld\n", conn->file_size);

	conn->send_len = strlen(conn->send_buffer);

	off_t offset = 0;

	// fac un loop pentru a trimite tot headerul
	while (offset < conn->send_len) {
		ssize_t bytes_sent = send(conn->sockfd, conn->send_buffer + offset, conn->send_len - offset, 0);

		if (bytes_sent <= 0) {
			dlog(LOG_INFO, "Terminat de trimis tot headerul\n");
			break;
		}
		offset += bytes_sent;
	}
}

void handle_input(struct connection *conn)
{
	/* TODO: Handle input information: may be a new message or notification of
	 * completion of an asynchronous I/O operation.
	 */
	ssize_t byte = 0;
	char abuffer[1024];
	size_t current_pos = 0;

	memset(conn->recv_buffer, 0, BUFSIZ);

	while (1) {
		byte = recv(conn->sockfd, abuffer, BUFSIZ, 0);

		if (byte <= 0) {
			handle_output(conn);
			break;
		}
		size_t remaining_space = sizeof(conn->recv_buffer) - current_pos;

		if (byte <= remaining_space) {
			strncat(conn->recv_buffer + current_pos, abuffer, byte);
			current_pos += byte;
			conn->recv_len = current_pos;
			conn->recv_buffer[current_pos] = '\0';
		}
	}
}

void set_filename(struct connection *conn)
{
	const char *path_start = strstr(conn->recv_buffer, "/");
	const char *path_end = strstr(path_start, ".dat") + 4;

	// calculez lungimea numelui fisierului
	size_t path_length = path_end - path_start;

	memset(conn->filename, 0, BUFSIZ);
	conn->filename[0] = '.';
	strncat(conn->filename, path_start, path_length);
	conn->filename[path_length + 2] = '\0';
}

int submit_aio_read(io_context_t ctx, int fd, void *buffer, off_t offset, struct iocb *iocb)
{
	io_prep_pread(iocb, fd, buffer, BUFSIZ, offset);
	return io_submit(ctx, 1, &iocb);
}

void connection_send_dynamic(struct connection *conn)
{
	/* TODO: Read data asynchronously.
	 * Returns 0 on success and -1 on error.
	 */
	int total_bytes = 0;

	io_setup(1, &conn->ctx);
	while (total_bytes != conn->file_size) {
		int file_fd = open(conn->filename, O_RDONLY);

		memset(conn->recv_buffer, 0, BUFSIZ);
		if (submit_aio_read(conn->ctx, file_fd, conn->recv_buffer, total_bytes, &conn->iocb) < 0) {
			close(file_fd);
			break;
		}

		struct io_event aio_events[1];

		io_getevents(conn->ctx, 1, 1, aio_events, NULL);

		int bytes_sent = send(conn->sockfd, conn->recv_buffer, BUFSIZ, 0);

		if (bytes_sent <= 0) {
			close(file_fd);
			break;
		}
		total_bytes += bytes_sent;

		close(file_fd);
	}
}

void connection_send_static(struct connection *conn)
{
	/* TODO: Send static data using sendfile(2). */
	off_t offset = 0;
	ssize_t sent_bytes = 0;

	sent_bytes = sendfile(conn->sockfd, conn->fd, &offset, BUFSIZ); // prima oara trimit o parte din fisieer
	conn->file_size -= sent_bytes;

	// fac un loop pentru a trimite tot fisierul
	while (conn->file_size > 0 && sent_bytes > 0) {
		sent_bytes = sendfile(conn->sockfd, conn->fd, &offset, BUFSIZ);
		conn->file_size -= sent_bytes;
	}
}

void handle_output(struct connection *conn)
{
	set_filename(conn);

	conn->fd = open(conn->filename, O_RDONLY);
	if (conn->fd == -1) {
		connection_prepare_send_404(conn);
		return;
	}

	connection_send_header(conn);

	if (strstr(conn->filename, "static")) { // verific daca fisierul este unul dinamic sau static
		connection_send_static(conn);
		close(conn->fd);
		return;
	}
	close(conn->fd);
	connection_send_dynamic(conn);
}

void handle_client(uint32_t event, struct connection *conn)
{
	/* TODO: Handle new client. There can be input and output connections.
	 * Take care of what happened at the end of a connection.
	 */
	if (event & EPOLLIN)
		handle_input(conn);

	connection_remove(conn);
}

int main(void)
{
	/* TODO: Initialize multiplexing. */
	epollfd = w_epoll_create();
	/* TODO: Create server socket. */
	listenfd = tcp_create_listener(AWS_LISTEN_PORT, DEFAULT_LISTEN_BACKLOG);

	/* TODO: Add server socket to epoll object */
	w_epoll_add_fd_in(epollfd, listenfd);

	/* server main loop */
	while (1) {
		struct epoll_event rev;

		/* TODO: Wait for events. */
		w_epoll_wait_infinite(epollfd, &rev);

		/* TODO: Switch event types; consider
		 *   - new connection requests (on server socket)
		 *   - socket communication (on connection sockets)
		 */
		if (rev.data.fd == listenfd) {
			if (rev.events & EPOLLIN)
				handle_new_connection();
		} else {
			handle_client(rev.events, rev.data.ptr);
		}
	}
	close(epollfd);
	close(listenfd);
	return 0;
}
