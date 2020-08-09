/***

  Olive - Non-Linear Video Editor
  Copyright (C) 2019 Olive Team

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

***/

#include "nodeparamviewconnectedlabel.h"

#include <QHBoxLayout>

#include "common/qtutils.h"
#include "core.h"
#include "node/node.h"
#include "widget/menu/menu.h"
#include "widget/nodeview/nodeviewundo.h"

OLIVE_NAMESPACE_ENTER

NodeParamViewConnectedLabel::NodeParamViewConnectedLabel(NodeInput *input, QWidget *parent) :
  QWidget(parent),
  input_(input)
{
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setSpacing(QFontMetricsWidth(fontMetrics(), QStringLiteral(" ")));
  layout->setMargin(0);

  layout->addWidget(new QLabel(tr("Connected to")));

  connected_to_lbl_ = new ClickableLabel();
  connected_to_lbl_->setCursor(Qt::PointingHandCursor);
  connected_to_lbl_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(connected_to_lbl_, &ClickableLabel::MouseClicked, this, &NodeParamViewConnectedLabel::ConnectionClicked);
  connect(connected_to_lbl_, &ClickableLabel::customContextMenuRequested, this, &NodeParamViewConnectedLabel::ShowLabelContextMenu);
  layout->addWidget(connected_to_lbl_);

  layout->addStretch();

  // Set up "link" font
  QFont link_font = connected_to_lbl_->font();
  link_font.setUnderline(true);
  connected_to_lbl_->setForegroundRole(QPalette::Link);
  connected_to_lbl_->setFont(link_font);

  UpdateConnected();

  connect(input_, &NodeInput::EdgeAdded, this, &NodeParamViewConnectedLabel::UpdateConnected);
  connect(input_, &NodeInput::EdgeRemoved, this, &NodeParamViewConnectedLabel::UpdateConnected);
}

void NodeParamViewConnectedLabel::UpdateConnected()
{
  QString connection_str;

  if (input_->is_connected()) {
    connection_str = input_->get_connected_node()->Name();
  } else {
    connection_str = tr("Nothing");
  }

  connected_to_lbl_->setText(connection_str);
}

void NodeParamViewConnectedLabel::ShowLabelContextMenu()
{
  Menu m(this);

  QAction* disconnect_action = m.addAction(tr("Disconnect"));
  connect(disconnect_action, &QAction::triggered, this, [this](){
    Core::instance()->undo_stack()->push(new NodeEdgeRemoveCommand(input_->get_connected_output(), input_));
  });

  m.exec(QCursor::pos());
}

OLIVE_NAMESPACE_EXIT
