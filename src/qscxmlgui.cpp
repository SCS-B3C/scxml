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

#include "qscxmlgui.h"
#include <QMenu>
#include <QDebug>
#include <QMessageBox>
#include <QScriptValueIterator>
#include <QScriptEngine>
#include <QSignalMapper>
/*

  { "parent" : parentObject,
    "trackHovers" : true/false
        "children": {{"type": "action", "text": "",},
                     {"type": "menu"},
                     {"type": "separator"} },
  */
namespace
{
    void traverseMenu (QMenu* menu, QScriptValue value, QSignalMapper* clickMap, QSignalMapper* hoverMap, bool trackHover)
    {
        QScriptValueIterator it (value);
        while (it.hasNext()) {
            it.next();
            if (it.name() == "trackHover") {
                trackHover = it.value().toBoolean();
            } else if (it.name() == "parent") {
            } else if (it.name() == "children") {
                QScriptValueIterator cit (it.value());
                while (cit.hasNext()) {
                    cit.next();
                    QString type = cit.value().property("type").toString();
                    if (type == "action") {
                        QAction* act = menu->addAction("");
                        QScriptValueIterator ait (cit.value());
                        while (ait.hasNext()) {
                            ait.next();
                            if (ait.name() != "type") {
                                act->setProperty(ait.name().toAscii().constData(),ait.value().toVariant());
                            }
                        }
                        QObject::connect(act,SIGNAL(triggered()),clickMap,SLOT(map()));
                        clickMap->setMapping(act,QString("menu.action." + cit.value().property("id").toString()));
                        if (trackHover) {
                            QObject::connect(act,SIGNAL(hovered()),hoverMap,SLOT(map()));
                            hoverMap->setMapping(act,QString("menu.hover." + cit.value().property("id").toString()));
                        }
                    } else if (type == "menu") {
                        traverseMenu(menu->addMenu(""),it.value(),clickMap,hoverMap,trackHover);
                    } else if (type == "separator") {
                        menu->addSeparator();
                    }
                }
            } else {
                menu->setProperty(it.name().toAscii().constData(),it.value().toVariant());
            }
        }
    }
};

void QScxmlMenuInvoker::activate ()
{
    QScxmlEvent* ie = initEvent;
    QScriptValue v = ie->content();
    QWidget* parent = qobject_cast<QWidget*>(v.property("parent").toQObject());
    menu = new QMenu(parent);
    QSignalMapper* clickMap = new QSignalMapper(this);
    QSignalMapper* hoverMap = new QSignalMapper(this);
    connect (clickMap,SIGNAL(mapped(QString)), this, SLOT(postParentEvent(QString)));
    connect (hoverMap,SIGNAL(mapped(QString)), this, SLOT(postParentEvent(QString)));
    traverseMenu(menu,v,clickMap,hoverMap,false);
    menu->setParent(parent,Qt::Widget);
    menu->move(QPoint(0,0));
    menu->resize(parent->size());
    menu->show();
}
void QScxmlMenuInvoker::cancel ()
{
    if (menu)
        menu->deleteLater();
    QScxmlInvoker::cancel();
}

Q_SCRIPT_DECLARE_QMETAOBJECT(QMenu,QWidget*)
Q_SCRIPT_DECLARE_QMETAOBJECT(QMessageBox,QWidget*)
 void QScxmlMenuInvoker::initInvokerFactory(QScxml* sm)
 {
     QScriptEngine* se = sm->scriptEngine();
    se->globalObject().setProperty("QMenu",qScriptValueFromQMetaObject<QMenu>(se));
 }
 void QScxmlMessageBoxInvoker::initInvokerFactory(QScxml* sm)
 {
     QScriptEngine* se = sm->scriptEngine();
    se->globalObject().setProperty("QMessageBox",qScriptValueFromQMetaObject<QMessageBox>(se));
 }

void QScxmlMessageBoxInvoker::onFinished(int n) {
    postParentEvent(new QScxmlEvent("q-messagebox.finished",QStringList()<<"result",QVariantList()<<QVariant(n)));
}
/*
    { "parent": someWidget, "buttons": ...}
  */
void QScxmlMessageBoxInvoker::activate()
{
    QScriptValue v = initEvent->content();
    QWidget* parent = qobject_cast<QWidget*>(v.property("parent").toQObject());
    messageBox = new QMessageBox(parent);
    messageBox->setModal(false);
    QScriptValueIterator it (v);
    while (it.hasNext()) {
        it.next();
        if (it.name() == "standardButtons") {
            messageBox->setStandardButtons((QMessageBox::StandardButtons)it.value().toInt32());
        } else if (it.name() == "icon") {
            messageBox->setIcon((QMessageBox::Icon)it.value().toInt32());
        } else if (it.name() != "parent") {
            messageBox->setProperty(it.name().toAscii().constData(),it.value().toVariant());
        }
    }
    connect(messageBox,SIGNAL(finished(int)),this,SLOT(onFinished(int)));
    messageBox->show ();
}

void QScxmlMessageBoxInvoker::cancel()
{
    messageBox->deleteLater();
    QScxmlInvoker::cancel();
}
