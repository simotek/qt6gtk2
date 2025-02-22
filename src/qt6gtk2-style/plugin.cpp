/***************************************************************************
 *   Copyright (C) 2015 The Qt Company Ltd.                                *
 *   Copyright (C) 2016-2023 Ilya Kotov, forkotov02@ya.ru                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include <QStylePlugin>
#include <QLibraryInfo>
#include "qgtkstyle_p.h"

QT_BEGIN_NAMESPACE

class Qt6Gtk2StylePlugin : public QStylePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QStyleFactoryInterface" FILE "qt6gtk2.json")

public:
    QStyle *create(const QString &key) override;
};

QStyle *Qt6Gtk2StylePlugin::create(const QString &key)
{
    QVersionNumber v = QLibraryInfo::version();
    if(v.majorVersion() != QT_VERSION_MAJOR || v.minorVersion() != QT_VERSION_MINOR)
    {
        qCritical("qt6gtk2 is compiled against incompatible Qt version (" QT_VERSION_STR ").");
        return nullptr;
    }

    if (key == "gtk2" || key == "qt6gtk2" || key == "qt5gtk2")
        return new QGtkStyle;
    return nullptr;
}

QT_END_NAMESPACE

#include "plugin.moc"
