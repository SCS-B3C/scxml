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

/*!
  \class QScxml

  \brief The QScxml class provides a way to use scripting with the Qt State Machine Framework.

  Though can be used alone, QScxml is mainly a runtime helper to using the
  state-machine framework with SCXML files.


  \sa QStateMachine
*/
#ifndef QT_NO_STATEMACHINE

#include "qscxml.h"
#include <QScriptEngine>
#include <QScriptValueIterator>
#include <QDebug>
#include <QTimer>
#include <QSignalMapper>
#include <QUuid>
#include <QHash>
#include <QXmlStreamReader>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QStack>
#include <QHistoryState>
#include <QFinalState>
#include <QState>
#include <QMetaMethod>
#include <QScriptProgram>



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

QObject* q_snoopConnect (
                                      QObject* sender,
                                      const char* signal,
                                      QObject* receiver,
                                      const char* method
                                     )
{
    QtScxmlSnoop* o = new QtScxmlSnoop(sender,signal);
    if (o->isValid()) {
        QObject::connect (o->inobj, SIGNAL(signal(QVariantList)),receiver,method);
        QObject::connect (receiver, SIGNAL(destroyed()), o, SLOT(deleteLater()));
    }
    return o;
}

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
QHash<QString,QScxml*> QScxmlPrivate::sessions;

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

static QScriptValue _q_deepCopy(const QScriptValue & val)
{
    if (val.isObject() || val.isArray()) {
        QScriptValue v = val.isArray() ? val.engine()->newArray() : val.engine()->newObject();
        v.setData(val.data());
        QScriptValueIterator it (val);
        while (it.hasNext()) {
            it.next();
            v.setProperty(it.name(), _q_deepCopy(it.value()));
        }
        return v;
    } else
        return val;
}
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
struct QScxmlFunctions
{
static QScriptValue connectSignalToEvent(QScriptContext* context, QScriptEngine*)
{
    QScxml* scxml = qobject_cast<QScxml*>(context->thisObject().toQObject());
    if (scxml) {
        QObject* obj = context->argument(0).toQObject();
        QString sig = ('0'+QSIGNAL_CODE)+context->argument(1).toString();
        QString ename = context->argument(2).toString();
        
        if (obj)
        {
            q_snoopConnect(obj,sig.toAscii().constData(),new QScxmlSignalReceiver(scxml,ename),SLOT(receiveSignal(QVariantList)));
        }
    }
    return QScriptValue();
}

static QScriptValue cssTime(QScriptContext *context, QScriptEngine *engine)
{
    QString str;
    if (context->argumentCount() > 0)
        str = context->argument(0).toString();
    if (str == "") {
        return qScriptValueFromValue<int>(engine,0);
    }
    else if (str.endsWith("ms")) {
        return qScriptValueFromValue<int>(engine,(str.left(str.length()-2).toInt()));
    }
    else if (str.endsWith("s")) {
        return qScriptValueFromValue<int>(engine,(str.left(str.length()-1).toInt())*1000);
    }
    else {
        return qScriptValueFromValue<int>(engine, (str.toInt()));
    }
}
static QScriptValue setTimeout(QScriptContext *context, QScriptEngine *engine)
{
        if (context->argumentCount() < 2)
                return QScriptValue();
        int timeout = context->argument(1).toInt32();
        QScxmlTimer* tmr = new QScxmlTimer(engine,context->argument(0),timeout);
        return engine->newQObject(tmr);
}
static QScriptValue script_print(QScriptContext *context, QScriptEngine *)
{
    if (context->argumentCount() > 0)
        qDebug() << context->argument(0).toString();
    return QScriptValue();
}
static QScriptValue clearTimeout(QScriptContext *context, QScriptEngine *)
{
        if (context->argumentCount() > 0) {
        QObject* obj = context->argument(0).toQObject();
        obj->deleteLater();
    }
        return QScriptValue();
}

static QScriptValue deepCopy(QScriptContext *context, QScriptEngine *)
{
    if (context->argumentCount() == 0)
        return QScriptValue();
    else
        return _q_deepCopy(context->argument(0));
}

static QScriptValue postEvent(QScriptContext *context, QScriptEngine *)
{
    QScxml* scxml = qobject_cast<QScxml*>(context->thisObject().toQObject());
    if (scxml) {
        QString eventName,target,type;
        QStringList pnames;
        QVariantList pvals;
        QScriptValue cnt;
        if (context->argumentCount() > 0)
            eventName = context->argument(0).toString();
        if (context->argumentCount() > 1)
            target = context->argument(1).toString();
        if (context->argumentCount() > 2)
            type = context->argument(2).toString();

        if (!eventName.isEmpty() || !target.isEmpty()) {
            if (context->argumentCount() > 3)
                qScriptValueToSequence<QStringList>(context->argument(3),pnames);
            if (context->argumentCount() > 4) {
                QScriptValueIterator it (context->argument(4));
                while (it.hasNext()) {
                    it.next();
                    pvals.append(it.value().toVariant());
                }
            } if (context->argumentCount() > 5)
                cnt = context->argument(5);
            QScxmlEvent* ev = new QScxmlEvent(eventName,pnames,pvals,cnt);
            if (type == "scxml" || type == "") {
                bool ok = true;
                if (target == "_internal") {
                    ev->metaData.kind = QScxmlEvent::MetaData::Internal;
                    scxml->postEvent(ev,QStateMachine::HighPriority);
                } else if (target == "scxml" || target == "") {
                    ev->metaData.kind = QScxmlEvent::MetaData::External;
                    scxml->postEvent(ev);
                } else if (target == "_parent") {
                    QScxmlInvoker* p = qobject_cast<QScxmlInvoker*>(scxml->parent());
                    if (p)
                        p->postParentEvent(ev);
                    else
                        ok = false;
                } else {
                    QScxml* session = QScxmlPrivate::sessions[target];
                    if (session) {
                        session->postEvent(ev);
                    } else
                        ok = false;
                }
                if (!ok)
                    scxml->postNamedEvent("error.targetunavailable");

            } else {
                scxml->postNamedEvent("error.send.typeinvalid");
            }
        }
    }
    return QScriptValue();
}

// scxml.invoke (type, target, paramNames, paramValues, content)
static QScriptValue invoke(QScriptContext *context, QScriptEngine *engine)
{
    QScxml* scxml = qobject_cast<QScxml*>(context->thisObject().toQObject());
    if (scxml) {
        QString type,target;
        QStringList pnames;
        QVariantList pvals;
        QScriptValue cnt;
        if (context->argumentCount() > 0)
            type = context->argument(0).toString();
        if (type.isEmpty())
            type = "scxml";
        if (context->argumentCount() > 1)
            target = context->argument(1).toString();
        if (context->argumentCount() > 2)
            qScriptValueToSequence<QStringList>(context->argument(2),pnames);
        if (context->argumentCount() > 3) {
                QScriptValueIterator it (context->argument(3));
                while (it.hasNext()) {
                    it.next();
                    pvals.append(it.value().toVariant());
                } 
        } if (context->argumentCount() > 4)
                cnt = context->argument(4);

        QScxmlInvokerFactory* invf = NULL;
        for (int i=0; i < scxml->pvt->invokerFactories.count() && invf == NULL; ++i)
            if (scxml->pvt->invokerFactories[i]->isTypeSupported(type))
                invf = scxml->pvt->invokerFactories[i];
        if (invf) {
            QScxmlEvent* ev = new QScxmlEvent("",pnames,pvals,cnt);
            ev->metaData.origin = scxml->baseUrl();
            ev->metaData.target = target;
            ev->metaData.targetType = type;
            ev->metaData.originType = "scxml";
            ev->metaData.kind = QScxmlEvent::MetaData::External;
            QScxmlInvoker* inv = invf->createInvoker(ev,scxml);
            if (inv)
                inv->activate();
            return engine->newQObject(inv);
        } else {
            scxml->postNamedEvent("error.invalidtargettype");
        }

    }
    return QScriptValue();
}

static QScriptValue dataAccess(QScriptContext *context, QScriptEngine *)
{
    if (context->argumentCount() == 0) {
        // getter
        return context->callee().property("value");
    } else if (context->argumentCount() == 1) {
        // setter
        QScriptValue val = context->argument(0);
        context->callee().setProperty("value",val);
        QScxml* scxml = qobject_cast<QScxml*>(context->callee().property("scxml").toQObject());
        if (scxml) {
            scxml->dataChanged(context->callee().property("key").toString(),val.toVariant());
        }
        return val;
    } else
        return QScriptValue();
}

static QScriptValue isInState(QScriptContext *context, QScriptEngine *engine)
{
    QScxml* scxml = qobject_cast<QScxml*>(context->thisObject().toQObject());
    if (scxml) {
        if (context->argumentCount() > 0) {
            QString name = context->argument(0).toString();
            if (!name.isEmpty()) {
                QSet<QAbstractState*> cfg = scxml->configuration();
                foreach (QAbstractState* st, cfg) {
                    if (st->objectName() == name)
                        return qScriptValueFromValue<bool>(engine,true);
                }
            }
        }
    }
    return qScriptValueFromValue<bool>(engine,false);

}



};

