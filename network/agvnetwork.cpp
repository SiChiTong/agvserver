﻿#include "agvnetwork.h"

#include "util/global.h"

AgvNetWork::AgvNetWork(QObject *parent) : QObject(parent),isQuit(false)
{

}

AgvNetWork::~AgvNetWork()
{
    isQuit = true;
    m_clientIOCP.Stop();
}

void AgvNetWork::sendToOne(SOCKET sock,const char *buf, int len)//发送给某个id的消息
{
    try{
        m_clientIOCP.doSend(sock,buf,len);
    }catch(const std::out_of_range& oor){

    }
}

void AgvNetWork::sendToAll(char *buf,int len)//发送给所有的人的信息
{
    m_clientIOCP._DoSend(buf,len);
}

void AgvNetWork::sendToSome(QList<int> ones,char *buf,int len)//发送给某些id的人的消息
{
    for(QList<int>::iterator itr = ones.begin();itr!=ones.end();++itr){
        int id = *itr;
        sendToOne(id,buf,len);
    }
}

void AgvNetWork::onRecvClientMsg(void *param, char *buf, int len,SOCKET sock,const QString &sIp,int port)
{
    //TODO:
    if (param != NULL)
    {
        ((AgvNetWork *)param)->recvClientMsgProcess(buf, len,sock,sIp,port);
    }
}

void AgvNetWork::onDisconnectClient(void *owner, SOCKET sock, const QString &sIp, int port)
{
    //一个客户端掉线了
    //1.提出所有它订阅的东西
    g_msgCenter.removeAgvPositionSubscribe(sock);
    g_msgCenter.removeAgvStatusSubscribe(sock);
    //2.将在线状态修改
    int user_id = -1;

    //查找socket对应的用户ID
    if(!loginUserIdSock.contains(sock)){
        return ;
    }
    user_id = loginUserIdSock[sock].id;
    //从已登录中移出
    loginUserIdSock.remove(sock);

    //设置用户的在线状态
    if(user_id>0){
        QString updateSql = "update agv_user set signState = 0 where id=?";
        QList<QVariant> params;
        params<<user_id;
        g_sql->exeSql(updateSql,params);
    }
}


void AgvNetWork::recvClientMsgProcess(char *buf, int len,SOCKET sock,const QString &ip, int port)
{
    if(len<=0||buf==NULL)return ;

    //数据
    std::string ss = std::string(buf,len);

    //加入缓冲区
    if(client2serverBuffer.contains(sock)){
        client2serverBuffer.insert(sock,ss);
    }else{
        client2serverBuffer[sock] = client2serverBuffer[sock]+ss;
    }

    //然后对其进行拆包，把拆出来的包放入队列
    while(true){
        size_t start = client2serverBuffer[sock].find("<xml>");
        size_t end = client2serverBuffer[sock].find("</xml>",start);
        if(start!=std::string::npos&&end!=std::string::npos){
            //这个是一个包:
            std::string onPack = client2serverBuffer[sock].substr(start,end-start+6);
            if(onPack.length()>0){
                //进行入队
                QyhMsgDateItem item;
                item.data= onPack;
                item.ip = ip;
                item.port = port;
                item.sock=sock;
                g_user_msg_queue.enqueue(item);
            }
            //然后将之前的数据丢弃
            client2serverBuffer[sock] = client2serverBuffer[sock].substr(end+6);
        }else{
            break;
        }
    }
}

bool AgvNetWork::initServer()
{
    if (false == m_clientIOCP.LoadSocketLib())
    {
        g_log->log(AGV_LOG_LEVEL_FATAL,"加载Winsock 2.2失败，服务器端无法运行！");
        return false;
    }
    m_clientIOCP.SetPort(8988);
    m_clientIOCP.setOwner(this);
    if (false == m_clientIOCP.Start(onRecvClientMsg,onDisconnectClient))
    {
        g_log->log(AGV_LOG_LEVEL_FATAL,"iocp服务器启动失败！");
        return false;
    }

    g_log->log(AGV_LOG_LEVEL_INFO,"iocp server start ok");
    return true;
}

