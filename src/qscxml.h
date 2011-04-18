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

#ifndef QSCXML_H
#define QSCXML_H
#ifndef QT_NO_STATEMACHINE

#include <QStateMachine>
#include <QAbstractTransition>
#include <QVariant>
#include <QEvent>
#include <QStringList>
#include <QScriptValue>
#include <QUrl>
#include <QScriptProgram>
class QScriptEngine;
class QScxml;

class QScxmlEvent : public QEvent
{
    public:
        static QEvent::Type eventType();
        QString eventName() const;
        QStringList paramNames () const;
        QVariantList paramValues () const;
        QScriptValue content () const;
        QVariant param (const QString & name) const;
        QScxmlEvent (
                const QString & name,
                const QStringList & paramNames = QStringList(),
                const QVariantList & paramValues = QVariantList(),
                const QScriptValue & content = QScriptValue());

        struct MetaData
        {
            QUrl origin,target;
            QString originType, targetType;
            QString invokeID;
            enum Kind { Platform, Internal, External } kind;
        };

        MetaData metaData;

    private:
        QString ename;
        QStringList pnames;
        QVariantList pvals;
        QScriptValue cnt;
};


class   QScxmlTransition : public QAbstractTransition
{
    Q_OBJECT
    Q_PROPERTY(QString conditionExpression READ conditionExpression WRITE setConditionExpression)
    Q_PROPERTY(QStringList eventPrefixes READ eventPrefixes WRITE setEventPrefixes)

    public:
        QScxmlTransition (QState* state, QScxml* machine);

        QString conditionExpression () const;
        void setConditionExpression (const QString & c);
        QStringList eventPrefixes () const { return ev; }
        void setEventPrefixes (const QStringList & e) { ev = e; }

    protected:
        bool eventTest(QEvent*);
        void onTransition (QEvent*);
    private:
        QScxml* scxml;
        QStringList ev;
        QScriptProgram prog;
};

class QScxmlInvoker : public QObject
{
    Q_OBJECT
    Q_PROPERTY (QString id READ id WRITE setID)

    protected:
        QScxmlInvoker(QScxmlEvent* ievent, QStateMachine* p) : QObject(p), initEvent(ievent),cancelled(false) {}

    public:
        virtual ~QScxmlInvoker();
        QString id () const;
        void setID(const QString &);

    public Q_SLOTS:
        virtual void activate() = 0;
        virtual void cancel() { deleteLater(); }

    protected Q_SLOTS:
        void postParentEvent (const QString & event);

    protected:
        QScxml* parentStateMachine() { return (QScxml*)parent(); }
        void postParentEvent (QScxmlEvent* ev);
        QScxmlEvent* initEvent;
        bool cancelled;

    friend struct QScxmlFunctions;
};

struct QScxmlInvokerFactory
{
    virtual QScxmlInvoker* createInvoker (QScxmlEvent* event, QScxml* stateMachine) = 0;
    virtual bool isTypeSupported (const QString & type) const = 0;
    virtual void init (QScxml*) = 0;
};

template <class T>
class QScxmlAutoInvokerFactory : public QScxmlInvokerFactory
{
    QScxmlInvoker* createInvoker (QScxmlEvent* _e, QScxml* _sm) { return new T(_e,_sm); }
    bool isTypeSupported(const QString & _s) const { return T::isTypeSupported(_s); }
    void init (QScxml* sm) { T::initInvokerFactory(sm); }
};


class QScxml : public QStateMachine
{
    Q_OBJECT

    Q_PROPERTY(QUrl baseUrl READ baseUrl WRITE setBaseUrl)


    public:
        QScxml(QScriptEngine* engine, QObject* o = NULL);
        QScxml(QObject* o = NULL);
        virtual ~QScxml();
    protected:
        // overloaded to store the event for the script environment's use (_event), and to convert
        // StateFinished events to "done." named events
        virtual void beginSelectTransitions(QEvent*);
        virtual void endMicrostep(QEvent*);

    public:
        QScriptEngine* scriptEngine () const;
        void registerObject (QObject* object, const QString & name = QString(), bool recursive = false);
        void registerInvokerFactory (QScxmlInvokerFactory* f);
        void setBaseUrl (const QUrl &);
        QUrl baseUrl () const;
        static QScxml* load (const QString & filename, QObject* o = NULL);
        QMap<QString,QVariant> data() const;
        QStringList knownEventNames() const;

    public Q_SLOTS:
        void postNamedEvent(const QString &);
        void executeScript (const QString &);
        void executeScript (const QScriptProgram &);
        void setData(const QString & id, const QVariant & value);

    private Q_SLOTS:
        void registerSession();
        void unregisterSession();
        void handleStateFinished();

    Q_SIGNALS:
        void eventTriggered(const QString &);
        void dataChanged (const QString &, const QVariant &);
        void configurationChanged();

    private:
        class QScxmlPrivate* pvt;
        friend class QScxmlLoader;
        friend struct QScxmlFunctions;
        void init();
};
#endif
#endif // QSCXML_H
