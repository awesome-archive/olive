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

#include "node.h"

#include <QApplication>
#include <QDebug>
#include <QFile>

#include "common/xmlutils.h"
#include "project/project.h"
#include "project/item/footage/footage.h"
#include "project/item/footage/imagestream.h"

OLIVE_NAMESPACE_ENTER

Node::Node() :
  can_be_deleted_(true)
{
  output_ = new NodeOutput("node_out");
  AddParameter(output_);
}

Node::~Node()
{
  DisconnectAll();

  // We delete in the Node destructor rather than relying on the QObject system because the parameter may need to
  // perform actions on this Node object and we want them to be done before the Node object is fully destroyed
  foreach (NodeParam* param, params_) {

    // We disconnect input signals because these will try to send invalidate cache signals that may involve the derived
    // class (which is now destroyed). Any node that this is connected to will handle cache invalidation so it's a waste
    // of time anyway.
    if (param->type() == NodeParam::kInput) {
      DisconnectInput(static_cast<NodeInput*>(param));
    }

    delete param;
  }
}

void Node::Load(QXmlStreamReader *reader, XMLNodeData& xml_node_data, const QAtomicInt* cancelled)
{
  while (XMLReadNextStartElement(reader)) {
    if (cancelled && *cancelled) {
      return;
    }

    if (reader->name() == QStringLiteral("input") || reader->name() == QStringLiteral("output")) {
      QString param_id;

      XMLAttributeLoop(reader, attr) {
        if (attr.name() == QStringLiteral("id")) {
          param_id = attr.value().toString();

          break;
        }
      }

      if (param_id.isEmpty()) {
        qDebug() << "Found parameter with no ID";
        continue;
      }

      NodeParam* param;

      if (reader->name() == QStringLiteral("input")) {
        param = GetInputWithID(param_id);
      } else {
        param = GetOutputWithID(param_id);
      }

      if (!param) {
        qDebug() << "No parameter in" << id() << "with parameter" << param_id;
        continue;
      }

      param->Load(reader, xml_node_data, cancelled);
    } else {
      LoadInternal(reader, xml_node_data);
    }
  }
}

void Node::Save(QXmlStreamWriter *writer, const QString &custom_name) const
{
  writer->writeStartElement(custom_name.isEmpty() ? QStringLiteral("node") : custom_name);

  writer->writeAttribute(QStringLiteral("id"), id());

  writer->writeAttribute(QStringLiteral("ptr"), QString::number(reinterpret_cast<quintptr>(this)));

  writer->writeAttribute(QStringLiteral("pos"),
                         QStringLiteral("%1:%2").arg(QString::number(GetPosition().x()),
                                                     QString::number(GetPosition().y())));

  writer->writeAttribute(QStringLiteral("label"),
                         GetLabel());

  foreach (NodeParam* param, parameters()) {
    param->Save(writer);
  }

  SaveInternal(writer);

  writer->writeEndElement(); // node
}

QString Node::ShortName() const
{
  return Name();
}

QString Node::Description() const
{
  // Return an empty string by default
  return QString();
}

void Node::Retranslate()
{
}

void Node::AddParameter(NodeParam *param)
{
  // Ensure no other param with this ID has been added to this Node (since that defeats the purpose)
  Q_ASSERT(!HasParamWithID(param->id()));

  if (params_.contains(param)) {
    return;
  }

  param->setParent(this);

  // Keep main output as the last parameter, assume if there are no parameters that this is the output parameter
  if (params_.isEmpty()) {
    params_.append(param);
  } else {
    params_.insert(params_.size()-1, param);
  }

  connect(param, &NodeParam::EdgeAdded, this, &Node::EdgeAdded);
  connect(param, &NodeParam::EdgeRemoved, this, &Node::EdgeRemoved);

  if (param->type() == NodeParam::kInput) {
    ConnectInput(static_cast<NodeInput*>(param));
  }
}

NodeValueTable Node::Value(NodeValueDatabase &value) const
{
  return value.Merge();
}

void Node::InvalidateCache(const TimeRange &range, NodeInput *from, NodeInput *source)
{
  Q_UNUSED(from)

  SendInvalidateCache(range, source);
}

void Node::BeginOperation()
{
  foreach (NodeParam* param, params_) {
    if (param->type() == NodeParam::kOutput) {
      foreach (NodeEdgePtr edge, param->edges()) {
        edge->input()->parentNode()->BeginOperation();
      }
    }
  }
}