void QScxmlPrivate::initScriptEngine(QScxml* thiz)
{
    QScriptValue glob = scriptEng->globalObject();
    QScriptValue scxmlObj = scriptEng->newQObject(thiz);
    glob.setProperty("In",scriptEng->newFunction(QScxmlFunctions::isInState));
    scxmlObj.setProperty("print",scriptEng->newFunction(QScxmlFunctions::script_print));
    scxmlObj.setProperty("postEvent",scriptEng->newFunction(QScxmlFunctions::postEvent));
    scxmlObj.setProperty("invoke",scriptEng->newFunction(QScxmlFunctions::invoke));
    scxmlObj.setProperty("cssTime",scriptEng->newFunction(QScxmlFunctions::cssTime));
    scxmlObj.setProperty("clone",scriptEng->newFunction(QScxmlFunctions::deepCopy));
    scxmlObj.setProperty("setTimeout",scriptEng->newFunction(QScxmlFunctions::setTimeout));
    scxmlObj.setProperty("clearTimeout",scriptEng->newFunction(QScxmlFunctions::clearTimeout));
    scxmlObj.setProperty("connectSignalToEvent",scriptEng->newFunction(QScxmlFunctions::connectSignalToEvent));
    QScriptValue dmObj = scriptEng->newObject();
    dataObj = scriptEng->newObject();
    dataObj.setProperty("_values",scriptEng->newObject());
    glob.setProperty("_data",dataObj);
    glob.setProperty("_global",scriptEng->globalObject());
    glob.setProperty("scxml",scxmlObj);
}

/*!
  \class QScxmlEvent
  \brief The QScxmlEvent class stands for a general named event with a list of parameter names and parameter values.

  Encapsulates an event that conforms to the SCXML definition of events.


*/
/*! \enum QScxmlEvent::MetaData::Kind

    This enum specifies the kind (or context) of the event.
    \value Platform     An event coming from the     itself, such as a script error.
    \value Internal     An event sent with a <raise> or <send target="_internal">.
    \value External     An event sent from an invoker, directly from C++, or from a <send target="scxml"> element.
*/

