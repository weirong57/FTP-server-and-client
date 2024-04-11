#include<stdio.h>
#include"ftpServer.h"

char g_recvBuf[1024];	 // Used to accept messages sent by the client
int g_fileSize;			 // File size
char* g_fileBuf;		 // Store the file

typedef struct _ThreadParam
{
	SOCKET socket;//套接字
	char ipAddr[1024];//接收的客户端ip地址。
}ThreadParam;

int main()
{

	initSocket();

	listenToClient();

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

//处理客户端消息的异步线程。
DWORD WINAPI processMsgThread(LPVOID lpThreadParameter)
{
	ThreadParam* thParam = (ThreadParam*)lpThreadParameter;//线程传递的参数
	while (processMsg(thParam->socket, thParam->ipAddr))
	{
	}
	closesocket(thParam->socket);//消息处理完后在线程中关闭该套接字。
	return 0;
}

// Listen for client links
void listenToClient() {
	SOCKET serfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == serfd) {
		printf("socket failed: %d\n", WSAGetLastError());
		return;
	}

	struct sockaddr_in serAddr;
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(SPORT);
	serAddr.sin_addr.S_un.S_addr = ADDR_ANY;

	if (bind(serfd, (struct sockaddr*)&serAddr, sizeof(serAddr)) == SOCKET_ERROR) {
		printf("bind failed: %d\n", WSAGetLastError());
		closesocket(serfd);
		return;
	}

	if (listen(serfd, SOMAXCONN) == SOCKET_ERROR) {
		printf("listen failed: %d\n", WSAGetLastError());
		closesocket(serfd);
		return;
	}
	printf("Listening for client connections...\n");

	while (true) {
		struct sockaddr_in cliAddr;
		int len = sizeof(cliAddr);
		SOCKET clifd = accept(serfd, (struct sockaddr*)&cliAddr, &len);
		if (INVALID_SOCKET == clifd) {
			printf("accept failed: %d\n", WSAGetLastError());
			continue; // 继续等待下一个连接
		}
		printf("New client connected.\n");

		//转换接收的ip地址。
		char ipAddress[1024];
		inet_ntop(AF_INET, &(cliAddr.sin_addr), ipAddress, 1024);
		ThreadParam thParam;
		thParam.socket = clifd;
		memcpy(thParam.ipAddr, ipAddress, 1024);

		//启动处理异步线程。
		HANDLE proMsgThHandle = CreateThread(NULL, 0, processMsgThread, (LPVOID)&thParam, 0, NULL);
		CloseHandle(proMsgThHandle);

		// 持续处理客户端消息
		/*while (processMsg(clifd));
		{}
		printf("Client disconnected or an error occurred.\n");
		closesocket(clifd);*/
	}

	// 通常不会到达这里，因为上面有一个无限循环
	closesocket(serfd); // 关闭服务器套接字
}

bool processMsg(SOCKET clifd, char* ipAddr) {
	int nRes = recv(clifd, g_recvBuf, 1024, 0);
	if (nRes <= 0) {
		if (nRes == 0) {
			printf("Client disconnected gracefully.\n");
		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
		}
		if (g_fileBuf != NULL) {
			free(g_fileBuf);
			g_fileBuf = NULL;
		}
		closesocket(clifd); // Properly close the socket on error or disconnection
		return false;
	}

	// Process the received message
	struct MsgHeader* msg = (struct MsgHeader*)g_recvBuf;

	switch (msg->msgID)
	{
	case MSG_FILENAME:
		printf("Filename: %s\n", msg->fileInfo.fileName);
		if (!readFile(clifd, msg)) {
			printf("Failed to read file.\n");
			return false;
		}
		break;

	case MSG_SENDFILE:
		if (!sendFile(clifd, msg)) {
			printf("Failed to send file.\n");
			return false;
		}
		break;

	case MSG_SUCCESSED:
		printf("Operation succeeded.\n");
		struct MsgHeader exitmsg;
		exitmsg.msgID = MSG_SUCCESSED;
		if (SOCKET_ERROR == send(clifd, (char*)&exitmsg, sizeof(struct MsgHeader), 0)) {
			printf("send failed with error: %d\n", WSAGetLastError());
		}
		return false;

	case MSG_UPLOAD_REQUEST:
		// Here, implement logic to handle file upload request
		// For example, prepare to receive file data and save it to a specific location
		if (!receiveAndSaveUploadedFile(clifd, msg, ipAddr)) {
			printf("Failed to receive and save uploaded file.\n");
			return false;
		}
		break;

	case MSG_UPLOAD_COMPLETE:
		printf("File upload completed successfully.\n");
		// Optionally send a confirmation or next action message back to the client
		break;

	default:
		printf("Unknown message ID: %d\n", msg->msgID);
		return false; // Return false on unknown message ID
	}

	return true; // Return true to continue processing messages
}

