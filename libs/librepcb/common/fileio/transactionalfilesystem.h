/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * https://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBREPCB_TRANSACTIONALFILESYSTEM_H
#define LIBREPCB_TRANSACTIONALFILESYSTEM_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "filepath.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/

class QuaZip;

namespace librepcb {

/*******************************************************************************
 *  Class TransactionalFileSystem
 ******************************************************************************/

class TransactionalFileSystem final : public QObject {
  Q_OBJECT

public:
  // Types
  // struct File {
  //  QString name;
  //  QByteArray content;
  //};

  struct Dir {
    QHash<QString, QByteArray> files;
    QHash<QString, Dir>        dirs;
  };

  // Constructors / Destructor
  TransactionalFileSystem(QObject* parent = nullptr) noexcept;
  TransactionalFileSystem(const TransactionalFileSystem& other) = delete;
  ~TransactionalFileSystem() noexcept;

  // File Operations
  QStringList getFiles(const QString& dir) const;
  QStringList getDirs(const QString& dir) const;
  QByteArray  read(const QString& path) const;
  void        write(const QString& path, const QByteArray& content);
  void        createDirectory(const QString& path);
  void        removeFile(const QString& path);
  void        removeDirRecursively(const QString& path);
  void        moveFile(const QString& src, const QString& dst);
  void        moveDir(const QString& src, const QString& dst);

  // General Methods
  void loadFromDirectory(const FilePath& fp);
  void saveToDirectory(const FilePath& fp);
  void saveToZip(const FilePath& fp);

private:  // Methods
  Dir&        getDir(const QString& path);
  QByteArray& getFile(const QString& path);
  static Dir  loadDir(const FilePath& fp);
  static void saveDir(const Dir& dir, const FilePath& fp);
  static void saveDir(const Dir& dir, const QString& path, QuaZip& zip);

private:  // Data
  Dir mRootDir;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif  // LIBREPCB_TRANSACTIONALFILESYSTEM_H
