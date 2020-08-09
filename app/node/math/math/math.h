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

#ifndef MATHNODE_H
#define MATHNODE_H

#include "mathbase.h"

OLIVE_NAMESPACE_ENTER

class MathNode : public MathNodeBase
{
public:
  MathNode();

  virtual Node* copy() const override;

  virtual QString Name() const override;
  virtual QString id() const override;
  virtual QList<CategoryID> Category() const override;
  virtual QString Description() const override;

  virtual void Retranslate() override;

  virtual ShaderCode GetShaderCode(const QString &shader_id) const override;

  Operation GetOperation() const
  {
    return static_cast<Operation>(method_in_->get_standard_value().toInt());
  }

  void SetOperation(Operation o)
  {
    method_in_->set_standard_value(o);
  }

  NodeInput* param_a_in() const
  {
    return param_a_in_;
  }

  NodeInput* param_b_in() const
  {
    return param_b_in_;
  }

  virtual NodeValueTable Value(NodeValueDatabase &value) const override;

  virtual void ProcessSamples(NodeValueDatabase &values, const SampleBufferPtr input, SampleBufferPtr output, int index) const override;

private:
  NodeInput* method_in_;

  NodeInput* param_a_in_;

  NodeInput* param_b_in_;

};

OLIVE_NAMESPACE_EXIT

#endif // MATHNODE_H
