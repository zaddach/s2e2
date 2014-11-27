/**
 * How to do a timeout for read/accept: http://stackoverflow.com/questions/2917881/how-to-implement-a-timeout-in-read-function-call
 * How to extend streambuf: http://uw714doc.sco.com/en/SDK_clib/_Deriving_New_streambuf_Classes.html
 * Example streambuf implementation with nullstreambuf: http://asmodehn.wordpress.com/2010/06/20/busy-c-coding-and-testing/
 * Extensive guide on streambuf: http://www.mr-edd.co.uk/blog/beginners_guide_streambuf
 * streambuf c++ class reference: http://www.cplusplus.com/reference/streambuf/streambuf/
 */ 

extern "C" {
    #include "qemu_socket.h"
}

#include <arpa/inet.h>
#include <memory>

#include <iostream>
#include <iomanip>

#include "QemuSocket.h"

#define PROTO_TCP 6

namespace s2e
{
    SocketException::~SocketException() throw()
    {
    }

    const char * SocketException::what() const throw() 
    {
        return m_message.c_str();
    }
    
     int SocketException::error_code() const throw()
     {
         return m_errorCode;
     }
    
    std::vector<std::tr1::shared_ptr<SocketAddress> > SocketAddress::parseAndResolve(const char * addr, int socktype)
    {
        size_t pos;
        std::string address(addr);
        pos = address.rfind(":");
        int error;
        
        if (pos == std::string::npos)
            throw SocketException("No port separator ':' found in socket address");
        
        std::string host = address.substr(0, pos);
        std::string port = address.substr(pos + 1, std::string::npos);
        
        struct addrinfo hints;
        memset(&hints, 0, sizeof(struct addrinfo));
        
        hints.ai_family= AF_UNSPEC;
        hints.ai_socktype = socktype;
        hints.ai_flags = 0;
        hints.ai_protocol = 0; // Any protocol
        
        struct addrinfo * results = NULL;
        
        error = getaddrinfo(host.c_str(), port.c_str(), &hints, &results);
        if (error != 0)
        {
            std::stringstream errorMsg;
            errorMsg << "getaddrinfo returned error: " << gai_strerror(error);
            
            throw SocketException(errorMsg.str().c_str(), error);
        }
        
        std::vector<std::tr1::shared_ptr<SocketAddress> > sockAddrs;
        
        for (struct addrinfo * cur = results; cur != NULL; cur = cur->ai_next)
        {
            if (cur->ai_family == AF_INET)
            {
                sockAddrs.push_back(std::tr1::shared_ptr<SocketAddress>(new Inet4SocketAddress(reinterpret_cast<struct sockaddr_in *>(cur->ai_addr))));
            }
            else if (cur->ai_family == AF_INET6)
            {
                sockAddrs.push_back(std::tr1::shared_ptr<SocketAddress>(new Inet6SocketAddress(reinterpret_cast<struct sockaddr_in6 *>(cur->ai_addr))));
            }
            //else unknown family, ignore
        }
        
        freeaddrinfo(results);
        
        return sockAddrs; 
    }
    
    Inet4SocketAddress::Inet4SocketAddress(struct sockaddr_in * addr)
    {
        if (addr)
            memcpy(&m_sockaddr, addr, sizeof(struct sockaddr_in));
    }
    
    sa_family_t Inet4SocketAddress::getAddressFamily() 
    {
        return AF_INET;
    }
    
    struct sockaddr * Inet4SocketAddress::getCSocketAddress() 
    {
        return reinterpret_cast<struct sockaddr *>(&m_sockaddr);
    }
    
    int Inet4SocketAddress::getCSocketAddressSize() 
    { 
        return sizeof(struct sockaddr_in);
    }
    
    std::string Inet4SocketAddress::toString()
    {
        char buffer[INET_ADDRSTRLEN + 1];
        
        inet_ntop(AF_INET, &m_sockaddr.sin_addr, buffer, sizeof(buffer));
        return std::string(buffer);
    }
    
    Inet6SocketAddress::Inet6SocketAddress(struct sockaddr_in6 * addr)
    {
        if (addr)
            memcpy(&m_sockaddr, addr, sizeof(struct sockaddr_in6));
    }

    sa_family_t Inet6SocketAddress::getAddressFamily() 
    {
        return AF_INET6;
    }
    
    struct sockaddr * Inet6SocketAddress::getCSocketAddress() 
    {
        return reinterpret_cast<struct sockaddr *>(&m_sockaddr);
    }
    
    int Inet6SocketAddress::getCSocketAddressSize() 
    {
        return sizeof(struct sockaddr_in6);
    }
    
    std::string Inet6SocketAddress::toString()
    {
        char buffer[INET6_ADDRSTRLEN + 1];
        
        inet_ntop(AF_INET6, &m_sockaddr.sin6_addr, buffer, sizeof(buffer));
        return std::string(buffer);
    }
    
