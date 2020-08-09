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

#ifndef GIZMOTRAVERSER_H
#define GIZMOTRAVERSER_H

#include "node/traverser.h"

OLIVE_NAMESPACE_ENTER

class GizmoTraverser : public NodeTraverser
{
public:
  GizmoTraverser(const QSize& sequence_resolution) :
    size_(sequence_resolution)
  {
  }

protected:
  virtual QVariant ProcessVideoFootage(StreamPtr stream, const rational &input_time);

  virtual QVariant ProcessShader(const Node *node, const TimeRange &range, const ShaderJob& job);

  // FIXME: Do something about audio?

private:
  QSize size_;

};

OLIVE_NAMESPACE_EXIT

#endif // GIZMOTRAVERSER_H
