#include<stdio.h>
#include"ftpClient.h"
char g_recvBuf[1024];		//Buffer for receiving messages
char* g_fileBuf;			//Store file contents
int  g_fileSize;			//total file size
char g_fileName[256];			//Save the file name sent by the server

int main()
{
	if (!initSocket())
	{
		printf("Failed to initialize socket.\n");
		return 1;
	}

	SOCKET serfd = connectToHost();
	if (serfd == INVALID_SOCKET)
	{
		printf("Failed to connect to the server.\n");
		closeSocket();
		return 1;
	}

	printf("Choose an option:\n");
	printf("1. Download a file\n");
	printf("2. upload a file\n");
	printf("Enter your choice: ");
	int choice;
	scanf("%d", &choice);
	getchar(); // 清理输入缓冲区中的换行符

	if (choice == 1) {
		downloadFileName(serfd);
	}
	else if (choice == 2) {
		char filePath[256];
		printf("Enter the path of the file to upload: ");
		fgets(filePath, sizeof(filePath), stdin);
		filePath[strcspn(filePath, "\n")] = 0;
		uploadFile(serfd, filePath);
	}
	else {
		printf("Invalid choice.\n");
	}

	closesocket(serfd);
	closeSocket();
	return 0;
}
//Example Initialize the socket library

bool initSocket()
{
	WSADATA wsadata;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsadata))
	{
		printf("WSAStartup faild:%d\n", WSAGetLastError());
		return false;
	}
	return true;
}
//Closing socket library

bool closeSocket()
{
	if (0 != WSACleanup())
	{
		printf("WSACleanup faild:%d\n", WSAGetLastError());
		return false;
	}
	return true;
}
//Listen for client links

SOCKET connectToHost()
{
	SOCKET serfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == serfd)
	{
		printf("socket failed: %d\n", WSAGetLastError());
		return INVALID_SOCKET; // 连接失败时返回 INVALID_SOCKET
	}

	struct sockaddr_in serAddr;
	serAddr.sin_family = AF_INET; // AF_INET 表示网络地址族为 IPv4
	serAddr.sin_port = htons(SPORT); // htons 用于将端口号转换为网络字节序
	serAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); // 127.0.0.1 是回送地址

	if (0 != connect(serfd, (struct sockaddr*)&serAddr, sizeof(serAddr)))
	{
		printf("connect failed: %d\n", WSAGetLastError());
		closesocket(serfd); // 无法连接到服务器，关闭套接字
		return INVALID_SOCKET;
	}

	return serfd; // 返回成功连接的套接字
}

// Process the message

bool processMsg(SOCKET serfd) {
	int nRes = recv(serfd, g_recvBuf, 1024, 0);
	if (nRes <= 0) {
		if (nRes == 0) {
			printf("Server closed the connection.\n");
		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
		}
		if (g_fileBuf != NULL) {
			free(g_fileBuf);
			g_fileBuf = NULL;
		}
		return false;
	}

	struct MsgHeader* msg = (struct MsgHeader*)g_recvBuf;
	switch (msg->msgID) {
	case MSG_OPENFILE_FAILD:
		downloadFileName(serfd);
		break;
	case MSG_FILESIZE:
		readyread(serfd, msg);
		break;
	case MSG_READY_READ:
		writeFile(serfd, msg);
		break;
	case MSG_SUCCESSED:
		printf("File transfer completed successfully.\n");
		break;
	}

	return true;
}



void  downloadFileName(SOCKET serfd)
{
	char fileName[1024] = "";
	printf("Please enter the filename to download:");
	gets_s(fileName, 1023);		// Get the file to download

	struct MsgHeader file;
	file.msgID = MSG_FILENAME;
	strcpy(file.fileInfo.fileName, fileName);

	send(serfd, (char*)&file, sizeof(struct MsgHeader), 0);
}


void readyread(SOCKET serfd, struct MsgHeader* pmsg)
{
	// Reserve memory for the file size
	strcpy(g_fileName, pmsg->fileInfo.fileName);
	g_fileSize = pmsg->fileInfo.fileSize;
	g_fileBuf = calloc(g_fileSize + 1, sizeof(char)); // Request space
	if (g_fileBuf == NULL) // Memory request failed
	{
		printf("Not enough memory, please try again\n");
		return;
	}

	struct MsgHeader msg;
	msg.msgID = MSG_SENDFILE;
	int sendResult = send(serfd, (char*)&msg, sizeof(struct MsgHeader), 0);
	if (sendResult == SOCKET_ERROR)
	{
		printf("send error: %d\n", WSAGetLastError());
		free(g_fileBuf); // Ensure to free allocated memory in case of error
		g_fileBuf = NULL;
		return;
	}
	printf("size: %d filename: %s\n", pmsg->fileInfo.fileSize, pmsg->fileInfo.fileName);
}

bool writeFile(SOCKET serfd, struct MsgHeader* pmsg)
{
	if (g_fileBuf == NULL)
	{
		return false;
	}
	int nStart = pmsg->packet.nStart;
	int nsize = pmsg->packet.nsize;
	memcpy(g_fileBuf + nStart, pmsg->packet.buf, nsize);

	if (nStart + nsize >= g_fileSize)
	{
		char newFileName[260]; // 假设文件名长度不会超过255
		snprintf(newFileName, sizeof(newFileName), "new_%s", g_fileName);

		FILE* pwrite = fopen(newFileName, "wb");
		if (pwrite == NULL)
		{
			printf("Unable to open file for writing.\n");
			return false;
		}

		size_t written = fwrite(g_fileBuf, sizeof(char), g_fileSize, pwrite);
		if (written < g_fileSize)
		{
			printf("Failed to write the complete file.\n");
			fclose(pwrite); // Ensure file is closed even if write failed
			return false;
		}
		fclose(pwrite);

		free(g_fileBuf);
		g_fileBuf = NULL;

		struct MsgHeader msg;
		msg.msgID = MSG_SUCCESSED;
		if (SOCKET_ERROR == send(serfd, (char*)&msg, sizeof(struct MsgHeader), 0))
		{
			printf("Failed to send success message: %d\n", WSAGetLastError());
			return false;
		}

		return false; // Return false to indicate completion
	}
	return true; // Return true to indicate continuation
}


void uploadFile(SOCKET serfd, const char* filePath) {
	FILE* file = fopen(filePath, "rb");
	if (!file) {
		printf("Unable to open file for upload.\n");
		return;
	}

	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	rewind(file);

	struct MsgHeader msgHeader;
	msgHeader.msgID = MSG_UPLOAD_REQUEST;
	msgHeader.fileInfo.fileSize = fileSize;
	strncpy(msgHeader.fileInfo.fileName, filePath, 255);
	msgHeader.fileInfo.fileName[255] = '\0';

	if (SOCKET_ERROR == send(serfd, (char*)&msgHeader, sizeof(msgHeader), 0)) {
		printf("send error: %d\n", WSAGetLastError());
		fclose(file);
		return;
	}

	char buffer[1024];
	int bytesRead;
	while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		if (SOCKET_ERROR == send(serfd, buffer, bytesRead, 0)) {
			printf("send error: %d\n", WSAGetLastError());
			fclose(file);
			return;
		}
	}

	fclose(file); // Always close the file handle when done
	printf("File upload completed.\n");
}