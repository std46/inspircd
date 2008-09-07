/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
#include "socket.h"
#include "socketengine.h"


/* Private static member data must be initialized in this manner */
unsigned int ListenSocket::socketcount = 0;
sockaddr* ListenSocket::sock_us = NULL;
sockaddr* ListenSocket::client = NULL;
sockaddr* ListenSocket::raddr = NULL;

ListenSocket::ListenSocket(InspIRCd* Instance, int port, char* addr) : ServerInstance(Instance), desc("plaintext"), bind_addr(addr), bind_port(port)
{
	this->SetFd(irc::sockets::OpenTCPSocket(addr));
	if (this->GetFd() > -1)
	{
		if (!Instance->BindSocket(this->fd,port,addr))
			this->fd = -1;
#ifdef IPV6
		if ((!*addr) || (strchr(addr,':')))
			this->family = AF_INET6;
		else
#endif
		this->family = AF_INET;
		Instance->SE->AddFd(this);
	}
	/* Saves needless allocations */
	if (socketcount == 0)
	{
		/* All instances of ListenSocket share these, so reference count it */
		ServerInstance->Logs->Log("SOCKET", DEBUG,"Allocate sockaddr structures");
		sock_us = new sockaddr[2];
		client = new sockaddr[2];
		raddr = new sockaddr[2];
	}
	socketcount++;
}

ListenSocket::~ListenSocket()
{
	if (this->GetFd() > -1)
	{
		ServerInstance->SE->DelFd(this);
		ServerInstance->Logs->Log("SOCKET", DEBUG,"Shut down listener on fd %d", this->fd);
		if (ServerInstance->SE->Shutdown(this, 2) || ServerInstance->SE->Close(this))
			ServerInstance->Logs->Log("SOCKET", DEBUG,"Failed to cancel listener: %s", strerror(errno));
		this->fd = -1;
	}
	socketcount--;
	if (socketcount == 0)
	{
		delete[] sock_us;
		delete[] client;
		delete[] raddr;
	}
}

void ListenSocket::HandleEvent(EventType e, int err)
{
	switch (e)
	{
		case EVENT_ERROR:
			ServerInstance->Logs->Log("SOCKET",DEFAULT,"ListenSocket::HandleEvent() received a socket engine error event! well shit! '%s'", strerror(err));
		break;
		case EVENT_WRITE:
			ServerInstance->Logs->Log("SOCKET",DEBUG,"*** BUG *** ListenSocket::HandleEvent() got a WRITE event!!!");
		break;
		case EVENT_READ:
		{
			ServerInstance->Logs->Log("SOCKET",DEBUG,"HandleEvent for Listensoket");
			socklen_t uslen, length;		// length of our port number
			int incomingSockfd, in_port;

#ifdef IPV6
			if (this->family == AF_INET6)
			{
				uslen = sizeof(sockaddr_in6);
				length = sizeof(sockaddr_in6);
			}
			else
#endif
			{
				uslen = sizeof(sockaddr_in);
				length = sizeof(sockaddr_in);
			}

			incomingSockfd = ServerInstance->SE->Accept(this, (sockaddr*)client, &length);

			if ((incomingSockfd > -1) && (!ServerInstance->SE->GetSockName(this, sock_us, &uslen)))
			{
				char buf[MAXBUF];
				char target[MAXBUF];	

				*target = *buf = '\0';

#ifdef IPV6
				if (this->family == AF_INET6)
				{
					in_port = ntohs(((sockaddr_in6*)sock_us)->sin6_port);
					inet_ntop(AF_INET6, &((const sockaddr_in6*)client)->sin6_addr, buf, sizeof(buf));
					socklen_t raddrsz = sizeof(sockaddr_in6);
					if (getsockname(incomingSockfd, (sockaddr*) raddr, &raddrsz) == 0)
						inet_ntop(AF_INET6, &((const sockaddr_in6*)raddr)->sin6_addr, target, sizeof(target));
					else
						ServerInstance->Logs->Log("SOCKET", DEBUG, "Can't get peername: %s", strerror(errno));
				}
				else
#endif
				{
					inet_ntop(AF_INET, &((const sockaddr_in*)client)->sin_addr, buf, sizeof(buf));
					in_port = ntohs(((sockaddr_in*)sock_us)->sin_port);
					socklen_t raddrsz = sizeof(sockaddr_in);
					if (getsockname(incomingSockfd, (sockaddr*) raddr, &raddrsz) == 0)
						inet_ntop(AF_INET, &((const sockaddr_in*)raddr)->sin_addr, target, sizeof(target));
					else
						ServerInstance->Logs->Log("SOCKET", DEBUG, "Can't get peername: %s", strerror(errno));
				}
				ServerInstance->SE->NonBlocking(incomingSockfd);
				ServerInstance->stats->statsAccept++;
				ServerInstance->Users->AddUser(ServerInstance, incomingSockfd, in_port, false, this->family, client, target);	
			}
			else
			{
				ServerInstance->SE->Shutdown(incomingSockfd, 2);
				ServerInstance->SE->Close(incomingSockfd);
				ServerInstance->stats->statsRefused++;
			}
		}
		break;
	}
}
