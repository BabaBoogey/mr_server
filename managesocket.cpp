#include "managesocket.h"
#include <QDataStream>
#include <QSqlQuery>
#include <QFile>
#include <QCoreApplication>
#include <QRegExp>
#include <QTime>
#include "basicdatamanage.h"
#include "messageserver.h"

ManageSocket::ManageSocket(qintptr handle,QObject *parent):QObject(parent)
{
    {
        socket=nullptr;
        socketDescriptor=handle;
        resetDataInfo();
        msgs.clear();
        filepaths.clear();
    }
    socket=new QTcpSocket;
    socket->setSocketDescriptor(socketDescriptor);
    connect(socket,&QTcpSocket::readyRead,this,&ManageSocket::onreadyRead);
    connect(socket,&QTcpSocket::disconnected,this,&ManageSocket::deleteLater);

}

void ManageSocket::onreadyRead()
{
    if(dataInfo.dataReadedSize==0&&socket->bytesAvailable()>=sizeof (qint32))
    {
        QDataStream in(socket);
        in>>dataInfo.dataSize;dataInfo.dataReadedSize+=sizeof (qint32);
        if(dataInfo.dataSize<0) {
            qDebug()<<"error";return;
        }
        if(dataInfo.dataSize==dataInfo.dataReadedSize)
        {
            resetDataInfo();return;
        }

        if(socket->bytesAvailable()>=dataInfo.dataSize-dataInfo.dataReadedSize)
        {
            QStringList list;
            while(dataInfo.dataSize!=dataInfo.dataReadedSize)
            {
                in>>dataInfo.stringOrFilenameSize>>dataInfo.filedataSize;
                QString messageOrFileName=QString::fromUtf8(socket->read(dataInfo.stringOrFilenameSize),dataInfo.stringOrFilenameSize);
                if(dataInfo.filedataSize==0)
                {
                    list.push_back("00"+messageOrFileName);
                }else
                {
                    QByteArray block=socket->read(dataInfo.dataSize-dataInfo.dataReadedSize);
                    QString filePath=QCoreApplication::applicationDirPath()+"/tmp/"+messageOrFileName;
                    if(!QDir(QCoreApplication::applicationDirPath()+"/tmp").exists())
                    {
                        QDir(QCoreApplication::applicationDirPath()).mkdir("tmp");
                    }
                    QFile file(filePath);
                    file.open(QIODevice::WriteOnly);
                    file.write(block);file.flush();
                    file.close();
                    list.push_back("11"+filePath);
                }
                dataInfo.dataReadedSize+=(2*sizeof (qint32)+dataInfo.stringOrFilenameSize+dataInfo.filedataSize);
            }
            resetDataInfo();
            processReaded(list);
        }
    }else
    {
        if(socket->bytesAvailable()>=dataInfo.dataSize-dataInfo.dataReadedSize)
        {
            QDataStream in(socket);
            QStringList list;
            while(dataInfo.dataSize!=dataInfo.dataReadedSize)
            {
                in>>dataInfo.stringOrFilenameSize>>dataInfo.filedataSize;
                QString messageOrFileName=QString::fromUtf8(socket->read(dataInfo.stringOrFilenameSize),dataInfo.stringOrFilenameSize);
                if(dataInfo.filedataSize==0)
                {
                    list.push_back("00"+messageOrFileName);
                }else
                {
                    QByteArray block=socket->read(dataInfo.dataSize-dataInfo.dataReadedSize);
                    QString filePath=QCoreApplication::applicationDirPath()+"/tmp/"+messageOrFileName;
                    if(!QDir(QCoreApplication::applicationDirPath()+"/tmp").exists())
                    {
                        QDir(QCoreApplication::applicationDirPath()).mkdir("tmp");
                    }
                    QFile file(filePath);
                    file.open(QIODevice::WriteOnly);
                    file.write(block);
                    file.close();
                    list.push_back("11"+filePath);
                }
                dataInfo.dataReadedSize+=(2*sizeof (qint32)+dataInfo.stringOrFilenameSize+dataInfo.filedataSize);
            }
            resetDataInfo();
            processReaded(list);
        }
    }
}

void ManageSocket::resetDataInfo()
{
     dataInfo.dataSize=0;dataInfo.stringOrFilenameSize=0;
     dataInfo.dataReadedSize=0;dataInfo.filedataSize=0;
}

void ManageSocket::sendMsg(QString msg)
{
    qint32 stringSize=msg.toUtf8().size();
    qint32 totalsize=3*sizeof (qint32)+stringSize;
    QByteArray block;
    QDataStream dts(&block,QIODevice::WriteOnly);
    dts<<qint32(totalsize)<<qint32(stringSize)<<qint32(0);
    block+=msg.toUtf8();
    socket->write(block);
    socket->waitForBytesWritten();
}

void ManageSocket::sendFiles(QStringList filePathList,QStringList fileNameList)
{
    qint32 totalsize=sizeof (qint32);
    QByteArray block1;
    QDataStream dts(&block1,QIODevice::WriteOnly);
    for(int i=0;i<filePathList.size();i++)
    {
        auto fileName=fileNameList[i].toUtf8();
        auto fileData=QFile(filePathList[i]).readAll();
        totalsize += 2* sizeof (qint32);
        totalsize += fileName.size();
        totalsize += fileData.size();
        dts<<qint32(fileName.size())<<qint32(fileData.size());
        block1 += fileName +=fileData;
    }
    QByteArray block;
    QDataStream dts1(&block,QIODevice::WriteOnly);
    dts<<qint32(totalsize);
    block+=block1;
    socket->write(block);
    socket->waitForBytesWritten();
    for(auto filepath:filePathList)
    {
        if(filepath.contains("/tmp/"))
        {
            QFile(filepath).remove();
        }
    }
}

void ManageSocket::processReaded(QStringList list)
{
    for(auto msg:list)
    {
        if(msg.startsWith("00"))
        {
            processFile(filepaths);
            msgs.push_back(msg.remove(0,2));
        }else if(msg.startsWith("11"))
        {
            processMsg(msgs);
            filepaths.push_back(msg.remove(0,2));
        }
    }
    processMsg(msgs);
    processFile(filepaths);
}
void ManageSocket::processMsg( QStringList &msgs)
{
     for(auto msg:msgs)
     {
        QRegExp Download("(.*):Download");//;;;;:Download
        QRegExp LoadANO("(.*):LoadANO");//17302_00001:LoadANO
        QRegExp FileList("(.*):CurrentFiles");//data:CurrentFiles

        if(Download.indexIn(msg)!=-1)
        {
            auto pathMapName=FE::getFilesPathFormFileName(Download.cap(1).trimmed());
            sendFiles(pathMapName.firstKey(),pathMapName.value(pathMapName.firstKey()));
        }else if(LoadANO.indexIn(msg)!=-1)
        {
            auto p=makeMessageServer(LoadANO.cap(1).trimmed());
            auto port=p?p->port:"-1";
            sendMsg(port+":Port");
        }else if(FileList.indexIn(msg)!=-1)
        {
            //返回当前所有文件的列表
            QString dirname=FileList.cap(1).trimmed().split(";").at(1).trimmed();
            QStringList datafileNames=FE::getFileNames(dirname);
            sendMsg(datafileNames.join(";")+";"+msg);
        }
    }
     msgs.clear();
}
void ManageSocket::processFile( QStringList &filePaths)
{
    FE::processFileFromClient(filePaths);
    filePaths.clear();
}

MessageServer* ManageSocket::makeMessageServer(QString neuron)
{
    return MessageServer::makeMessageServer(neuron);
}