void Node::EndOperation()
{
  foreach (NodeParam* param, params_) {
    if (param->type() == NodeParam::kOutput) {
      foreach (NodeEdgePtr edge, param->edges()) {
        edge->input()->parentNode()->EndOperation();
      }
    }
  }
}

TimeRange Node::InputTimeAdjustment(NodeInput *, const TimeRange &input_time) const
{
  // Default behavior is no time adjustment at all
  return input_time;
}

TimeRange Node::OutputTimeAdjustment(NodeInput *, const TimeRange &input_time) const
{
  // Default behavior is no time adjustment at all
  return input_time;
}

void Node::SendInvalidateCache(const TimeRange &range, NodeInput *source)
{
  // Loop through all parameters (there should be no children that are not NodeParams)
  foreach (NodeParam* param, params_) {
    // If the Node is an output, relay the signal to any Nodes that are connected to it
    if (param->type() == NodeParam::kOutput) {
      foreach (NodeEdgePtr edge, param->edges()) {
        NodeInput* connected_input = edge->input();
        Node* connected_node = connected_input->parentNode();

        // Send clear cache signal to the Node
        connected_node->InvalidateCache(range, connected_input, source);
      }
    }
  }
}

void Node::LoadInternal(QXmlStreamReader *reader, XMLNodeData &)
{
  reader->skipCurrentElement();
}

void Node::SaveInternal(QXmlStreamWriter *) const
{
}

QList<NodeInput *> Node::GetInputsToHash() const
{
  return GetInputsIncludingArrays();
}

QString Node::ReadFileAsString(const QString &filename)
{
  QFile f(filename);
  QString file_data;
  if (f.open(QFile::ReadOnly | QFile::Text)) {
    QTextStream text_stream(&f);
    file_data = text_stream.readAll();
    f.close();
  }
  return file_data;
}

void GetInputsIncludingArraysInternal(NodeInputArray* array, QList<NodeInput *>& list)
{
  foreach (NodeInput* input, array->sub_params()) {
    list.append(input);

    if (input->IsArray()) {
      GetInputsIncludingArraysInternal(static_cast<NodeInputArray*>(input), list);
    }
  }
}

QList<NodeInput *> Node::GetInputsIncludingArrays() const
{
  QList<NodeInput *> inputs;

  foreach (NodeParam* param, params_) {
    if (param->type() == NodeParam::kInput) {
      NodeInput* input = static_cast<NodeInput*>(param);

      inputs.append(input);

      if (input->IsArray()) {
        GetInputsIncludingArraysInternal(static_cast<NodeInputArray*>(input), inputs);
      }
    }
  }

  return inputs;
}

QList<NodeOutput *> Node::GetOutputs() const
{
  // The current design only uses one output per node. This function returns a list just in case that changes.
  return {output_};
}

bool Node::HasGizmos() const
{
  return false;
}

void Node::DrawGizmos(NodeValueDatabase &, QPainter *, const QVector2D &, const QSize &) const
{
}

bool Node::GizmoPress(NodeValueDatabase &, const QPointF &, const QVector2D &, const QSize &)
{
  return false;
}

void Node::GizmoMove(const QPointF &, const QVector2D &, const rational &)
{
}

void Node::GizmoRelease()
{
}

const QString &Node::GetLabel() const
{
  return label_;
}

void Node::SetLabel(const QString &s)
{
  if (label_ != s) {
    label_ = s;

    emit LabelChanged(label_);
  }
}

