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

#ifndef AUDIOOUTPUTDEVICEPROXY_H
#define AUDIOOUTPUTDEVICEPROXY_H

#include <QFile>

#include "common/define.h"
#include "tempoprocessor.h"

OLIVE_NAMESPACE_ENTER

/**
 * @brief QIODevice wrapper that can adjust speed/reverse an audio file
 */
class AudioOutputDeviceProxy : public QIODevice
{
  Q_OBJECT
public:
  AudioOutputDeviceProxy() = default;

  virtual ~AudioOutputDeviceProxy() override;

  void SetParameters(const AudioParams& params);

  void SetDevice(const QString &filename, qint64 offset, int playback_speed);

  virtual void close() override;

protected:
  virtual qint64 readData(char *data, qint64 maxlen) override;

  virtual qint64 writeData(const char *data, qint64 maxSize) override;

private:
  qint64 ReverseAwareRead(char* data, qint64 maxlen);

  QFile file_;

  TempoProcessor tempo_processor_;

  AudioParams params_;

  int playback_speed_;

};

OLIVE_NAMESPACE_EXIT

#endif // AUDIOOUTPUTDEVICEPROXY_H