/*!
  Returns the name of the event.
  */
  QString QScxmlEvent::eventName() const
{
    return ename;
}
  /*!
    Return a list containing the parameter names.
    */
QStringList QScxmlEvent::paramNames () const
{
    return pnames;
}
  /*!
    Return a list containing the parameter values.
    */
QVariantList QScxmlEvent::paramValues () const
{
    return pvals;
}
  /*!
    Return a QtScript object that can be passed as an additional parameter.
    */
QScriptValue QScxmlEvent::content () const
{
    return cnt;
}
  /*!
    Returns the parameter value equivalent to parameter \a name.
    */
QVariant QScxmlEvent::param (const QString & name) const
{
    int idx = pnames.indexOf(name);
    if (idx >= 0)
        return pvals[idx];
    else
        return QVariant();
}
/*!
  Creates a QScxmlEvent named \a name, with parameter names \a paramNames, parameter values \a paramValues, and
  a QtScript object \a content as an additional parameter.
*/
QScxmlEvent::QScxmlEvent(
        const QString & name,
        const QStringList & paramNames,
        const QVariantList & paramValues,
        const QScriptValue & content)

        : QEvent(QScxmlEvent::eventType()),ename(name),pnames(paramNames),pvals(paramValues),cnt(content)
{
    metaData.kind = MetaData::Internal;
}

/*! \class QScxmlTransition
  \brief The QScxmlTransition class stands for a transition that responds to QScxmlEvent, and can be made conditional with a \l conditionExpression.
  Equivalent to the SCXML transition tag.

  */
  

/*! \property QScxmlTransition::eventPrefix
  The event prefix to be used when testing if the transition needs to be invoked.
  Uses SCXML prefix matching. Use * to handle any event.
  */
/*! \property QScxmlTransition::conditionExpression
  A QtScript expression that's evaluated to test whether the transition needs to be invoked.
  */

/*!
  Creates a new QScxmlTransition from \a state, that uses \a machine to evaluate the conditions.
  */
QScxmlTransition::QScxmlTransition (QState* state,QScxml* machine)
    : QAbstractTransition(state),scxml(machine)
{
}

void QScxmlTransition::setConditionExpression(const QString & c)
{
    prog = QScriptProgram(c,scxml->baseUrl().toLocalFile());
}

QString QScxmlTransition::conditionExpression() const
{
    return prog.sourceCode();
}
/*!
   \reimp
*/
void QScxmlTransition::onTransition(QEvent*)
{
}
/*!
  \internal
  */
bool QScxmlTransition::eventTest(QEvent *e)
{
    QScriptEngine* engine = scxml->scriptEngine();
    QString event;

    if (e) {
        if (e->type() == QScxmlEvent::eventType()) {
            event = ((QScxmlEvent*)e)->eventName();
        }
        bool found = false;
        for (int i=0; i < ev.size() && !found; ++i) {
            QString prefix = ev[i];
            found = prefix=="*" || prefix==event || event.startsWith(prefix)
                     || (prefix.endsWith(".*") && event.startsWith(prefix.left(prefix.indexOf(".*"))))
                     ;
        }
        if (!found  )
            return false;
    }


    if (!conditionExpression().isEmpty()) {
        QScriptValue v = engine->evaluate(prog);
        if (engine->hasUncaughtException()) {

            QScxmlEvent* e = new QScxmlEvent("error.illegalcond",
                                         QStringList()<< "error" << "expr" << "line" << "backtrace",
                                         QVariantList()
                                            << QVariant(engine->uncaughtException().toString())
                                            << QVariant(conditionExpression())
                                            << QVariant(engine->uncaughtExceptionLineNumber())
                                            << QVariant(engine->uncaughtExceptionBacktrace()));
            qDebug() << engine->uncaughtException().toString() << prog.sourceCode();
            e->metaData.kind = QScxmlEvent::MetaData::Platform;
            scxml->postEvent(e);
            engine->clearExceptions();
            return false;
        }
        return v.toBoolean();
    }

    return true;
}

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

  
/*!
\fn QScxmlInvoker::~QScxmlInvoker()
*/


/*!
\fn QScxml::eventTriggered(const QString & name)

This signal is emitted when external event \a name is handled in the state machine. 
*/

void QScxml::init()
{
    static QScxmlAutoInvokerFactory<QScxmlDefaultInvoker> _s_defaultInvokerFactory;
    static QScxmlAutoInvokerFactory<QScxmlBindingInvoker> _s_bindingInvokerFactory;
    registerInvokerFactory(&_s_defaultInvokerFactory);
    registerInvokerFactory(&_s_bindingInvokerFactory);
    connect(this,SIGNAL(started()),this,SLOT(registerSession()));
    connect(this,SIGNAL(stopped()),this,SLOT(unregisterSession()));
    pvt->initScriptEngine(this);
}
/*!
  Creates a new QScxml object, with parent \a parent.
  */

QScxml::QScxml(QObject* parent)
    : QStateMachine(parent)
{
    pvt = new QScxmlPrivate;
    pvt->scriptEng = new QScriptEngine(this);
    init();

}
/*!
  Creates a new QScxml object, with parent \a parent. The state machine will operate on script-engine \a eng.
  */

QScxml::QScxml(QScriptEngine* eng, QObject* parent)
    : QStateMachine(parent)
{
    pvt = new QScxmlPrivate;
    pvt->scriptEng = eng;
    init();
}

