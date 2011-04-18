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
#ifndef QSCXMLGUI_H
#define QSCXMLGUI_H

#include "qscxml.h"
class QMenu;
class QMessageBox;

class QScxmlMenuInvoker: public QScxmlInvoker
{
    Q_OBJECT

    public:
        QScxmlMenuInvoker(QScxmlEvent* ievent, QScxml* p) : QScxmlInvoker(ievent,p),menu(0)
        {
        }
        static void initInvokerFactory(QScxml*);
        static bool isTypeSupported (const QString & s) { return s== "q-menu"; }
    public Q_SLOTS:
        void activate ();
        void cancel ();

    private:
        QMenu* menu;
};


class QScxmlMessageBoxInvoker: public QScxmlInvoker
{
    Q_OBJECT

    public:
        QScxmlMessageBoxInvoker(QScxmlEvent* ievent, QScxml* p) : QScxmlInvoker(ievent,p),messageBox(0)
        {
        }

        static void initInvokerFactory(QScxml*);
        static bool isTypeSupported (const QString & s) { return s== "q-messagebox"; }
    public Q_SLOTS:
        void activate ();
        void cancel ();

    private Q_SLOTS:
        void onFinished (int);
    private:
        QMessageBox* messageBox;
};

#endif // QSSMGUIINVOKERS_P_H
