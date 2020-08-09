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

#include "viewerdisplay.h"

#include <OpenImageIO/imagebuf.h>
#include <QFileInfo>
#include <QMessageBox>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QPainter>

#include "common/define.h"
#include "common/functiontimer.h"
#include "gizmotraverser.h"
#include "render/backend/opengl/openglrenderfunctions.h"
#include "render/backend/opengl/openglshader.h"
#include "render/pixelformat.h"

OLIVE_NAMESPACE_ENTER

ViewerDisplayWidget::ViewerDisplayWidget(QWidget *parent) :
  ManagedDisplayWidget(parent),
  signal_cursor_color_(false),
  gizmos_(nullptr),
  gizmo_click_(false),
  last_loaded_buffer_(nullptr)
{
}

ViewerDisplayWidget::~ViewerDisplayWidget()
{
  ContextCleanup();
}

void ViewerDisplayWidget::SetMatrix(const QMatrix4x4 &mat)
{
  matrix_ = mat;
  update();
}

void ViewerDisplayWidget::SetSignalCursorColorEnabled(bool e)
{
  signal_cursor_color_ = e;
  setMouseTracking(e);
}

void ViewerDisplayWidget::SetImage(FramePtr in_buffer)
{
  last_loaded_buffer_ = in_buffer;

  if (last_loaded_buffer_) {
    makeCurrent();

    if (!texture_.IsCreated()
        || texture_.width() != in_buffer->width()
        || texture_.height() != in_buffer->height()
        || texture_.format() != in_buffer->format()) {
      texture_.Create(context(), in_buffer->video_params(), in_buffer->data(), in_buffer->linesize_pixels());
    } else {
      texture_.Upload(in_buffer);
    }

    doneCurrent();
  }

  update();
}

const ViewerSafeMarginInfo &ViewerDisplayWidget::GetSafeMargin() const
{
  return safe_margin_;
}

void ViewerDisplayWidget::SetSafeMargins(const ViewerSafeMarginInfo &safe_margin)
{
  if (safe_margin_ != safe_margin) {
    safe_margin_ = safe_margin;

    update();
  }
}

void ViewerDisplayWidget::SetGizmos(Node *node)
{
  if (gizmos_ != node) {
    gizmos_ = node;

    update();
  }
}

void ViewerDisplayWidget::SetVideoParams(const VideoParams &params)
{
  gizmo_params_ = params;

  if (gizmos_) {
    update();
  }
}

void ViewerDisplayWidget::SetTime(const rational &time)
{
  time_ = time;

  if (gizmos_) {
    update();
  }
}

FramePtr ViewerDisplayWidget::last_loaded_buffer() const
{
  return last_loaded_buffer_;
}

void ViewerDisplayWidget::mousePressEvent(QMouseEvent *event)
{
  if (gizmos_
      && gizmos_->GizmoPress(gizmo_db_, event->pos(), QVector2D(GetTexturePosition(size())), size())) {
    gizmo_click_ = true;
    gizmo_drag_time_ = GetGizmoTime();
    return;
  }

  QOpenGLWidget::mousePressEvent(event);

  if (event->button() == Qt::LeftButton) {
    emit DragStarted();
  }
}

void ViewerDisplayWidget::mouseMoveEvent(QMouseEvent *event)
{
  if (gizmo_click_) {
    gizmos_->GizmoMove(event->pos(), QVector2D(GetTexturePosition(size())), gizmo_drag_time_);
    return;
  }

  QOpenGLWidget::mouseMoveEvent(event);

  if (signal_cursor_color_) {
    Color reference, display;

    if (last_loaded_buffer_) {
      QVector3D pixel_pos(static_cast<float>(event->x()) / static_cast<float>(width()) * 2.0f - 1.0f,
                          static_cast<float>(event->y()) / static_cast<float>(height()) * 2.0f - 1.0f,
                          0);

      pixel_pos = pixel_pos * matrix_.inverted();

      int frame_x = qRound((pixel_pos.x() + 1.0f) * 0.5f * last_loaded_buffer_->width());
      int frame_y = qRound((pixel_pos.y() + 1.0f) * 0.5f * last_loaded_buffer_->height());

      reference = last_loaded_buffer_->get_pixel(frame_x, frame_y);
      display = color_service()->ConvertColor(reference);
    }

    emit CursorColor(reference, display);
  }
}

