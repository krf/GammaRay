/*
  methodargument.h

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2013 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef GAMMARAY_METHODARGUMENT_H
#define GAMMARAY_METHODARGUMENT_H

#include <QSharedDataPointer>
#include <QVariant>

namespace GammaRay {

class MethodArgumentPrivate;

/** Q[Generic]Argument that works on a QVariant, with some memory handling safety. */
class MethodArgument
{
  public:
    MethodArgument();
    explicit MethodArgument(const QVariant &v);
    MethodArgument(const MethodArgument &other);
    ~MethodArgument();

    MethodArgument& operator=(const MethodArgument &other);
    operator QGenericArgument() const;

  private:
    QExplicitlySharedDataPointer<MethodArgumentPrivate> d;
};

}

#endif // GAMMARAY_METHODARGUMENT_H
