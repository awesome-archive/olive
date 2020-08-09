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

#ifndef VIEWERGLWIDGET_H
#define VIEWERGLWIDGET_H

#include <QOpenGLWidget>

#include "node/node.h"
#include "render/backend/opengl/openglcolorprocessor.h"
#include "render/backend/opengl/openglframebuffer.h"
#include "render/backend/opengl/openglshader.h"
#include "render/backend/opengl/opengltexture.h"
#include "render/color.h"
#include "render/colormanager.h"
#include "viewersafemargininfo.h"
#include "widget/manageddisplay/manageddisplay.h"
#include "widget/timetarget/timetarget.h"

OLIVE_NAMESPACE_ENTER

/**
 * @brief The inner display/rendering widget of a Viewer class.
 *
 * Actual composition occurs elsewhere offscreen and
 * multithreaded, so its main purpose is receiving a finalized OpenGL texture and displaying it.
 *
 * The main entry point is SetTexture() which will receive an OpenGL texture ID, store it, and then call update() to
 * draw it on screen. The drawing function is in paintGL() (called during the update() process by Qt) and is fairly
 * simple OpenGL drawing code standardized around OpenGL ES 3.2 Core.
 *
 * If the texture has been modified and you're 100% sure this widget is using the same texture object, it's possible
 * to call update() directly to trigger a repaint, however this is not recommended. If you are not 100% sure it'll be
 * the same texture object, use SetTexture() since it will nearly always be faster to just set it than to check *and*
 * set it.
 */
class ViewerDisplayWidget : public ManagedDisplayWidget, public TimeTargetObject
{
  Q_OBJECT
public:
  /**
   * @brief ViewerGLWidget Constructor
   *
   * @param parent
   *
   * QWidget parent.
   */
  ViewerDisplayWidget(QWidget* parent = nullptr);

  virtual ~ViewerDisplayWidget() override;

  const QMatrix4x4& GetMatrix();

  const ViewerSafeMarginInfo& GetSafeMargin() const;
  void SetSafeMargins(const ViewerSafeMarginInfo& safe_margin);

  void SetGizmos(Node* node);
  void SetVideoParams(const VideoParams &params);
  void SetTime(const rational& time);

  FramePtr last_loaded_buffer() const;

public slots:
  /**
   * @brief Set the transformation matrix to draw with
   *
   * Set this if you want the drawing to pass through some sort of transform (most of the time you won't want this).
   */
  void SetMatrix(const QMatrix4x4& mat);

  /**
   * @brief Enables or disables whether this color at the cursor should be emitted
   *
   * Since tracking the mouse every movement, reading pixels, and doing color transforms are processor intensive, we
   * have an option for it. Ideally, this should be connected to a PixelSamplerPanel::visibilityChanged signal so that
   * it can automatically be enabled when the user is pixel sampling and disabled for optimization when they're not.
   */
  void SetSignalCursorColorEnabled(bool e);

  /**
   * @brief Overrides the image with the load buffer of another ViewerGLWidget
   *
   * If there are multiple ViewerGLWidgets showing the same thing, this is faster than decoding the image from file
   * each time.
   */
  void SetImage(FramePtr in_buffer);

signals:
  /**
   * @brief Signal emitted when the user starts dragging from the viewer
   */
  void DragStarted();

  /**
   * @brief Signal emitted when cursor color is enabled and the user's mouse position changes
   */
  void CursorColor(const Color& reference, const Color& display);

protected:
  /**
   * @brief Override the mouse press event for the DragStarted() signal and gizmos
   */
  virtual void mousePressEvent(QMouseEvent* event) override;

  /**
   * @brief Override mouse move to signal for the pixel sampler and gizmos
   */
  virtual void mouseMoveEvent(QMouseEvent* event) override;

  /**
   * @brief Override mouse release event for gizmos
   */
  virtual void mouseReleaseEvent(QMouseEvent* event) override;

  /**
   * @brief Initialize function to set up the OpenGL context upon its construction
   *
   * Currently primarily used to regenerate the pipeline shader used for drawing.
   */
  virtual void initializeGL() override;

  /**
   * @brief Paint function to display the texture (received in SetTexture()) on screen.
   *
   * Simple OpenGL drawing function for painting the texture on screen. Standardized around OpenGL ES 3.2 Core.
   */
  virtual void paintGL() override;

private:
  QPointF GetTexturePosition(const QPoint& screen_pos);
  QPointF GetTexturePosition(const QSize& size);
  QPointF GetTexturePosition(const double& x, const double& y);

  rational GetGizmoTime();

  /**
   * @brief Internal reference to the OpenGL texture to draw. Set in SetTexture() and used in paintGL().
   */
  OpenGLTexture texture_;

  /**
   * @brief Drawing matrix (defaults to identity)
   */
  QMatrix4x4 matrix_;

  bool signal_cursor_color_;

  ViewerSafeMarginInfo safe_margin_;

  Node* gizmos_;
  NodeValueDatabase gizmo_db_;
  rational gizmo_drag_time_;
  VideoParams gizmo_params_;
  bool gizmo_click_;

  rational time_;

  FramePtr last_loaded_buffer_;

private slots:
  /**
   * @brief Slot to connect just before the OpenGL context is destroyed to clean up resources
   */
  void ContextCleanup();

};

OLIVE_NAMESPACE_EXIT

#endif // VIEWERGLWIDGET_H