    TcpStreamBuffer::TcpStreamBuffer(QemuTcpSocket * sock, size_t size) 
        : std::streambuf(),
          m_outBuffer(0),
          m_inBuffer(0),
          m_bufferSize(size),
          m_sock(sock)
    {
        m_outBuffer = new char[size];
        m_inBuffer = new char[size];
        setp(m_outBuffer, m_outBuffer + size - 1);
        setg(m_inBuffer, m_inBuffer + size, m_inBuffer + size);
    }
    
    TcpStreamBuffer::~TcpStreamBuffer() 
    {
        if (m_outBuffer)
            delete[] m_outBuffer;
    }
    
    int TcpStreamBuffer::underflow()
    {
        size_t num_bytes = m_bufferSize;
        
        if (gptr() < egptr())
        {
            std::cout << "ERROR - TcpStreamBuffer::underflow: underflow saw invalid pointer constellation" << std::endl;
            return 0;
        }

        try 
        {
            m_sock->read(m_inBuffer, num_bytes);
        }
        catch(SocketConnectionClosedException& ex)
        {
            return std::char_traits<char>::eof();
        }
        
        setg(m_inBuffer, m_inBuffer, m_inBuffer + num_bytes); //num_bytes has been updated by read to be the number of bytes really read
        
        return std::char_traits<char>::not_eof(*eback());
    }
    
    int TcpStreamBuffer::overflow(int c)
    {
        size_t todoLength = pptr() - pbase();

        if (c != EOF)
        {
            *pptr() = static_cast<char>(c);
            todoLength += 1;
        }

        size_t length = todoLength;
        m_sock->write(pbase(), length);

        assert(todoLength == length);
        setp(pbase(), epptr());
        
        return std::char_traits<char>::not_eof(c);
    }
    
    int TcpStreamBuffer::sync()
    {
        return overflow(EOF);
    }
    
    std::streamsize TcpStreamBuffer::showmanyc()
    {
        return m_sock->getAvailableBytes(0);
    }
    
    TcpStream::TcpStream(QemuTcpSocket * sock, size_t buffer_size) 
        : std::iostream(0),
            m_buffer(sock, buffer_size)
    {
        init(&m_buffer);
    }
    
    QemuTcpSocket::QemuTcpSocket()
        : TcpStream(this, 2048),
          m_socket(0),
          m_isConnected(false)
    {
    }
    
    QemuTcpSocket::QemuTcpSocket(const char * remote_address) throw (SocketConnectException)
        : TcpStream(this, 2048),
          m_socket(0),
          m_isConnected(false)
    {
        std::vector<std::tr1::shared_ptr<SocketAddress> > resolved = SocketAddress::parseAndResolve(remote_address, SOCK_STREAM);
        
        for (std::vector<std::tr1::shared_ptr<SocketAddress> >::iterator itr = resolved.begin(); itr != resolved.end(); itr++)
        {
            int sock = ::qemu_socket((*itr)->getAddressFamily(), SOCK_STREAM, 0);
            
            if (sock == -1)
                continue;

            int size = 1024;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, NULL, 0);
            setsockopt(sock, IPPROTO_TCP, TCP_MAXSEG, &size, sizeof(size));
            if (connect(sock, (*itr)->getCSocketAddress(), (*itr)->getCSocketAddressSize()) != -1)
            {
                m_socket = sock;
                m_remoteAddress = *itr;
                m_isConnected = true;
                return;
            }
            
            ::close(sock);
        }
        
