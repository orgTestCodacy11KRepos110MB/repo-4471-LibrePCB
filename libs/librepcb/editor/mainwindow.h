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

#ifndef LIBREPCB_EDITOR_MainWindow_H
#define LIBREPCB_EDITOR_MainWindow_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/fileio/filepath.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Workspace;

namespace editor {

namespace Ui {
class MainWindow;
}

/*******************************************************************************
 *  Class MainWindow
 ******************************************************************************/

/**
 * @brief The main window of LibrePCB
 */
class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  // Constructors / Destructor
  MainWindow() = delete;
  MainWindow(const MainWindow& other) = delete;
  explicit MainWindow(Workspace& workspace, bool fileFormatIsOutdated);
  virtual ~MainWindow() noexcept;

  // Operator Overloadings
  MainWindow& operator=(const MainWindow& rhs) = delete;

private: // Attributes
  Workspace& mWorkspace;
  QScopedPointer<Ui::MainWindow> mUi;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
