#include <QCoreApplication>
#include <stdlib.h>
#include <QMutex>
#include <QDateTime>
#include <QFile>
#include <cstdlib>
#include <cstdio>
#include "manageserver.h"
//传入的apo需要重新保存，使得n按顺序
#include "basicdatamanage.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <iostream>
//传入的apo需要重新保存，使得n按顺序
QString vaa3dPath;
QMap<QString,QStringList> m_MapImageIdWithRes;
QMap<QString,QString> m_MapImageIdWithDir;
void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
void processImageSrc();
int main(int argc, char *argv[])
{


//    qInstallMessageHandler(myMessageOutput);
    QCoreApplication a(argc, argv);
//       std::cerr<< DB::registerCommunicate({"gfdgddshl44333","","huanglei","123456"});
    vaa3dPath=QCoreApplication::applicationDirPath()+"/vaa3d";
    processImageSrc();
    ManageServer server;
    if(!server.listen(QHostAddress::Any,23763))
    {
        qDebug()<<"Error:cannot start server in port 9999,please check!";
        exit(-1);
    }else
    {
        if(!DB::initDB())
        {
            qDebug()<<"mysql error";
            exit(-1);
        }else
        {
            qDebug()<<"server(2.0.5.0) for vr_farm started!\nBuild "<<__DATE__<<__TIME__;
        }
    }
    return a.exec();
}

void processImageSrc()
{
    m_MapImageIdWithDir.clear();
    m_MapImageIdWithRes.clear();
    QFile data(QCoreApplication::applicationDirPath()+"/imageSrc.txt");
    if (data.open(QFile::ReadOnly)) {
        QTextStream in(&data);
        while (!in.atEnd()) {
            QString imageId;
            QString imageName;
            int resCnt;
            in>>imageId>>imageName>>resCnt;
            m_MapImageIdWithDir.insert(imageId,imageName);
            QStringList list;
            for(int i=0;i<resCnt;i++)
            {
                in>>imageName;
                list.push_back(imageName);
            }
            m_MapImageIdWithRes.insert(imageId,list);
        }
    }
    data.close();
}


void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 加锁
    static QMutex mutex;
    mutex.lock();

    QByteArray localMsg = msg.toLocal8Bit();

    // 设置输出信息格式
    QString strDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss : ");
//    QString strMessage = QString("%1 File:%2  Line:%3  Function:%4  DateTime:%5\n")
//            .arg(localMsg.constData()).arg(context.file).arg(context.line).arg(context.function).arg(strDateTime);
    QString strMessage=strDateTime+localMsg.constData()+"\n";
    // 输出信息至文件中（读写、追加形式）
    QFile file("log.txt");
    file.open(QIODevice::ReadWrite | QIODevice::Append);
    QTextStream stream(&file);
    stream << strMessage ;
    file.flush();

    file.close();
    // 解锁
    mutex.unlock();
    fprintf(stderr,strMessage.toStdString().c_str());
}
