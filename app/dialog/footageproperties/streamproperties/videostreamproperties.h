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

#ifndef VIDEOSTREAMPROPERTIES_H
#define VIDEOSTREAMPROPERTIES_H

#include <QCheckBox>
#include <QComboBox>

#include "project/item/footage/videostream.h"
#include "streamproperties.h"
#include "undo/undocommand.h"
#include "widget/slider/integerslider.h"
#include "widget/standardcombos/standardcombos.h"

OLIVE_NAMESPACE_ENTER

class VideoStreamProperties : public StreamProperties
{
public:
  VideoStreamProperties(ImageStreamPtr stream);

  virtual void Accept(QUndoCommand* parent) override;

  virtual bool SanityCheck() override;

private:
  static bool IsImageSequence(ImageStream* stream);

  /**
   * @brief Attached video stream
   */
  ImageStreamPtr stream_;

  /**
   * @brief Setting for associated/premultiplied alpha
   */
  QCheckBox* video_premultiply_alpha_;

  /**
   * @brief Setting for this media's color space
   */
  QComboBox* video_color_space_;

  /**
   * @brief Setting for video interlacing
   */
  InterlacedComboBox* video_interlace_combo_;

  /**
   * @brief Sets the start index for image sequences
   */
  IntegerSlider* imgseq_start_time_;

  /**
   * @brief Sets the end index for image sequences
   */
  IntegerSlider* imgseq_end_time_;

  /**
   * @brief Sets the pixel aspect ratio of the stream
   */
  PixelAspectRatioComboBox* pixel_aspect_combo_;

  class VideoStreamChangeCommand : public UndoCommand {
  public:
    VideoStreamChangeCommand(ImageStreamPtr stream,
                             bool premultiplied,
                             QString colorspace,
                             VideoParams::Interlacing interlacing,
                             const rational& pixel_ar,
                             QUndoCommand* parent = nullptr);

    virtual Project* GetRelevantProject() const override;

  protected:
    virtual void redo_internal() override;
    virtual void undo_internal() override;

  private:
    ImageStreamPtr stream_;

    bool new_premultiplied_;
    QString new_colorspace_;
    VideoParams::Interlacing new_interlacing_;
    rational new_pixel_ar_;

    bool old_premultiplied_;
    QString old_colorspace_;
    VideoParams::Interlacing old_interlacing_;
    rational old_pixel_ar_;

  };

  class ImageSequenceChangeCommand : public UndoCommand {
  public:
    ImageSequenceChangeCommand(VideoStreamPtr video_stream,
                               int64_t start_index,
                               int64_t duration,
                               QUndoCommand* parent = nullptr);

    virtual Project* GetRelevantProject() const override;

  protected:
    virtual void redo_internal() override;
    virtual void undo_internal() override;

  private:
    VideoStreamPtr video_stream_;

    int64_t new_start_index_;
    int64_t old_start_index_;

    int64_t new_duration_;
    int64_t old_duration_;

  };

};

OLIVE_NAMESPACE_EXIT

#endif // VIDEOSTREAMPROPERTIES_H