/*! \internal */
void QScxml::beginSelectTransitions(QEvent* ev)
{
    QScriptValue eventObj = pvt->scriptEng->newObject();
    if (ev) {
        if (ev->type() == QScxmlEvent::eventType()) {
            QScxmlEvent* se = (QScxmlEvent*)ev;
            eventObj.setProperty("name",qScriptValueFromValue<QString>(pvt->scriptEng,se->eventName()));
            eventObj.setProperty("target",QScriptValue(se->metaData.target.toString()));
            eventObj.setProperty("targettype",qScriptValueFromValue<QString>(pvt->scriptEng,se->metaData.targetType));
            eventObj.setProperty("invokeid",qScriptValueFromValue<QString>(pvt->scriptEng,se->metaData.invokeID));
            eventObj.setProperty("origin",QScriptValue(se->metaData.origin.toString()));
            eventObj.setProperty("originType",qScriptValueFromValue<QString>(pvt->scriptEng,se->metaData.originType));
            switch (se->metaData.kind) {
                case QScxmlEvent::MetaData::Internal:
                    eventObj.setProperty("kind",qScriptValueFromValue<QString>(pvt->scriptEng, "internal"));
                break;
                case QScxmlEvent::MetaData::External:
                    eventObj.setProperty("kind",qScriptValueFromValue<QString>(pvt->scriptEng, "external"));
                break;
                case QScxmlEvent::MetaData::Platform:
                    eventObj.setProperty("kind",qScriptValueFromValue<QString>(pvt->scriptEng, "platform"));
                default:
                break;

            }

            QScriptValue dataObj = pvt->scriptEng->newObject();
            int i=0;
            foreach (QString s, se->paramNames()) {
                QScriptValue v = qScriptValueFromValue(pvt->scriptEng, se->paramValues()[i]);
                dataObj.setProperty(QString::number(i),v);
                dataObj.setProperty(s,v);
                ++i;
            }
            eventObj.setProperty("data",dataObj);
            emit eventTriggered(se->eventName());
        }
    }
    scriptEngine()->globalObject().setProperty("_event",eventObj);

    QHash<QString,QAbstractState*> curTargets;
}

static QString _q_configToString (QAbstractState* from,int level, const QSet<QAbstractState*> & config)
{
    QString str;
    if (from) {
        if (level >= 0) {
            for (int i=0; i < level; ++i)
                str += "\t";

            QState* p = qobject_cast<QState*>(from->parent());
            char c = '$';
            if (qobject_cast<QHistoryState*>(from))
                c = '^';
            else if (qobject_cast<QFinalState*>(from))
                c = '~';
            else if (p) {
             if (p->childMode() == QState::ParallelStates)
                c = '{';
            }
            str += QString("%1%2 %3\n").arg(config.contains(from)?">":" ").arg(c).arg(from->objectName());
        }
        QObjectList ch = from->children();
        foreach (QObject* o, ch)
            str += _q_configToString(qobject_cast<QAbstractState*>(o),level+1,config);
    }
    
    return str;
}

/*! \internal */


void QScxml::endMicrostep(QEvent*)
{
    scriptEngine()->globalObject().setProperty("_event",QScriptValue());

    emit configurationChanged();
}

/*! Returns the script engine attached to the state-machine. */
QScriptEngine* QScxml::scriptEngine () const
{
    return pvt->scriptEng;
}

/*!
    Registers object \a o to the script engine attached to the state machine.
    The object can be accessible from global variable \a name. If \a name is not provided,
    the object's name is used. If \a recursive is true, all the object's decendants are registered
    as global objects, with their respective object names as variable names.
*/
void QScxml::registerObject (QObject* o, const QString & name, bool recursive)
{
    QString n(name);
    if (n.isEmpty())
        n = o->objectName();
    if (!n.isEmpty())
        pvt->scriptEng->globalObject().setProperty(n,pvt->scriptEng->newQObject(o));
    if (recursive) {
        QObjectList ol = o->findChildren<QObject*>();
        foreach (QObject* oo, ol) {
            if (!oo->objectName().isEmpty())
                registerObject(oo);
        }
    }
}

/*!
  Posts a QScxmlEvent named \a event, with no payload.
  \sa QScxmlEvent
  */
void QScxml::postNamedEvent(const QString & event)
{
    QScxmlEvent* e = new QScxmlEvent(event);
    e->metaData.kind = QScxmlEvent::MetaData::External;
    postEvent(e);
}

void QScxml::setData(const QString & id, const QVariant & val)
{
    QScriptValue accessor = pvt->dataObj.property(id);
    if (accessor.isFunction()) {
        accessor.setProperty("value",scriptEngine()->newVariant(val));
    }
}

/*!
    Executes script \a s in the attached script engine.
    If the script fails, a "error.illegalvalue" event is posted to the state machine.
*/

void QScxml::executeScript (const QScriptProgram & s)
{
//    qDebug() << "Executing\n--------------------------\n"<<s.sourceCode();
        pvt->scriptEng->evaluate (s);
        if (pvt->scriptEng->hasUncaughtException()) {
            QScxmlEvent* e = new QScxmlEvent("error.illegalvalue",
                                         QStringList()<< "error" << "expr" << "line" << "backtrace",
                                         QVariantList()
                                            << QVariant(pvt->scriptEng->uncaughtException().toString())
                                            << QVariant(s.sourceCode())
                                            << QVariant(pvt->scriptEng->uncaughtExceptionLineNumber())
                                            << QVariant(pvt->scriptEng->uncaughtExceptionBacktrace()));
            e->metaData.kind = QScxmlEvent::MetaData::Platform;
            qDebug() << pvt->scriptEng->uncaughtException().toString() << s.sourceCode();
            postEvent(e);
            pvt->scriptEng->clearExceptions();
        }
//    qDebug() <<"\n--------------------\n";
}
void QScxml::executeScript (const QString & s)
{
    executeScript(QScriptProgram(s,baseUrl().toLocalFile()));
}

