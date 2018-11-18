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
#include "libraryoverviewwidget.h"

#include "librarylisteditorwidget.h"
#include "ui_libraryoverviewwidget.h"

#include <librepcb/common/dialogs/filedialog.h>
#include <librepcb/common/fileio/fileutils.h>
#include <librepcb/library/cmd/cmdlibraryedit.h>
#include <librepcb/library/elements.h>
#include <librepcb/workspace/library/workspacelibrarydb.h>
#include <librepcb/workspace/settings/workspacesettings.h>
#include <librepcb/workspace/workspace.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace library {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

LibraryOverviewWidget::LibraryOverviewWidget(const Context&          context,
                                             QSharedPointer<Library> lib,
                                             QWidget* parent) noexcept
  : EditorWidgetBase(context, lib->getFilePath(), parent),
    mLibrary(lib),
    mUi(new Ui::LibraryOverviewWidget) {
  mUi->setupUi(this);
  connect(mUi->btnIcon, &QPushButton::clicked, this,
          &LibraryOverviewWidget::btnIconClicked);
  connect(mUi->lstCmpCat, &QListWidget::doubleClicked, this,
          &LibraryOverviewWidget::lstCmpCatDoubleClicked);
  connect(mUi->lstPkgCat, &QListWidget::doubleClicked, this,
          &LibraryOverviewWidget::lstPkgCatDoubleClicked);
  connect(mUi->lstSym, &QListWidget::doubleClicked, this,
          &LibraryOverviewWidget::lstSymDoubleClicked);
  connect(mUi->lstPkg, &QListWidget::doubleClicked, this,
          &LibraryOverviewWidget::lstPkgDoubleClicked);
  connect(mUi->lstCmp, &QListWidget::doubleClicked, this,
          &LibraryOverviewWidget::lstCmpDoubleClicked);
  connect(mUi->lstDev, &QListWidget::doubleClicked, this,
          &LibraryOverviewWidget::lstDevDoubleClicked);

  // Insert dependencies editor widget.
  mDependenciesEditorWidget.reset(
      new LibraryListEditorWidget(mContext.workspace, this));
  int                   row;
  QFormLayout::ItemRole role;
  mUi->formLayout->getWidgetPosition(mUi->lblDependencies, &row, &role);
  mUi->formLayout->setWidget(row, QFormLayout::FieldRole,
                             mDependenciesEditorWidget.data());

  // Load metadata.
  updateMetadata();

  // Reload metadata on undo stack state changes.
  connect(mUndoStack.data(), &UndoStack::stateModified, this,
          &LibraryOverviewWidget::updateMetadata);

  // Handle changes of metadata.
  connect(mUi->edtName, &QLineEdit::editingFinished, this,
          &LibraryOverviewWidget::commitMetadata);
  connect(mUi->edtDescription, &PlainTextEdit::editingFinished, this,
          &LibraryOverviewWidget::commitMetadata);
  connect(mUi->edtKeywords, &QLineEdit::editingFinished, this,
          &LibraryOverviewWidget::commitMetadata);
  connect(mUi->edtAuthor, &QLineEdit::editingFinished, this,
          &LibraryOverviewWidget::commitMetadata);
  connect(mUi->edtVersion, &QLineEdit::editingFinished, this,
          &LibraryOverviewWidget::commitMetadata);
  connect(mUi->cbxDeprecated, &QCheckBox::clicked, this,
          &LibraryOverviewWidget::commitMetadata);
  connect(mUi->edtUrl, &QLineEdit::editingFinished, this,
          &LibraryOverviewWidget::commitMetadata);
  connect(mDependenciesEditorWidget.data(), &LibraryListEditorWidget::edited,
          this, &LibraryOverviewWidget::commitMetadata);

  // Set up context menu triggers
  connect(mUi->lstCmpCat, &QListWidget::customContextMenuRequested, this,
          &LibraryOverviewWidget::openContextMenuAtPos);
  connect(mUi->lstSym, &QListWidget::customContextMenuRequested, this,
          &LibraryOverviewWidget::openContextMenuAtPos);
  connect(mUi->lstCmp, &QListWidget::customContextMenuRequested, this,
          &LibraryOverviewWidget::openContextMenuAtPos);
  connect(mUi->lstPkgCat, &QListWidget::customContextMenuRequested, this,
          &LibraryOverviewWidget::openContextMenuAtPos);
  connect(mUi->lstPkg, &QListWidget::customContextMenuRequested, this,
          &LibraryOverviewWidget::openContextMenuAtPos);
  connect(mUi->lstDev, &QListWidget::customContextMenuRequested, this,
          &LibraryOverviewWidget::openContextMenuAtPos);

  // Load all library elements.
  updateElementLists();
  connect(&mContext.workspace.getLibraryDb(),
          &workspace::WorkspaceLibraryDb::scanSucceeded, this,
          &LibraryOverviewWidget::updateElementLists);
}

LibraryOverviewWidget::~LibraryOverviewWidget() noexcept {
}

/*******************************************************************************
 *  Public Slots
 ******************************************************************************/

