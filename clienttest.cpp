#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// C++ library
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

#define MAX_BUF_LENGTH 2048

using namespace std;
string folderpath = "clientdata/";

void *Recv(void *p)
{
	int sockfd = *(int *)p;
	vector<char> buffer(MAX_BUF_LENGTH);
	string server_msg = "";
	while (1)
	{
		int bytesReceived = recv(sockfd, &buffer[0], buffer.size(), 0);
		if (bytesReceived < 0)
		{
			break;
		}
		else
		{
			server_msg.append(buffer.data(), 0, bytesReceived);
		}
		if (server_msg.length() > 0)
		{
			cout << server_msg << "\n";
			server_msg = "";
		}
		sleep(1);
	}
	return 0;
}

long getFileSize(string &filename)
{
	FILE *fin = fopen(&filename[0], "rb");
	fseek(fin, 0, SEEK_END);
	long size = ftell(fin);
	fclose(fin);
	return size;
}

int main()
{
	struct sockaddr_in sockIn;
	int sock_fd;
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket error");
		exit(-1);
	}
	else
	{
		// set basic properties
		bzero(&sockIn, sizeof(sockIn)); //初始化
		sockIn.sin_family = AF_INET;
		sockIn.sin_addr.s_addr = inet_addr("127.0.0.1");
		sockIn.sin_port = htons(8100);
		/* 強制轉換成struct sockaddr */
		if (connect(sock_fd, (struct sockaddr *)&sockIn, sizeof(sockIn)) < 0)
		{
			perror("Connect Fail\n");
			cout << "失敗\n";
		}
		else
		{
			cout << "成功 connect\n";
		}
	}
	string server_msg = "";
	pthread_t tid;
	while (1)
	{
		vector<char> buffer(MAX_BUF_LENGTH);
		int bytesReceived = recv(sock_fd, &buffer[0], buffer.size(), 0);
		if (bytesReceived > 0)
		{
			server_msg.append(buffer.data(), 0, bytesReceived);
			cout << server_msg << "\n";
			server_msg = "";
			break;
		}
	}
	//pthread_create(&tid, NULL, Recv, &sock_fd);
	while (1)
	{
		cin.clear();
		while (1)
		{
			getline(cin, server_msg);
			if (server_msg.length() > 0)
			{
				break;
			}
		}
		send(sock_fd, &server_msg[0], server_msg.length(), 0);
		string line2 = server_msg;
		stringstream test(line2);
		string segment;
		vector<string> seglist;
		while (std::getline(test, segment, ' '))
		{
			seglist.push_back(segment);
		}
		if (seglist[0] == "read")
		{
			vector<char> buffer(MAX_BUF_LENGTH);
			while (1)
			{
				int bytesReceived = recv(sock_fd, &buffer[0], buffer.size(), 0);
				string message = "";
				int nbytes;
				int sum = 0;
				if (bytesReceived > 0)
				{
					message.append(buffer.data(), 0, bytesReceived);
					char msg[1024] = {0};
					if (message == "ok")
					{
						cout << "開始讀取" << endl;
						char msg[1024] = {0};
						while (1)
						{
							int fl = recv(sock_fd, msg, sizeof(msg), 0); // get file length
							if (fl > 0)
							{
								break;
							}
						};
						int fileLen = stoi(string(msg));
						string path = folderpath + "/" + seglist[1];
						FILE *fout = fopen(&path[0], "wb+");
						int nbytes;
						int sum = 0;
						while (sum < fileLen)
						{
							nbytes = read(sock_fd, msg, sizeof(msg));
							nbytes = fwrite(msg, sizeof(char), nbytes, fout);
							sum += nbytes;
							printf("\rProgress: %.2f %%", ((double)sum / (double)fileLen) * 100);
							fflush(stdout);
						}
						fclose(fout);
					}
					else if (message == "有人正在寫入 正在等候")
					{
						cout << message << endl;
					}
					else
					{
						cout << message << endl;
						break;
					}
				}
				sleep(1);
			}
		}
		else if (seglist[0] == "write")
		{
			vector<char> buffer(MAX_BUF_LENGTH);
			while (1)
			{
				int bytesReceived = recv(sock_fd, &buffer[0], buffer.size(), 0);
				string message = "";
				int nbytes;
				int sum = 0;
				if (bytesReceived > 0)
				{
					message.append(buffer.data(), 0, bytesReceived);
					char msg[1024] = {0};
					if (message == "ok")
					{
						cout << "開始寫入" << endl;
						string path = folderpath + seglist[1];
						long fileLen = getFileSize(path);
						sprintf(msg, "%ld%c", fileLen, '\0'); // tell server how many bytes will be sent
						write(sock_fd, msg, sizeof(msg));
						FILE *fin = fopen(&path[0], "rb");
						while (!feof(fin))
						{
							nbytes = fread(msg, sizeof(char), sizeof(msg), fin);
							nbytes = write(sock_fd, msg, nbytes);
							sum += nbytes;
							printf("\rProgress: %.2f %%", ((double)sum / (double)fileLen) * 100);
							fflush(stdout);
						}
						fclose(fin);
					}
					else if (message == "有人正在寫入或讀取 正在等候")
					{
						cout << message << endl;
					}
					else
					{
						cout << message << endl;
						break;
					}
				}
			}
		}
		else
		{
			server_msg = "";
			vector<char> buffer(MAX_BUF_LENGTH);
			int bytesReceived = recv(sock_fd, &buffer[0], buffer.size(), 0);
			if (bytesReceived > 0)
			{
				server_msg.append(buffer.data(), 0, bytesReceived);
				cout << server_msg << "\n";
			}
		}
	}
	cout << "close Socket\n";
	close(sock_fd);
	return 0;
}