/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of SCXML on Qt labs
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QApplication>
#include "sc_controller.h"
#include <QLabel>
#include <QMessageBox>
#include "ui_frame.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslSocket>
class AbstractModel : public QObject
{
    Q_OBJECT
    public:
    AbstractModel(QObject* o = NULL) : QObject(o)
    {
    }
    public slots:
        virtual void login(const QString & u, const QString & p) = 0;
    signals:
        void loginComplete(bool);
};

class DummyModel : public AbstractModel
{
    Q_OBJECT
    public:
    DummyModel(QObject* o = NULL) : AbstractModel(o)
    {
    }
    public slots:
        void test()
        {
            emit loginComplete(user == "user" && password == "password");
        }
        virtual void login(const QString & u, const QString & p)
        {
            user = u; password = p;
            QTimer::singleShot(5000,this,SLOT(test()));
        }
    signals:
        void loginComplete(bool);
    private:
        QString user,password;
        QNetworkAccessManager netAccess;
};
class GMailModel : public AbstractModel
{
    Q_OBJECT
    public:
    GMailModel(QObject* o = NULL) : AbstractModel(o)
    {
    }
    public slots:
        void loginFinished()
        {
            QNetworkReply* rep = qobject_cast<QNetworkReply*>(sender());
            rep->deleteLater();
            qDebug() << rep->error() << rep->errorString();
            emit loginComplete(rep->error() == QNetworkReply::NoError);
        }
        virtual void login(const QString & u, const QString & p)
        {
            QNetworkRequest req;
            QUrl url("https://mail.google.com/mail/feed/atom");
            url.setUserName(u);
            url.setPassword(p);
            req.setUrl(url);
            QNetworkReply* reply = netAccess.get(req);
            reply->ignoreSslErrors();
            connect(reply,SIGNAL(finished()),this,SLOT(loginFinished()));
        }
    private:
        QNetworkAccessManager netAccess;
};

class MyView : public QFrame, public virtual Ui::Frame
{
    Q_OBJECT
    public:
    QString welcomeText;
    MyView(QWidget* o = NULL) : QFrame(o)
    {
        setupUi(this);
        connect(loginButton,SIGNAL(clicked()),this,SIGNAL(loginIntent()));
        connect(logoutButton,SIGNAL(clicked()),this,SIGNAL(logoutIntent()));
        connect(cancelButton,SIGNAL(clicked()),this,SIGNAL(cancelIntent()));
    }

    public slots:

        void notifyLogin()
        {
            QMessageBox::information(this,"Logged In","You are now logged in!");
            emit contIntent();
        }
        void notifyTimeout()
        {
            QMessageBox::information(this,"Timeout...","Sorry, login has timed out");
            emit contIntent();
        }
        void notifyWelcome()
        {
            QMessageBox::information(this,"Welcome!",welcomeText);
        }
        void notifyError()
        {
            qDebug() << "notifyError";
            QMessageBox::warning(this,"Not Logged in","Login has failed...");
            emit contIntent();
        }

        void notifyHello(const QString & name)
        {
            QMessageBox::information(this,"Welcome",QString("Hello ") + name);
        }

    signals:
        void loginIntent();
        void logoutIntent();
        void cancelIntent();
        void contIntent();
};
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    bool use_gmail = QSslSocket::supportsSsl () && !a.arguments().contains("--dummy");
    AbstractModel* model = use_gmail ? (AbstractModel*)new GMailModel() : (AbstractModel*)new DummyModel();
    MyView* view = new MyView();
    SMClass_controller *controller = new SMClass_controller();
    controller->setupStateMachine();
    controller->set_loginButton(view->loginButton);
    controller->set_logoutButton(view->logoutButton);
    controller->set_cancelButton(view->cancelButton);
    view->welcomeText = (use_gmail  ?"Please enter your GMail user/password":"Username=user, Password=password");
    QObject::connect(controller,SIGNAL(event_login_action(QString,QString)),model,SLOT(login(QString,QString)));
    QObject::connect(controller,SIGNAL(event_notify_loggedIn()),view,SLOT(notifyLogin()));
    QObject::connect(controller,SIGNAL(event_notify_error()),view,SLOT(notifyError()));
    QObject::connect(controller,SIGNAL(event_notify_hello(QString)),view,SLOT(notifyHello(QString)));
    QObject::connect(controller,SIGNAL(event_notify_welcome()),view,SLOT(notifyWelcome()));
    QObject::connect(controller,SIGNAL(event_notify_timeout()),view,SLOT(notifyTimeout()));
    QObject::connect(model,SIGNAL(loginComplete(bool)),controller,SIGNAL(event_login_complete(bool)));
    QObject::connect(view,SIGNAL(loginIntent()),controller,SIGNAL(event_intent_login()));
    QObject::connect(view,SIGNAL(cancelIntent()),controller,SIGNAL(event_cancel_login()));
    QObject::connect(view,SIGNAL(logoutIntent()),controller,SIGNAL(event_intent_logout()));
    QObject::connect(view,SIGNAL(contIntent()),controller,SIGNAL(event_intent_continue()));
    QObject::connect(view->usernameEdit,SIGNAL(textChanged(QString)),controller,SLOT(set_username(QString)));
    QObject::connect(view->passwordEdit,SIGNAL(textChanged(QString)),controller,SLOT(set_password(QString)));
    QObject::connect(view->timeoutSlider,SIGNAL(valueChanged(int)),controller,SLOT(set_loginTimeout(int)));
    controller->start();
    view->show();
    return a.exec();
}
#include <main.moc>
