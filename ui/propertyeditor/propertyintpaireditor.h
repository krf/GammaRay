/*
  propertyintpaireditor.h

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2011-2013 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#ifndef GAMMARAY_PROPERTYINTPAIREDITOR_H
#define GAMMARAY_PROPERTYINTPAIREDITOR_H

#include <QWidget>

class QPoint;

namespace GammaRay {

namespace Ui {
  class PropertyIntPairEditor;
}

/** Property editor for QPoint, QSize and anything else consisting of two integer values. */
class PropertyIntPairEditor : public QWidget
{
  Q_OBJECT
  public:
    explicit PropertyIntPairEditor(QWidget *parent = 0);

  protected:
    QScopedPointer<Ui::PropertyIntPairEditor> ui;
};

/** Property editor for points. Since QStyledItemDelegate ignore valuePropertyName and insists on
 * USER properties we need one class per type here...
 */
class PropertyPointEditor : public PropertyIntPairEditor
{
  Q_OBJECT
  Q_PROPERTY(QPoint point READ point WRITE setPoint USER true)
  public:
    explicit PropertyPointEditor(QWidget *parent = 0);

    QPoint point() const;
    void setPoint(const QPoint &point);
};

/** Same again for size. */
class PropertySizeEditor : public PropertyIntPairEditor
{
  Q_OBJECT
  Q_PROPERTY(QSize sizeValue READ sizeValue WRITE setSizeValue USER true)
  public:
    explicit PropertySizeEditor(QWidget *parent = 0);

    QSize sizeValue() const;
    void setSizeValue(const QSize &size);
};

}

#endif // GAMMARAY_PROPERTYINTPAIREDITOR_H