/*!
  Enabled invoker factory \a f to be called from <invoke /> tags.
  */

void QScxml::registerInvokerFactory (QScxmlInvokerFactory* f)
{
    pvt->invokerFactories << f;
    f->init(this);
}

/*! \class QScxmlInvoker
    \brief The QScxmlInvoker class an invoker, which the state-machine context can activate or cancel
        with an <invoke> tag.


    An invoker is a object that represents an external component that the state machine
    can activate when the encompassing state is entered, or cancel when the encompassing
    state is exited from.
  */

/*! \fn QScxmlInvoker::QScxmlInvoker(QScxmlEvent* ievent, QStateMachine* parent)
    When reimplementing the constructor, always use the two parameters (\a ievent and \a parent),
    as they're called from QScxmlInvokerFactory.
*/

/*! \fn  QScxmlInvoker::activate() 
    This function is called when the encompassing state is entered.
    The call to this function from the state-machine context is asynchronous, to make sure
    that the state is not exited during the same step in which it's entered.

*/

/*! \fn QScxmlInvoker::cancel()
    Reimplement this function to allow for asynchronous cancellation of the invoker.
    It's the invoker's responsibility to delete itself after this function has been called.
    The default implementation deletes the invoker.
*/

/*! \fn QScxml* QScxmlInvoker::parentStateMachine()
  Returns the state machine encompassing the invoker.
  */

/*!
  Posts an event \a e to the state machine encompassing the invoker.
  */
void QScxmlInvoker::postParentEvent (QScxmlEvent* e)
{
    e->metaData.origin = initEvent->metaData.target;
    e->metaData.target = initEvent->metaData.origin;
    e->metaData.originType = initEvent->metaData.targetType;
    e->metaData.targetType = initEvent->metaData.originType;
    e->metaData.kind = QScxmlEvent::MetaData::External;
    e->metaData.invokeID = initEvent->metaData.invokeID;
    parentStateMachine()->postEvent(e);
}
/*! \overload
  Posts a QScxmlEvent named \a e to the encompassing state machine.
  */
void QScxmlInvoker::postParentEvent(const QString & e)
{
    QScxmlEvent* ev = new QScxmlEvent(e);
    ev->metaData.kind = QScxmlEvent::MetaData::External;
    postParentEvent(ev);
}
/*! \internal */
QScxml::~QScxml()
{
    delete pvt;
}
/*! 
returns the id for this invoker 
*/
QString QScxmlInvoker::id () const
{
    return initEvent->metaData.invokeID;
}
void QScxmlInvoker::setID(const QString & id)
{
    initEvent->metaData.invokeID = id;
}

QScxmlInvoker::~QScxmlInvoker()
{
    if (cancelled)
        postParentEvent("CancelResponse");
    else
        postParentEvent(QString("done.invoke.%1").arg(initEvent->metaData.invokeID));
}
/*!
    \property QScxml::baseUrl
    The url used to resolve scripts and invoke urls.
*/
QUrl QScxml::baseUrl() const
{
    return pvt->burl;
}

void QScxml::setBaseUrl(const QUrl & u)
{
    pvt->burl = u;
}

QStringList QScxml::knownEventNames() const
{
    return pvt->knownEvents.toList();
}

void QScxml::registerSession()
{
    pvt->sessionID = QUuid::createUuid().toString();
    pvt->sessions[pvt->sessionID] = this;
    pvt->scriptEng->globalObject().setProperty("_sessionid",qScriptValueFromValue<QString>(scriptEngine(), pvt->sessionID));
    executeScript(pvt->startScript);
}

void QScxml::unregisterSession()
{
    pvt->scriptEng->globalObject().setProperty("_sessionid",QScriptValue());
    pvt->sessions.remove(pvt->sessionID);
}

/*!
    Returns a statically-generated event type to be used by SCXML events.
*/
QEvent::Type QScxmlEvent::eventType()
{
    static QEvent::Type _t = (QEvent::Type)QEvent::registerEventType(QEvent::User+200);
    return _t;
}
const char SCXML_NAMESPACE [] = "http://www.w3.org/2005/07/scxml";



struct ScTransitionInfo
{

