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

#include <QMetaMethod>
#include <QScriptEngine>
#include <QTimer>
#include <QScriptValueIterator>
#include <QHistoryState>
#include <QFileInfo>





//class QScriptEngine;
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

class QtScxmlSnoopInternal : public QObject
    {
        Q_OBJECT

        friend class QtScxmlSnoop;

        QtScxmlSnoopInternal(QObject* o) :QObject(o) { }
        ~QtScxmlSnoopInternal() { 
            if (parent()) parent()->deleteLater(); 
            }

        signals:
            void signal (const QVariantList &);
    };
    class QtScxmlSnoop: public QObject
    {
        public:
            QtScxmlSnoopInternal* inobj;
            QtScxmlSnoop(QObject *obj, const char *aSignal):QObject(obj)
            {
#ifdef Q_CC_BOR
                const int memberOffset = QObject::staticMetaObject.methodCount();
#else
                static const int memberOffset = QObject::staticMetaObject.methodCount();
#endif
                Q_ASSERT(obj);
                Q_ASSERT(aSignal);

                if (aSignal[0] - '0' != QSIGNAL_CODE) {
                    qWarning("QtScxmlSnoop: Not a valid signal, use the SIGNAL macro");
                    return;
                }

                QByteArray ba = QMetaObject::normalizedSignature(aSignal + 1);
                const QMetaObject *mo = obj->metaObject();
                int sigIndex = mo->indexOfMethod(ba.constData());
                if (sigIndex < 0) {
                    qWarning("QtScxmlSnoop: No such signal: '%s'", ba.constData());
                    return;
                }

                if (!QMetaObject::connect(obj, sigIndex, this, memberOffset,
                     Qt::QueuedConnection, 0)) {
                         qWarning("QtScxmlSnoop: QMetaObject::connect returned false. Unable to connect.");
                         return;
                     }
                     sig = ba;
                     QMetaMethod member = mo->method(sigIndex);
                     QList<QByteArray> params = member.parameterTypes();
                     for (int i = 0; i < params.count(); ++i) {
                         int tp = QMetaType::type(params.at(i).constData());
                         if (tp == QMetaType::Void)
                             qWarning("Don't know how to handle '%s', use qRegisterMetaType to register it.",
                                      params.at(i).constData());
                         args << tp;
                     }
                inobj = new QtScxmlSnoopInternal (this);
            }

            inline bool isValid() const { return !sig.isEmpty(); }
            inline QByteArray signal() const { return sig; }


            int qt_metacall(QMetaObject::Call call, int id, void **a)
            {
                id = QObject::qt_metacall(call, id, a);
                if (id < 0)
                    return id;

                if (call == QMetaObject::InvokeMetaMethod) {
                    if (id == 0) {
                        QVariantList list;
                        for (int i = 0; i < args.count(); ++i) {
                            QMetaType::Type type = static_cast<QMetaType::Type>(args.at(i));
                            QVariant v(type, a[i + 1]);
                            list << v;

                        }
                        emit inobj->signal (list);
                    }
                    --id;
                }
                return id;
            }



    // the full, normalized signal name
            QByteArray sig;
    // holds the QMetaType types for the argument list of the signal
            QList<int> args;

    };


class QScxmlPrivate
{
    public:

        void initScriptEngine(QScxml* thiz);

        QScriptValue dataObj;


        QScriptEngine* scriptEng;
        QList<QScxmlInvokerFactory*> invokerFactories;
        QUrl burl;
        QString sessionID;
        QString startScript;

        QSet<QString> knownEvents;

        static QHash<QString,QScxml*> sessions;
};

class QScxmlTimer : public QObject
{
    Q_OBJECT
    public:
        QScxmlTimer(QScriptEngine* engine, const QScriptValue & scr, int delay) : QObject(engine)
        {
            QTimer::singleShot(delay,this,SLOT(exec()));
            if (scr.isString()) {
                script = engine->evaluate(QString("function(){%1}").arg(scr.toString()));
            } else
                script = scr;
        }
    protected Q_SLOTS:
        void exec()
        {

            if (script.isFunction())
                script.call();
            deleteLater();
        }
        
    private:
        QScriptValue script;
    
};

class QScxmlSignalReceiver : public QObject
{
    Q_OBJECT
    QScxml* scxml;
    QString eventName;
    public:
        QScxmlSignalReceiver(QScxml* s, QString ename) : QObject(s),scxml(s),eventName(ename)
        {
        }
    public Q_SLOTS:
        void  receiveSignal(const QVariantList & pvals)
        {
            QStringList pnames;
            for (int i=0; i < pvals.count(); ++i) {
                pnames << QString::number(i);
            }
            QScxmlEvent* ev = new QScxmlEvent(eventName,pnames,pvals,QScriptValue());
            ev->metaData.kind = QScxmlEvent::MetaData::Platform;
            scxml->postEvent(ev);
        }
};

class QScxmlDefaultInvoker : public QScxmlInvoker
{
    Q_OBJECT
    

    public:
    QScxmlDefaultInvoker(QScxmlEvent* ievent, QScxml* p) : QScxmlInvoker(ievent,p),cancelled(false),childSm(0)
    {
        childSm = QScxml::load (ievent->metaData.origin.resolved(ievent->metaData.target).toLocalFile(),this);
        if (childSm == NULL) {
            postParentEvent("error.targetunavailable");
        } else {
           connect(childSm,SIGNAL(finished()),this,SLOT(deleteLater()));

        }
    }
    
    

