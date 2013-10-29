/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "debuggerkitinformation.h"
#include "debuggerkitconfigwidget.h"
#include "debuggeroptionspage.h"

#include <projectexplorer/abi.h>
#include <utils/fileutils.h>

#include <QProcess>
#include <QUuid>

using namespace Debugger::Internal;
using namespace ProjectExplorer;
using namespace Utils;

static const char DEBUGGER_INFORMATION_COMMAND[] = "Binary";
static const char DEBUGGER_INFORMATION_DISPLAYNAME[] = "DisplayName";
static const char DEBUGGER_INFORMATION_ID[] = "Id";
static const char DEBUGGER_INFORMATION_ENGINETYPE[] = "EngineType";
static const char DEBUGGER_INFORMATION_AUTODETECTED[] = "AutoDetected";
static const char DEBUGGER_INFORMATION_ABIS[] = "Abis";

namespace Debugger {

// --------------------------------------------------------------------------
// DebuggerItem
// --------------------------------------------------------------------------

DebuggerItem::DebuggerItem()
{
    m_id = QUuid::createUuid().toString();
    m_engineType = NoEngineType;
    m_isAutoDetected = false;
}

DebuggerItem::DebuggerItem(const QVariant &id)
{
    m_id = id;
    m_engineType = NoEngineType;
    m_isAutoDetected = false;
}

DebuggerItem::DebuggerItem(const QVariantMap &data)
{
    m_command = FileName::fromUserInput(data.value(QLatin1String(DEBUGGER_INFORMATION_COMMAND)).toString());
    m_id = data.value(QLatin1String(DEBUGGER_INFORMATION_ID)).toString();
    m_displayName = data.value(QLatin1String(DEBUGGER_INFORMATION_DISPLAYNAME)).toString();
    m_isAutoDetected = data.value(QLatin1String(DEBUGGER_INFORMATION_AUTODETECTED), false).toBool();
    m_engineType = DebuggerEngineType(data.value(QLatin1String(DEBUGGER_INFORMATION_ENGINETYPE),
                                                 static_cast<int>(NoEngineType)).toInt());

    foreach (const QString &a, data.value(QLatin1String(DEBUGGER_INFORMATION_ABIS)).toStringList()) {
        Abi abi(a);
        if (abi.isValid())
            m_abis.append(abi);
    }
}

void DebuggerItem::reinitializeFromFile()
{
    QProcess proc;
    proc.start(m_command.toString(), QStringList() << QLatin1String("--version"));
    proc.waitForStarted();
    proc.waitForFinished();
    QByteArray ba = proc.readAll();
    if (ba.contains("gdb")) {
        m_engineType = GdbEngineType;
        const char needle[] = "This GDB was configured as \"";
        // E.g.  "--host=i686-pc-linux-gnu --target=arm-unknown-nto-qnx6.5.0".
        // or "i686-linux-gnu"
        int pos1 = ba.indexOf(needle);
        if (pos1 != -1) {
            pos1 += int(sizeof(needle));
            int pos2 = ba.indexOf('"', pos1 + 1);
            QByteArray target = ba.mid(pos1, pos2 - pos1);
            int pos3 = target.indexOf("--target=");
            if (pos3 >= 0)
                target = target.mid(pos3 + 9);
            m_abis.append(Abi::abiFromTargetTriplet(QString::fromLatin1(target)));
        } else {
            // Fallback.
            m_abis = Abi::abisOfBinary(m_command); // FIXME: Wrong.
        }
        return;
    }
    if (ba.contains("lldb") || ba.startsWith("LLDB")) {
        m_engineType = LldbEngineType;
        m_abis = Abi::abisOfBinary(m_command);
        return;
    }
    if (ba.startsWith("Python")) {
        m_engineType = PdbEngineType;
        return;
    }
    m_engineType = NoEngineType;
}

QString DebuggerItem::engineTypeName() const
{
    switch (m_engineType) {
    case Debugger::NoEngineType:
        return DebuggerOptionsPage::tr("Not recognized");
    case Debugger::GdbEngineType:
        return QLatin1String("GDB");
    case Debugger::CdbEngineType:
        return QLatin1String("CDB");
    case Debugger::LldbEngineType:
        return QLatin1String("LLDB");
    default:
        return QString();
    }
}

QStringList DebuggerItem::abiNames() const
{
    QStringList list;
    foreach (const Abi &abi, m_abis)
        list.append(abi.toString());
    return list;
}

bool DebuggerItem::operator==(const DebuggerItem &other) const
{
    return m_id == other.m_id
            && m_displayName == other.m_displayName
            && m_isAutoDetected == other.m_isAutoDetected
            && m_command == other.m_command;
}

QVariantMap DebuggerItem::toMap() const
{
    QVariantMap data;
    data.insert(QLatin1String(DEBUGGER_INFORMATION_DISPLAYNAME), m_displayName);
    data.insert(QLatin1String(DEBUGGER_INFORMATION_ID), m_id);
    data.insert(QLatin1String(DEBUGGER_INFORMATION_COMMAND), m_command.toUserOutput());
    data.insert(QLatin1String(DEBUGGER_INFORMATION_ENGINETYPE), int(m_engineType));
    data.insert(QLatin1String(DEBUGGER_INFORMATION_AUTODETECTED), m_isAutoDetected);
    data.insert(QLatin1String(DEBUGGER_INFORMATION_ABIS), abiNames());
    return data;
}

void DebuggerItem::setDisplayName(const QString &displayName)
{
    m_displayName = displayName;
}

void DebuggerItem::setEngineType(const DebuggerEngineType &engineType)
{
    m_engineType = engineType;
}

void DebuggerItem::setCommand(const Utils::FileName &command)
{
    m_command = command;
}

void DebuggerItem::setAutoDetected(bool isAutoDetected)
{
    m_isAutoDetected = isAutoDetected;
}

void DebuggerItem::setAbis(const QList<ProjectExplorer::Abi> &abis)
{
    m_abis = abis;
}

void DebuggerItem::setAbi(const Abi &abi)
{
    m_abis.clear();
    m_abis.append(abi);
}

static DebuggerItem::MatchLevel matchSingle(const Abi &debuggerAbi, const Abi &targetAbi)
{
    if (debuggerAbi.architecture() != Abi::UnknownArchitecture
            && debuggerAbi.architecture() != targetAbi.architecture())
        return DebuggerItem::DoesNotMatch;

    if (debuggerAbi.os() != Abi::UnknownOS
            && debuggerAbi.os() != targetAbi.os())
        return DebuggerItem::DoesNotMatch;

    if (debuggerAbi.binaryFormat() != Abi::UnknownFormat
            && debuggerAbi.binaryFormat() != targetAbi.binaryFormat())
        return DebuggerItem::DoesNotMatch;

    if (debuggerAbi.os() == Abi::WindowsOS) {
        if (debuggerAbi.osFlavor() == Abi::WindowsMSysFlavor && targetAbi.osFlavor() != Abi::WindowsMSysFlavor)
            return DebuggerItem::DoesNotMatch;
        if (debuggerAbi.osFlavor() != Abi::WindowsMSysFlavor && targetAbi.osFlavor() == Abi::WindowsMSysFlavor)
            return DebuggerItem::DoesNotMatch;
    }

    if (debuggerAbi.wordWidth() == 64 && targetAbi.wordWidth() == 32)
        return DebuggerItem::MatchesSomewhat;
    if (debuggerAbi.wordWidth() != 0 && debuggerAbi.wordWidth() != targetAbi.wordWidth())
        return DebuggerItem::DoesNotMatch;

    return DebuggerItem::MatchesPerfectly;
}

DebuggerItem::MatchLevel DebuggerItem::matchTarget(const Abi &targetAbi) const
{
    MatchLevel bestMatch = DoesNotMatch;
    foreach (const Abi &debuggerAbi, m_abis) {
        MatchLevel currentMatch = matchSingle(debuggerAbi, targetAbi);
        if (currentMatch > bestMatch)
            bestMatch = currentMatch;
    }
    return bestMatch;
}

bool Debugger::DebuggerItem::isValid() const
{
    return !m_id.isNull();
}

} // namespace Debugger;