bool readFile(SOCKET clifd, struct MsgHeader* pmsg) {
	FILE* pread = fopen(pmsg->fileInfo.fileName, "rb");
	if (pread == NULL) {
		printf("The [%s] file could not be found...\n", pmsg->fileInfo.fileName);
		struct MsgHeader msg;
		msg.msgID = MSG_OPENFILE_FAILD;
		if (SOCKET_ERROR == send(clifd, (char*)&msg, sizeof(struct MsgHeader), 0)) {
			printf("send failed:%d\n", WSAGetLastError());
		}
		return false;
	}

	// Get file size
	fseek(pread, 0, SEEK_END);
	g_fileSize = ftell(pread);
	fseek(pread, 0, SEEK_SET);

	// Send the file size to the client
	struct MsgHeader msg;
	msg.msgID = MSG_FILESIZE;
	msg.fileInfo.fileSize = g_fileSize;

	char tfname[200] = { 0 }, text[100];
	_splitpath(pmsg->fileInfo.fileName, NULL, NULL, tfname, text);
	strcat(tfname, text);
	strcpy(msg.fileInfo.fileName, tfname);
	if (SOCKET_ERROR == send(clifd, (char*)&msg, sizeof(struct MsgHeader), 0)) {
		printf("send failed with error: %d\n", WSAGetLastError());
		fclose(pread); // Close the file before returning
		return false;
	}

	// Read file contents
	g_fileBuf = calloc(PACKET_SIZE, sizeof(char)); // Allocate memory for file buffer
	if (g_fileBuf == NULL) {
		printf("Not enough memory, please try again\n");
		fclose(pread); // Close the file before returning
		return false;
	}

	fread(g_fileBuf, sizeof(char), g_fileSize, pread); // Read file contents
	fclose(pread); // Close the file after reading
	return true;
}

bool sendFile(SOCKET clifd, struct MsgHeader* pmsg)
{
	struct MsgHeader msg;//Tell the client to prepare to accept the file
	msg.msgID = MSG_READY_READ;

	//If the length of the file is larger than the size that can be transferred per packet (PACKET_SIZE) then it is partitioned
	for (size_t i = 0; i < g_fileSize; i += PACKET_SIZE)
	{
		msg.packet.nStart = i;
		//The size of the package is larger than the size of the total file data
		if (i + PACKET_SIZE + 1 > g_fileSize)
		{
			msg.packet.nsize = g_fileSize - i;
		}
		else
		{
			msg.packet.nsize = PACKET_SIZE;
		}

		memcpy(msg.packet.buf, g_fileBuf + msg.packet.nStart, msg.packet.nsize);

		if (SOCKET_ERROR == send(clifd, (char*)&msg, sizeof(struct MsgHeader), 0))
		{
			printf("File sending failure:%d\n", WSAGetLastError());
		}
	}
	return true;
}

bool receiveAndSaveUploadedFile(SOCKET clifd, struct MsgHeader* msg, char* ipAddr) {
	char* filePath = msg->fileInfo.fileName; // Client may send full path
	int fileSize = msg->fileInfo.fileSize;

	// Ensure the upload directory exists
	if (_access(".\\uploads", 0) == -1) {
		if (_mkdir(".\\uploads") == -1) {
			perror("Could not create the upload directory");
			return false;
		}
	}

	// Extract the file name and ensure the path is removed
	char* fileName = strrchr(filePath, '\\');
	if (fileName == NULL) {
		fileName = filePath; // Use the original string as file name if no backslash is found
	}
	else {
		fileName++; // Move to the character after the backslash, which is the beginning of the file name
	}

	char newFileName[260]; // Enough space to store the new file name
	_snprintf(newFileName, sizeof(newFileName), ".\\uploads\\new_%s_%s", ipAddr, fileName); // Add "new_" prefix

	FILE* file = fopen(newFileName, "wb");
	if (!file) {
		// Print out the specific reason for failing to create the file
		perror("Failed to create file on server");
		return false;
	}

	char buffer[1024];
	int bytesReceived = 0;
	int totalBytesReceived = 0;

	while (totalBytesReceived < fileSize) {
		bytesReceived = recv(clifd, buffer, sizeof(buffer), 0);
		if (bytesReceived > 0) {
			fwrite(buffer, 1, bytesReceived, file);
			totalBytesReceived += bytesReceived;
		}
		else if (bytesReceived == SOCKET_ERROR) {
			printf("Recv error: %d\n", WSAGetLastError());
			fclose(file);
			remove(newFileName); // Delete the incomplete file
			return false; // Error should be handled here and file should be closed
		}
		else {
			// The other side may have gracefully closed the connection
			break;
		}
	}

	fclose(file); // Close the file early to ensure it's closed before error checking

	if (totalBytesReceived != fileSize) {
		// File size doesn't match, might be a problem during transmission
		printf("Error: Received file size does not match expected size.\n");
		remove(newFileName); // Delete the incomplete file
		return false;
	}

	// Confirm upload completion
	struct MsgHeader confirmMsg;
	confirmMsg.msgID = MSG_UPLOAD_COMPLETE;
	if (SOCKET_ERROR == send(clifd, (char*)&confirmMsg, sizeof(confirmMsg), 0)) {
		printf("Failed to send confirmation message: %d\n", WSAGetLastError());
		return false;
	}

	printf("File uploaded successfully.\n");

	return true;
}