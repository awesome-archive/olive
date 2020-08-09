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

#include "project.h"

#include <QDir>
#include <QFileInfo>

#include "common/xmlutils.h"
#include "core.h"
#include "dialog/progress/progress.h"
#include "render/diskmanager.h"
#include "window/mainwindow/mainwindow.h"

OLIVE_NAMESPACE_ENTER

Project::Project() :
  is_modified_(false),
  autorecovery_saved_(true)
{
  root_.set_project(this);
}

void Project::Load(QXmlStreamReader *reader, MainWindowLayoutInfo* layout, const QAtomicInt* cancelled)
{
  XMLNodeData xml_node_data;

  // Set project filename (hacky)
  xml_node_data.real_project_url = static_cast<QFile*>(reader->device())->fileName();

  while (XMLReadNextStartElement(reader)) {
    if (reader->name() == QStringLiteral("folder")) {

      // Assume this folder is our root
      root_.Load(reader, xml_node_data, cancelled);

    } else if (reader->name() == QStringLiteral("colormanagement")) {

      // Read color management info
      while (XMLReadNextStartElement(reader)) {
        if (reader->name() == QStringLiteral("config")) {
          color_manager_.SetConfig(reader->readElementText());
        } else if (reader->name() == QStringLiteral("default")) {
          color_manager_.SetDefaultInputColorSpace(reader->readElementText());
        } else {
          reader->skipCurrentElement();
        }
      }

    } else if (reader->name() == QStringLiteral("cachepath")) {

      set_cache_path(reader->readElementText());

    } else if (reader->name() == QStringLiteral("layout")) {

      // Since the main window's functions have to occur in the GUI thread (and we're likely
      // loading in a secondary thread), we load all necessary data into a separate struct so we
      // can continue loading and queue it with the main window so it can handle the data
      // appropriately in its own thread.

      *layout = MainWindowLayoutInfo::fromXml(reader, xml_node_data);

    } else if (reader->name() == QStringLiteral("url")) {

      // This should be read in before most other elements
      xml_node_data.saved_project_url = reader->readElementText();

    } else {

      // Skip this
      reader->skipCurrentElement();

    }
  }

  foreach (const XMLNodeData::FootageConnection& con, xml_node_data.footage_connections) {
    if (con.footage) {
      con.input->set_standard_value(QVariant::fromValue(xml_node_data.footage_ptrs.value(con.footage)));
    }
  }
}

void Project::Save(QXmlStreamWriter *writer) const
{
  writer->writeStartElement("project");

  writer->writeTextElement("url", filename_);

  writer->writeTextElement("cachepath", cache_path(false));

  root_.Save(writer);

  writer->writeStartElement("colormanagement");

  writer->writeTextElement("config", color_manager_.GetConfigFilename());

  writer->writeTextElement("default", color_manager_.GetDefaultInputColorSpace());

  writer->writeEndElement(); // colormanagement

  // Save main window project layout
  MainWindowLayoutInfo main_window_info = Core::instance()->main_window()->SaveLayout();
  main_window_info.toXml(writer);

  writer->writeEndElement(); // project
}

Folder *Project::root()
{
  return &root_;
}

QString Project::name() const
{
  if (filename_.isEmpty()) {
    return tr("(untitled)");
  } else {
    return QFileInfo(filename_).completeBaseName();
  }
}

const QString &Project::filename() const
{
  return filename_;
}

QString Project::pretty_filename() const
{
  QString fn = filename();

  if (fn.isEmpty()) {
    return tr("(untitled)");
  } else {
    return fn;
  }
}

void Project::set_filename(const QString &s)
{
  filename_ = s;

#ifdef Q_OS_WINDOWS
  // Prevents filenames
  filename_.replace('/', '\\');
#endif

  emit NameChanged();
}

ColorManager *Project::color_manager()
{
  return &color_manager_;
}

QList<ItemPtr> Project::get_items_of_type(Item::Type type) const
{
  return root_.get_children_of_type(type, true);
}

bool Project::is_modified() const
{
  return is_modified_;
}

void Project::set_modified(bool e)
{
  is_modified_ = e;
  autorecovery_saved_ = !e;

  emit ModifiedChanged(is_modified_);
}

bool Project::has_autorecovery_been_saved() const
{
  return autorecovery_saved_;
}

void Project::set_autorecovery_saved(bool e)
{
  autorecovery_saved_ = e;
}

bool Project::is_new() const
{
  return !is_modified_ && filename_.isEmpty();
}

const QString &Project::cache_path(bool default_if_empty) const
{
  if (cache_path_.isEmpty() && default_if_empty) {
    return DiskManager::instance()->GetDefaultCachePath();
  }
  return cache_path_;
}

OLIVE_NAMESPACE_EXIT
