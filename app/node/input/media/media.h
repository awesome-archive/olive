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

#ifndef MEDIAINPUT_H
#define MEDIAINPUT_H

#include "codec/decoder.h"
#include "node/node.h"

OLIVE_NAMESPACE_ENTER

/**
 * @brief A node that imports an image
 */
class MediaInput : public Node
{
  Q_OBJECT
public:
  MediaInput();

  virtual QList<CategoryID> Category() const override;

  StreamPtr footage();
  void SetFootage(StreamPtr f);

  virtual void Retranslate() override;

  virtual NodeValueTable Value(NodeValueDatabase& value) const override;

protected:
  NodeInput* footage_input_;

  StreamPtr connected_footage_;

private slots:
  void FootageChanged();

  void FootageParametersChanged();

};

OLIVE_NAMESPACE_EXIT

#endif // MEDIAINPUT_H
