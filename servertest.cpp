#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>

// C++ library
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <fstream>
#include <algorithm>

#define MAX_CONNECT_NUM 2
#define MAX_BUF_LENGTH 1024
using namespace std;
int server_sockfd;
string folderpath = "serverdata/";
int write_connect_cnt = 0;
int read_connect_cnt = 0;
class fileinfo
{
public:
    string filename;
    string rights;
    string owner;
    string group;
    int bytes;
    string updatetime;

public:
    fileinfo(string _owner, string _group, string _filename, string _rights, int _bytes, string _time)
    {
        filename = _filename;
        rights = _rights;
        owner = _owner;
        group = _group;
        bytes = _bytes;
        updatetime = _time;
    }
};
map<string, fileinfo> filelist;
map<string, string> userlist;
long getFileSize(string &filename)
{
    FILE *fin = fopen(&filename[0], "rb");
    fseek(fin, 0, SEEK_END);
    long size = ftell(fin);
    fclose(fin);
    return size;
}
//gettime
string get_time()
{
    time_t timep;
    time(&timep);
    static char buf[32];
    char *p = NULL;
    strcpy(buf, ctime(&timep));
    p = strchr(buf, '\n');
    *p = '\0';
    return buf;
}
bool createfile(string user, string user_grp, string filename, string right, int clientfd)
{
    string path = folderpath + filename;
    map<string, fileinfo>::iterator iter;
    iter = filelist.find(filename);
    if (iter != filelist.end())
    {
        char message[] = {"檔案已存在建立失敗"};
        send(clientfd, &message[0], sizeof(message), 0);
        return false;
    }
    else
    {
        //create file
        string time = get_time();
        FILE *pFile;
        pFile = fopen(&path[0], "w+");
        fclose(pFile);
        fileinfo newfile(user, user_grp, filename, right, 0, time);
        filelist.insert(std::make_pair(filename, newfile));
        //寫入權限
        FILE *my_filelist;
        my_filelist = fopen("filelist.txt", "a+");
        string message = right + "," + user + "," + user_grp + ",0," + time + "," + filename + "\r\n";
        fwrite(&message[0], message.length(), 1, my_filelist);
        fclose(my_filelist);
        message = "檔案建立成功";
        send(clientfd, &message[0], message.length(), 0);
        return true;
    }
}
void writefile(string user, string user_grp, string filename, int clientfd, string mode)
{
    string path = folderpath + filename;
    map<string, fileinfo>::iterator iter;
    iter = filelist.find(filename);
    bool hasright = false;
    string message = "";
    if (mode != "o" && mode != "a")
    {
        message = "請輸入正確格式 wrtie 檔名 o/a";
    }
    else if (iter != filelist.end())
    { //檔案存在
        fileinfo writefile = iter->second;
        char seglist[6];
        for (int i = 0; i < writefile.rights.size(); i++)
        {
            seglist[i] = writefile.rights[i];
        }
        if (writefile.owner == user)
        { //檢查權限
            if (seglist[1] == 'w')
            {
                hasright = true;
            }
        }
        if (writefile.group == user_grp)
        {
            if (seglist[3] == 'w')
            {
                hasright = true;
            }
        }
        if (seglist[5] == 'w')
        {
            hasright = true;
        }
    }
    else
    { //檔案不存在
        message += filename + "檔案不存在";
    }
    if (!hasright)
    {
        message += "權限不夠";
    }
    if (message == "" && hasright)
    {        
        while (write_connect_cnt > 0 || read_connect_cnt > 0)
        {
            message = "有人正在寫入或讀取 正在等候";
            send(clientfd, &message[0], message.length(), 0);
            sleep(1);
        }
        message = "ok";
    }

    send(clientfd, &message[0], message.length(), 0);
    string filebyte = "";
    if (message == "ok")
    {
        write_connect_cnt++;
        FILE *fout;
        if (mode == "o") // write
            fout = fopen(&path[0], "wb");
        else if (mode == "a") // append
            fout = fopen(&path[0], "ab");
        int nbytes;
        int sum = 0;
        char msg[1024] = {0};
        while (1)
        {
            int fl = read(clientfd, msg, sizeof(msg));
            if (fl > 0)
            {
                break;
            }
        }; // get file length
        int fileLen = stoi(string(msg));
        while (sum < fileLen)
        {
            nbytes = read(clientfd, msg, sizeof(msg));
            nbytes = fwrite(msg, sizeof(char), nbytes, fout);
            sum += nbytes;
        }
        fclose(fout);
        map<string, fileinfo>::iterator iter;
        iter = filelist.find(filename);
        if (iter != filelist.end())
        {
            fileinfo writefile = iter->second;
            if (mode == "o")
            {
                writefile.bytes = fileLen;
            }
            else if (mode == "a")
            {
                writefile.bytes += fileLen;
            }
            writefile.updatetime = get_time();
            filelist.find(filename)->second = writefile;
            FILE *my_filelist;
            my_filelist = fopen("filelist.txt", "w");
            map<string, fileinfo>::iterator iter;
            for (iter = filelist.begin(); iter != filelist.end(); iter++)
            {
                string message = iter->second.rights + "," + iter->second.owner + "," + iter->second.group + "," + std::to_string(iter->second.bytes) + "," + get_time() + "," + iter->second.filename + "\r\n";
                fwrite(&message[0], message.length(), 1, my_filelist);
            }
            fclose(my_filelist);
            string message = "上傳成功";
            send(clientfd, &message[0], message.length(), 0);
            write_connect_cnt--;
        }
    }
}

