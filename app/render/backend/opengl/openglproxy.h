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

#ifndef OPENGLPROXY_H
#define OPENGLPROXY_H

#include <QOffscreenSurface>
#include <QOpenGLContext>

#include "common/timerange.h"
#include "node/value.h"
#include "openglcolorprocessor.h"
#include "openglframebuffer.h"
#include "opengltexturecache.h"
#include "render/shaderinfo.h"

OLIVE_NAMESPACE_ENTER

class OpenGLProxy : public QObject
{
  Q_OBJECT
public:
  OpenGLProxy(QObject* parent = nullptr);

  virtual ~OpenGLProxy() override;

  static void CreateInstance();

  static void DestroyInstance();

  static OpenGLProxy* instance()
  {
    return instance_;
  }

  /**
   * @brief Initialize OpenGL instance in whatever thread this object is a part of
   *
   * This function creates a context (shared with share_ctx provided in the constructor) as well as various other
   * OpenGL thread-specific objects necessary for rendering. This function should only ever be called from the main
   * thread (i.e. the thread where share_ctx is current on) but AFTER this object has been pushed to its thread with
   * moveToThread(). If this function is called from a different thread, it could fail or even segfault on some
   * platforms.
   *
   * The reason this function must be called in the main thread (rather than initializing asynchronously in a separate
   * thread) is because different platforms have different rules about creating a share context with a context that
   * is still "current" in another thread. While some implementations do allow this, Windows OpenGL (wgl) explicitly
   * forbids it and other platforms/drivers will segfault attempting it. While we can obviously call "doneCurrent", I
   * haven't found any reliable way to prevent the main thread from making it current again before initialization is
   * complete other than blocking it entirely.
   *
   * To get around this, we create all share contexts in the main thread and then move them to the other thread
   * afterwards (which is completely legal). While annoying, this gets around the issue listed above by both preventing
   * the main thread from using the context during initialization and preventing more than one shared context being made
   * at the same time (which may or may not actually make a difference).
   */
  bool Init();

  void Close();

public slots:
  QVariant RunNodeAccelerated(const OLIVE_NAMESPACE::Node *node,
                              const OLIVE_NAMESPACE::TimeRange &range,
                              const OLIVE_NAMESPACE::ShaderJob &job,
                              const OLIVE_NAMESPACE::VideoParams &params);

  void TextureToBuffer(const QVariant& texture,
                       OLIVE_NAMESPACE::FramePtr frame,
                       const QMatrix4x4& matrix);

  QVariant FrameToValue(OLIVE_NAMESPACE::FramePtr frame,
                        OLIVE_NAMESPACE::StreamPtr stream,
                        const OLIVE_NAMESPACE::VideoParams &params,
                        const OLIVE_NAMESPACE::RenderMode::Mode &mode);

  QVariant PreCachedFrameToValue(OLIVE_NAMESPACE::FramePtr frame);

private:
  OpenGLShaderPtr ResolveShaderFromCache(const Node* node, const QString &shader_id);

  QOpenGLContext* ctx_;
  QOffscreenSurface surface_;

  QOpenGLFunctions* functions_;

  OpenGLFramebuffer buffer_;

  OpenGLColorProcessorCache color_cache_;

  OpenGLShaderPtr copy_pipeline_;

  QHash<QString, OpenGLShaderPtr> shader_cache_;

  OpenGLTextureCache texture_cache_;

  static OpenGLProxy* instance_;

private slots:
  void FinishInit();

};

OLIVE_NAMESPACE_EXIT

#endif // OPENGLPROXY_H