bool LibraryOverviewWidget::save() noexcept {
  // Commit metadata.
  QString errorMsg = commitMetadata();
  if (!errorMsg.isEmpty()) {
    QMessageBox::critical(this, tr("Invalid metadata"), errorMsg);
    return false;
  }

  // Save element.
  try {
    mLibrary->save();  // can throw
    return EditorWidgetBase::save();
  } catch (const Exception& e) {
    QMessageBox::critical(this, tr("Save failed"), e.getMsg());
    return false;
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void LibraryOverviewWidget::updateMetadata() noexcept {
  setWindowTitle(*mLibrary->getNames().getDefaultValue());
  setWindowIcon(mLibrary->getIconAsPixmap());
  mUi->btnIcon->setIcon(mLibrary->getIconAsPixmap());
  if (mLibrary->getIconAsPixmap().isNull()) {
    mUi->btnIcon->setText(mUi->btnIcon->toolTip());
  } else {
    mUi->btnIcon->setText(QString());
  }
  mUi->edtName->setText(*mLibrary->getNames().getDefaultValue());
  mUi->edtDescription->setPlainText(
      mLibrary->getDescriptions().getDefaultValue());
  mUi->edtKeywords->setText(mLibrary->getKeywords().getDefaultValue());
  mUi->edtAuthor->setText(mLibrary->getAuthor());
  mUi->edtVersion->setText(mLibrary->getVersion().toStr());
  mUi->cbxDeprecated->setChecked(mLibrary->isDeprecated());
  mUi->edtUrl->setText(mLibrary->getUrl().toString());
  mDependenciesEditorWidget->setUuids(mLibrary->getDependencies());
  mIcon = mLibrary->getIcon();
}

QString LibraryOverviewWidget::commitMetadata() noexcept {
  try {
    QScopedPointer<CmdLibraryEdit> cmd(new CmdLibraryEdit(*mLibrary));
    try {
      // throws on invalid name
      cmd->setName("", ElementName(mUi->edtName->text().trimmed()));
    } catch (const Exception& e) {
    }
    cmd->setDescription("", mUi->edtDescription->toPlainText().trimmed());
    cmd->setKeywords("", mUi->edtKeywords->text().trimmed());
    try {
      // throws on invalid version
      cmd->setVersion(Version::fromString(mUi->edtVersion->text().trimmed()));
    } catch (const Exception& e) {
    }
    cmd->setAuthor(mUi->edtAuthor->text().trimmed());
    cmd->setDeprecated(mUi->cbxDeprecated->isChecked());
    cmd->setUrl(QUrl::fromUserInput(mUi->edtUrl->text().trimmed()));
    cmd->setDependencies(mDependenciesEditorWidget->getUuids());
    cmd->setIcon(mIcon);

    // Commit all changes.
    mUndoStack->execCmd(cmd.take());  // can throw

    // Reload metadata into widgets to discard invalid input.
    updateMetadata();
  } catch (const Exception& e) {
    return e.getMsg();
  }
  return QString();
}

void LibraryOverviewWidget::updateElementLists() noexcept {
  updateElementList<ComponentCategory>(*mUi->lstCmpCat,
                                       QIcon(":/img/places/folder.png"));
  updateElementList<PackageCategory>(*mUi->lstPkgCat,
                                     QIcon(":/img/places/folder_green.png"));
  updateElementList<Symbol>(*mUi->lstSym, QIcon(":/img/library/symbol.png"));
  updateElementList<Package>(*mUi->lstPkg, QIcon(":/img/library/package.png"));
  updateElementList<Component>(*mUi->lstCmp,
                               QIcon(":/img/library/component.png"));
  updateElementList<Device>(*mUi->lstDev, QIcon(":/img/library/device.png"));
}

template <typename ElementType>
void LibraryOverviewWidget::updateElementList(QListWidget& listWidget,
                                              const QIcon& icon) noexcept {
  QHash<FilePath, QString> elementNames;

  try {
    // get all library element names
    QList<FilePath> elements =
        mContext.workspace.getLibraryDb().getLibraryElements<ElementType>(
            mLibrary->getFilePath());  // can throw
    foreach (const FilePath& filepath, elements) {
      QString name;
      mContext.workspace.getLibraryDb().getElementTranslations<ElementType>(
          filepath, getLibLocaleOrder(), &name);  // can throw
      elementNames.insert(filepath, name);
    }
  } catch (const Exception& e) {
    listWidget.clear();
    QListWidgetItem* item = new QListWidgetItem(&listWidget);
    item->setText(e.getMsg());
    item->setToolTip(e.getMsg());
    item->setIcon(QIcon(":/img/status/dialog_error.png"));
    item->setBackground(Qt::red);
    item->setForeground(Qt::white);
    return;
  }

  // update/remove existing list widget items
  for (int i = listWidget.count() - 1; i >= 0; --i) {
    QListWidgetItem* item = listWidget.item(i);
    Q_ASSERT(item);
    FilePath filePath(item->data(Qt::UserRole).toString());
    if (elementNames.contains(filePath)) {
      item->setText(elementNames.take(filePath));
    } else {
      delete item;
    }
  }

  // add new list widget items
  foreach (const FilePath& fp, elementNames.keys()) {
    QString          name = elementNames.value(fp);
    QListWidgetItem* item = new QListWidgetItem(&listWidget);
    item->setText(name);
    item->setToolTip(name);
    item->setData(Qt::UserRole, fp.toStr());
    item->setIcon(icon);
  }
}

bool LibraryOverviewWidget::openContextMenuAtPos(const QPoint& pos) noexcept {
  Q_UNUSED(pos);

  // Build the context menu
  QMenu    menu;
  QAction* aRemove = menu.addAction(QIcon(":/img/actions/delete.png"), tr("Remove"));

  // Execute the context menu
  QAction* action = menu.exec(QCursor::pos());
  if (action == aRemove) {
    return removeSelectedItem(sender());
  } else {
    return false;
  }
}

bool LibraryOverviewWidget::removeSelectedItem(const QObject* sender) noexcept {
  // Get reference to list widget that triggered the action
  const QListWidget* list = dynamic_cast<const QListWidget*>(sender);
  Q_ASSERT(list);

  // Find file path of selected library element
  QListWidgetItem* selectedItem = list->item(list->currentRow());
  FilePath fp = selectedItem ? FilePath(selectedItem->data(Qt::UserRole).toString()) : FilePath();
  if (!fp.isValid()) {
      return false;
  }

  int ret = QMessageBox::warning(
      this, tr("Remove %1").arg("TODO"),
      QString(tr("WARNING: Library elements must normally NOT be removed "
                 "because this will break "
                 "other elements which depend on this one! They should be just "
                 "marked as "
                 "deprecated instead.\n\nAre you still sure to delete the "
                 "whole library element "
                 "\"%1\"?\n\nThis cannot be undone!"))
          .arg("TODO"),
      QMessageBox::Yes, QMessageBox::Cancel);
  if (ret == QMessageBox::Yes) {
    try {
      FileUtils::removeDirRecursively(fp);
    } catch (const Exception& e) {
      QMessageBox::critical(this, tr("Error"), e.getMsg());
      mContext.workspace.getLibraryDb().startLibraryRescan();
      return false;
    }
    mContext.workspace.getLibraryDb().startLibraryRescan();
    return true;
  } else {
    return false;
  }
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

void LibraryOverviewWidget::btnIconClicked() noexcept {
  QString fp = FileDialog::getOpenFileName(
      this, tr("Choose library icon"), mLibrary->getIconFilePath().toNative(),
      tr("Portable Network Graphics (*.png)"));
  if (!fp.isEmpty()) {
    try {
      mIcon = FileUtils::readFile(FilePath(fp));  // can throw
      commitMetadata();
    } catch (const Exception& e) {
      QMessageBox::critical(this, tr("Could not open file"), e.getMsg());
    }
  }
}

void LibraryOverviewWidget::lstCmpCatDoubleClicked(
    const QModelIndex& index) noexcept {
  QListWidgetItem* item = mUi->lstCmpCat->item(index.row());
  FilePath         fp =
      item ? FilePath(item->data(Qt::UserRole).toString()) : FilePath();
  if (fp.isValid()) {
    emit editComponentCategoryTriggered(fp);
  }
}

void LibraryOverviewWidget::lstPkgCatDoubleClicked(
    const QModelIndex& index) noexcept {
  QListWidgetItem* item = mUi->lstPkgCat->item(index.row());
  FilePath         fp =
      item ? FilePath(item->data(Qt::UserRole).toString()) : FilePath();
  if (fp.isValid()) {
    emit editPackageCategoryTriggered(fp);
  }
}

void LibraryOverviewWidget::lstSymDoubleClicked(
    const QModelIndex& index) noexcept {
  QListWidgetItem* item = mUi->lstSym->item(index.row());
  FilePath         fp =
      item ? FilePath(item->data(Qt::UserRole).toString()) : FilePath();
  if (fp.isValid()) {
    emit editSymbolTriggered(fp);
  }
}

void LibraryOverviewWidget::lstPkgDoubleClicked(
    const QModelIndex& index) noexcept {
  QListWidgetItem* item = mUi->lstPkg->item(index.row());
  FilePath         fp =
      item ? FilePath(item->data(Qt::UserRole).toString()) : FilePath();
  if (fp.isValid()) {
    emit editPackageTriggered(fp);
  }
}

void LibraryOverviewWidget::lstCmpDoubleClicked(
    const QModelIndex& index) noexcept {
  QListWidgetItem* item = mUi->lstCmp->item(index.row());
  FilePath         fp =
      item ? FilePath(item->data(Qt::UserRole).toString()) : FilePath();
  if (fp.isValid()) {
    emit editComponentTriggered(fp);
  }
}

void LibraryOverviewWidget::lstDevDoubleClicked(
    const QModelIndex& index) noexcept {
  QListWidgetItem* item = mUi->lstDev->item(index.row());
  FilePath         fp =
      item ? FilePath(item->data(Qt::UserRole).toString()) : FilePath();
  if (fp.isValid()) {
    emit editDeviceTriggered(fp);
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace library
}  // namespace librepcb
