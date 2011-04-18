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

#include <QCoreApplication>
#include <QXmlQuery>
#include <QUrl>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QXmlSchema>
#include <QXmlSchemaValidator>
void usage()
{
    printf("scc [--no-comments] -i input-file -o output-file");
    exit(-1);
}
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QXmlQuery query(QXmlQuery::XSLT20);
    if (a.arguments().count() < 3 || a.arguments().contains("--help"))
    {
        usage();
    }
    int idxOfInput = a.arguments().indexOf("-i")+1;
    int idxOfOutput = a.arguments().indexOf("-o")+1;
    if (idxOfInput <= 1 && idxOfOutput <= 1)
    {
        usage();
    }
    QString input = a.arguments().at(idxOfInput);
    QString output = a.arguments().at(idxOfOutput);
    QUrl target = QUrl::fromLocalFile(QDir::current().absoluteFilePath(".")).resolved(QUrl(input));
//    QXmlSchema schema;
//    QFile schemaFile(":/scxml.xsd");
//    schemaFile.open(QIODevice::ReadOnly);
//    schema.load(&schemaFile);
//    QXmlSchemaValidator validator;
//    if (!validator.validate(target)) {
//        return -1;
//    }
    query.setFocus(target);
    QFile q(":/scc/scc.xslt");
    q.open(QIODevice::ReadOnly);
    query.bindVariable("target",QXmlItem(QFileInfo(target.toLocalFile()).baseName()));
    query.bindVariable("comments",QXmlItem(!a.arguments().contains("--no-comments")));
    query.setQuery(QString(q.readAll()));
    if (!query.isValid())
        return -1;

    QString s;
    query.evaluateTo(&s);
    s = s.replace("&lt;","<").replace("&quot;","\"").replace("&gt;",">").replace("&amp;","&");
    QFile f(output);
    f.open(QIODevice::WriteOnly);
    f.write(s.toUtf8());
    return 0;
//    return a.exec();
}