void ViewerDisplayWidget::mouseReleaseEvent(QMouseEvent *event)
{
  if (gizmo_click_) {
    gizmos_->GizmoRelease();
    gizmo_click_ = false;

    return;
  }

  QOpenGLWidget::mouseReleaseEvent(event);
}

void ViewerDisplayWidget::initializeGL()
{
  ManagedDisplayWidget::initializeGL();

  connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &ViewerDisplayWidget::ContextCleanup, Qt::DirectConnection);
}

void ViewerDisplayWidget::paintGL()
{
  // Get functions attached to this context (they will already be initialized)
  QOpenGLFunctions* f = context()->functions();

  // Clear background to empty
  f->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  f->glClear(GL_COLOR_BUFFER_BIT);

  // We only draw if we have a pipeline
  if (last_loaded_buffer_ && color_service()) {

    // Bind retrieved texture
    f->glBindTexture(GL_TEXTURE_2D, texture_.texture());

    // Blit using the color service
    color_service()->ProcessOpenGL(true, matrix_);

    // Release retrieved texture
    f->glBindTexture(GL_TEXTURE_2D, 0);

  }

  // Draw gizmos if we have any
  if (gizmos_) {
    GizmoTraverser gt(QSize(gizmo_params_.width(), gizmo_params_.height()));

    rational node_time = GetGizmoTime();

    gizmo_db_ = gt.GenerateDatabase(gizmos_, TimeRange(node_time, node_time));

    QPainter p(this);
    gizmos_->DrawGizmos(gizmo_db_, &p, QVector2D(GetTexturePosition(size())), size());
  }

  // Draw action/title safe areas
  if (safe_margin_.is_enabled()) {
    QPainter p(this);
    p.setPen(Qt::lightGray);
    p.setBrush(Qt::NoBrush);

    int x = 0, y = 0, w = width(), h = height();

    if (safe_margin_.custom_ratio()) {
      double widget_ar = static_cast<double>(width()) / static_cast<double>(height());

      if (widget_ar > safe_margin_.ratio()) {
        // Widget is wider than margins
        w = h * safe_margin_.ratio();
        x = width() / 2 - w / 2;
      } else {
        h = w / safe_margin_.ratio();
        y = height() / 2 - h / 2;
      }
    }

    p.drawRect(w / 20 + x, h / 20 + y, w / 10 * 9, h / 10 * 9);
    p.drawRect(w / 10 + x, h / 10 + y, w / 10 * 8, h / 10 * 8);

    int cross = qMin(w, h) / 32;

    QLine lines[] = {QLine(rect().center().x() - cross, rect().center().y(),rect().center().x() + cross, rect().center().y()),
                    QLine(rect().center().x(), rect().center().y() - cross, rect().center().x(), rect().center().y() + cross)};

    p.drawLines(lines, 2);
  }
}

QPointF ViewerDisplayWidget::GetTexturePosition(const QPoint &screen_pos)
{
  return GetTexturePosition(screen_pos.x(), screen_pos.y());
}

QPointF ViewerDisplayWidget::GetTexturePosition(const QSize &size)
{
  return GetTexturePosition(size.width(), size.height());
}

QPointF ViewerDisplayWidget::GetTexturePosition(const double &x, const double &y)
{
  return QPointF(x / gizmo_params_.width(),
                 y / gizmo_params_.height());
}

rational ViewerDisplayWidget::GetGizmoTime()
{
  return GetAdjustedTime(GetTimeTarget(), gizmos_, time_, NodeParam::kInput);
}

void ViewerDisplayWidget::ContextCleanup()
{
  makeCurrent();

  texture_.Destroy();

  doneCurrent();
}

OLIVE_NAMESPACE_EXIT
