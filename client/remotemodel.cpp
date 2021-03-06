/*
  remotemodel.cpp

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

#include "remotemodel.h"
#include "client.h"

#include <network/message.h>

#include <QDataStream>
#include <QDebug>

using namespace GammaRay;

RemoteModel::Node::~Node()
{
  qDeleteAll(children);
}


RemoteModel::RemoteModel(const QString &serverObject, QObject *parent) :
  QAbstractItemModel(parent),
  m_serverObject(serverObject),
  m_myAddress(Protocol::InvalidObjectAddress),
  m_currentSyncBarrier(0),
  m_targetSyncBarrier(0)
{
  m_root = new Node;

  m_myAddress = Client::instance()->objectAddress(serverObject);
  connect(Client::instance(), SIGNAL(objectRegistered(QString,Protocol::ObjectAddress)), SLOT(serverRegistered(QString,Protocol::ObjectAddress)));
  connect(Client::instance(), SIGNAL(objectUnregistered(QString,Protocol::ObjectAddress)), SLOT(serverUnregistered(QString,Protocol::ObjectAddress)));

  connectToServer();
}

RemoteModel::~RemoteModel()
{
  delete m_root;
}

bool RemoteModel::isConnected() const
{
  return m_myAddress != Protocol::InvalidObjectAddress;
}

QModelIndex RemoteModel::index(int row, int column, const QModelIndex &parent) const
{
  if (!isConnected() || row < 0 || column < 0)
    return QModelIndex();

//   qDebug() << row << column << parent << rowCount(parent);
  Node *parentNode = nodeForIndex(parent);
  Q_ASSERT(parentNode->children.size() >= parentNode->rowCount);
  if (parentNode->rowCount <= row || parentNode->columnCount <= column)
    return QModelIndex();
  return createIndex(row, column, parentNode->children.at(row));
}

QModelIndex RemoteModel::parent(const QModelIndex &index) const
{
  Node *currentNode = nodeForIndex(index);
  Q_ASSERT(currentNode);
  if (currentNode == m_root || currentNode->parent == m_root)
    return QModelIndex();
  Q_ASSERT(currentNode->parent && currentNode->parent->parent);
  Q_ASSERT(currentNode->parent->children.contains(currentNode));
  Q_ASSERT(currentNode->parent->parent->children.contains(currentNode->parent));
  return createIndex(currentNode->parent->parent->children.indexOf(currentNode->parent), 0, currentNode->parent);
}

int RemoteModel::rowCount(const QModelIndex &index) const
{
  if (!isConnected() || index.column() > 0)
    return 0;

  Node* node = nodeForIndex(index);
  Q_ASSERT(node);
  if (node->rowCount < 0) {
    if (node->columnCount < 0) // not yet requested vs. in the middle of insertion
      requestRowColumnCount(index);
    return 0;
  }
  return node->rowCount;
}

int RemoteModel::columnCount(const QModelIndex &index) const
{
  if (!isConnected())
    return 0;

  Node* node = nodeForIndex(index);
  Q_ASSERT(node);
  if (node->columnCount < 0) {
    requestRowColumnCount(index);
    return 0;
  }
  return node->columnCount;
}

QVariant RemoteModel::data(const QModelIndex &index, int role) const
{
  if (!isConnected() || !index.isValid())
    return QVariant();

  Node* node = nodeForIndex(index);
  Q_ASSERT(node);

  if (node->data.contains(index.column())) {
    if (node->data.value(index.column()).contains(role))
      return node->data.value(index.column()).value(role);
    else if (role == Qt::DisplayRole)
      return tr("Loading...");
    return QVariant();
  }

  requestDataAndFlags(index);
  if (role == Qt::DisplayRole)
    return tr("Loading...");
  return QVariant();
}

bool RemoteModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (!isConnected())
    return false;

  Message msg(m_myAddress, Protocol::ModelSetDataRequest);
  msg.payload() << Protocol::fromQModelIndex(index) << role << value;
  Client::send(msg);
  return false;
}

Qt::ItemFlags RemoteModel::flags(const QModelIndex& index) const
{
  Node* node = nodeForIndex(index);
  Q_ASSERT(node);

  return node->flags.value(index.column());
}

QVariant RemoteModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (!isConnected())
    return QVariant();

  if (!m_headers.contains(orientation) || !m_headers.value(orientation).contains(section))
    requestHeaderData(orientation, section);

  return m_headers.value(orientation).value(section).value(role);
}

void RemoteModel::newMessage(const GammaRay::Message& msg)
{
  if (!checkSyncBarrier(msg))
    return;

  switch (msg.type()) {
    case Protocol::ModelRowColumnCountReply:
    {
      Protocol::ModelIndex index;
      msg.payload() >> index;
      Node *node = nodeForIndex(index);
      Q_ASSERT(node->rowCount == -2 && node->columnCount == -1);
      int rowCount, columnCount;
      msg.payload() >> rowCount >> columnCount;
      Q_ASSERT(rowCount >= 0 && columnCount >= 0);

      if (!rowCount || !columnCount)
        break;

      const QModelIndex qmi = modelIndexForNode(node, 0);
      qDebug() << "insert" << this << index << qmi << rowCount << columnCount;
      beginInsertColumns(qmi, 0, columnCount - 1);
      node->columnCount = columnCount;
      endInsertColumns();

      beginInsertRows(qmi, 0, rowCount - 1);
      node->children.reserve(rowCount);
      for (int i = 0; i < rowCount; ++i) {
        Node *child = new Node;
        child->parent = node;
        node->children.push_back(child);
      }
      node->rowCount = rowCount;
      endInsertRows();
      break;
    }

    case Protocol::ModelContentReply:
    {
      Protocol::ModelIndex index;
      msg.payload() >> index;
      Node *node = nodeForIndex(index);
      Q_ASSERT(node);
      typedef QMap<int, QVariant> ItemData;
      ItemData itemData;
      qint32 flags;
      msg.payload() >> itemData >> flags;
      for (ItemData::const_iterator it = itemData.constBegin(); it != itemData.constEnd(); ++it) {
        node->data[index.last().second].insert(it.key(), it.value());
      }
      node->flags[index.last().second] = static_cast<Qt::ItemFlags>(flags);
      const QModelIndex qmi = modelIndexForNode(node, index.last().second);
      emit dataChanged(qmi, qmi);
      break;
    }

    case Protocol::ModelHeaderReply:
    {
      qint8 orientation;
      qint32 section;
      QHash<qint32, QVariant> data;
      msg.payload() >> orientation >> section >> data;
      Q_ASSERT(orientation == Qt::Horizontal || orientation == Qt::Vertical);
      Q_ASSERT(section >= 0);
      m_headers[static_cast<Qt::Orientation>(orientation)][section] = data;
      if ((orientation == Qt::Horizontal && m_root->columnCount > section) || (orientation == Qt::Vertical && m_root->rowCount > section))
        emit headerDataChanged(static_cast<Qt::Orientation>(orientation), section, section);
      break;
    }

    case Protocol::ModelContentChanged:
    {
      Protocol::ModelIndex beginIndex, endIndex;
      msg.payload() >> beginIndex >> endIndex;
      Node *node = nodeForIndex(beginIndex);
      if (node == m_root)
        break;

      Q_ASSERT(beginIndex.last().first <= endIndex.last().first);
      Q_ASSERT(beginIndex.last().second <= endIndex.last().second);

      // reset content for refetching
      for (int row = beginIndex.last().first; row <= endIndex.last().first; ++row) {
        Node *currentRow = node->parent->children.at(row);
        for (int col = beginIndex.last().second; col <= endIndex.last().second; ++col) {
          currentRow->data.remove(col);
          currentRow->flags.remove(col);
        }
      }

      const QModelIndex qmiBegin = modelIndexForNode(node, beginIndex.last().second);
      const QModelIndex qmiEnd = qmiBegin.sibling(endIndex.last().first, endIndex.last().second);

      emit dataChanged(qmiBegin, qmiEnd);
      break;
    }

    case Protocol::ModelHeaderChanged:
    {
      qint8 ori;
      int first, last;
      msg.payload() >> ori >> first >> last;
      const Qt::Orientation orientation = static_cast<Qt::Orientation>(ori);

      for (int i = first; i < last; ++i)
        m_headers[orientation].remove(i);

      emit headerDataChanged(orientation, first, last);
      break;
    }

    case Protocol::ModelRowsAdded:
    {
      Protocol::ModelIndex parentIndex;
      int first, last;
      msg.payload() >> parentIndex >> first >> last;
      Q_ASSERT(last >= first);

      Node *parentNode = nodeForIndex(parentIndex);
      if (!parentNode || parentNode->rowCount < 0)
        return; // we don't know the parent yet, so we don't care about changes to it either
      Q_ASSERT(parentNode->rowCount == parentNode->children.size());

      const QModelIndex qmiParent = modelIndexForNode(parentNode, 0);
      beginInsertRows(qmiParent, first, last);

      // allocate rows in the right spot
      if (first == parentNode->children.size())
        parentNode->children.resize(parentNode->children.size() + 1 + last - first);
      else
        parentNode->children.insert(first, last - first + 1, 0);

      // create nodes for the new rows
      for (int i = first; i <= last; ++i) {
        Node *child = new Node;
        child->parent = parentNode;
        parentNode->children[i] = child;
      }

      // adjust row count
      parentNode->rowCount += last - first + 1;
      Q_ASSERT(parentNode->rowCount == parentNode->children.size());

      endInsertRows();
      break;
    }

    case Protocol::ModelRowsRemoved:
    {
      Protocol::ModelIndex parentIndex;
      int first, last;
      msg.payload() >> parentIndex >> first >> last;
      Q_ASSERT(last >= first);

      Node *parentNode = nodeForIndex(parentIndex);
      if (!parentNode || parentNode->rowCount < 0)
        return; // we don't know the parent yet, so we don't care about changes to it either
      Q_ASSERT(parentNode->rowCount == parentNode->children.size());

      const QModelIndex qmiParent = modelIndexForNode(parentNode, 0);
      beginRemoveRows(qmiParent, first, last);

      // delete nodes
      for (int i = first; i <= last; ++i)
        delete parentNode->children.at(i);
      parentNode->children.remove(first, last - first + 1);

      // adjust row count
      parentNode->rowCount -= last - first + 1;
      Q_ASSERT(parentNode->rowCount == parentNode->children.size());

      endRemoveRows();
      break;
    }

    case Protocol::ModelRowsMoved:
    case Protocol::ModelColumnsAdded:
    case Protocol::ModelColumnsMoved:
    case Protocol::ModelColumnsRemoved:
    case Protocol::ModelLayoutChanged:
    {
      // TODO
      qDebug() << Q_FUNC_INFO << "not implemented yet" << msg.type();
    }

    case Protocol::ModelReset:
    {
      clear();
      break;
    }
  }
}

void RemoteModel::serverRegistered(const QString& objectName, Protocol::ObjectAddress objectAddress)
{
  if (m_serverObject == objectName) {
    m_myAddress = objectAddress;
    connectToServer();
  }
}

void RemoteModel::serverUnregistered(const QString& objectName, Protocol::ObjectAddress objectAddress)
{
  Q_UNUSED(objectName);
  if (m_myAddress == objectAddress) {
    m_myAddress = Protocol::InvalidObjectAddress;
    clear();
  }
}

RemoteModel::Node* RemoteModel::nodeForIndex(const QModelIndex &index) const
{
  if (!index.isValid())
    return m_root;
  return reinterpret_cast<Node*>(index.internalPointer());
}

RemoteModel::Node* RemoteModel::nodeForIndex(const Protocol::ModelIndex &index) const
{
  Node *node = m_root;
  for (int i = 0; i < index.size(); ++i) {
    if (node->children.size() <= index[i].first)
      return 0;
    node = node->children[index[i].first];
  }
  return node;
}

QModelIndex RemoteModel::modelIndexForNode(Node* node, int column) const
{
  Q_ASSERT(node);
  if (node == m_root)
    return QModelIndex();
  return createIndex(node->parent->children.indexOf(node), column, node);
}

void RemoteModel::requestRowColumnCount(const QModelIndex &index) const
{
  Node *node = nodeForIndex(index);
  Q_ASSERT(node);
  Q_ASSERT(node->rowCount < 0 && node->columnCount < 0);

  if (node->rowCount < -1) // already requesting
    return;
  node->rowCount = -2;

  Message msg(m_myAddress, Protocol::ModelRowColumnCountRequest);
  msg.payload() << Protocol::fromQModelIndex(index);
  Client::send(msg);
}

void RemoteModel::requestDataAndFlags(const QModelIndex& index) const
{
  Node *node = nodeForIndex(index);
  Q_ASSERT(node);
  Q_ASSERT(!node->data.contains(index.column()));
  Q_ASSERT(!node->flags.contains(index.column()));

  node->data.insert(index.column(), QHash<int, QVariant>()); // mark pending request

  Message msg(m_myAddress, Protocol::ModelContentRequest);
  msg.payload() << Protocol::fromQModelIndex(index);
  Client::send(msg);
}

void RemoteModel::requestHeaderData(Qt::Orientation orientation, int section) const
{
  Q_ASSERT(section >= 0);
  Q_ASSERT(!m_headers.value(orientation).contains(section));
  m_headers[orientation][section][Qt::DisplayRole] = tr("Loading...");

  Message msg(m_myAddress, Protocol::ModelHeaderRequest);
  msg.payload() << qint8(orientation) << qint32(section);
  Client::send(msg);
}

void RemoteModel::clear()
{
  qDebug() << Q_FUNC_INFO;
  beginResetModel();

  Message msg(m_myAddress, Protocol::ModelSyncBarrier);
  msg.payload() << ++m_targetSyncBarrier;
  Client::send(msg);

  delete m_root;
  m_root = new Node;
  m_headers.clear();
  endResetModel();
}

void RemoteModel::connectToServer()
{
  if (m_myAddress == Protocol::InvalidObjectAddress)
    return;

  beginResetModel();
  Client::instance()->registerForObject(m_myAddress, this, "newMessage");
  endResetModel();
}

bool RemoteModel::checkSyncBarrier(const Message& msg)
{
  if (msg.type() == Protocol::ModelSyncBarrier)
    msg.payload() >> m_currentSyncBarrier;

  return m_currentSyncBarrier == m_targetSyncBarrier;
}

#include "remotemodel.moc"
