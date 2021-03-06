#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariant>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QDateTime>
#include <QNetworkReply>
#include "basicdatamanage.h"
#include "sha1.h"
#include <QEventLoop>
#include <QObject>
namespace DB {
    uint count =0;
    QMutex locker;
    const QString databaseName="BrainTell";
    const QString dbHostName="localhost";
    const QString dbUserName="root";
    const QString dbPassword="1234";

    const QString TableForUser="TableForUser";
    const QString TableForUserScore="TableForUserScore";
    //    const QString TableForImage="TableForImage";//图像数据表
    //    const QString TableForPreReConstruct="TableForPreReConstruct";//预重建数据表
    //    const QString TableForFullSwc="TableForFullSwc";//重建完成数据表
    //    const QString TableForProof="TableForProof";//校验数据表
    //    const QString TableForCheckResult="TableForCheckResult";//校验结果数据表
    QSqlDatabase getNewDbConnection()
    {
        locker.lock();
        QSqlDatabase db=QSqlDatabase::addDatabase("QMYSQL",QString::number(count++));
        locker.unlock();
        db.setDatabaseName(databaseName);
        db.setHostName(dbHostName);
        db.setUserName(dbUserName);
        db.setPassword(dbPassword);
        return db;
    }

    bool registerCommunicate(const QStringList &registerInfo)
    {

            QNetworkAccessManager *accessManager = new QNetworkAccessManager;
            QNetworkRequest request;
            request.setUrl(QUrl("https://api.netease.im/nimserver/user/create.action"));
            request.setRawHeader("AppKey","f7ded615794f989e8d8850c931b1d77a");
            request.setRawHeader("Nonce","12345");
            QDateTime::currentSecsSinceEpoch();
            QString curTime=QString::number(QDateTime::currentSecsSinceEpoch());
            request.setRawHeader("CurTime",curTime.toStdString().c_str());
            QString appSecret = "9fb785abd90d";
            request.setRawHeader("CheckSum",sha1(QString(appSecret+"12345"+curTime).toStdString()).c_str());
            request.setRawHeader("Content-Type","application/x-www-form-urlencoded;charset=utf-8");
            QByteArray postData;
            postData.append(QString("accid=%1&name=%2&token=%3").arg(registerInfo[0]).arg(registerInfo[2]).arg(registerInfo[3]));

            QNetworkReply* reply = accessManager->post(request, postData);

            QEventLoop eventLoop;
            QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
            eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

            if(reply->error()==QNetworkReply::NoError)
            {
                QByteArray bytes = reply->readAll();      //读取所有字节；
                std::cerr<<bytes.toStdString().c_str();
                QJsonParseError error;
                QJsonDocument doucment = QJsonDocument::fromJson(bytes, &error);
                if (doucment.isObject())
                {
                    QJsonObject obj = doucment.object();
                    QJsonValue val;
                    QJsonValue data_value;

                    if (obj.contains("code")&&obj.value("code").toInt()==200)
                            return true;
                }
            }
            return false;
    }

    bool initDB()
    {
        auto db=getNewDbConnection();
        if(!db.open())
        {
            qDebug()<<"Error:can not connect SQL";
            return false;
        }
        //TableForUser
        QSqlQuery query(db);
        {
            QString order="id INTEGER PRIMARY KEY AUTO_INCREMENT,"
                "userName VARCHAR(100) NOT NULL,"
                "passWord VARCHAR(20) NOT NULL,"
                "email VARCHAR(100) NOT NULL,"
                "nickName VARCHAR(50) NOT NULL"
                ;
            QString sql=QString("CREATE TABLE IF NOT EXISTS %1 (%2)").arg(TableForUser).arg(order);
//                    qDebug()<<sql;
            if(!query.exec(sql)){
                qDebug()<<query.lastError().text();
                return false;
            }
        }
        //TableForUserScore
        {
            QString order="userName VARCHAR(100) PRIMARY KEY NOT NULL,"
                "score int NOT NULL"
                ;
            QString sql=QString("CREATE TABLE IF NOT EXISTS %1 (%2)").arg(TableForUserScore).arg(order);
            if(!query.exec(sql)){

                qDebug()<<query.lastError().text();
                return false;
            }
        }
        return true;
    }