    QScxmlTransition* transition;
    QStringList targets;
    QString script;
    ScTransitionInfo() : transition(NULL) {}
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



void QScxmlLoader::loadState (
        QState* stateParam,
        QIODevice *dev,
        const QString & stateID,
        const QString & filename)
{
    QXmlStreamReader r (dev);
    QState* curState = NULL;
    ScExecContext curExecContext;
    curExecContext.sm = stateMachine;
    QState* topLevelState = NULL;
    QHistoryState* curHistoryState = NULL;
    QString initialID = "";
    QString idLocation, target, targetType, eventName, delay, content;
    QStringList paramNames, paramVals;
    QScxmlTransition* curTransition = NULL;
    bool inRoot = true;
    while (!r.atEnd()) {
        r.readNext();
        if (r.hasError()) {
            qDebug() << QString("SCXML read error at line %1, column %2: %3").arg(r.lineNumber()).arg(r. columnNumber()).arg(r.errorString());
            return;
        }
        if (r.namespaceUri() == SCXML_NAMESPACE || r.namespaceUri() == "") {
            if (r.isStartElement()) {
                if (r.name().toString().compare("scxml",Qt::CaseInsensitive) == 0) {
                    if (stateID == "") {
                        topLevelState = curState = stateParam;
                        stateInfo[curState].initial = r.attributes().value("initial").toString();
                        if (curState == stateMachine) {
                            stateMachine->scriptEngine()->globalObject().setProperty("_name",qScriptValueFromValue<QString>(stateMachine->scriptEngine(),r.attributes().value("name").toString()));
                        }

                    }
                } else if (r.name().toString().compare("state",Qt::CaseInsensitive) == 0 || r.name().toString().compare("parallel",Qt::CaseInsensitive) == 0) {
                    inRoot = false;
                    QString id = r.attributes().value("id").toString();
                    QState* newState = NULL;
                    if (curState) {
                        newState= new QState(r.name().toString().compare("parallel",Qt::CaseInsensitive) == 0 ? QState::ParallelStates : QState::ExclusiveStates,
                                                        curState);
                    } else if (id == stateID) {
                        topLevelState = newState = stateParam;

                    }
                    if (newState) {
                        stateInfo[newState].initial = r.attributes().value("initial").toString();
                        newState->setObjectName(id);
                        if (!id.isEmpty() && stateInfo[curState].initial == id) {

                            if (curState == stateMachine)
                                stateMachine->setInitialState(newState);
                            else
                                curState->setInitialState(newState);
                        }
                        QString src = r.attributes().value("src").toString();
                        if (!src.isEmpty()) {
                            int refidx = src.indexOf('#');
                            QString srcfile, refid;
                            if (refidx > 0) {
                                srcfile = src.left(refidx);
                                refid = src.mid(refidx+1);
                            } else
                                srcfile = src;
                            srcfile = QDir::cleanPath( QFileInfo(filename).dir().absoluteFilePath(srcfile));
                            QFile newFile (srcfile);
                            if (newFile.exists()) {
                                newFile.open(QIODevice::ReadOnly);
                                loadState(newState,&newFile,refid,srcfile);
                            }
                        }
                        initialID = r.attributes().value("initial").toString();
                        stateByID[id] = newState;
                        curState = newState;
                        curExecContext.state = newState;
                    }

                } else if (r.name().toString().compare("initial",Qt::CaseInsensitive) == 0) {
                    if (curState && stateInfo[curState].initial == "") {
                        QState* newState = new QState(curState);
                        curState->setInitialState(newState);
                    }
                } else if (r.name().toString().compare("history",Qt::CaseInsensitive) == 0) {
                    if (curState) {
                        QString id = r.attributes().value("id").toString();
                        curHistoryState = new QHistoryState(r.attributes().value("type") == "shallow" ? QHistoryState::ShallowHistory : QHistoryState::DeepHistory,curState);
                        curHistoryState->setObjectName(id);
                        stateByID[id] = curHistoryState;
                    }
                } else if (r.name().toString().compare("final",Qt::CaseInsensitive) == 0) {
                    if (curState) {
                        QString id = r.attributes().value("id").toString();
                        QFinalState* f = new QFinalState(curState);
                        f->setObjectName(id);
                        curExecContext.state = f;
                        statesWithFinal.insert(curState);
                        QState* gp = qobject_cast<QState*>(curState->parentState());
                        if (gp) {
                            if (gp->childMode() == QState::ParallelStates) {
                                statesWithFinal.insert(gp);
                            }
                        }
                        stateByID[id] = f;
                    }
                } else if (r.name().toString().compare("script",Qt::CaseInsensitive) == 0) {
                    QString txt = r.readElementText().trimmed();
                    if (curExecContext.type == ScExecContext::None && curState == topLevelState) {
                        stateMachine->executeScript(QScriptProgram(txt,stateMachine->baseUrl().toLocalFile()));
                    } else
                        curExecContext.script += txt;
                } else if (r.name().toString().compare("log",Qt::CaseInsensitive) == 0) {
                    curExecContext.script +=
                            QString("scxml.print('[' + %1 + '][' + %2 + ']' + %3);")
                            .arg(r.attributes().value("label").toString())
                            .arg(r.attributes().value("level").toString())
                            .arg(r.attributes().value("expr").toString());

                } else if (r.name().toString().compare("assign",Qt::CaseInsensitive) == 0) {
                    QString locattr = r.attributes().value("location").toString();
                    if (locattr.isEmpty()) {
                        locattr = r.attributes().value("dataid").toString();
                        if (!locattr.isEmpty())
                            locattr = "_data." + locattr;
                    }
                    if (!locattr.isEmpty()) {
                        curExecContext.script += QString ("%1 = %2;").arg(locattr).arg(r.attributes().value("expr").toString());
                    }
                } else if (r.name().toString().compare("if",Qt::CaseInsensitive) == 0) {
                    curExecContext.script += QString("if (%1) {").arg(r.attributes().value("cond").toString());
                } else if (r.name().toString().compare("elseif",Qt::CaseInsensitive) == 0) {
                    curExecContext.script += QString("} elseif (%1) {").arg(r.attributes().value("cond").toString());
                } else if (r.name().toString().compare("else",Qt::CaseInsensitive) == 0) {
                    curExecContext.script += " } else { ";
                } else if (r.name().toString().compare("cancel",Qt::CaseInsensitive) == 0) {
                    curExecContext.script += QString("scxml.clearTimeout (%1);").arg(r.attributes().value("id").toString());
                } else if (r.name().toString().compare("onentry",Qt::CaseInsensitive) == 0) {
                    curExecContext.type = ScExecContext::StateEntry;
                    curExecContext.script = "";
                } else if (r.name().toString().compare("onexit",Qt::CaseInsensitive) == 0) {
                    curExecContext.type = ScExecContext::StateExit;
                    curExecContext.script = "";
                } else if (r.name().toString().compare("raise",Qt::CaseInsensitive) == 0 || r.name().toString().compare("event",Qt::CaseInsensitive) == 0 ) {
                    eventName = r.attributes().value("event").toString();
                    stateMachine->pvt->knownEvents.insert(eventName);
                    eventName = QString("\"%1\"").arg(eventName);
                    target = "'_internal'";
                    targetType = "scxml";
                    content = "{}";
                    paramNames.clear();
                    paramVals.clear();
                } else if (r.name().toString().compare("send",Qt::CaseInsensitive) == 0) {
                    paramNames.clear ();
                    paramVals.clear();
                    content = "{}";

                    target = r.attributes().value("target").toString();
                    if (target == "") {
                        target = r.attributes().value("targetexpr").toString();
                        if (target == "")
                            target = "\"\"";
                    } else
                        target = "\""+target+"\"";
                    targetType = r.attributes().value("type").toString();
                    eventName = r.attributes().value("event").toString();
                    if (eventName == "") {
                        eventName = r.attributes().value("eventexpr").toString();
                    } else {
                        stateMachine->pvt->knownEvents.insert(eventName);
                        eventName = "'"+eventName+"'";
                    }
                    QStringList nameList = r.attributes().value("namelist").toString().split(" ");
                    foreach (QString name,nameList) {
                        if (name != "") {
                            paramNames << name;
                            paramVals << QString("_data.") + name;
                        }
                    }
                    idLocation = r.attributes().value("idlocation").toString();
                    if (idLocation.isEmpty())
                        idLocation = r.attributes().value("sendid").toString();
                        
                    delay = r.attributes().value("delay").toString();
                    if (delay == "") {
                        delay = r.attributes().value("delayexpr").toString();
                        if (delay == "")
                            delay = "0";
                    } else
                        delay = "'"+delay+"'";

                    delay = QString("scxml.cssTime(%1)").arg(delay);

                } else if (r.name().toString().compare("invoke",Qt::CaseInsensitive) == 0) {
                    idLocation = r.attributes().value("idlocation").toString();
                        if (idLocation.isEmpty())
                            idLocation = r.attributes().value("invokeid").toString();
                        QObject::connect (curState, SIGNAL(exited()),new QScxmlScriptExec(QString("_data.invoke_%1.cancel();").arg(curState->objectName()),stateMachine),SLOT(exec()));

                    QString type = r.attributes().value("type").toString();
                    if (type.isEmpty())
                        type = "scxml";
                    curExecContext.type = ScExecContext::StateEntry;
                    curExecContext.state = curState;
                    paramNames.clear ();
                    paramVals.clear ();
                    content = "{}";
                    target = r.attributes().value("src").toString();
                    if (target == "")
                        target = "\"\"";
                    targetType = r.attributes().value("type").toString();
                } else if (r.name().toString().compare("transition",Qt::CaseInsensitive) == 0) {
                    if (curHistoryState) {
                        ScHistoryInfo inf;
                        inf.hstate = curHistoryState;
                        inf.defaultStateID = r.attributes().value("target").toString();
                        historyInfo.append(inf);
                    } else {
                        ScTransitionInfo inf;
                        inf.targets = r.attributes().value("target").toString().split(' ');
                        curExecContext.type = ScExecContext::Transition;
                        curExecContext.script = "";
                        curTransition = new QScxmlTransition(curState,stateMachine);
                        curTransition->setConditionExpression(r.attributes().value("cond").toString());
                        curTransition->setEventPrefixes(r.attributes().value("event").toString().split(' '));
                        foreach(QString pfx, curTransition->eventPrefixes()) {
                            if (pfx != "*")
                                stateMachine->pvt->knownEvents.insert(pfx);
                        }
                        curExecContext.trans = curTransition;
                        inf.transition = curTransition;
                        transitions.append(inf);
                        foreach (QString prefix, curTransition->eventPrefixes()) {
                            if (prefix.startsWith("q-signal:")) {
                                signalEvents.insert(prefix);
                            }
                        }
                        curTransition->setObjectName(QString ("%1 to %2 on %3 if %4").arg(curState->objectName()).arg(inf.targets.join(" ")).arg(curTransition->eventPrefixes().join(" ")).arg(curTransition->conditionExpression()));
                    }
                } else if (r.name().toString().compare("data",Qt::CaseInsensitive) == 0) {
                    QScriptValue val = qScriptValueFromValue<QString>(stateMachine->scriptEngine(),"")  ;
                    QString id = r.attributes().value("id").toString();
                    if (r.attributes().value("src").length())
                        val = evaluateFile(QFileInfo(filename).dir().absoluteFilePath(r.attributes().value("src").toString()));
                    else {
                        if (r.attributes().value("expr").length()) {
                            val = stateMachine->scriptEngine()->evaluate(r.attributes().value("expr").toString());
                        } else {
                            QString t = r.readElementText();
                            if (!t.isEmpty())
                                val = stateMachine->scriptEngine()->evaluate(t);
                        }
                    }
                    QScriptValue func = stateMachine->scriptEngine()->newFunction(QScxmlFunctions::dataAccess);
                    func.setProperty("key",id);
                    stateMachine->pvt->dataObj.setProperty(id,func,QScriptValue::PropertyGetter|QScriptValue::PropertySetter);
                    stateMachine->pvt->dataObj.setProperty(id,val);
                } else if (r.name().toString().compare("param",Qt::CaseInsensitive) == 0) {
                    paramNames << r.attributes().value("name").toString();
                    paramVals << r.attributes().value("expr").toString();
                } else if (r.name().toString().compare("content",Qt::CaseInsensitive) == 0) {
                    content = r.readElementText();
                }
        } else if (r.isEndElement()) {
             if (r.name().toString().compare("state",Qt::CaseInsensitive) == 0 || r.name().toString().compare("parallel",Qt::CaseInsensitive) == 0) {
                 if (curState == topLevelState) {
                     return;
                 } else {
                     curState = qobject_cast<QState*>(curState->parent());
                     curExecContext.state = curState;
                 }
             } else if (r.name().toString().compare("history",Qt::CaseInsensitive) == 0) {
                 curHistoryState = NULL;
             } else if (r.name().toString().compare("final",Qt::CaseInsensitive) == 0) {
                 curExecContext.state = (curExecContext.state->parentState());
             } else if (r.name().toString().compare("if",Qt::CaseInsensitive) == 0) {
                curExecContext.script += "}\n";
                 } else if (r.name().toString().compare("send",Qt::CaseInsensitive) == 0 || r.name().toString().compare("raise",Qt::CaseInsensitive) == 0) {
                if (!idLocation.isEmpty())
                    curExecContext.script += idLocation + " = ";
                    QString pnames;
                    bool first = true;
                    foreach (QString n, paramNames) {
                        if (!first)
                            pnames +=",";
                        pnames += QString("\"%1\"").arg(n);
                        first = false;
                    }
                    QString innerScript = QString("scxml.postEvent(%1,%2,\"%3\",[%4],[%5],%6);")
                                                        .arg(eventName).arg(target).arg(targetType)
                                                        .arg(pnames).arg(paramVals.join(",")).arg(content);
                    if (target == "'_internal'")
                        curExecContext.script += innerScript;
                    else
                        curExecContext.script += QString("scxml.setTimeout(function() {%1}, %2);")
                                .arg(innerScript).arg(delay);
               idLocation = "";
            } else if (    
                    r.name().toString().compare("onentry",Qt::CaseInsensitive) == 0
                    || r.name().toString().compare("onexit",Qt::CaseInsensitive) == 0
                    || r.name().toString().compare("scxml",Qt::CaseInsensitive) == 0) {
                curExecContext.state = curState;
                curExecContext.type = r.name().toString().compare("onexit",Qt::CaseInsensitive)==0 ? ScExecContext::StateExit : ScExecContext::StateEntry;
                curExecContext.applyScript();
                curExecContext.type = ScExecContext::None;
                curExecContext.script = "";
            } else if (r.name().toString().compare("transition",Qt::CaseInsensitive) == 0) {
                if (!curHistoryState) {
                    curExecContext.trans = curTransition;
                    curExecContext.type = ScExecContext::Transition;
                    curExecContext.applyScript();
                }
                curExecContext.type = ScExecContext::None;
            } else if (r.name().toString().compare("invoke",Qt::CaseInsensitive) == 0) {
                    QString pnames;
                    bool first = true;
                    foreach (QString n, paramNames) {
                        if (!first)
                            pnames +=",";
                        pnames += QString("\"%1\"").arg(n);
                        first = false;
                    }
                curExecContext.script +=  QString("_data.invoke_%1 = scxml.invoke(\"%2\",%3,[%4],[%5],%6); _data.invoke_%1.id = \"%1\";").arg(curState->objectName()).arg(targetType).arg(target).arg(pnames).arg(paramVals.join(",")).arg(content);
                if (!idLocation.isEmpty()) {
                    curExecContext.script +=  QString("%1 = _data.invoke_%2;").arg(idLocation).arg(curState->objectName());
                }
                curExecContext.state = curState;
                curExecContext.type = ScExecContext::StateEntry;
                curExecContext.applyScript();
                idLocation = "";
                curExecContext.type = ScExecContext::None;
            }
        }
    }
    }
}

QMap<QString,QVariant> QScxml::data() const
{
    QMap<QString,QVariant> d;
    QScriptValueIterator it (scriptEngine()->evaluate("_data"));
    while (it.hasNext()) {
        it.next();
        d[it.name()] = it.value().toVariant();
    }
    return d;
}

QScxml* QScxmlLoader::load(QIODevice* device, QObject* obj, const QString & filename)
{
    if (device->bytesAvailable() == 0) {
        qWarning() << QString("File %1 invalid or not found").arg(filename);
        return NULL;
    }
    stateMachine = new QScxml(obj);
    // traverse through the states
    loadState(stateMachine,device,"",filename);

    // resolve history default state
    foreach (ScHistoryInfo h, historyInfo) {
        h.hstate->setDefaultState(stateByID[h.defaultStateID]);
    }
    foreach (QString s, signalEvents) {
        QString sig = s;
        sig = sig.mid(sig.indexOf(':')+1);
        int liop = sig.lastIndexOf('.');
        QString obj = sig.left(liop);
        sig = sig.mid(liop+1);
        stateMachine->pvt->startScript += QString("scxml.connectSignalToEvent(%1,'%2',\"%3\");").arg(obj).arg(sig).arg(s);
    }
    
    foreach (QState* s, statesWithFinal) {
        QObject::connect(s,SIGNAL(finished()),stateMachine,SLOT(handleStateFinished()));
    }

    // resolve transitions

    foreach (ScTransitionInfo t, transitions) {
        QList<QAbstractState*> states;
        if (!t.targets.isEmpty()) {
            foreach (QString s, t.targets) {
                if (!s.trimmed().isEmpty()) {
                    QAbstractState* st = stateByID[s];
                    if (st)
                        states.append(st);
                }
            }
            t.transition->setTargetStates(states);
        }
    }

    return stateMachine;
}

void QScxml::handleStateFinished()
{
    QState* state = qobject_cast<QState*>(sender());
    if (state) {
        postEvent(new QScxmlEvent("done.state." + state->objectName()));
    }
}

/*!
    Loads a state machine from an scxml file located at \a filename, with parent object \a o.
  */
QScxml* QScxml::load (const QString & filename, QObject* o)
{
    QScxmlLoader l;
    QFile f (filename);
    f.open(QIODevice::ReadOnly);
    return l.load(&f,o,filename);
}

#include "moc_qscxml.cxx"
#endif
