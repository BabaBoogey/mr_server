#include "tcpsocket.h"
#include <QHostAddress>
#include <iostream>
#include <QCoreApplication>
#include <QDir>
TcpSocket::TcpSocket(qintptr handle,QObject *parent) : QObject(parent)
{
    socket=nullptr;
    socketDescriptor=handle;
    resetDataType();

    socket=new QTcpSocket(this);
    socket->setSocketDescriptor(socketDescriptor);
    connect(socket,&QTcpSocket::readyRead,this,&TcpSocket::onreadyRead);
    connect(socket,&QTcpSocket::disconnected,this,[=]{
        delete socket;
        socket=nullptr;
        resetDataType();
        emit disconnected();
    });
}

bool TcpSocket::sendMsg(QString str)
{
    if(socket->state()==QAbstractSocket::ConnectedState)
    {
        const QString data=str+"\n";
        int datalength=data.size();
        QString header=QString("DataTypeWithSize:%1 %2\n").arg(0).arg(datalength);
        socket->write(header.toStdString().c_str(),header.size());
        socket->write(data.toStdString().c_str(),data.size());
        socket->flush();
        return true;
    }
    return false;
}

bool TcpSocket::sendFiles(QStringList filePathList,QStringList fileNameList)
{
    if(socket->state()==QAbstractSocket::ConnectedState)
    {
        for(auto filepath:filePathList)
        {
            QString filename=filepath.section('/',-1);
            QFile f(filepath);
            if(!f.open(QIODevice::ReadOnly))
            {
                std::cout<<"can not read file "<<filename.toStdString().c_str()<<","<<f.errorString().toStdString().c_str()<<std::endl;
                continue;
            }
            QByteArray fileData=f.readAll();
            f.close();
            QString header=QString("DataTypeWithSize:%1 %2 %3\n").arg(1).arg(filename).arg(fileData.size());
            this->socket->write(header.toStdString().c_str(),header.size());
            this->socket->write(fileData,fileData.size());
            this->socket->flush();
        }
        return true;
    }
    return true;
}

void TcpSocket::onreadyRead()
{
    if(!datatype.isFile)
    {
        if(datatype.datasize==0)
        {
            if(socket->bytesAvailable()>=1024||socket->canReadLine())
            {
                //read head
                QString msg=socket->readLine(1024);
                if(processHeader(msg))
                    onreadyRead();
            }
        }else
        {
            //read msg
            if(socket->bytesAvailable()>=datatype.datasize)
            {
                QString msg=socket->readLine(datatype.datasize);
                if(processMsg(msg))
                {
                    onreadyRead();
                }
            }
        }
    }else if(socket->bytesAvailable())
    {
        int ret = 0;
        auto bytes=socket->read(datatype.datasize);
        if(datatype.f->write(bytes)==bytes.size())
        {
            datatype.datasize-=bytes.size();
            if(datatype.datasize==0)
            {
                QString filename=datatype.f->fileName();
                datatype.f->close();
                delete datatype.f;
                datatype.f=nullptr;
                resetDataType();
                processFile(filename);
                onreadyRead();
            }else if(datatype.datasize<0)
            {
                ret = 6;
            }
        }else{
            ret = 5;
        }
        if(ret!=0)
        {
            errorprocess(ret,datatype.f->fileName());
        }
    }
}

void TcpSocket::resetDataType()
{
    datatype.isFile=false;
    datatype.datasize=0;
    datatype.filename.clear();
    if(datatype.f)
    {
        delete datatype.f;
        datatype.f=nullptr;
    }
}

bool TcpSocket::processHeader(const QString rmsg)
{
    int ret = 0;
    if(rmsg.endsWith('\n'))
    {
        QString msg=rmsg.trimmed();
        if(msg.startsWith("DataTypeWithSize:"))
        {
            msg=msg.right(msg.size()-QString("DataTypeWithSize:").size());
            QStringList paras=msg.split(";;",QString::SkipEmptyParts);
            if(paras.size()==2&&paras[0]=="0")
            {
                datatype.datasize=paras[1].toInt();
            }else if(paras.size()==3&&paras[0]=="1")
            {
                datatype.isFile=true;
                datatype.filename=paras[1];
                datatype.datasize=paras[2].toInt();
                if(!QDir(QCoreApplication::applicationDirPath()+"/tmp").exists())
                    QDir(QCoreApplication::applicationDirPath()).mkdir("tmp");
                QString filePath=QCoreApplication::applicationDirPath()+"/tmp/"+datatype.filename;
                datatype.f=new QFile(filePath);
                if(!datatype.f->open(QIODevice::WriteOnly))
                    ret=4;
            }else
            {
                ret=3;
            }
        }else
        {
            ret = 2;
        }
    }else
    {
        ret = 1;
    }
    if(!ret) return true;
    errorprocess(ret,rmsg.trimmed()); return false;
}

void TcpSocket::errorprocess(int errcode,QString msg)
{
    //errcode
    //1:not end with '\n';
    //2:not start wth "DataTypeWithSize"
    //3:msg not 2/3 paras
    //4:cannot open file
    //5:read socket != write file
    //6:next read size < 0
    if(errcode==1)
    {
        std::cerr<<"ERROR:msg not end with '\n',";
    }else if(errcode==2)
    {
        std::cerr<<QString("ERROR:%1 not start wth \"DataTypeWithSize\"").arg(msg).toStdString().c_str();
    }else if(errcode==3)
    {
        std::cerr<<QString("ERROR:%1 not 2/3 paras").arg(msg).toStdString().c_str();
    }else if(errcode==4){
        std::cerr<<QString("ERROR:%1 cannot open file").arg(msg).toStdString().c_str();
    }else if(errcode==5){
        std::cerr<<QString("ERROR:%1 read socket != write file").arg(msg).toStdString().c_str();
    }else if(errcode==6){
        std::cerr<<QString("ERROR:%1 next read size < 0").arg(msg).toStdString().c_str();
    }
    std::cerr<<",We will disconnect the socket\n";
    this->socket->disconnectFromHost();
}