#ifdef WITH_TESTS

#    include <QTest>
#    include "debuggerplugin.h"

void Debugger::DebuggerPlugin::testDebuggerMatching_data()
{
    QTest::addColumn<QStringList>("debugger");
    QTest::addColumn<QString>("target");
    QTest::addColumn<int>("result");

    QTest::newRow("Invalid data")
            << QStringList()
            << QString()
            << int(DebuggerItem::DoesNotMatch);
    QTest::newRow("Invalid debugger")
            << QStringList()
            << QString::fromLatin1("x86-linux-generic-elf-32bit")
            << int(DebuggerItem::DoesNotMatch);
    QTest::newRow("Invalid target")
            << (QStringList() << QLatin1String("x86-linux-generic-elf-32bit"))
            << QString()
            << int(DebuggerItem::DoesNotMatch);

    QTest::newRow("Fuzzy match 1")
            << (QStringList() << QLatin1String("unknown-unknown-unknown-unknown-0bit"))
            << QString::fromLatin1("x86-linux-generic-elf-32bit")
            << int(DebuggerItem::MatchesPerfectly); // Is this the expected behavior?
    QTest::newRow("Fuzzy match 2")
            << (QStringList() << QLatin1String("unknown-unknown-unknown-unknown-0bit"))
            << QString::fromLatin1("arm-windows-msys-pe-64bit")
            << int(DebuggerItem::MatchesPerfectly); // Is this the expected behavior?

    QTest::newRow("Architecture mismatch")
            << (QStringList() << QLatin1String("x86-linux-generic-elf-32bit"))
            << QString::fromLatin1("arm-linux-generic-elf-32bit")
            << int(DebuggerItem::DoesNotMatch);
    QTest::newRow("OS mismatch")
            << (QStringList() << QLatin1String("x86-linux-generic-elf-32bit"))
            << QString::fromLatin1("x86-macosx-generic-elf-32bit")
            << int(DebuggerItem::DoesNotMatch);
    QTest::newRow("Format mismatch")
            << (QStringList() << QLatin1String("x86-linux-generic-elf-32bit"))
            << QString::fromLatin1("x86-linux-generic-pe-32bit")
            << int(DebuggerItem::DoesNotMatch);

    QTest::newRow("Linux perfect match")
            << (QStringList() << QLatin1String("x86-linux-generic-elf-32bit"))
            << QString::fromLatin1("x86-linux-generic-elf-32bit")
            << int(DebuggerItem::MatchesPerfectly);
    QTest::newRow("Linux match")
            << (QStringList() << QLatin1String("x86-linux-generic-elf-64bit"))
            << QString::fromLatin1("x86-linux-generic-elf-32bit")
            << int(DebuggerItem::MatchesSomewhat);

    QTest::newRow("Windows perfect match 1")
            << (QStringList() << QLatin1String("x86-windows-msvc2013-pe-64bit"))
            << QString::fromLatin1("x86-windows-msvc2013-pe-64bit")
            << int(DebuggerItem::MatchesPerfectly);
    QTest::newRow("Windows perfect match 2")
            << (QStringList() << QLatin1String("x86-windows-msvc2013-pe-64bit"))
            << QString::fromLatin1("x86-windows-msvc2012-pe-64bit")
            << int(DebuggerItem::MatchesPerfectly);
    QTest::newRow("Windows match 1")
            << (QStringList() << QLatin1String("x86-windows-msvc2013-pe-64bit"))
            << QString::fromLatin1("x86-windows-msvc2013-pe-32bit")
            << int(DebuggerItem::MatchesSomewhat);
    QTest::newRow("Windows match 2")
            << (QStringList() << QLatin1String("x86-windows-msvc2013-pe-64bit"))
            << QString::fromLatin1("x86-windows-msvc2012-pe-32bit")
            << int(DebuggerItem::MatchesSomewhat);
    QTest::newRow("Windows mismatch on word size")
            << (QStringList() << QLatin1String("x86-windows-msvc2013-pe-32bit"))
            << QString::fromLatin1("x86-windows-msvc2013-pe-64bit")
            << int(DebuggerItem::DoesNotMatch);
    QTest::newRow("Windows mismatch on osflavor 1")
            << (QStringList() << QLatin1String("x86-windows-msvc2013-pe-32bit"))
            << QString::fromLatin1("x86-windows-msys-pe-64bit")
            << int(DebuggerItem::DoesNotMatch);
    QTest::newRow("Windows mismatch on osflavor 2")
            << (QStringList() << QLatin1String("x86-windows-msys-pe-32bit"))
            << QString::fromLatin1("x86-windows-msvc2010-pe-64bit")
            << int(DebuggerItem::DoesNotMatch);
}

void Debugger::DebuggerPlugin::testDebuggerMatching()
{
    QFETCH(QStringList, debugger);
    QFETCH(QString, target);
    QFETCH(int, result);

    DebuggerItem::MatchLevel expectedLevel = static_cast<DebuggerItem::MatchLevel>(result);

    QList<Abi> debuggerAbis;
    foreach (const QString &abi, debugger)
        debuggerAbis << Abi(abi);

    DebuggerItem item;
    item.setAbis(debuggerAbis);

    DebuggerItem::MatchLevel level = item.matchTarget(Abi(target));

    QCOMPARE(expectedLevel, level);
}
#endif
