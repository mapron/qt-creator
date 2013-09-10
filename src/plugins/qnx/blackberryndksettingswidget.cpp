/**************************************************************************
**
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
**
** Contact: BlackBerry (qt@blackberry.com)
** Contact: KDAB (info@kdab.com)
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

#include "blackberryndksettingswidget.h"
#include "ui_blackberryndksettingswidget.h"
#include "qnxutils.h"
#include "blackberrysigningutils.h"
#include "blackberrysetupwizard.h"

#include "blackberryconfigurationmanager.h"
#include "blackberryconfiguration.h"

#include <utils/pathchooser.h>

#include <coreplugin/icore.h>

#include <QMessageBox>
#include <QFileDialog>

#include <QStandardItemModel>
#include <QTreeWidgetItem>

namespace Qnx {
namespace Internal {

BlackBerryNDKSettingsWidget::BlackBerryNDKSettingsWidget(QWidget *parent) :
    QWidget(parent),
    m_ui(new Ui_BlackBerryNDKSettingsWidget),
    m_autoDetectedNdks(0),
    m_manualNdks(0)
{
    m_bbConfigManager = &BlackBerryConfigurationManager::instance();
    m_ui->setupUi(this);

    m_ui->removeNdkButton->setEnabled(false);
    m_ui->activateNdkTargetButton->setEnabled(false);
    m_ui->deactivateNdkTargetButton->setEnabled(false);

    m_activatedTargets << m_bbConfigManager->activeConfigurations();

    initNdkList();

    connect(m_ui->wizardButton, SIGNAL(clicked()), this, SLOT(launchBlackBerrySetupWizard()));
    connect(m_ui->addNdkButton, SIGNAL(clicked()), this, SLOT(addNdkTarget()));
    connect(m_ui->removeNdkButton, SIGNAL(clicked()), this, SLOT(removeNdkTarget()));
    connect(m_ui->activateNdkTargetButton, SIGNAL(clicked()), this, SLOT(activateNdkTarget()));
    connect(m_ui->deactivateNdkTargetButton, SIGNAL(clicked()), this, SLOT(deactivateNdkTarget()));
    connect(m_ui->ndksTreeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(updateInfoTable(QTreeWidgetItem*)));
}

void BlackBerryNDKSettingsWidget::setWizardMessageVisible(bool visible)
{
    m_ui->wizardLabel->setVisible(visible);
    m_ui->wizardButton->setVisible(visible);
}

bool BlackBerryNDKSettingsWidget::hasActiveNdk() const
{
    return !m_bbConfigManager->configurations().isEmpty();
}

QList<BlackBerryConfiguration *> BlackBerryNDKSettingsWidget::activatedTargets()
{
    return m_activatedTargets;
}

QList<BlackBerryConfiguration *> BlackBerryNDKSettingsWidget::deactivatedTargets()
{
    return m_deactivatedTargets;
}

void BlackBerryNDKSettingsWidget::launchBlackBerrySetupWizard() const
{
    BlackBerrySigningUtils &blackBerryUtils = BlackBerrySigningUtils::instance();
    const bool alreadyConfigured = blackBerryUtils.hasRegisteredKeys() && blackBerryUtils.hasDefaultCertificate();

    if (alreadyConfigured) {
        QMessageBox::information(0, tr("Qt Creator"),
            tr("It appears that your BlackBerry environment has already been configured."));
            return;
    }

    BlackBerrySetupWizard wizard;
    wizard.exec();
}

void BlackBerryNDKSettingsWidget::updateInfoTable(QTreeWidgetItem* currentItem)
{
    if (!currentItem)
        return;

    QString envFilePath = currentItem->text(1);
    if (envFilePath.isEmpty()) {
        m_ui->removeNdkButton->setEnabled(false);
        m_ui->activateNdkTargetButton->setEnabled(false);
        m_ui->deactivateNdkTargetButton->setEnabled(false);
        return;
    }

    BlackBerryConfiguration *config = m_bbConfigManager->configurationFromEnvFile(Utils::FileName::fromString(envFilePath));
    if (!config)
        return;

    foreach (const NdkInstallInformation &ndkInfo, QnxUtils::installedNdks())
    {
        if (ndkInfo.name == config->displayName()) {
            m_ui->baseNameLabel->setText(ndkInfo.name);
            m_ui->ndkPathLabel->setText(ndkInfo.path);
            m_ui->versionLabel->setText(ndkInfo.version);
            m_ui->hostLabel->setText(ndkInfo.host);
            m_ui->targetLabel->setText(ndkInfo.target);
            break;
        }
    }

    updateUi(currentItem, config);
}

void BlackBerryNDKSettingsWidget::updateNdkList()
{
    foreach (BlackBerryConfiguration *config, m_bbConfigManager->configurations()) {
        QTreeWidgetItem *parent = config->isAutoDetected() ? m_autoDetectedNdks : m_manualNdks;
        QTreeWidgetItem *item = new QTreeWidgetItem(parent);
        item->setText(0, config->displayName());
        item->setText(1, config->ndkEnvFile().toString());
        QFont font;
        font.setBold(config->isActive());
        item->setFont(0, font);
        item->setFont(1, font);
    }

    if (m_autoDetectedNdks->child(0)) {
        m_autoDetectedNdks->child(0)->setSelected(true);
        updateInfoTable(m_autoDetectedNdks->child(0));
    }
}

void BlackBerryNDKSettingsWidget::addNdkTarget()
{
    QString selectedPath = QFileDialog::getOpenFileName(0, tr("Select the NDK Environment file"),
                                                        QString(), tr("BlackBerry Environment File (*.sh *.bat)"));
    if (selectedPath.isEmpty() || !QFileInfo(selectedPath).exists())
        return;

    BlackBerryConfiguration *config = m_bbConfigManager->configurationFromEnvFile(Utils::FileName::fromString(selectedPath));

    if (!config) {
        config = new BlackBerryConfiguration(Utils::FileName::fromString(selectedPath), false);
        if (!m_bbConfigManager->addConfiguration(config)) {
            delete config;
            return;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(m_manualNdks);
        item->setText(0, selectedPath.split(QDir::separator()).last());
        item->setText(1, selectedPath);
        updateInfoTable(item);
    }
}

void BlackBerryNDKSettingsWidget::removeNdkTarget()
{
    if (!m_ui->ndksTreeWidget->currentItem())
        return;

    QString ndk = m_ui->ndksTreeWidget->currentItem()->text(0);
    QString envFilePath = m_ui->ndksTreeWidget->currentItem()->text(1);
    QMessageBox::StandardButton button =
            QMessageBox::question(Core::ICore::mainWindow(),
                                  tr("Clean BlackBerry 10 Configuration"),
                                  tr("Are you sure you want to remove:\n %1?").arg(ndk),
                                  QMessageBox::Yes | QMessageBox::No);

    if (button == QMessageBox::Yes) {
        BlackBerryConfiguration *config = m_bbConfigManager->configurationFromEnvFile(Utils::FileName::fromString(envFilePath));
        if (config) {
            m_bbConfigManager->removeConfiguration(config);
            m_manualNdks->removeChild(m_ui->ndksTreeWidget->currentItem());
        }
    }
}

void BlackBerryNDKSettingsWidget::activateNdkTarget()
{
    if (!m_ui->ndksTreeWidget->currentItem())
        return;

    QString envFilePath = m_ui->ndksTreeWidget->currentItem()->text(1);

    BlackBerryConfiguration *config = m_bbConfigManager->configurationFromEnvFile(Utils::FileName::fromString(envFilePath));
    if (config && !m_activatedTargets.contains(config)) {
        m_activatedTargets << config;
        if (m_deactivatedTargets.contains(config))
           m_deactivatedTargets.removeAt(m_deactivatedTargets.indexOf(config));

        updateUi(m_ui->ndksTreeWidget->currentItem(), config);
    }
}

void BlackBerryNDKSettingsWidget::deactivateNdkTarget()
{
    if (!m_ui->ndksTreeWidget->currentItem())
        return;

    QString envFilePath = m_ui->ndksTreeWidget->currentItem()->text(1);

    BlackBerryConfiguration *config = m_bbConfigManager->configurationFromEnvFile(Utils::FileName::fromString(envFilePath));
    if (config && m_activatedTargets.contains(config)) {
        m_deactivatedTargets << config;
        m_activatedTargets.removeAt(m_activatedTargets.indexOf(config));
        updateUi(m_ui->ndksTreeWidget->currentItem(), config);
    }
}

void BlackBerryNDKSettingsWidget::updateUi(QTreeWidgetItem *item, BlackBerryConfiguration *config)
{
    if (!item || !config)
        return;

    QFont font;
    font.setBold(m_activatedTargets.contains(config));
    item->setFont(0, font);
    item->setFont(1, font);

    m_ui->activateNdkTargetButton->setEnabled((!m_activatedTargets.contains(config))
                                               && config->isAutoDetected());
    m_ui->deactivateNdkTargetButton->setEnabled((m_activatedTargets.contains(config))
                                                && m_activatedTargets.size() > 1
                                                && config->isAutoDetected());
    m_ui->removeNdkButton->setEnabled(!config->isAutoDetected());
}

void BlackBerryNDKSettingsWidget::initNdkList()
{
    m_ui->ndksTreeWidget->header()->setResizeMode(QHeaderView::Stretch);
    m_ui->ndksTreeWidget->header()->setStretchLastSection(false);
    m_ui->ndksTreeWidget->setHeaderItem(new QTreeWidgetItem(QStringList() << tr("NDK") << tr("NDK Environment File")));
    m_ui->ndksTreeWidget->setTextElideMode(Qt::ElideNone);
    m_ui->ndksTreeWidget->setColumnCount(2);
    m_autoDetectedNdks = new QTreeWidgetItem(m_ui->ndksTreeWidget);
    m_autoDetectedNdks->setText(0, tr("Auto-Detected"));
    m_autoDetectedNdks->setFirstColumnSpanned(true);
    m_autoDetectedNdks->setFlags(Qt::ItemIsEnabled);
    m_manualNdks = new QTreeWidgetItem(m_ui->ndksTreeWidget);
    m_manualNdks->setText(0, tr("Manual"));
    m_manualNdks->setFirstColumnSpanned(true);
    m_manualNdks->setFlags(Qt::ItemIsEnabled);

    m_ui->ndksTreeWidget->expandAll();

    updateNdkList();
}

} // namespace Internal
} // namespace Qnx

