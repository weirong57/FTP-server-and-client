#pragma once
#include<WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#include<stdbool.h>
#define SPORT 15896	//�������˿ں�

//������
enum MSGTAG
{
	MSG_FILENAME = 1,		//�ļ���
	MSG_FILESIZE = 2,		//�ļ���С
	MSG_READY_READ =3,		//׼������
	MSG_SENDFILE = 4,			//����
	MSG_SUCCESSED =5,		//�������

	MSG_OPENFILE_FAILD =6,	//���߿ͻ����ļ��Ҳ���

	MSG_UPLOAD_REQUEST = 7, // �ͻ��������ϴ��ļ�
	MSG_UPLOAD_COMPLETE = 8 // �ϴ��ļ����

};
#pragma pack(1)		//���ýṹ��1�ֽڶ���
#define PACKET_SIZE (1024 - sizeof(int)*3)
struct MsgHeader	//��װ��Ϣͷ
{
	enum MSGTAG msgID;	//��ǰ��Ϣ���		4byte
	union MyUnion
	{
		struct
		{
			int fileSize;		//�ļ���С  4
			char fileName[256];	//�ļ���  256    
		}fileInfo;		//260
		struct
		{
			int nStart;			//���ı��
			int nsize;			 //�ð������ݴ�С
			char buf[PACKET_SIZE];
		}packet;
	};
};
#pragma pack()


//��ʼ��socket��
bool initSocket();
//�ر�socket��
bool closeSocket();
//�����ͻ�������
void listenToClient();
//������Ϣ
bool processMsg(SOCKET socket, char* ipAddr);
//��ȡ�ļ�������ļ���С
bool readFile(SOCKET, struct MsgHeader*);
//�����ļ�
bool sendFile(SOCKET, struct MsgHeader*);

bool receiveAndSaveUploadedFile(SOCKET clifd, struct MsgHeader* msg, char* ipAddr);