void readfile(string user, string user_grp, string filename, int clientfd)
{
    string path = folderpath + filename;
    map<string, fileinfo>::iterator iter;
    iter = filelist.find(filename);
    string message = "";
    bool hasright = false;
    if (iter != filelist.end())
    { //檔案存在
        fileinfo readfile = iter->second;
        char seglist[6];
        for (int i = 0; i < readfile.rights.size(); i++)
        {
            seglist[i] = readfile.rights[i];
        }
        if (readfile.owner == user)
        { //檢查權限
            if (seglist[0] == 'r')
            {
                hasright = true;
            }
        }
        if (readfile.group == user_grp)
        {
            if (seglist[2] == 'r')
            {
                hasright = true;
            }
        }
        if (seglist[4] == 'r')
        {
            hasright = true;
        }
    }
    else
    { //檔案不存在
        message += filename + "檔案不存在";
    }
    if (!hasright)
    {
        message += "權限不夠";
    }
    if (message == "" && hasright)
    {
        while (write_connect_cnt > 0)
        {
            message = "有人正在寫入 正在等候";
            send(clientfd, &message[0], message.length(), 0);
            sleep(1);
        }
        message = "ok";
    }
    send(clientfd, &message[0], message.length(), 0);
    if (message == "ok")
    {

        read_connect_cnt++;
        int nbytes;
        int sum = 0;
        char msg[1024] = {0};
        string path = folderpath + filename;
        long fileLen = getFileSize(path);
        sprintf(msg, "%ld%c", fileLen, '\0'); // tell server how many bytes will be sent
        send(clientfd, msg, sizeof(msg), 0);
        // start to upload
        FILE *fin = fopen(&path[0], "rb");
        while (!feof(fin))
        {
            nbytes = fread(msg, sizeof(char), sizeof(msg), fin);
            nbytes = write(clientfd, msg, nbytes);
        }
        fclose(fin);
        message = "下載成功";
        send(clientfd, &message[0], message.length(), 0);
        read_connect_cnt--;
    }
}
void changemodefile(string user, string filename, string right, int clientfd)
{
    string path = folderpath + filename;
    map<string, fileinfo>::iterator iter;
    iter = filelist.find(filename);
    if (iter != filelist.end())
    { //檔案存在
        fileinfo readfile = iter->second;
        if (readfile.owner == user)
        {
            readfile.rights = right;
            filelist.find(filename)->second = readfile;
            FILE *my_filelist;
            my_filelist = fopen("filelist.txt", "w");
            map<string, fileinfo>::iterator iter;
            for (iter = filelist.begin(); iter != filelist.end(); iter++)
            {
                string message = iter->second.rights + "," + iter->second.owner + "," + iter->second.group + "," + to_string(iter->second.bytes) + "," + get_time() + "," + iter->second.filename + "\r\n";
                fwrite(&message[0], message.length(), 1, my_filelist);
            }
            fclose(my_filelist);
            string message = "變更權限完畢";
            send(clientfd, &message[0], message.length(), 0);
        }
        else
        {
            string message = "不是擁有者無法變更";
            send(clientfd, &message[0], message.length(), 0);
        }
    }
    else
    { //檔案不存在
        string message = "檔案不存在";
        send(clientfd, &message[0], message.length(), 0);
    }
    /*for (const auto &s : filelist)
    {
        std::cout << " name: " << s.second.filename << "," << s.second.rights << "\n";
    }*/
}
void *Recv(void *p)
{
    int newfd = *(int *)p;
    string server_msg = "";
    vector<char> buffer(MAX_BUF_LENGTH);
    string message = "please input account";
    string user = "";
    string user_grp = "";
    int bytesReceived;
    map<string, string>::iterator iter;
    if (send(newfd, &message[0], message.length(), 0) > 0)
    {
        while (1)
        {
            user = "";
            user_grp = "";
            bytesReceived = recv(newfd, &buffer[0], buffer.size(), 0);
            if (bytesReceived > 0)
            {
                user.append(buffer.data(), 0, bytesReceived);
                iter = userlist.find(user);
                if (iter != userlist.end())
                {
                    user_grp = userlist[user];
                    message = user + "登入成功";
                    send(newfd, &message[0], message.length(), 0);
                    break;
                }
                else
                {
                    message = user + "無此帳號,請在輸入一次";
                    send(newfd, &message[0], message.length(), 0);
                }
            }
        }
    }
    while (1)
    {
        server_msg = "";
        bytesReceived = recv(newfd, &buffer[0], buffer.size(), 0);
        if (bytesReceived <= 0)
        {
            break;
        }
        else
        {
            server_msg.append(buffer.data(), 0, bytesReceived);
        }
        if (server_msg.length() > 0)
        {
            std::stringstream test(server_msg);
            std::string segment;
            std::vector<std::string> seglist;
            while (std::getline(test, segment, ' '))
            {
                seglist.push_back(segment);
            }
            if (seglist[0] == "create")
            {
                if (seglist.size() == 3)
                {
                    createfile(user, user_grp, seglist[1], seglist[2], newfd);
                }
                else
                {
                    string message = "請輸入正確格式 create 檔名 權限";
                    send(newfd, &message[0], message.length(), 0);
                }
            }
            else if (seglist[0] == "read")
            {
                if (seglist.size() == 2)
                {
                    readfile(user, user_grp, seglist[1], newfd);
                }
                else
                {
                    string message = "請輸入正確格式 read 檔名";
                    send(newfd, &message[0], message.length(), 0);
                }
            }
            else if (seglist[0] == "write")
            {
                if (seglist.size() == 3)
                {
                    writefile(user, user_grp, seglist[1], newfd, seglist[2]);
                }
                else
                {
                    string message = "請輸入正確格式 wrtie 檔名 o/a";
                    send(newfd, &message[0], message.length(), 0);
                }
            }
            else if (seglist[0] == "changemode")
            {
                if (seglist.size() == 3 && seglist[2].length() == 6)
                {
                    changemodefile(user, seglist[1], seglist[2], newfd);
                }
                else
                {
                    string message = "請輸入正確格式 changemode 檔名 權限";
                    send(newfd, &message[0], message.length(), 0);
                }
            }
            else
            {
                string message = "請輸入正確指令";
                send(newfd, &message[0], message.length(), 0);
            }
        }
    }
    cout << "close client_Socket\n";
    close(newfd);
    return 0;
}
void *accept(void *arg)
{
    int client_sockfd;
    pthread_t tid2, tid3;
    while (true)
    {
        client_sockfd = accept(server_sockfd, NULL, NULL);
        //cout<<client_sockfd<<"\n";
        pthread_create(&tid2, NULL, Recv, &client_sockfd); // 建立執行緒  執行緒接收資料資
    }
    cout << "close Socket\n";
    close(server_sockfd);
    return 0;
}
void initload()
{
    //user
    FILE *my_userlist = fopen("userlist.txt", "r");
    if (my_userlist == NULL)
    {
        my_userlist = fopen("userlist.txt", "w+");
    }
    else
    {
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, my_userlist)) != -1)
        {
            string line2 = line;
            stringstream test(line2);
            string segment;
            vector<string> seglist;
            while (std::getline(test, segment, ','))
            {
                seglist.push_back(segment);
            }
            /*cout << "seglist大小：" << seglist.size() << endl;
            for (int i = 0; i < seglist.size(); i++)
            {
                cout << seglist[i] << endl;
            }*/
            userlist.insert(pair<string, string>(seglist[0], seglist[1].erase(seglist[1].length() - 2, seglist[1].length())));
        }
    }
    /*map<string, string>::iterator iter;
    for (iter = userlist.begin(); iter != userlist.end(); iter++)
    {
        cout << iter->first << " " << iter->second<<endl;
    }*/
    fclose(my_userlist);
    //file
    FILE *my_filelist = fopen("filelist.txt", "r");
    if (my_filelist == NULL)
    {
        my_filelist = fopen("filelist.txt", "w+");
    }
    else
    {
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, my_filelist)) != -1)
        {
            string line2 = line;
            stringstream test(line2);
            string segment;
            vector<string> seglist;
            while (std::getline(test, segment, ','))
            {
                seglist.push_back(segment);
            }
            /*cout << "seglist大小：" << seglist.size() << endl;
            for (int i = 0; i < seglist.size(); i++)
            {
                cout << seglist[i] << endl;
            }*/
            fileinfo newfile(seglist[1], seglist[2], seglist[5].erase(seglist[5].length() - 2, seglist[5].length()), seglist[0], atoi(&seglist[3][0]), seglist[4]);
            filelist.insert(std::make_pair(seglist[5], newfile));
        }
    }
    /*map<string, fileinfo>::iterator iter;
    for (iter = filelist.begin(); iter != filelist.end(); iter++)
    {
        cout << iter->first << " " << iter->second.rights << "," << iter->second.owner << "," << iter->second.group << "," << iter->second.bytes << "," << iter->second.updatetime << "," << iter->second.filename << endl;
    }
    fclose(my_filelist);*/
}
int main()
{
    initload();
    struct sockaddr_in un;
    pthread_t tid;
    mkdir("serverdata", 0777); // if "data" directory doesn't exist, create it
    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket error");
        exit(1);
    }
    // set basic properties
    struct sockaddr_in server_sockIn;
    bzero(&server_sockIn, sizeof(server_sockIn)); //初始化
    server_sockIn.sin_family = AF_INET;
    server_sockIn.sin_addr.s_addr = INADDR_ANY;
    server_sockIn.sin_port = htons(8100);
    if (bind(server_sockfd, (struct sockaddr *)&server_sockIn, sizeof(server_sockIn)) < 0)
    {
        perror("Bind Fail\n");
        exit(1);
    }
    if (listen(server_sockfd, 10) < 0)
    {
        cout << "listen failed!\n";
        return -1;
    }
    int ret = pthread_create(&tid, NULL, accept, NULL); // 建立執行緒  執行緒接收資料資訊
    while (1)
    {
        string command;
        getline(cin, command);
        if (command.length() > 0)
        {
            stringstream test(command);
            string segment;
            vector<string> seglist;
            map<string, string>::iterator iter;
            while (std::getline(test, segment, ' '))
            {
                seglist.push_back(segment);
            }
            if (seglist[0] == "add")
            {
                if (seglist.size() == 3)
                {
                    iter = userlist.find(seglist[1]);
                    if (iter != userlist.end())
                    {
                        cout << "此帳號已存在" << endl;
                    }
                    else
                    {
                        userlist.insert(pair<string, string>(seglist[1], seglist[2]));
                        cout << "帳號:" << seglist[1] << "新增成功" << endl;
                        //寫入權限
                        FILE *my_userlist;
                        my_userlist = fopen("userlist.txt", "a+");
                        string message = seglist[1] + "," + seglist[2] + "\r\n";
                        fwrite(&message[0], message.length(), 1, my_userlist);
                        fclose(my_userlist);
                    }
                }
                else
                {
                    cout << "請輸入正確格式" << endl;
                }
            }
            else if (command == "ls")
            {
                FILE *my_filelist = fopen("filelist.txt", "r");
                if (my_filelist == NULL)
                {
                    my_filelist = fopen("filelist.txt", "w+");
                }
                map<string, fileinfo>::iterator iter;
                for (iter = filelist.begin(); iter != filelist.end(); iter++)
                {
                    cout << iter->second.rights << "," << iter->second.owner << "," << iter->second.group << "," << iter->second.bytes << "," << iter->second.updatetime << "," << iter->second.filename << endl;
                }
                fclose(my_filelist);
            }
            else
            {
                cout << "請輸入正確格式" << endl;
            }
        }
    }
    return 0;
}