        throw SocketConnectException();
    }

    QemuTcpSocket::~QemuTcpSocket()
    {
    }
    
    
    #define DEBUG_LOG(msg) std::cout << "DEBUG - " << __FILE__ << ":" << std::dec << __LINE__ << ": " << msg
    
    void QemuTcpSocket::read(char buffer[], size_t& length, uint64_t timeout)
    {
        int error_counter = 0;
        
repeat_read:
        if (!m_isConnected)
        {
            throw SocketNotConnectedException();
        }
            
        //Wait timeout milliseconds and then check if bytes are available if a timeout is requested
        size_t available_bytes = getAvailableBytes(timeout);
        
        if (available_bytes < 1)
        {
            length = 0;
            return;
        }
        
        int read_length = ::read(m_socket, buffer, length);
        
/*        {
            int i;
            DEBUG_LOG("Read " << std::dec << read_length << " {");
            for (i = 0; i < read_length; i++)
            {
                if (i != 0)
                    std::cout << " ";
                std::cout << "0x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned int>(buffer[i] & 0xFF);
            }
            std::cout << "}" << std::endl;
        } */
        
        
        if (read_length == -1 && errno == 4)
        {
            error_counter += 1;
            
            if (error_counter < 2)
                goto repeat_read;
            
            throw SocketException("Error during read", errno);
        }
        
        if (read_length == -1)
            throw SocketException("Error during read", errno);
        
        if (read_length == 0 && available_bytes > 0)
        {
            m_isConnected = false;
            throw SocketConnectionClosedException();
        }
    
        length = read_length;
    }
    
    void QemuTcpSocket::write(const char buffer[], size_t& length)
    {
        if (!m_isConnected)
            throw SocketNotConnectedException();
            
        int origLen = length;
        int writtenLen = 0;
        do {
            int wBytes = ::write(m_socket, buffer+writtenLen, length);
            if (likely(wBytes >= 0))
            {
                writtenLen += wBytes;
                length -= wBytes;
            } else {
                std::stringstream msg;
                msg << "Error " << std::dec << errno << " during socket write";
                throw SocketException(msg.str().c_str(), errno);
            }
            if (unlikely(writtenLen != origLen)) {
                // Receiver is not keeping up, lowering pressure
                sleep(1);
            }
        } while (writtenLen != origLen);

        length = writtenLen;
    }
    
    size_t QemuTcpSocket::getAvailableBytes(uint64_t timeout)
    {
        struct timeval tv_timeout;
        fd_set set;
        int retval;
        int error_counter = 0;

    repeat_select:
        if (!m_isConnected)
            throw SocketNotConnectedException();
        
        tv_timeout.tv_sec = timeout / 1000;
        tv_timeout.tv_usec = (timeout % 1000) * 1000;
        FD_ZERO(&set);
        FD_SET(m_socket, &set);
        
        if (timeout == -1)
            retval = ::select(m_socket + 1, &set, NULL, NULL, NULL);
        else
            retval = ::select(m_socket + 1, &set, NULL, NULL, &tv_timeout);
        
        //EINTR - restart system call
        //TODO: error counter
        if (retval == -1 && errno == 4)
        {
            error_counter += 1;
            
            if (error_counter < 2)
                goto repeat_select;
            
            throw SocketException("error during select", errno);
        }
            
        if (retval == -1)
            throw SocketException("error during select", errno);
        
        return retval;
    }
    
    QemuTcpServerSocket::QemuTcpServerSocket(const char * bind_address, int backlog) throw (SocketException)
    {
        std::vector<std::tr1::shared_ptr<SocketAddress> > resolved = SocketAddress::parseAndResolve(bind_address, SOCK_STREAM);
        
        for (std::vector<std::tr1::shared_ptr<SocketAddress> >::iterator itr = resolved.begin(); itr != resolved.end(); itr++)
        {
            int sock = ::qemu_socket((*itr)->getAddressFamily(), SOCK_STREAM, 0);
            
            if (sock == -1)
                continue;
               
            int optval = 1;
            //Allow address reuse
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
            optval = 1024;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, NULL, 0);
            setsockopt(sock, IPPROTO_TCP, TCP_MAXSEG, &optval, sizeof(optval));
            
            if (bind(sock, (*itr)->getCSocketAddress(), (*itr)->getCSocketAddressSize()) != -1)
            {
                m_socket = sock;
                m_bindAddress = *itr;
                
                if (::listen(m_socket, backlog) == -1)
                {
                    throw SocketException("Error doing listen on socket", errno);
                }    
                        
                return;
            }
            
            ::close(sock);
        }
        
        throw SocketConnectException();
    }
    
    QemuTcpServerSocket::~QemuTcpServerSocket()
    {
    }
    
    void QemuTcpServerSocket::accept(QemuTcpSocket& sock, uint64_t timeout) throw(SocketException)
    {
        struct sockaddr_storage sockaddr;
        socklen_t sockaddr_size = sizeof(struct sockaddr_storage);
        int acceptedSocket = 0;
        
        if (timeout != -1)
        {
            struct timeval tv_timeout;
            fd_set set;
            int result;
        
            tv_timeout.tv_sec = timeout / 1000;
            tv_timeout.tv_usec = (timeout % 1000) * 1000;
            FD_ZERO(&set);
            FD_SET(m_socket, &set);
        
            result = ::select(m_socket + 1, &set, NULL, NULL, &tv_timeout);
            if (result == -1)
                throw SocketException("error during select", errno);
                
            if (result == 0)
                throw SocketTimeoutException();
        }         
        
        acceptedSocket = ::qemu_accept(m_socket, reinterpret_cast<struct sockaddr *>(&sockaddr), &sockaddr_size);
        
        if (acceptedSocket == -1)
            throw SocketException("accept returned error in QemuTcpServerSocket::accept", errno);
            
        sock.m_socket = acceptedSocket;
        if (sockaddr.ss_family == AF_INET)
            sock.m_remoteAddress = std::tr1::shared_ptr<SocketAddress>(new Inet4SocketAddress(reinterpret_cast<struct sockaddr_in *>(&sockaddr)));
        else if (sockaddr.ss_family == AF_INET6)
            sock.m_remoteAddress = std::tr1::shared_ptr<SocketAddress>(new Inet6SocketAddress(reinterpret_cast<struct sockaddr_in6 *>(&sockaddr)));
        else
            throw SocketException("Unknown address family in QemuTcpServerSocket::accept");
            
        sock.m_isConnected = true;
    }
} //namespace s2e
