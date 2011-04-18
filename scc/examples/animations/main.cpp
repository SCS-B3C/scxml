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

#include "sc_animations.h"
#include <QApplication>
#include <QGraphicsView>
#include <QLabel>
#include <QGraphicsProxyWidget>
#include <QShortcut>
#include <QGraphicsScene>
#include <QPushButton>
#include <QVBoxLayout>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SMClass_animations stateMachine;
    QGraphicsView* gv = new QGraphicsView();
    QPushButton* b = new QPushButton();
    QWidget* w = new QWidget();
    QVBoxLayout* lyt = new QVBoxLayout();
    lyt->addWidget(gv);
    lyt->addWidget(b);
    b->setText("Push Me");
    QObject::connect(b,SIGNAL(clicked()),&stateMachine,SIGNAL(event_ev1()));
    w->setLayout(lyt);
    gv->setScene(new QGraphicsScene());
    gv->setInteractive(true);
    gv->setFocus();
    QGraphicsProxyWidget* wdg = gv->scene()->addWidget(new QLabel("Hello World!"));
    wdg->setOpacity(0);
    QPropertyAnimation* anim = new QPropertyAnimation();
    anim->setPropertyName("opacity");
    anim->setTargetObject(wdg);
    anim->setDuration(1000);
    anim->setEasingCurve(QEasingCurve(QEasingCurve::InOutCubic));
    stateMachine.set_indicator(wdg);
    stateMachine.set_anim(anim);
    stateMachine.setupStateMachine();
    w->show();
    stateMachine.start();
    return a.exec();
}
