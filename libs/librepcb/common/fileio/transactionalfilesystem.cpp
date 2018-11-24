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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "transactionalfilesystem.h"

#include "fileutils.h"

#include <quazip/quazip.h>
#include <quazip/quazipdir.h>
#include <quazip/quazipfile.h>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

TransactionalFileSystem::TransactionalFileSystem(QObject* parent) noexcept
  : QObject(parent) {
}

TransactionalFileSystem::~TransactionalFileSystem() noexcept {
}

/*******************************************************************************
 *  File Operations
 ******************************************************************************/

// QStringList TransactionalFileSystem::getFiles(const QString& dir) const {
//
//}
//
// QStringList TransactionalFileSystem::getDirs(const QString& dir) const {
//
//}
//
// QByteArray TransactionalFileSystem::read(const QString& path) const {
//
//}
//
void TransactionalFileSystem::write(const QString&    path,
                                    const QByteArray& content) {
  getFile(path) = content;
}

// void TransactionalFileSystem::createDirectory(const QString& path) {
//
//}
//
// void TransactionalFileSystem::removeFile(const QString& path) {
//
//}
//
// void TransactionalFileSystem::removeDirRecursively(const QString& path) {
//
//}
//
// void TransactionalFileSystem::moveFile(const QString& src, const QString&
// dst) {
//
//}
//
// void TransactionalFileSystem::moveDir(const QString& src, const QString& dst)
// {
//
//}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void TransactionalFileSystem::loadFromDirectory(const FilePath& fp) {
  if (!fp.isExistingDir()) {
    throw LogicError(
        __FILE__, __LINE__,
        QString(tr("The directory \"%1\" does not exist.")).arg(fp.toNative()));
  }
  mRootDir = loadDir(fp);
}

void TransactionalFileSystem::saveToDirectory(const FilePath& fp) {
  saveDir(mRootDir, fp);
}

void TransactionalFileSystem::saveToZip(const FilePath& fp) {
  QuaZip zip(fp.toStr());
  if (!zip.open(QuaZip::mdCreate)) {
    throw RuntimeError(
        __FILE__, __LINE__,
        QString(tr("Could not create the ZIP file '%1'.")).arg(fp.toNative()));
  }
  saveDir(mRootDir, "", zip);
  zip.close();
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

TransactionalFileSystem::Dir& TransactionalFileSystem::getDir(
    const QString& path) {
  Dir* dir = &mRootDir;
  foreach (const QString& subdir, path.split('/', QString::SkipEmptyParts)) {
    dir = &(dir->dirs[subdir]);
  }
  return *dir;
}

QByteArray& TransactionalFileSystem::getFile(const QString& path) {
  int     pathLen  = path.lastIndexOf('/') + 1;
  Dir&    dir      = getDir(path.left(pathLen));
  QString filename = path.right(path.length() - pathLen);
  return dir.files[filename];
}

TransactionalFileSystem::Dir TransactionalFileSystem::loadDir(
    const FilePath& fp) {
  Dir  dir;
  QDir qDir(fp.toStr());
  qDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  foreach (const QFileInfo& info, qDir.entryInfoList()) {
    FilePath entryFp(info.absoluteFilePath());
    if (info.isFile()) {
      qDebug() << "Loading file" << entryFp.toNative();
      dir.files.insert(info.fileName(), FileUtils::readFile(entryFp));
    } else if (info.isDir()) {
      dir.dirs.insert(info.fileName(), loadDir(entryFp));
    } else {
      qCritical() << "Unknown file item:" << info.absoluteFilePath();
    }
  }
  return dir;
}

void TransactionalFileSystem::saveDir(const Dir& dir, const FilePath& fp) {
  // create root directory
  FileUtils::makePath(fp);

  // save files
  QHashIterator<QString, QByteArray> fileIterator(dir.files);
  while (fileIterator.hasNext()) {
    fileIterator.next();
    FileUtils::writeFile(fp.getPathTo(fileIterator.key()),
                         fileIterator.value());
  }

  // create subdirectories and save their files
  QHashIterator<QString, Dir> dirIterator(dir.dirs);
  while (dirIterator.hasNext()) {
    dirIterator.next();
    saveDir(dirIterator.value(), fp.getPathTo(dirIterator.key()));
  }
}

void TransactionalFileSystem::saveDir(const Dir& dir, const QString& path,
                                      QuaZip& zip) {
  QuaZipFile file(&zip);

  // save files
  QHashIterator<QString, QByteArray> fileIterator(dir.files);
  while (fileIterator.hasNext()) {
    fileIterator.next();
    QString       filepath = path % fileIterator.key();
    QuaZipNewInfo newFileInfo(filepath);
    newFileInfo.setPermissions(QFileDevice::ReadOwner | QFileDevice::ReadGroup |
                               QFileDevice::ReadOther |
                               QFileDevice::WriteOwner);
    if (!file.open(QIODevice::WriteOnly, newFileInfo)) {
      throw RuntimeError(__FILE__, __LINE__);
    }
    qint64 bytesWritten = file.write(fileIterator.value());
    file.close();
    if (bytesWritten != fileIterator.value().length()) {
      throw RuntimeError(__FILE__, __LINE__);
    }
  }

  // create subdirectories and save their files
  QHashIterator<QString, Dir> dirIterator(dir.dirs);
  while (dirIterator.hasNext()) {
    dirIterator.next();
    QString       subdirpath = path % dirIterator.key() % "/";
    QuaZipNewInfo newFileInfo(subdirpath);
    newFileInfo.setPermissions(QFileDevice::ExeOwner | QFileDevice::ExeGroup |
                               QFileDevice::ExeOther | QFileDevice::ReadOwner |
                               QFileDevice::ReadGroup | QFileDevice::ReadOther |
                               QFileDevice::WriteOwner);
    if (!file.open(QIODevice::WriteOnly, newFileInfo)) {
      throw RuntimeError(__FILE__, __LINE__);
    }
    file.close();
    saveDir(dirIterator.value(), subdirpath, zip);
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
