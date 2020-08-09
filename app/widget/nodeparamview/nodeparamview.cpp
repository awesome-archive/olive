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

#include "nodeparamview.h"

#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>

#include "common/timecodefunctions.h"
#include "node/output/viewer/viewer.h"

OLIVE_NAMESPACE_ENTER

NodeParamView::NodeParamView(QWidget *parent) :
  TimeBasedWidget(true, false, parent),
  last_scroll_val_(0)
{
  // Create horizontal layout to place scroll area in (and keyframe editing eventually)
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setSpacing(0);
  layout->setMargin(0);

  QSplitter* splitter = new QSplitter(Qt::Horizontal);
  layout->addWidget(splitter);

  // Set up scroll area for params
  QScrollArea* scroll_area = new QScrollArea();
  scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll_area->setWidgetResizable(true);
  splitter->addWidget(scroll_area);

  // Param widget
  param_widget_area_ = new QWidget();
  scroll_area->setWidget(param_widget_area_);

  // Set up scroll area layout
  param_layout_ = new QVBoxLayout(param_widget_area_);
  param_layout_->setSpacing(0);

  // KeyframeView is offset by a ruler, so to stay synchronized with it, we should be too
  param_layout_->setContentsMargins(0, ruler()->height(), 0, 0);

  // Add a stretch to allow empty space at the bottom of the layout
  param_layout_->addStretch();

  // Set up keyframe view
  QWidget* keyframe_area = new QWidget();
  QVBoxLayout* keyframe_area_layout = new QVBoxLayout(keyframe_area);
  keyframe_area_layout->setSpacing(0);
  keyframe_area_layout->setMargin(0);

  // Create ruler object
  keyframe_area_layout->addWidget(ruler());

  // Create keyframe view
  keyframe_view_ = new KeyframeView();
  keyframe_view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ConnectTimelineView(keyframe_view_);
  connect(keyframe_view_, &KeyframeView::RequestCenterScrollOnPlayhead, this, &NodeParamView::CenterScrollOnPlayhead);
  keyframe_area_layout->addWidget(keyframe_view_);

  // Connect ruler and keyframe view together
  connect(ruler(), &TimeRuler::TimeChanged, keyframe_view_, &KeyframeView::SetTime);
  connect(keyframe_view_, &KeyframeView::TimeChanged, ruler(), &TimeRuler::SetTime);
  connect(keyframe_view_, &KeyframeView::TimeChanged, this, &NodeParamView::SetTimestamp);

  // Connect keyframe view scaling to this
  connect(keyframe_view_, &KeyframeView::ScaleChanged, this, &NodeParamView::SetScale);

  splitter->addWidget(keyframe_area);

  // Set both widgets to 50/50
  splitter->setSizes({INT_MAX, INT_MAX});

  // Disable collapsing param view (but collapsing keyframe view is permitted)
  splitter->setCollapsible(0, false);

  // Create global vertical scrollbar on the right
  vertical_scrollbar_ = new QScrollBar();
  vertical_scrollbar_->setMaximum(0);
  layout->addWidget(vertical_scrollbar_);

  // Connect scrollbars together
  connect(scroll_area->verticalScrollBar(), &QScrollBar::rangeChanged, vertical_scrollbar_, &QScrollBar::setRange);
  connect(scroll_area->verticalScrollBar(), &QScrollBar::rangeChanged, this, &NodeParamView::ForceKeyframeViewToScroll);

  connect(keyframe_view_->verticalScrollBar(), &QScrollBar::valueChanged, vertical_scrollbar_, &QScrollBar::setValue);
  connect(keyframe_view_->verticalScrollBar(), &QScrollBar::valueChanged, scroll_area->verticalScrollBar(), &QScrollBar::setValue);
  connect(scroll_area->verticalScrollBar(), &QScrollBar::valueChanged, vertical_scrollbar_, &QScrollBar::setValue);
  connect(scroll_area->verticalScrollBar(), &QScrollBar::valueChanged, keyframe_view_->verticalScrollBar(), &QScrollBar::setValue);
  connect(vertical_scrollbar_, &QScrollBar::valueChanged, scroll_area->verticalScrollBar(), &QScrollBar::setValue);
  connect(vertical_scrollbar_, &QScrollBar::valueChanged, keyframe_view_->verticalScrollBar(), &QScrollBar::setValue);

  // TimeBasedWidget's scrollbar has extra functionality that we can take advantage of
  keyframe_view_->setHorizontalScrollBar(scrollbar());
  keyframe_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  connect(keyframe_view_->horizontalScrollBar(), &QScrollBar::valueChanged, ruler(), &TimeRuler::SetScroll);

  // Set a default scale - FIXME: Hardcoded
  SetScale(120);

  SetMaximumScale(TimelineViewBase::kMaximumScale);
}

