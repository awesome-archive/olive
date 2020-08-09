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

#include "speedduration.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include "core.h"
#include "common/timecodefunctions.h"
#include "widget/nodeview/nodeviewundo.h"
#include "widget/timelinewidget/undo/undo.h"

OLIVE_NAMESPACE_ENTER

SpeedDurationDialog::SpeedDurationDialog(const rational& timebase, const QList<ClipBlock *> &clips, QWidget *parent) :
  QDialog(parent),
  clips_(clips),
  timebase_(timebase)
{
  setWindowTitle(tr("Speed/Duration"));

  QVBoxLayout* layout = new QVBoxLayout(this);

  {
    // Create groupbox for the speed/duration
    QGroupBox* speed_groupbox = new QGroupBox(tr("Speed/Duration"));
    layout->addWidget(speed_groupbox);
    QGridLayout* speed_layout = new QGridLayout(speed_groupbox);

    int row = 0;

    // For any other clips that are selected, determine if they share speeds and lengths. If they don't, the UI can't
    // show them all as having the same parameters
    bool same_speed = true;
    bool same_duration = true;
    bool all_reversed = true;

    for (int i=1;i<clips.size();i++) {
      ClipBlock* prev_clip = clips_.at(i-1);
      ClipBlock* this_clip = clips_.at(i);

      // Check if the speeds are different
      if (same_speed
          && qAbs(prev_clip->speed()) != qAbs(this_clip->speed())) {
        same_speed = false;
      }

      // Check if the durations are different
      if (same_duration
          && prev_clip->length() != this_clip->length()) {
        same_duration = false;
      }

      // Check if all are reversed
      if (all_reversed
          && prev_clip->is_reversed() != this_clip->is_reversed()) {
        all_reversed = false;
      }

      // If we've already determined both are different, no need to continue
      if (!same_speed
          && !same_duration
          && !all_reversed) {
        break;
      }
    }

    speed_layout->addWidget(new QLabel(tr("Speed:")), row, 0);

    // Create "Speed" slider
    speed_slider_ = new FloatSlider();
    speed_slider_->SetMinimum(0);
    speed_slider_->SetDisplayType(FloatSlider::kPercentage);
    speed_slider_->SetDefaultValue(1);
    speed_layout->addWidget(speed_slider_, row, 1);

    if (same_speed) {
      // All clips share the same speed so we can show the value
      speed_slider_->SetValue(qAbs(clips_.first()->speed().toDouble()));
    } else {
      // Else, we show an invalid initial state
      speed_slider_->SetTristate();
    }

    row++;

    speed_layout->addWidget(new QLabel(tr("Duration:")), row, 0);

    // Create "Duration" slider
    duration_slider_ = new TimeSlider();
    duration_slider_->SetTimebase(timebase_);
    duration_slider_->SetMinimum(1);
    speed_layout->addWidget(duration_slider_, row, 1);

    // Calculate duration that would occur if the speed was 100%
    duration_slider_->SetDefaultValue(GetUnadjustedLengthTimestamp(clips_.first()));

    if (same_duration) {
      duration_slider_->SetValue(Timecode::time_to_timestamp(clips_.first()->length(), timebase_));
    } else {
      duration_slider_->SetTristate();
    }

    row++;

    link_speed_and_duration_ = new QCheckBox(tr("Link Speed and Duration"));
    link_speed_and_duration_->setChecked(true);
    speed_layout->addWidget(link_speed_and_duration_, row, 0, 1, 2);

    // Pick up when the speed or duration slider changes so we can programmatically link them
    connect(speed_slider_, &FloatSlider::ValueChanged, this, &SpeedDurationDialog::SpeedChanged);
    connect(duration_slider_, &TimeSlider::ValueChanged, this, &SpeedDurationDialog::DurationChanged);

    reverse_speed_checkbox_ = new QCheckBox(tr("Reverse Speed"));
    if (all_reversed) {
      reverse_speed_checkbox_->setChecked(clips_.first()->is_reversed());
    } else {
      reverse_speed_checkbox_->setTristate();
    }
    layout->addWidget(reverse_speed_checkbox_);
  }

  maintain_audio_pitch_checkbox_ = new QCheckBox(tr("Maintain Audio Pitch"));
  layout->addWidget(maintain_audio_pitch_checkbox_);

  ripple_clips_checkbox_ = new QCheckBox(tr("Ripple Clips"));
  layout->addWidget(ripple_clips_checkbox_);

  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->setCenterButtons(true);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &SpeedDurationDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &SpeedDurationDialog::reject);
}

