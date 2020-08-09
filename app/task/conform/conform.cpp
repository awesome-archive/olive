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

#include "conform.h"

#include "codec/decoder.h"

OLIVE_NAMESPACE_ENTER

ConformTask::ConformTask(AudioStreamPtr stream, const AudioParams& params) :
  stream_(stream),
  params_(params)
{
  SetTitle(tr("Conforming Audio %1:%2").arg(stream_->footage()->filename(), QString::number(stream_->index())));
}

bool ConformTask::Run()
{
  if (stream_->footage()->decoder().isEmpty()) {
    SetError(tr("Failed to find decoder to conform audio stream"));
    return false;
  } else {
    DecoderPtr decoder = Decoder::CreateFromID(stream_->footage()->decoder());

    decoder->set_stream(stream_);

    connect(decoder.get(), &Decoder::IndexProgress, this, &ConformTask::ProgressChanged);

    if (!decoder->ConformAudio(&IsCancelled(), params_)) {
      SetError(tr("Failed to conform audio"));
      return false;
    } else {
      return true;
    }
  }
}

OLIVE_NAMESPACE_EXIT
