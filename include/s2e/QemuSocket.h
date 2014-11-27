#ifndef S2E_QEMU_SOCKET_H
#define S2E_QEMU_SOCKET_H

//C includes
#include <stdint.h>
#include <cstring>



//Network includes
#include <sys/socket.h> /* socket, connect, ... */
#include <netdb.h> /* getaddrinfo */

//C++ stdlib includes
#include <string>
#include <vector>
#include <sstream>

/* Include the shared_ptr */
#include <cstddef> // for __GLIBCXX__
 
#ifdef __GLIBCXX__
    #include <tr1/memory>
#else
    #ifdef __IBMCPP__
        #define __IBMCPP_TR1__
    #endif
    #include <memory>
#endif

#ifdef QEMU_SOCKETS
#define socket(address_family, sock_type, unknown) qemu_socket(address_family, sock_type, unknown)
#endif /* QEMU_SOCKETS */

namespace s2e {
    
    class SocketException;
    class SocketAddress;
    class Inet4SocketAddress;
    class Inet6SocketAddress;
    class QemuTcpSocket;
    class QemuTcpServerSocket;

    class SocketException : public std::exception
    {
    public:
        SocketException(const char * msg, int error_code = 0) : m_message(msg), m_errorCode(error_code) {}
        virtual ~SocketException() throw();
        virtual const char * what() const throw();
        virtual int error_code() const throw();
    private:
        std::string m_message;
        int m_errorCode;
    };
    
    class SocketConnectException : public SocketException
    {
    public:
        SocketConnectException(const char * msg) : SocketException(msg) {}
        SocketConnectException() : SocketException("Error connecting to remote host") {}
    };
    
    class SocketTimeoutException : public SocketException
    {
    public:
        SocketTimeoutException() : SocketException("Timeout") {}
    };
    
    class SocketNotConnectedException : public SocketException
    {
    public:
        SocketNotConnectedException() : SocketException("Not connected") {}
    };
    
    class SocketConnectionClosedException : public SocketException
    {
    public:
        SocketConnectionClosedException() : SocketException("Connection closed") {}
    };

    class SocketAddress
    {
    public:
        /**
        * Get the address family.
        */
        virtual sa_family_t getAddressFamily() = 0;
        /**
        * Get a string representation of the address.
        */
        virtual std::string toString() = 0;
        /**
        * Get the pointer to the unterlying C socket structure.
        */
        virtual struct sockaddr * getCSocketAddress() = 0;
        /**
        * Get the size of the underlying C socket structure.
        */
        virtual int getCSocketAddressSize() = 0;
        /**
        * Parses and resolves a socket address (host + port) given in <b>address</b>.
        * Returns a subclass of SocketAddress that needs to be deleted.
        */
        static std::vector< std::tr1::shared_ptr<SocketAddress> > parseAndResolve(const char * address, int socktype = SOCK_STREAM);
    };

    class Inet4SocketAddress : public SocketAddress
    {
    public:
        Inet4SocketAddress(struct sockaddr_in *);
        virtual sa_family_t getAddressFamily();
        virtual struct sockaddr * getCSocketAddress();
        virtual int getCSocketAddressSize();
        virtual std::string toString();
        
    private:
        struct sockaddr_in m_sockaddr;
    };

    class Inet6SocketAddress : public SocketAddress
    {
    public:
        Inet6SocketAddress(struct sockaddr_in6 *);
        virtual sa_family_t getAddressFamily();
        virtual struct sockaddr * getCSocketAddress();
        virtual int getCSocketAddressSize();
        virtual std::string toString();
        
    private:
        struct sockaddr_in6 m_sockaddr;
    };
    
    class TcpStreamBuffer : public std::streambuf
    {
    public:
        TcpStreamBuffer(QemuTcpSocket * sock, size_t buffer_size);
        ~TcpStreamBuffer();
    protected:
        int overflow(int c);
        int underflow();
        int sync();
        std::streamsize showmanyc();
    private:
        char * m_outBuffer; 
        char * m_inBuffer;
        size_t m_bufferSize;
        ///Lifetime of the socket should be the same as the buffer, so no deletion necessary
        QemuTcpSocket * m_sock;
    };
    
    class TcpStream : public std::iostream
    {
    public:
        TcpStream(QemuTcpSocket * sock, size_t buffer_size);
        
    private:
        TcpStreamBuffer m_buffer;
    };
    
    class QemuTcpSocket : public TcpStream
    {
        friend class QemuTcpServerSocket;
        
    public:
        QemuTcpSocket();
        QemuTcpSocket(const char * remote_address) throw(SocketConnectException);
        virtual ~QemuTcpSocket();
        
        std::tr1::shared_ptr<SocketAddress> getRemoteAddress() {return m_remoteAddress;}
        /**
         * Read characters from the socket.
         * The timeout can be either -1, to wait indefinitely, 0 to return immediatly 
         * (possibly with 0 bytes read), or a positive integer to specify the timeout in
         * milliseconds.
         */
        void read(char buffer[], size_t& length, uint64_t timeout = -1);
        void write(const char buffer[], size_t& length);
        /**
         * Get the number of bytes that are directly available for reading.
         * @param timeout Number of milliseconds to wait for data.
         */
        size_t getAvailableBytes(uint64_t timeout = 0);
        
        bool isConnected() {return m_isConnected;}
        
        void close();
    private:
        std::tr1::shared_ptr<SocketAddress> m_localAddress;
        std::tr1::shared_ptr<SocketAddress> m_remoteAddress;
        int m_socket;
        bool m_isConnected;
    };
    
    class QemuTcpServerSocket
    {
    public:
        QemuTcpServerSocket(const char * bind_address, int backlog = 10) throw(SocketException);
        virtual ~QemuTcpServerSocket();
        
        void accept(QemuTcpSocket& sock, uint64_t timeout = -1) throw(SocketException);
    private:
        int m_socket;
        std::tr1::shared_ptr<SocketAddress> m_bindAddress;
    };
        
} /* namespace s2e */

#endif /* S2E_QEMU_SOCKET_H */