void Node::Hash(QCryptographicHash &hash, const rational& time) const
{
  // Add this Node's ID
  hash.addData(id().toUtf8());

  QList<NodeInput*> inputs = GetInputsToHash();

  foreach (NodeInput* input, inputs) {
    // For each input, try to hash its value

    // Get time adjustment
    // For a single frame, we only care about one of the times
    rational input_time = InputTimeAdjustment(input, TimeRange(time, time)).in();

    if (input->is_connected()) {
      // Traverse down this edge
      input->get_connected_node()->Hash(hash, input_time);
    } else {
      // Grab the value at this time
      QVariant value = input->get_value_at_time(input_time);
      hash.addData(NodeParam::ValueToBytes(input->data_type(), value));
    }

    // We have one exception for FOOTAGE types, since we resolve the footage into a frame in the renderer
    if (input->data_type() == NodeParam::kFootage) {
      StreamPtr stream = input->get_standard_value().value<StreamPtr>();

      if (stream) {
        // Add footage details to hash

        // Footage filename
        hash.addData(stream->footage()->filename().toUtf8());

        // Footage last modified date
        hash.addData(stream->footage()->timestamp().toString().toUtf8());

        // Footage stream
        hash.addData(QString::number(stream->index()).toUtf8());

        if (stream->type() == Stream::kImage || stream->type() == Stream::kVideo) {
          ImageStreamPtr image_stream = std::static_pointer_cast<ImageStream>(stream);

          // Current color config and space
          hash.addData(image_stream->footage()->project()->color_manager()->GetConfigFilename().toUtf8());
          hash.addData(image_stream->colorspace().toUtf8());

          // Alpha associated setting
          hash.addData(QString::number(image_stream->premultiplied_alpha()).toUtf8());
        }

        // Footage timestamp
        if (stream->type() == Stream::kVideo) {
          hash.addData(QStringLiteral("%1/%2").arg(QString::number(input_time.numerator()),
                                                   QString::number(input_time.denominator())).toUtf8());

          hash.addData(QString::number(static_cast<VideoStream*>(stream.get())->start_time()).toUtf8());

        }
      }
    }
  }
}

void Node::CopyInputs(Node *source, Node *destination, bool include_connections)
{
  Q_ASSERT(source->id() == destination->id());

  const QList<NodeParam*>& src_param = source->params_;
  const QList<NodeParam*>& dst_param = destination->params_;

  for (int i=0;i<src_param.size();i++) {
    NodeParam* p = src_param.at(i);

    if (p->type() == NodeParam::kInput) {
      NodeInput* src = static_cast<NodeInput*>(p);

      NodeInput* dst = static_cast<NodeInput*>(dst_param.at(i));

      NodeInput::CopyValues(src, dst, include_connections);
    }
  }

  destination->SetPosition(source->GetPosition());
  destination->SetLabel(source->GetLabel());
}

bool Node::CanBeDeleted() const
{
  return can_be_deleted_;
}

void Node::SetCanBeDeleted(bool s)
{
  can_be_deleted_ = s;
}

bool Node::IsBlock() const
{
  return false;
}

bool Node::IsTrack() const
{
  return false;
}

const QList<NodeParam *>& Node::parameters() const
{
  return params_;
}

int Node::IndexOfParameter(NodeParam *param) const
{
  return params_.indexOf(param);
}

/**
 * @brief Recursively collects dependencies of Node `n` and appends them to QList `list`
 *
 * @param traverse
 *
 * TRUE to recursively traverse each node for a complete dependency graph. FALSE to return only the immediate
 * dependencies.
 */
QList<Node*> Node::GetDependenciesInternal(bool traverse, bool exclusive_only) const {
  QList<NodeInput*> inputs = GetInputsIncludingArrays();
  QList<Node*> list;

  foreach (NodeInput* i, inputs) {
    i->GetDependencies(list, traverse, exclusive_only);
  }

  return list;
}

QList<Node *> Node::GetDependencies() const
{
  return GetDependenciesInternal(true, false);
}

QList<Node *> Node::GetExclusiveDependencies() const
{
  return GetDependenciesInternal(true, true);
}

QList<Node *> Node::GetImmediateDependencies() const
{
  return GetDependenciesInternal(false, false);
}

ShaderCode Node::GetShaderCode(const QString &shader_id) const
{
  Q_UNUSED(shader_id)

  return ShaderCode(QString(), QString());
}

void Node::ProcessSamples(NodeValueDatabase &, const SampleBufferPtr, SampleBufferPtr, int) const
{
}

void Node::GenerateFrame(FramePtr frame, const GenerateJob &job) const
{
  Q_UNUSED(frame)
  Q_UNUSED(job)
}

NodeInput *Node::GetInputWithID(const QString &id) const
{
  QList<NodeInput*> inputs = GetInputsIncludingArrays();

  foreach (NodeInput* i, inputs) {
    if (i->id() == id) {
      return i;
    }
  }

  return nullptr;
}

NodeOutput *Node::GetOutputWithID(const QString &id) const
{
  foreach (NodeParam* p, params_) {
    if (p->type() == NodeParam::kOutput
        && p->id() == id) {
      return static_cast<NodeOutput*>(p);
    }
  }

  return nullptr;
}

