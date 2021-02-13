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
        socket->deleteLater();
        socket=nullptr;
        resetDataType();
        emit disconnected();
    },Qt::DirectConnection);
}

bool TcpSocket::sendMsg(QString str)
{
    if(socket->state()==QAbstractSocket::ConnectedState)
    {
        const QString data=str+"\n";
        int datalength=data.size();
        QString header=QString("DataTypeWithSize:%1 %2\n").arg(0).arg(datalength);
        qDebug()<<socket->write(header.toStdString().c_str(),header.size())<<header;
        qDebug()<<socket->write(data.toStdString().c_str(),data.size())<<data;
        socket->flush();
        qDebug()<<"end1111";
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
                qDebug()<<"fileP:"<<filepath;
            QFile f(filepath);
            QString filename=filepath.section('/',-1);

            if(!f.open(QIODevice::ReadOnly))
            {
                std::cout<<"can not read file "<<filename.toStdString().c_str()<<","<<f.errorString().toStdString().c_str()<<std::endl;
                continue;
            }
            QByteArray fileData=f.readAll();
            f.flush();
            QString header=QString("DataTypeWithSize:%1 %2 %3\n").arg(1).arg(filename).arg(fileData.size());
            this->socket->write(header.toStdString().c_str(),header.size());
            this->socket->write(fileData);
            this->socket->flush();
            f.close();
        }
        return true;
    }
    return true;
}

void TcpSocket::onreadyRead()
{
    qDebug()<<"emm read:"<<this;
    if(!datatype.isFile)
    {
        qDebug()<<"1111"<<this;
        if(datatype.datasize==0)
        {
            qDebug()<<"1112"<<this;
            if(socket->bytesAvailable()>=1024||socket->canReadLine())
            {
                //read head
                qDebug()<<"1117"<<this;
                QString msg=socket->readLine(1024);
                int ret=processHeader(msg);
                qDebug()<<"1112"<<this<<ret<<msg;
                if(!ret) onreadyRead();
                else
                {
                    resetDataType();
                    errorprocess(ret,msg);
                }
            }
        }else
        {
            qDebug()<<"1113"<<this;
            //read msg
            int cnt=socket->bytesAvailable();
            if(cnt>=datatype.datasize)
            {
                int ret=0;
                QString msg=socket->readLine(datatype.datasize+1);
                resetDataType();
                ret=processMsg(msg)?0:7;
                if(!ret) onreadyRead();
                else errorprocess(ret,msg);
            }/*else
            {
                qDebug()<<"bytesAvailable= "<<cnt;
            }*/
        }
    }else if(socket->bytesAvailable())
    {
        qDebug()<<"1114"<<this;
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
                ret=0;
            }else if(datatype.datasize<0)
            {
                resetDataType();
                ret = 6;
            }
        }else{
            resetDataType();
            ret = 5;
        }
        if(!ret) onreadyRead();
        else errorprocess(ret);
    }
qDebug()<<"1151"<<this;
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

char TcpSocket::processHeader(const QString rmsg)
{
    qDebug()<<"processHeader:"<<rmsg;
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
    return ret;
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
    std::cerr<<"errorcode = "<<errcode<<",";
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
    }else if(errcode==7)
    {
        std::cerr<<"";
    }
    this->socket->disconnectFromHost();
//    std::cerr<<"We have disconnected socket.\n";
//    while (this->socket->state()!=QAbstractSocket::UnconnectedState) {
//        this->socket->waitForDisconnected();
//    }
}