void SpeedDurationDialog::accept()
{
  QUndoCommand* command = new QUndoCommand();

  bool change_duration = !duration_slider_->IsTristate() || (!speed_slider_->IsTristate() && link_speed_and_duration_->isChecked());
  bool change_speed = !speed_slider_->IsTristate() || (!duration_slider_->IsTristate() && link_speed_and_duration_->isChecked());

  foreach (ClipBlock* clip, clips_) {
    double new_speed = speed_slider_->GetValue();

    if (change_duration) {
      // Change the duration
      int64_t current_duration = Timecode::time_to_timestamp(clip->length(), timebase_);
      int64_t new_duration = current_duration;

      // Check if we're getting the duration value directly from the slider or calculating it from the speed
      if (duration_slider_->IsTristate()) {
        // Calculate duration from speed
        new_duration = GetAdjustedDuration(clip, new_speed);
      } else {
        // Get duration directly from slider
        new_duration = duration_slider_->GetValue();

        // Check if we're calculating the speed from this duration
        if (speed_slider_->IsTristate() && change_speed) {
          // If we're here, the duration is going to override the speed
          new_speed = GetAdjustedSpeed(clip, new_duration);
        }
      }

      if (new_duration != current_duration) {
        // Calculate new clip length
        rational new_clip_length = Timecode::timestamp_to_time(new_duration, timebase_);

        if (ripple_clips_checkbox_->isChecked()) {

          // FIXME: Make this a REAL ripple...
          new BlockResizeCommand(clip, new_clip_length, command);

        } else {

          // If "ripple clips" isn't checked, we may be limited to how much we can change the length
          Block* next_block = clip->next();
          if (next_block) {
            if (new_clip_length > clip->length()) {
              if (next_block->type() == Block::kGap) {
                // Check if next clip is a gap, and if so we can take it all up
                new_clip_length = qMin(next_block->out(), clip->in() + new_clip_length);
              } else {
                // Otherwise we can't extend any further
                new_clip_length = clip->length();
              }
            }
          }

          if (new_clip_length != clip->length()) {
            new BlockTrimCommand(TrackOutput::TrackFromBlock(clip), clip, new_clip_length, Timeline::kTrimOut, command);
          }

        }
      }
    }

    if (change_speed) {
      rational new_block_speed = rational::fromDouble(new_speed);

      if (clip->is_reversed()) {
        new_block_speed = -new_block_speed;
      }

      // Change the speed
      new BlockSetSpeedCommand(clip, new_block_speed, command);
    }

    if (!reverse_speed_checkbox_->isTristate()
        && clip->is_reversed() != reverse_speed_checkbox_->isChecked()) {
      new BlockReverseCommand(clip, command);
    }
  }

  Core::instance()->undo_stack()->pushIfHasChildren(command);

  QDialog::accept();
}

double SpeedDurationDialog::GetUnadjustedLengthTimestamp(ClipBlock *clip) const
{
  double duration = static_cast<double>(Timecode::time_to_timestamp(clip->length(), timebase_));

  // Convert duration to non-speed adjusted duration
  duration *= qAbs(clip->speed().toDouble());

  return duration;
}

int64_t SpeedDurationDialog::GetAdjustedDuration(ClipBlock *clip, const double &new_speed) const
{
  double duration = GetUnadjustedLengthTimestamp(clip);

  // Re-adjust by new speed
  duration /= new_speed;

  // Return rounded time
  return qRound64(duration);
}

double SpeedDurationDialog::GetAdjustedSpeed(ClipBlock *clip, const int64_t &new_duration) const
{
  double duration = GetUnadjustedLengthTimestamp(clip);

  // Create a fraction of the original duration over the new duration
  duration /= static_cast<double>(new_duration);

  return duration;
}

void SpeedDurationDialog::SpeedChanged()
{
  if (link_speed_and_duration_->isChecked()) {
    double new_speed = speed_slider_->GetValue();

    if (qIsNull(new_speed)) {
      // A speed of 0 is considered a still frame. Since we can't divide by zero and a still frame could be any length,
      // we don't bother updating the
      return;
    }

    bool same_durations = true;
    int64_t new_duration = GetAdjustedDuration(clips_.first(), new_speed);

    for (int i=1;i<clips_.size();i++) {
      // Calculate what this clip's duration will be
      int64_t this_clip_duration = GetAdjustedDuration(clips_.at(i), new_speed);

      // If they won't be the same, we won't show the value
      if (this_clip_duration != new_duration) {
        same_durations = false;
        break;
      }
    }

    if (same_durations) {
      duration_slider_->SetValue(new_duration);
    } else {
      duration_slider_->SetTristate();
    }
  }
}

void SpeedDurationDialog::DurationChanged()
{
  if (link_speed_and_duration_->isChecked()) {
    int64_t new_duration = duration_slider_->GetValue();

    bool same_speeds = true;
    double new_speed = GetAdjustedSpeed(clips_.first(), new_duration);

    for (int i=1;i<clips_.size();i++) {
      double this_clip_speed = GetAdjustedSpeed(clips_.at(i), new_duration);

      if (!qFuzzyCompare(this_clip_speed, new_speed)) {
        same_speeds = false;
        break;
      }
    }

    if (same_speeds) {
      speed_slider_->SetValue(new_speed);
    } else {
      speed_slider_->SetTristate();
    }
  }
}

BlockReverseCommand::BlockReverseCommand(Block *block, QUndoCommand *parent) :
  UndoCommand(parent),
  block_(block)
{
}

Project *BlockReverseCommand::GetRelevantProject() const
{
  return static_cast<Sequence*>(block_->parent())->project();
}

void BlockReverseCommand::redo_internal()
{
  block_->set_media_in(block_->media_out());
  block_->set_speed(-block_->speed());
}

void BlockReverseCommand::undo_internal()
{
  redo_internal();
}

OLIVE_NAMESPACE_EXIT