void NodeParamView::SelectNodes(const QList<Node *> &nodes)
{
  foreach (Node* n, nodes) {
    NodeParamViewItem* item = new NodeParamViewItem(n);

    // Insert the widget before the stretch
    param_layout_->insertWidget(param_layout_->count() - 1, item);

    connect(item, &NodeParamViewItem::KeyframeAdded, keyframe_view_, &KeyframeView::AddKeyframe);
    connect(item, &NodeParamViewItem::KeyframeRemoved, keyframe_view_, &KeyframeView::RemoveKeyframe);
    connect(item, &NodeParamViewItem::RequestSetTime, this, &NodeParamView::ItemRequestedTimeChanged);
    connect(item, &NodeParamViewItem::InputDoubleClicked, this, &NodeParamView::InputDoubleClicked);
    connect(item, &NodeParamViewItem::RequestSelectNode, this, &NodeParamView::RequestSelectNode);

    items_.insert(n, item);
  }

  UpdateItemTime(GetTimestamp());

  // Re-arrange keyframes
  QMetaObject::invokeMethod(this, "PlaceKeyframesOnView", Qt::QueuedConnection);
}

void NodeParamView::DeselectNodes(const QList<Node *> &nodes)
{
  // Remove item from map and delete the widget
  foreach (Node* n, nodes) {
    // Remove all keyframes from this node
    keyframe_view_->RemoveKeyframesOfNode(n);

    delete items_.take(n);
  }

  // Re-arrange keyframes
  QMetaObject::invokeMethod(this, "PlaceKeyframesOnView", Qt::QueuedConnection);
}

void NodeParamView::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);

  vertical_scrollbar_->setPageStep(vertical_scrollbar_->height());
}

void NodeParamView::ScaleChangedEvent(const double &scale)
{
  TimeBasedWidget::ScaleChangedEvent(scale);

  keyframe_view_->SetScale(scale);
}

void NodeParamView::TimebaseChangedEvent(const rational &timebase)
{
  TimeBasedWidget::TimebaseChangedEvent(timebase);

  keyframe_view_->SetTimebase(timebase);

  UpdateItemTime(GetTimestamp());
}

void NodeParamView::TimeChangedEvent(const int64_t &timestamp)
{
  TimeBasedWidget::TimeChangedEvent(timestamp);

  keyframe_view_->SetTime(timestamp);

  UpdateItemTime(timestamp);
}

void NodeParamView::ConnectedNodeChanged(ViewerOutput *n)
{
  // Set viewer as a time target
  keyframe_view_->SetTimeTarget(n);

  foreach (NodeParamViewItem* item, items_) {
    item->SetTimeTarget(n);
  }
}

Node *NodeParamView::GetTimeTarget() const
{
  return keyframe_view_->GetTimeTarget();
}

void NodeParamView::DeleteSelected()
{
  keyframe_view_->DeleteSelected();
}

void NodeParamView::UpdateItemTime(const int64_t &timestamp)
{
  rational time = Timecode::timestamp_to_time(timestamp, timebase());

  foreach (NodeParamViewItem* item, items_) {
    item->SetTime(time);
  }
}

void NodeParamView::ItemRequestedTimeChanged(const rational &time)
{
  SetTimeAndSignal(Timecode::time_to_timestamp(time, keyframe_view_->timebase()));
}

void NodeParamView::ForceKeyframeViewToScroll()
{
  keyframe_view_->SetMaxScroll(param_widget_area_->height() - ruler()->height());
}

void NodeParamView::PlaceKeyframesOnView()
{
  foreach (NodeParamViewItem* item, items_) {
    QMetaObject::invokeMethod(item, "SignalAllKeyframes", Qt::QueuedConnection);
  }
}

OLIVE_NAMESPACE_EXIT