    static void initInvokerFactory(QScxml*) {}

    static bool isTypeSupported(const QString & t) { return t.isEmpty() || t.toLower() == "scxml"; }

    public Q_SLOTS:
    void activate ()
    {
        if (childSm)
            childSm->start();
    }

    void cancel ()
    {
        cancelled = true;
        if (childSm)
            childSm->stop();

    }
    
    private:
        bool cancelled;
        QScxml* childSm;
};
class QScxmlBindingInvoker : public QScxmlInvoker
{
    Q_OBJECT
    QScriptValue content;
    QScriptValue stored;

    public:
    QScxmlBindingInvoker(QScxmlEvent* ievent, QScxml* p) : QScxmlInvoker(ievent,p)
    {
    }

    static void initInvokerFactory(QScxml*) {}

    static bool isTypeSupported(const QString & t) { return t.toLower() == "q-bindings"; }

    public Q_SLOTS:
    void activate ()
    {
        QScriptEngine* engine = ((QScxml*)parent())->scriptEngine();
        QScriptValue content = initEvent->content();
        if (content.isArray()) {
            stored = content.engine()->newArray(content.property("length").toInt32());

            QScriptValueIterator it (content);
            for (int i=0; it.hasNext(); ++i) {
                it.next();
                if (it.value().isArray()) {
                    QScriptValue object = it.value().property(0);
                    QString property = it.value().property(1).toString();
                    QScriptValue val = it.value().property(2);
                    QScriptValue arr = engine->newArray(3);
                    arr.setProperty("0",it.value().property(0));
                    arr.setProperty("1",it.value().property(1));
                    if (object.isQObject()) {
                        QObject* o = object.toQObject();
                        arr.setProperty("2",engine->newVariant(o->property(property.toAscii().constData())));
                        o->setProperty(property.toAscii().constData(),val.toVariant());
                    } else if (object.isObject()) {
                        arr.setProperty("2",object.property(property));
                        object.setProperty(property,val);
                    }
                    stored.setProperty(i,arr);
                }
            }
        }
    }

    void cancel ()
    {
        if (stored.isArray()) {
            QScriptValueIterator it (stored);
            while (it.hasNext()) {
                it.next();
                if (it.value().isArray()) {
                    QScriptValue object = it.value().property(0);
                    QString property = it.value().property(1).toString();
                    QScriptValue val = it.value().property(2);
                    if (object.isQObject()) {
                        QObject* o = object.toQObject();
                        o->setProperty(property.toAscii().constData(),val.toVariant());
                    } else if (object.isObject()) {
                        object.setProperty(property,val);
                    }
                }
            }
        }
    }
};

class QScxmlScriptExec : public QObject
{
    Q_OBJECT
    QScriptProgram prog;
    QScxml* scxml;
    public:
        QScxmlScriptExec(const QString & scr, QScxml* scx) :
                prog(QScriptProgram(scr,scx->baseUrl().toLocalFile())),scxml(scx)
        {
        }
    public Q_SLOTS:
        void exec()
        {
            scxml->executeScript(prog);
        }
};

struct ScTransitionInfo
{

    QScxmlTransition* transition;
    QStringList targets;
    QString script;
    ScTransitionInfo() : transition(NULL) {}
};


struct ScStateInfo
{
    QString initial;
};

struct ScHistoryInfo
{
    QHistoryState* hstate;
    QString defaultStateID;
};

struct ScExecContext
{
    QScxml* sm;
    QString script;
    enum {None, StateEntry,StateExit,Transition } type;
    QScxmlTransition* trans;
    QAbstractState* state;
    ScExecContext() : sm(NULL),type(None),trans(NULL),state(NULL)
    {
    }

    void applyScript()
    {
        if (!script.isEmpty()) {
            QScxmlScriptExec* exec = new QScxmlScriptExec(script,sm);
            switch(type) {
                case StateEntry:
                    QObject::connect(state,SIGNAL(entered()),exec,SLOT(exec()));
                break;
                case StateExit:
                    QObject::connect(state,SIGNAL(exited()),exec,SLOT(exec()));
                break;
                case Transition:
                    QObject::connect(trans,SIGNAL(triggered()),exec,SLOT(exec()));
                break;
                default:
                delete exec;
                break;
            }
        }
    }
};


class QScxmlLoader
{
    public:
    QScxml* stateMachine;

    QList<ScTransitionInfo> transitions;
    QHash<QState*,ScStateInfo> stateInfo;
    QList<ScHistoryInfo> historyInfo;
    QHash<QString,QAbstractState*> stateByID;
    QSet<QString> signalEvents;
     QSet<QState*> statesWithFinal;
   void loadState (QState* state, QIODevice* dev, const QString & stateID,const QString & filename);
    QScxml* load (QIODevice* device, QObject* obj = NULL, const QString & filename = "");

    QScriptValue evaluateFile (const QString & fn)
    {
        QFile f (fn);
        f.open(QIODevice::ReadOnly);
        return stateMachine->scriptEngine()->evaluate(QString::fromUtf8(f.readAll()),fn);
    }
};

#endif
#endif // QSCXML_H