bool Node::OutputsTo(Node *n, bool recursively) const
{
  QList<NodeOutput*> outputs = GetOutputs();

  foreach (NodeOutput* output, outputs) {
    foreach (NodeEdgePtr edge, output->edges()) {
      Node* connected = edge->input()->parentNode();

      if (connected == n) {
        return true;
      } else if (recursively && connected->OutputsTo(n, recursively)) {
        return true;
      }
    }
  }

  return false;
}

bool Node::OutputsTo(const QString &id, bool recursively) const
{
  QList<NodeOutput*> outputs = GetOutputs();

  foreach (NodeOutput* output, outputs) {
    foreach (NodeEdgePtr edge, output->edges()) {
      Node* connected = edge->input()->parentNode();

      if (connected->id() == id) {
        return true;
      } else if (recursively && connected->OutputsTo(id, recursively)) {
        return true;
      }
    }
  }

  return false;
}

bool Node::OutputsTo(NodeInput *input, bool recursively, bool include_arrays) const
{
  QList<NodeOutput*> outputs = GetOutputs();

  foreach (NodeOutput* output, outputs) {
    foreach (NodeEdgePtr edge, output->edges()) {
      NodeInput* connected = edge->input();

      if (connected == input) {
        return true;
      } else if (include_arrays && input->IsArray()
                 && static_cast<NodeInputArray*>(input)->sub_params().contains(connected)) {
        return true;
      } else if (recursively && connected->parentNode()->OutputsTo(input, recursively, include_arrays)) {
        return true;
      }
    }
  }

  return false;
}

bool Node::InputsFrom(Node *n, bool recursively) const
{
  QList<NodeInput*> inputs = GetInputsIncludingArrays();

  foreach (NodeInput* input, inputs) {
    foreach (NodeEdgePtr edge, input->edges()) {
      Node* connected = edge->output()->parentNode();

      if (connected == n) {
        return true;
      } else if (recursively && connected->InputsFrom(n, recursively)) {
        return true;
      }
    }
  }

  return false;
}

bool Node::InputsFrom(const QString &id, bool recursively) const
{
  QList<NodeInput*> inputs = GetInputsIncludingArrays();

  foreach (NodeInput* input, inputs) {
    foreach (NodeEdgePtr edge, input->edges()) {
      Node* connected = edge->output()->parentNode();

      if (connected->id() == id) {
        return true;
      } else if (recursively && connected->InputsFrom(id, recursively)) {
        return true;
      }
    }
  }

  return false;
}

int Node::GetRoutesTo(Node *n) const
{
  bool outputs_directly = false;
  int routes = 0;

  QList<NodeOutput*> outputs = GetOutputs();

  foreach (NodeOutput* o, outputs) {
    foreach (NodeEdgePtr edge, o->edges()) {
      Node* connected_node = edge->input()->parentNode();

      if (connected_node == n) {
        outputs_directly = true;
      } else {
        routes += connected_node->GetRoutesTo(n);
      }
    }
  }

  if (outputs_directly) {
    routes++;
  }

  return routes;
}

bool Node::HasInputs() const
{
  return HasParamOfType(NodeParam::kInput, false);
}

bool Node::HasOutputs() const
{
  return HasParamOfType(NodeParam::kOutput, false);
}

bool Node::HasConnectedInputs() const
{
  return HasParamOfType(NodeParam::kInput, true);
}

bool Node::HasConnectedOutputs() const
{
  return HasParamOfType(NodeParam::kOutput, true);
}

void Node::DisconnectAll()
{
  foreach (NodeParam* param, params_) {
    param->DisconnectAll();
  }
}

QString Node::GetCategoryName(const CategoryID &c)
{
  switch (c) {
  case kCategoryInput:
    return tr("Input");
  case kCategoryOutput:
    return tr("Output");
  case kCategoryGeneral:
    return tr("General");
  case kCategoryMath:
    return tr("Math");
  case kCategoryColor:
    return tr("Color");
  case kCategoryFilter:
    return tr("Filter");
  case kCategoryTimeline:
    return tr("Timeline");
  case kCategoryGenerator:
    return tr("Generator");
  case kCategoryChannels:
    return tr("Channel");
  case kCategoryTransition:
    return tr("Transition");
  case kCategoryUnknown:
  case kCategoryCount:
    break;
  }

  return tr("Uncategorized");
}

