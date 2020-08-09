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

#include "clip.h"

OLIVE_NAMESPACE_ENTER

ClipBlock::ClipBlock()
{
  texture_input_ = new NodeInput("buffer_in", NodeInput::kBuffer);
  texture_input_->set_is_keyframable(false);
  AddInput(texture_input_);
}

Node *ClipBlock::copy() const
{
  return new ClipBlock();
}

Block::Type ClipBlock::type() const
{
  return kClip;
}

QString ClipBlock::Name() const
{
  return tr("Clip");
}

QString ClipBlock::id() const
{
  return QStringLiteral("org.olivevideoeditor.Olive.clip");
}

QString ClipBlock::Description() const
{
  return tr("A time-based node that represents a media source.");
}

NodeInput *ClipBlock::texture_input() const
{
  return texture_input_;
}

void ClipBlock::InvalidateCache(const TimeRange &range, NodeInput *from, NodeInput *source)
{
  // If signal is from texture input, transform all times from media time to sequence time
  if (from == texture_input_) {
    // Adjust range from media time to sequence time
    rational start = MediaToSequenceTime(range.in());
    rational end = MediaToSequenceTime(range.out());

    Block::InvalidateCache(TimeRange(start, end), from, source);
  } else {
    // Otherwise, pass signal along normally
    Block::InvalidateCache(range, from, source);
  }
}

TimeRange ClipBlock::InputTimeAdjustment(NodeInput *input, const TimeRange &input_time) const
{
  if (input == texture_input_) {
    return TimeRange(SequenceToMediaTime(input_time.in()), SequenceToMediaTime(input_time.out()));
  }

  return Block::InputTimeAdjustment(input, input_time);
}

TimeRange ClipBlock::OutputTimeAdjustment(NodeInput *input, const TimeRange &input_time) const
{
  if (input == texture_input_) {
    return TimeRange(MediaToSequenceTime(input_time.in()), MediaToSequenceTime(input_time.out()));
  }

  return Block::InputTimeAdjustment(input, input_time);
}

NodeValueTable ClipBlock::Value(NodeValueDatabase &value) const
{
  // We discard most values here except for the buffer we received
  NodeValue data = value[texture_input()].GetWithMeta(NodeParam::kBuffer);

  NodeValueTable table;
  if (data.type() != NodeParam::kNone) {
    table.Push(data);
  }
  return table;
}

void ClipBlock::Retranslate()
{
  Block::Retranslate();

  texture_input_->set_name(tr("Buffer"));
}

void ClipBlock::Hash(QCryptographicHash &hash, const rational &time) const
{
  if (texture_input_->is_connected()) {
    rational t = InputTimeAdjustment(texture_input_, TimeRange(time, time)).in();

    texture_input_->get_connected_node()->Hash(hash, t);
  }
}

OLIVE_NAMESPACE_EXIT
