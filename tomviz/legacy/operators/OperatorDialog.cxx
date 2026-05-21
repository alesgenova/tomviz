/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

namespace tomviz {

OperatorDialog::OperatorDialog(QWidget* parentObject) : Superclass(parentObject)
{
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel("(Operator parameters not available)", this));
  QDialogButtonBox* buttons = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  this->setLayout(layout);
  layout->addWidget(buttons);
}

OperatorDialog::~OperatorDialog() {}

void OperatorDialog::setJSONDescription(const QString& /*json*/)
{
  // Stubbed out: OperatorWidget has been removed.
}

QMap<QString, QVariant> OperatorDialog::values() const
{
  // Stubbed out: OperatorWidget has been removed.
  return {};
}
} // namespace tomviz
