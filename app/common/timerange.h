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

#ifndef TIMERANGE_H
#define TIMERANGE_H

#include "rational.h"

OLIVE_NAMESPACE_ENTER

class TimeRange {
public:
  TimeRange() = default;
  TimeRange(const rational& in, const rational& out);

  const rational& in() const;
  const rational& out() const;
  const rational& length() const;

  void set_in(const rational& in);
  void set_out(const rational& out);
  void set_range(const rational& in, const rational& out);

  bool operator==(const TimeRange& r) const;
  bool operator!=(const TimeRange& r) const;

  bool OverlapsWith(const TimeRange& a, bool in_inclusive = true, bool out_inclusive = true) const;
  bool Contains(const TimeRange& a, bool in_inclusive = true, bool out_inclusive = true) const;
  bool Contains(const rational& r) const;

  TimeRange Combined(const TimeRange& a) const;
  static TimeRange Combine(const TimeRange &a, const TimeRange &b);
  TimeRange Intersected(const TimeRange& a) const;
  static TimeRange Intersect(const TimeRange &a, const TimeRange &b);

  TimeRange operator+(const rational& rhs) const;
  TimeRange operator-(const rational& rhs) const;

  const TimeRange& operator+=(const rational &rhs);
  const TimeRange& operator-=(const rational &rhs);

private:
  void normalize();

  rational in_;
  rational out_;
  rational length_;

};

class TimeRangeList : public QList<TimeRange> {
public:
  TimeRangeList() = default;

  TimeRangeList(std::initializer_list<TimeRange> r) :
    QList<TimeRange>(r)
  {
  }

  void InsertTimeRange(const TimeRange& range);

  void RemoveTimeRange(const TimeRange& remove);

  bool ContainsTimeRange(const TimeRange& range, bool in_inclusive = true, bool out_inclusive = true) const;

  TimeRangeList Intersects(const TimeRange& range) const;

private:
  void PrintTimeList();

};

uint qHash(const TimeRange& r, uint seed);

OLIVE_NAMESPACE_EXIT

QDebug operator<<(QDebug debug, const OLIVE_NAMESPACE::TimeRange& r);

Q_DECLARE_METATYPE(OLIVE_NAMESPACE::TimeRange)

#endif // TIMERANGE_H
