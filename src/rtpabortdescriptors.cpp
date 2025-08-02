#include "rtpabortdescriptors.h"
#include "rtpsocketutilinternal.h"
#include "rtperrors.h"
#include "rtpselect.h"



RTPAbortDescriptors::RTPAbortDescriptors()
{
	m_descriptors[0] = RTPSOCKERR;
	m_descriptors[1] = RTPSOCKERR;
	m_init = false;
}

RTPAbortDescriptors::~RTPAbortDescriptors()
{
	Destroy();
}

#ifdef RTP_SOCKETTYPE_WINSOCK

int RTPAbortDescriptors::Init()
{
	if (m_init)
		return ERR_RTP_ABORTDESC_ALREADYINIT;

	SOCKET listensock;
	int size;
	struct sockaddr_in addr;

	listensock = socket(PF_INET,SOCK_STREAM,0);
	if (listensock == RTPSOCKERR)
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	
	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (bind(listensock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);
	if (getsockname(listensock,(struct sockaddr*)&addr,&size) != 0)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	unsigned short connectport = ntohs(addr.sin_port);

	m_descriptors[0] = socket(PF_INET,SOCK_STREAM,0);
	if (m_descriptors[0] == RTPSOCKERR)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (bind(m_descriptors[0],(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(m_descriptors[0]);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	if (listen(listensock,1) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(m_descriptors[0]);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(connectport);
	
	if (connect(m_descriptors[0],(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(m_descriptors[0]);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);
	m_descriptors[1] = accept(listensock,(struct sockaddr *)&addr,&size);
	if (m_descriptors[1] == RTPSOCKERR)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(m_descriptors[0]);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	// 连接已建立，关闭监听套接字

	RTPCLOSE(listensock);

	m_init = true;
	return 0;
}

void RTPAbortDescriptors::Destroy()
{
	if (!m_init)
		return;

	RTPCLOSE(m_descriptors[0]);
	RTPCLOSE(m_descriptors[1]);
	m_descriptors[0] = RTPSOCKERR;
	m_descriptors[1] = RTPSOCKERR;

	m_init = false;
}

int RTPAbortDescriptors::SendAbortSignal()
{
	if (!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	send(m_descriptors[1],"*",1,0);
	return 0;
}

int RTPAbortDescriptors::ReadSignallingByte()
{
	if (!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	char buf[1];
		
	recv(m_descriptors[0],buf,1,0);
	return 0;
}

#else // unix-style

int RTPAbortDescriptors::Init()
{
	if (m_init)
		return ERR_RTP_ABORTDESC_ALREADYINIT;

	if (pipe(m_descriptors) < 0)
		return ERR_RTP_ABORTDESC_CANTCREATEPIPE;

	m_init = true;
	return 0;
}

void RTPAbortDescriptors::Destroy()
{
	if (!m_init)
		return;

	close(m_descriptors[0]);
	close(m_descriptors[1]);
	m_descriptors[0] = RTPSOCKERR;
	m_descriptors[1] = RTPSOCKERR;

	m_init = false;
}

int RTPAbortDescriptors::SendAbortSignal()
{
	if (!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	if (write(m_descriptors[1],"*",1))
	{
		// 为了消除与 __wur 相关的编译器警告
	}

	return 0;
}

int RTPAbortDescriptors::ReadSignallingByte()
{
	if (!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	unsigned char buf[1];

	if (read(m_descriptors[0],buf,1))
	{
		// 为了消除与 __wur 相关的编译器警告
	}
	return 0;
}

#endif // RTP_SOCKETTYPE_WINSOCK

// 持续调用 'ReadSignallingByte' 直到没有字节可读
int RTPAbortDescriptors::ClearAbortSignal()
{
	if (!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	bool done = false;
	while (!done)
	{
		int8_t isset = 0;

		// 未使用: struct timeval tv = { 0, 0 };

		int status = RTPSelect(&m_descriptors[0], &isset, 1, RTPTime(0));
		if (status < 0)
			return status;

		if (!isset)
			done = true;
		else
		{
			int status = ReadSignallingByte();
			if (status < 0)
				return status;
		}
	}

	return 0;
}