QList<TimeRange> Node::TransformTimeTo(const TimeRange &time, Node *target, NodeParam::Type direction)
{
  QList<TimeRange> paths_found;

  if (direction == NodeParam::kInput) {
    // Get list of all inputs
    QList<NodeInput *> inputs = GetInputsIncludingArrays();

    // If this input is connected, traverse it to see if we stumble across the specified `node`
    foreach (NodeInput* input, inputs) {
      if (input->is_connected()) {
        TimeRange input_adjustment = InputTimeAdjustment(input, time);
        Node* connected = input->get_connected_node();

        if (connected == target) {
          // We found the target, no need to keep traversing
          if (!paths_found.contains(input_adjustment)) {
            paths_found.append(input_adjustment);
          }
        } else {
          // We did NOT find the target, traverse this
          paths_found.append(connected->TransformTimeTo(input_adjustment, target, direction));
        }
      }
    }
  } else {
    // Get list of all outputs
    QList<NodeOutput*> outputs = GetOutputs();

    // If this input is connected, traverse it to see if we stumble across the specified `node`
    foreach (NodeOutput* output, outputs) {
      if (output->is_connected()) {
        foreach (NodeEdgePtr edge, output->edges()) {
          Node* input_node = edge->input()->parentNode();

          TimeRange output_adjustment = input_node->OutputTimeAdjustment(edge->input(), time);

          if (input_node == target) {
            paths_found.append(output_adjustment);
          } else {
            paths_found.append(input_node->TransformTimeTo(output_adjustment, target, direction));
          }
        }
      }
    }
  }

  return paths_found;
}

QVariant Node::PtrToValue(void *ptr)
{
  return reinterpret_cast<quintptr>(ptr);
}

bool Node::HasParamWithID(const QString &id) const
{
  foreach (NodeParam* p, params_)
  {
    if (p->id() == id)
    {
      return true;
    }
  }

  return false;
}

NodeOutput *Node::output() const
{
  return output_;
}

const QPointF &Node::GetPosition() const
{
  return position_;
}

void Node::SetPosition(const QPointF &pos)
{
  position_ = pos;

  emit PositionChanged(position_);
}

void Node::AddInput(NodeInput *input)
{
  AddParameter(input);
}

bool Node::HasParamOfType(NodeParam::Type type, bool must_be_connected) const
{
  foreach (NodeParam* p, params_) {
    if (p->type() == type
        && (p->is_connected() || !must_be_connected)) {
      return true;
    }
  }

  return false;
}

void Node::ConnectInput(NodeInput *input)
{
  connect(input, &NodeInput::ValueChanged, this, &Node::InputChanged);
  connect(input, &NodeInput::EdgeAdded, this, &Node::InputConnectionChanged);
  connect(input, &NodeInput::EdgeRemoved, this, &Node::InputConnectionChanged);

  if (input->IsArray()) {
    NodeInputArray* array = static_cast<NodeInputArray*>(input);

    connect(array, &NodeInputArray::SubParamEdgeAdded, this, &Node::InputConnectionChanged);
    connect(array, &NodeInputArray::SubParamEdgeRemoved, this, &Node::InputConnectionChanged);
    connect(array, &NodeInputArray::SubParamEdgeAdded, this, &Node::EdgeAdded);
    connect(array, &NodeInputArray::SubParamEdgeRemoved, this, &Node::EdgeRemoved);
  }
}

void Node::DisconnectInput(NodeInput *input)
{
  if (input->IsArray()) {
    NodeInputArray* array = static_cast<NodeInputArray*>(input);

    disconnect(array, &NodeInputArray::SubParamEdgeAdded, this, &Node::InputConnectionChanged);
    disconnect(array, &NodeInputArray::SubParamEdgeRemoved, this, &Node::InputConnectionChanged);
    disconnect(array, &NodeInputArray::SubParamEdgeAdded, this, &Node::EdgeAdded);
    disconnect(array, &NodeInputArray::SubParamEdgeRemoved, this, &Node::EdgeRemoved);
  }

  disconnect(input, &NodeInput::ValueChanged, this, &Node::InputChanged);
  disconnect(input, &NodeInput::EdgeAdded, this, &Node::InputConnectionChanged);
  disconnect(input, &NodeInput::EdgeRemoved, this, &Node::InputConnectionChanged);
}

void Node::InputChanged(const TimeRange& range)
{
  InvalidateCache(range, static_cast<NodeInput*>(sender()), static_cast<NodeInput*>(sender()));
}

void Node::InputConnectionChanged(NodeEdgePtr edge)
{
  InvalidateCache(TimeRange(RATIONAL_MIN, RATIONAL_MAX), edge->input(), edge->input());
}

OLIVE_NAMESPACE_EXIT
