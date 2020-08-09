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

#ifndef SOLIDGENERATOR_H
#define SOLIDGENERATOR_H

#include "node/node.h"

OLIVE_NAMESPACE_ENTER

class SolidGenerator : public Node
{
public:
  SolidGenerator();

  virtual Node* copy() const override;

  virtual QString Name() const override;
  virtual QString id() const override;
  virtual QList<CategoryID> Category() const override;
  virtual QString Description() const override;

  virtual void Retranslate() override;

  virtual NodeValueTable Value(NodeValueDatabase &value) const override;
  virtual ShaderCode GetShaderCode(const QString &shader_id) const override;

private:
  NodeInput* color_input_;

};

OLIVE_NAMESPACE_EXIT

#endif // SOLIDGENERATOR_H
