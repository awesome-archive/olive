/***

    Olive - Non-Linear Video Editor
    Copyright (C) 2019  Olive Team

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

#include "renderthread.h"

#include <QApplication>
#include <QImage>
#include <QOpenGLFunctions>
#include <QDateTime>
#include <QDebug>
#ifdef OLIVE_OCIO
#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;
#endif

#include "rendering/renderfunctions.h"
#include "project/sequence.h"

RenderThread::RenderThread()
{
  gizmos = nullptr;
  share_ctx = nullptr;
  ctx = nullptr;
  blend_mode_program = nullptr;
  premultiply_program = nullptr;
  seq = nullptr;
  tex_width = -1;
  tex_height = -1;
  queued = false;
  texture_failed = false;
  running = true;

  surface.create();
}

RenderThread::~RenderThread() {
  surface.destroy();
}

void RenderThread::run() {
  wait_lock_.lock();

  while (running) {
    if (!queued) {
      wait_cond_.wait(&wait_lock_);
    }
    if (!running) {
      break;
    }
    queued = false;

    if (share_ctx != nullptr) {
      if (ctx != nullptr) {
        ctx->makeCurrent(&surface);

        // cache texture size
        tex_width = seq->width;
        tex_height = seq->height;

        // gen fbo
        if (!front_buffer_1.is_created()) {
          front_buffer_1.create(ctx, tex_width, tex_height);
        }
        if (!front_buffer_2.is_created()) {
          front_buffer_2.create(ctx, tex_width, tex_height);
        }
        if (!back_buffer_1.is_created()) {
          back_buffer_1.create(ctx, tex_width, tex_height);
        }
        if (!back_buffer_2.is_created()) {
          back_buffer_2.create(ctx, tex_width, tex_height);
        }

        if (blend_mode_program == nullptr) {
          // create shader program to make blending modes work
          delete_shader_program();

          blend_mode_program = new QOpenGLShaderProgram();
          blend_mode_program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/internalshaders/common.vert");
          blend_mode_program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/internalshaders/blending.frag");
          blend_mode_program->link();

          premultiply_program = new QOpenGLShaderProgram();
          premultiply_program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/internalshaders/common.vert");
          premultiply_program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/internalshaders/premultiply.frag");
          premultiply_program->link();
        }

        // draw frame
        paint();

        front_buffer_switcher = !front_buffer_switcher;

        emit ready();
      }
    }
  }

  delete_ctx();

  wait_lock_.unlock();
}

QMutex *RenderThread::get_texture_mutex()
{
  // return the mutex for the opposite texture being drawn to by the renderer
  return front_buffer_switcher ? &front_mutex2 : &front_mutex1;
}

GLuint RenderThread::get_texture()
{
  // return the opposite texture to the texture being drawn to by the renderer
  return front_buffer_switcher ? front_buffer_2.texture() : front_buffer_1.texture();
}

void RenderThread::set_up_ocio()
{
}

void RenderThread::paint() {
  // set up compose_sequence() parameters
  ComposeSequenceParams params;
  params.viewer = nullptr;
  params.ctx = ctx;
  params.seq = seq;
  params.video = true;
  params.texture_failed = false;
  params.gizmos = &gizmos;
  params.wait_for_mutexes = true;
  params.playback_speed = 1;
  params.blend_mode_program = blend_mode_program;
  params.premultiply_program = premultiply_program;
  params.backend_buffer1 = back_buffer_1.buffer();
  params.backend_buffer2 = back_buffer_2.buffer();
  params.backend_attachment1 = back_buffer_1.texture();
  params.backend_attachment2 = back_buffer_2.texture();
  params.main_buffer = front_buffer_switcher ? front_buffer_1.buffer() : front_buffer_2.buffer();
  params.main_attachment = front_buffer_switcher ? front_buffer_1.texture() : front_buffer_2.texture();

  QMutex& active_mutex = front_buffer_switcher ? front_mutex1 : front_mutex2;
  active_mutex.lock();

  // bind framebuffer for drawing
  ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, params.main_buffer);

  glLoadIdentity();

  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glEnable(GL_DEPTH);

  gizmos = nullptr;

  compose_sequence(params);

  // flush changes
  ctx->functions()->glFinish();

  texture_failed = params.texture_failed;

  active_mutex.unlock();

  if (!save_fn.isEmpty()) {
    if (texture_failed) {
      // texture failed, try again
      queued = true;
    } else {
      ctx->functions()->glBindFramebuffer(GL_READ_FRAMEBUFFER, params.main_buffer);
      QImage img(tex_width, tex_height, QImage::Format_RGBA8888);
      glReadPixels(0, 0, tex_width, tex_height, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
      img.save(save_fn);
      ctx->functions()->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
      save_fn = "";
    }
  }

  if (pixel_buffer != nullptr) {

    // set main framebuffer to the current read buffer
    ctx->functions()->glBindFramebuffer(GL_READ_FRAMEBUFFER, params.main_buffer);

    // store pixels in buffer
    glReadPixels(0,
                 0,
                 pixel_buffer_linesize == 0 ? tex_width : pixel_buffer_linesize,
                 tex_height,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 pixel_buffer);

    // release current read buffer
    ctx->functions()->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    pixel_buffer = nullptr;
  }

  glDisable(GL_DEPTH);
  glDisable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);

  // release
  ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void RenderThread::start_render(QOpenGLContext *share, SequencePtr s, const QString& save, GLvoid* pixels, int pixel_linesize, int idivider) {
  Q_UNUSED(idivider);

  seq = s;

  // stall any dependent actions
  texture_failed = true;

  if (share != nullptr && (ctx == nullptr || ctx->shareContext() != share_ctx)) {
    share_ctx = share;

    // clean any previous context
    delete_ctx();

    // create new context
    ctx = new QOpenGLContext();
    ctx->setFormat(share_ctx->format());
    ctx->setShareContext(share_ctx);
    ctx->create();
    ctx->moveToThread(this);
  }

  save_fn = save;
  pixel_buffer = pixels;
  pixel_buffer_linesize = pixel_linesize;

  queued = true;

  wait_cond_.wakeAll();
}

bool RenderThread::did_texture_fail() {
  return texture_failed;
}

void RenderThread::cancel() {
  running = false;
  wait_cond_.wakeAll();
  wait();
}

void RenderThread::delete_buffers() {
  front_buffer_1.destroy();
  front_buffer_2.destroy();
  back_buffer_1.destroy();
  back_buffer_2.destroy();
}

void RenderThread::delete_shader_program() {
  delete blend_mode_program;
  blend_mode_program = nullptr;

  delete premultiply_program;
  premultiply_program = nullptr;
}

void RenderThread::delete_ctx() {
  delete_shader_program();
  delete_buffers();

  delete ctx;
  ctx = nullptr;
}

GLFboWrapper::GLFboWrapper() :
  ctx_(nullptr)
{
  buffer_ = 0;
  texture_ = 0;
}

GLFboWrapper::~GLFboWrapper() {
  destroy();
}

bool GLFboWrapper::is_created()
{
  return ctx_ != nullptr;
}

void GLFboWrapper::create(QOpenGLContext* ctx, int width, int height) {
  destroy();

  ctx_ = ctx;

  // generate framebuffer
  ctx_->functions()->glGenFramebuffers(1, &buffer_);

  // generate texture attachment
  ctx_->functions()->glGenTextures(1, &texture_);

  // bind framebuffer to attach texture
  ctx_->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, buffer_);

  // bind texture
  ctx_->functions()->glBindTexture(GL_TEXTURE_2D, texture_);

  // set texture filtering to bilinear
  ctx_->functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  ctx_->functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // allocate texture storage
  ctx_->functions()->glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
        );

  // attach texture to framebuffer
  ctx_->functions()->glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0
        );

  // release texture
  ctx_->functions()->glBindTexture(GL_TEXTURE_2D, 0);

  // release framebuffer
  ctx_->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void GLFboWrapper::destroy() {
  if (ctx_ != nullptr) {
    ctx_->functions()->glDeleteTextures(1, &texture_);
    ctx_->functions()->glDeleteFramebuffers(1, &buffer_);
  }

  ctx_ = nullptr;
}

const GLuint &GLFboWrapper::buffer()
{
  return buffer_;
}

const GLuint &GLFboWrapper::texture()
{
  return texture_;
}
