/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "WelcomeDialog.h"
#include "ui_WelcomeDialog.h"

#include "ActiveObjects.h"
#include "MainWindow.h"
#include "legacy/modules/ModuleManager.h"

#include <QCheckBox>
#include <QPushButton>

#include <pqApplicationCore.h>
#include <pqSettings.h>

namespace tomviz {

WelcomeDialog::WelcomeDialog(MainWindow* mw)
  : QDialog(mw), m_ui(new Ui::WelcomeDialog)
{
  m_ui->setupUi(this);
  connect(m_ui->doNotShowAgain, &QCheckBox::checkStateChanged, this,
          &WelcomeDialog::onDoNotShowAgainStateChanged);
  connect(m_ui->noButton, &QPushButton::clicked, this, &WelcomeDialog::hide);
  connect(m_ui->yesButton, &QPushButton::clicked, this,
          &WelcomeDialog::onLoadSampleDataClicked);
}

WelcomeDialog::~WelcomeDialog() = default;

void WelcomeDialog::onLoadSampleDataClicked()
{
  auto mw = qobject_cast<MainWindow*>(this->parent());
  mw->openRecon();
  // TODO: migrate to new pipeline — remove auto-created slice and add
  // volume sink via the new pipeline API
  hide();
}

void WelcomeDialog::onDoNotShowAgainStateChanged(Qt::CheckState state)
{
  bool showDialog = (state != Qt::Checked);

  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("GeneralSettings.ShowWelcomeDialog", showDialog);
}
} // namespace tomviz
