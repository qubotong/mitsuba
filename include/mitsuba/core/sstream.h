#if !defined(__SSTREAM_H)
#define __SSTREAM_H

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/stream.h>

#ifdef WIN32
#include <io.h>
#include <ws2tcpip.h>
#endif

MTS_NAMESPACE_BEGIN

/** \brief Portable Stream implementation, which encapsulates a socket 
 * for IPv4/IPv6 network communications. Set to use network byte order
 * by default.
 */
class MTS_EXPORT_CORE SocketStream : public Stream {
public:
	/// Create a stream from an existing socket
#if defined(WIN32)
	SocketStream(SOCKET socket);
#else
	SocketStream(int socket);
#endif

	/// Connect to the given host/port
	SocketStream(const std::string &host, int port);

	/* Stream implementation */
	void read(void *ptr, size_t size);
	void write(const void *ptr, size_t size);
	void setPos(size_t pos);
	size_t getPos() const;
	size_t getSize() const;
	void truncate(size_t size);
	void flush();
	bool canWrite() const;
	bool canRead() const;

	/// Return the peer's name
	inline const std::string &getPeer() const { return m_peer; }
	
	/// Return the number of received bytes
	inline size_t getReceivedBytes() const { return m_received; }
	
	/// Return the number of sent bytes
	inline size_t getSentBytes() const { return m_sent; }

	/// Return a string representation
	std::string toString() const;

	/// Handle a socket error
	static void handleError(const std::string &cmd, ELogLevel level = EError);

	MTS_DECLARE_CLASS()
protected:
	/** \brief Virtual destructor
	 *
	 * The destructor frees all resources and closes
	 * the socket if it is still open
	 */
	virtual ~SocketStream();
protected:
#if defined(WIN32)
	SOCKET m_socket;
#else
	int m_socket;
#endif
	size_t m_received, m_sent;
	std::string m_peer;
};

#ifdef WIN32
extern MTS_EXPORT_CORE const char *inet_ntop(int af, const void *src, char *dst, socklen_t len);
#endif

MTS_NAMESPACE_END

#endif /* __SSTREAM_H */