    /**
     * @brief userLogin
     * @param userName
     * @param passward
     * @return 0:success;-1:db error;-2:no name;-3 wrong password
     */
    char userLogin(QStringList loginInfo,QStringList & res)
    {
        auto db=getNewDbConnection();
        if(!db.open())
        {
            qDebug()<<"Error:can not connect SQL";
            return -1;
        }
        QSqlQuery query(db);
        QString order=QString("SELECT * FROM %1 WHERE userName = ?").arg(TableForUser);
        query.prepare(order);
        query.addBindValue(loginInfo[0]);
        if(query.exec()){
            if(!query.next())
            {
                return -2;
            }else
            {
                if(query.value(2).toString()==loginInfo[1])
                {
                    res=loginInfo;
                    res.push_back(query.value(3).toString());
                    res.push_back(query.value(4).toString());
                    return 0;
                }
                else return -3;
            }
        }else
        {
            return -1;
        }
    }

    /**
     * @brief userRegister
     * @param userName
     * @param passward
     * @return 0:success;-1:db error;-2:same name;-3
     */
    char userRegister(const QStringList registerInfo)
    {
        //username,email,nickname,password,invite
        if(!registerCommunicate(registerInfo))
            return -3;
        auto db=getNewDbConnection();
        if(!db.open())
        {
            qDebug()<<"Error:can not connect SQL";
            return -1;
        }
        QSqlQuery query(db);
        {
            QString order=QString("SELECT * FROM %1 WHERE userName = ?").arg(TableForUser);
            query.prepare(order);
            query.addBindValue(registerInfo[0]);
            if(query.exec()){
                if(query.next())
                {
                    return -2;
                }else
                {
                    order=QString("INSERT INTO %1 (userName,passWord,email,nickName) VALUES (?,?,?,?)").arg(TableForUser);
                    query.prepare(order);
                    query.addBindValue(registerInfo[0]);
                    query.addBindValue(registerInfo[3]);
                    query.addBindValue(registerInfo[1]);
                    query.addBindValue(registerInfo[2]);
                    if(query.exec())
                    {
                        order=QString("INSERT INTO %1 (userName,score) VALUES(?,?)").arg(TableForUserScore);
                        query.prepare(order);
                        query.addBindValue(registerInfo[0]);
                        query.addBindValue(0);
                        if(query.exec())
                        {
                             return 0;
                        }
                    }
                    else return -1;
                }
            }else
            {
                return -1;
            }
        }

    }


    char findPassword(QString data,QStringList & res)
    {
        auto db=getNewDbConnection();
        if(!db.open())
        {
            qDebug()<<"Error:can not connect SQL";
            return -1;
        }
        QSqlQuery query(db);
        QString order=QString("SELECT * FROM %1 WHERE email = ?").arg(TableForUser);
        query.prepare(order);
        query.addBindValue(data);
        if(query.exec()){
            if(query.next())
            {
                return -2;
            }else
            {
                res.push_back(query.value(1).toString());
                res.push_back(query.value(2).toString());
//                res={query.value(1).toString(),query.value(2).toString()};
                    return 0;
            }
        }else
        {
            return -1;
        }
    }

    int getid(QString userName)
    {
        auto db=getNewDbConnection();
        if(!db.open())
        {
            qDebug()<<"Error:can not connect SQL";
            return -1;
        }
        QSqlQuery query(db);
        QString order=QString("SELECT id FROM %1 WHERE userName = ?").arg(TableForUser);
        query.prepare(order);
        query.addBindValue(userName);
        if(query.exec()){
            if(!query.next())
            {
                return -2;
            }else
            {
                return query.value(0).toUInt();
            }
        }else
        {
            return -1;
        }
    }

    int getScore(QString userName)
    {
        auto db=getNewDbConnection();
        if(!db.open())
        {
            qDebug()<<"Error:can not connect SQL";
            return -1;
        }
        QSqlQuery query(db);
        QString order=QString("SELECT score FROM %1 WHERE userName = ?").arg(TableForUserScore);
        query.prepare(order);
        query.addBindValue(userName);
        if(query.exec()&&query.next())
        {
           return query.value(0).toUInt();
        }
        return 0;
    }

    bool setScores(QStringList userNames,std::vector<int> scores)
    {
        auto db=getNewDbConnection();
        if(!db.open())
        {
            qDebug()<<"Error:can not connect SQL";
            return false;
        }

        QSqlQuery query(db);
        QString order=QString("update %1 set score = ? WHERE userName = ?").arg(TableForUserScore);
        query.prepare(order);
        QVariantList ints;
        for(auto v:scores)
        {
            ints<<v;
        }
        query.addBindValue(ints);
        query.addBindValue(userNames);

        if(query.execBatch())
        {
           return true;
        }
        return false;
    }
}
