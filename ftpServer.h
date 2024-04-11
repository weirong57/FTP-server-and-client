#pragma once
#include<WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#include<stdbool.h>
#define SPORT 15896	//服务器端口号

//定义标记
enum MSGTAG
{
	MSG_FILENAME = 1,		//文件名
	MSG_FILESIZE = 2,		//文件大小
	MSG_READY_READ =3,		//准备接受
	MSG_SENDFILE = 4,			//发送
	MSG_SUCCESSED =5,		//传输完成

	MSG_OPENFILE_FAILD =6,	//告诉客户端文件找不到

	MSG_UPLOAD_REQUEST = 7, // 客户端请求上传文件
	MSG_UPLOAD_COMPLETE = 8 // 上传文件完成

};
#pragma pack(1)		//设置结构体1字节对齐
#define PACKET_SIZE (1024 - sizeof(int)*3)
struct MsgHeader	//封装消息头
{
	enum MSGTAG msgID;	//当前消息标记		4byte
	union MyUnion
	{
		struct
		{
			int fileSize;		//文件大小  4
			char fileName[256];	//文件名  256    
		}fileInfo;		//260
		struct
		{
			int nStart;			//包的编号
			int nsize;			 //该包的数据大小
			char buf[PACKET_SIZE];
		}packet;
	};
};
#pragma pack()


//初始化socket库
bool initSocket();
//关闭socket库
bool closeSocket();
//监听客户端链接
void listenToClient();
//处理消息
bool processMsg(SOCKET socket, char* ipAddr);
//读取文件，获得文件大小
bool readFile(SOCKET, struct MsgHeader*);
//发送文件
bool sendFile(SOCKET, struct MsgHeader*);

bool receiveAndSaveUploadedFile(SOCKET clifd, struct MsgHeader* msg, char* ipAddr);