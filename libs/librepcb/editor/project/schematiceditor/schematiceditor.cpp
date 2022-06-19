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
#include "schematiceditor.h"

#include "../../dialogs/filedialog.h"
#include "../../dialogs/gridsettingsdialog.h"
#include "../../editorcommandset.h"
#include "../../project/cmd/cmdschematicadd.h"
#include "../../project/cmd/cmdschematicedit.h"
#include "../../project/cmd/cmdschematicremove.h"
#include "../../undostack.h"
#include "../../utils/exclusiveactiongroup.h"
#include "../../utils/menubuilder.h"
#include "../../utils/standardeditorcommandhandler.h"
#include "../../utils/toolbarproxy.h"
#include "../../utils/undostackactiongroup.h"
#include "../../widgets/searchtoolbar.h"
#include "../../workspace/desktopservices.h"
#include "../bomgeneratordialog.h"
#include "../erc/ercmsgdock.h"
#include "../projecteditor.h"
#include "../projectpropertieseditordialog.h"
#include "fsm/schematiceditorfsm.h"
#include "schematicpagesdock.h"

#include <librepcb/core/application.h>
#include <librepcb/core/project/circuit/circuit.h>
#include <librepcb/core/project/circuit/componentinstance.h>
#include <librepcb/core/project/project.h>
#include <librepcb/core/project/projectmetadata.h>
#include <librepcb/core/project/projectsettings.h>
#include <librepcb/core/project/schematic/items/si_symbol.h>
#include <librepcb/core/project/schematic/schematic.h>
#include <librepcb/core/project/schematic/schematicpainter.h>
#include <librepcb/core/types/gridproperties.h>
#include <librepcb/core/workspace/workspace.h>
#include <librepcb/core/workspace/workspacelibrarydb.h>
#include <librepcb/core/workspace/workspacesettings.h>

#include <QSvgGenerator>
#include <QtCore>
#include <QtPrintSupport>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

SchematicEditor::SchematicEditor(ProjectEditor& projectEditor, Project& project)
  : QMainWindow(0),
    mProjectEditor(projectEditor),
    mProject(project),
    mUi(new Ui::SchematicEditor),
    mCommandToolBarProxy(new ToolBarProxy(this)),
    mStandardCommandHandler(new StandardEditorCommandHandler(
        mProjectEditor.getWorkspace().getSettings(), this)),
    mActiveSchematicIndex(-1),
    mFsm() {
  mUi->setupUi(this);

  // Setup graphics view.
  mUi->graphicsView->setUseOpenGl(
      mProjectEditor.getWorkspace().getSettings().useOpenGl.get());
  mUi->graphicsView->setEventHandlerObject(this);
  connect(mUi->graphicsView, &GraphicsView::cursorScenePositionChanged,
          mUi->statusbar, &StatusBar::setAbsoluteCursorPosition);

  // Setup status bar.
  mUi->statusbar->setFields(StatusBar::AbsolutePosition |
                            StatusBar::ProgressBar);
  mUi->statusbar->setProgressBarTextFormat(tr("Scanning libraries (%p%)"));
  connect(&mProjectEditor.getWorkspace().getLibraryDb(),
          &WorkspaceLibraryDb::scanProgressUpdate, mUi->statusbar,
          &StatusBar::setProgressBarPercent, Qt::QueuedConnection);

  // Set window title.
  QString filenameStr = mProject.getFilepath().getFilename();
  if (!mProject.getDirectory().isWritable()) {
    filenameStr.append(QStringLiteral(" [Read-Only]"));
  }
  setWindowTitle(tr("%1 - LibrePCB Schematic Editor").arg(filenameStr));

  // Build the whole schematic editor finite state machine.
  SchematicEditorFsm::Context fsmContext{mProjectEditor.getWorkspace(),
                                         mProject,
                                         *this,
                                         *mUi->graphicsView,
                                         *mCommandToolBarProxy,
                                         mProjectEditor.getUndoStack()};
  mFsm.reset(new SchematicEditorFsm(fsmContext));

  // Create all actions, window menus, toolbars and dock widgets.
  createActions();
  createToolBars();
  createDockWidgets();
  createMenus();  // Depends on dock widgets!

  // Restore window geometry.
  QSettings clientSettings;
  restoreGeometry(
      clientSettings.value("schematic_editor/window_geometry").toByteArray());
  restoreState(
      clientSettings.value("schematic_editor/window_state_v2").toByteArray());

  // Load first schematic page
  if (mProject.getSchematics().count() > 0) setActiveSchematicIndex(0);

  // Set focus to graphics view (avoid having the focus in some arbitrary
  // widget).
  mUi->graphicsView->setFocus();

  // mGraphicsView->zoomAll(); does not work properly here, should be executed
  // later in the event loop (ugly, but seems to work...)
  QTimer::singleShot(200, mUi->graphicsView, &GraphicsView::zoomAll);
}

SchematicEditor::~SchematicEditor() {
  // Save window geometry.
  QSettings clientSettings;
  clientSettings.setValue("schematic_editor/window_geometry", saveGeometry());
  clientSettings.setValue("schematic_editor/window_state_v2", saveState());

  // Important: Release command toolbar proxy since otherwise the actions will
  // be deleted first.
  mCommandToolBarProxy->setToolBar(nullptr);
}

/*******************************************************************************
 *  Getters
 ******************************************************************************/

Schematic* SchematicEditor::getActiveSchematic() const noexcept {
  return mProject.getSchematicByIndex(mActiveSchematicIndex);
}

/*******************************************************************************
 *  Setters
 ******************************************************************************/

bool SchematicEditor::setActiveSchematicIndex(int index) noexcept {
  if (index == mActiveSchematicIndex) return true;

  // "Ask" the FSM if changing the scene is allowed at the moment.
  // If the FSM accepts the event, we can switch to the specified schematic
  // page.
  if (!mFsm->processSwitchToSchematicPage(index)) {
    return false;  // changing the schematic page is not allowed!
  }

  // event accepted --> change the schematic page
  Schematic* schematic = getActiveSchematic();
  if (schematic) {
    // save current view scene rect
    schematic->saveViewSceneRect(mUi->graphicsView->getVisibleSceneRect());
  }
  schematic = mProject.getSchematicByIndex(index);
  if (schematic) {
    // show scene, restore view scene rect, set grid properties
    mUi->graphicsView->setScene(&schematic->getGraphicsScene());
    mUi->graphicsView->setVisibleSceneRect(schematic->restoreViewSceneRect());
    mUi->graphicsView->setGridProperties(schematic->getGridProperties());
    mUi->statusbar->setLengthUnit(schematic->getGridProperties().getUnit());
  } else {
    mUi->graphicsView->setScene(nullptr);
  }

  // update toolbars
  mActionGridProperties->setEnabled(schematic != nullptr);
  mActionGridIncrease->setEnabled(schematic != nullptr);
  mActionGridDecrease->setEnabled(schematic != nullptr);

  // schematic page has changed!
  mActiveSchematicIndex = index;
  emit activeSchematicChanged(mActiveSchematicIndex);
  return true;
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void SchematicEditor::abortAllCommands() noexcept {
  // ugly... ;-)
  mFsm->processAbortCommand();
  mFsm->processAbortCommand();
  mFsm->processAbortCommand();
}

/*******************************************************************************
 *  Inherited Methods
 ******************************************************************************/

void SchematicEditor::closeEvent(QCloseEvent* event) {
  if (!mProjectEditor.windowIsAboutToClose(*this))
    event->ignore();
  else
    QMainWindow::closeEvent(event);
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void SchematicEditor::createActions() noexcept {
  const EditorCommandSet& cmd = EditorCommandSet::instance();

  mActionAboutLibrePcb.reset(cmd.aboutLibrePcb.createAction(
      this, mStandardCommandHandler.data(),
      &StandardEditorCommandHandler::aboutLibrePcb));
  mActionAboutQt.reset(
      cmd.aboutQt.createAction(this, qApp, &QApplication::aboutQt));
  mActionOnlineDocumentation.reset(cmd.documentationOnline.createAction(
      this, mStandardCommandHandler.data(),
      &StandardEditorCommandHandler::onlineDocumentation));
  mActionKeyboardShortcutsReference.reset(
      cmd.keyboardShortcutsReference.createAction(
          this, mStandardCommandHandler.data(),
          &StandardEditorCommandHandler::shortcutsReference));
  mActionWebsite.reset(
      cmd.website.createAction(this, mStandardCommandHandler.data(),
                               &StandardEditorCommandHandler::website));
  mActionSaveProject.reset(cmd.projectSave.createAction(
      this, &mProjectEditor, &ProjectEditor::saveProject));
  mActionSaveProject->setEnabled(mProject.getDirectory().isWritable());
  mActionCloseProject.reset(cmd.projectClose.createAction(
      this, this, [this]() { mProjectEditor.closeAndDestroy(true, this); }));
  mActionCloseWindow.reset(
      cmd.windowClose.createAction(this, this, &SchematicEditor::close));
  mActionQuit.reset(cmd.applicationQuit.createAction(
      this, qApp, &Application::quitTriggered));
  mActionFileManager.reset(cmd.fileManager.createAction(this, this, [this]() {
    mStandardCommandHandler->fileManager(mProject.getPath());
  }));
  mActionBoardEditor.reset(cmd.boardEditor.createAction(
      this, &mProjectEditor, &ProjectEditor::showBoardEditor));
  mActionControlPanel.reset(cmd.controlPanel.createAction(
      this, &mProjectEditor, &ProjectEditor::showControlPanelClicked));
  mActionProjectProperties.reset(
      cmd.projectProperties.createAction(this, this, [this]() {
        ProjectPropertiesEditorDialog dialog(
            mProject.getMetadata(), mProjectEditor.getUndoStack(), this);
        dialog.exec();
      }));
  mActionProjectSettings.reset(cmd.projectSettings.createAction(
      this, this,
      [this]() { mProjectEditor.execProjectSettingsDialog(this); }));
  mActionNetClasses.reset(cmd.netClasses.createAction(this, this, [this]() {
    mProjectEditor.execNetClassesEditorDialog(this);
  }));
  mActionUpdateLibrary.reset(
      cmd.projectLibraryUpdate.createAction(this, this, [this]() {
        // Ugly hack until we have a *real* project library updater...
        emit mProjectEditor.openProjectLibraryUpdaterClicked(
            mProject.getFilepath());
      }));
  mActionExportLppz.reset(cmd.exportLppz.createAction(
      this, this, [this]() { mProjectEditor.execLppzExportDialog(this); }));
  mActionExportImage.reset(cmd.exportImage.createAction(this, this, [this]() {
    execGraphicsExportDialog(GraphicsExportDialog::Output::Image,
                             "image_export");
  }));
  mActionExportPdf.reset(cmd.exportPdf.createAction(this, this, [this]() {
    execGraphicsExportDialog(GraphicsExportDialog::Output::Pdf, "pdf_export");
  }));
  mActionPrint.reset(cmd.print.createAction(this, this, [this]() {
    execGraphicsExportDialog(GraphicsExportDialog::Output::Print, "print");
  }));
  mActionGenerateBom.reset(cmd.generateBom.createAction(this, this, [this]() {
    const Board* board = mProject.getBoards().count() == 1
        ? mProject.getBoardByIndex(0)
        : nullptr;
    BomGeneratorDialog dialog(mProject, board, this);
    dialog.exec();
  }));
  mActionOrderPcb.reset(cmd.orderPcb.createAction(this, this, [this]() {
    mProjectEditor.execOrderPcbDialog(nullptr, this);
  }));
  mActionNewSheet.reset(
      cmd.sheetNew.createAction(this, this, &SchematicEditor::addSchematic));
  mActionRenameSheet.reset(cmd.sheetRename.createAction(
      this, this, [this]() { renameSchematic(mActiveSchematicIndex); }));
  mActionRemoveSheet.reset(cmd.sheetRemove.createAction(
      this, this, [this]() { removeSchematic(mActiveSchematicIndex); }));
  mActionNextPage.reset(cmd.pageNext.createAction(this, this, [this]() {
    const int newIndex = mActiveSchematicIndex + 1;
    if (newIndex < mProject.getSchematics().count()) {
      setActiveSchematicIndex(newIndex);
    }
  }));
  addAction(mActionNextPage.data());
  mActionPreviousPage.reset(cmd.pagePrevious.createAction(this, this, [this]() {
    const int newIndex = mActiveSchematicIndex - 1;
    if (newIndex >= 0) {
      setActiveSchematicIndex(newIndex);
    }
  }));
  addAction(mActionPreviousPage.data());
  mActionFind.reset(cmd.find.createAction(this));
  mActionFindNext.reset(cmd.findNext.createAction(this));
  mActionFindPrevious.reset(cmd.findPrevious.createAction(this));
  mActionSelectAll.reset(cmd.selectAll.createAction(
      this, mFsm.data(), &SchematicEditorFsm::processSelectAll));
  mActionGridProperties.reset(cmd.gridProperties.createAction(
      this, this, &SchematicEditor::execGridPropertiesDialog));
  mActionGridIncrease.reset(cmd.gridIncrease.createAction(this, this, [this]() {
    if (const Schematic* schematic = getActiveSchematic()) {
      GridProperties grid = schematic->getGridProperties();
      grid.setInterval(PositiveLength(grid.getInterval() * 2));
      setGridProperties(grid, true);
    }
  }));
  mActionGridDecrease.reset(cmd.gridDecrease.createAction(this, this, [this]() {
    if (const Schematic* schematic = getActiveSchematic()) {
      GridProperties grid = schematic->getGridProperties();
      if ((*grid.getInterval()) % 2 == 0) {
        grid.setInterval(PositiveLength(grid.getInterval() / 2));
        setGridProperties(grid, true);
      }
    }
  }));
  mActionZoomFit.reset(cmd.zoomFitContent.createAction(this, mUi->graphicsView,
                                                       &GraphicsView::zoomAll));
  mActionZoomIn.reset(
      cmd.zoomIn.createAction(this, mUi->graphicsView, &GraphicsView::zoomIn));
  mActionZoomOut.reset(cmd.zoomOut.createAction(this, mUi->graphicsView,
                                                &GraphicsView::zoomOut));
  mActionUndo.reset(cmd.undo.createAction(this));
  mActionRedo.reset(cmd.redo.createAction(this));
  mActionCut.reset(cmd.clipboardCut.createAction(
      this, mFsm.data(), &SchematicEditorFsm::processCut));
  mActionCopy.reset(cmd.clipboardCopy.createAction(
      this, mFsm.data(), &SchematicEditorFsm::processCopy));
  mActionPaste.reset(cmd.clipboardPaste.createAction(
      this, mFsm.data(), &SchematicEditorFsm::processPaste));
  mActionMoveLeft.reset(cmd.moveLeft.createAction(this, this, [this]() {
    if (!mFsm->processMove(
            Point(-mUi->graphicsView->getGridProperties().getInterval(), 0))) {
      // Workaround for consumed keyboard shortcuts for scrolling.
      mUi->graphicsView->horizontalScrollBar()->triggerAction(
          QScrollBar::SliderSingleStepSub);
    }
  }));
  addAction(mActionMoveLeft.data());
  mActionMoveRight.reset(cmd.moveRight.createAction(this, this, [this]() {
    if (!mFsm->processMove(
            Point(*mUi->graphicsView->getGridProperties().getInterval(), 0))) {
      // Workaround for consumed keyboard shortcuts for scrolling.
      mUi->graphicsView->horizontalScrollBar()->triggerAction(
          QScrollBar::SliderSingleStepAdd);
    }
  }));
  addAction(mActionMoveRight.data());
  mActionMoveUp.reset(cmd.moveUp.createAction(this, this, [this]() {
    if (!mFsm->processMove(
            Point(0, *mUi->graphicsView->getGridProperties().getInterval()))) {
      // Workaround for consumed keyboard shortcuts for scrolling.
      mUi->graphicsView->verticalScrollBar()->triggerAction(
          QScrollBar::SliderSingleStepSub);
    }
  }));
  addAction(mActionMoveUp.data());
  mActionMoveDown.reset(cmd.moveDown.createAction(this, this, [this]() {
    if (!mFsm->processMove(
            Point(0, -mUi->graphicsView->getGridProperties().getInterval()))) {
      // Workaround for consumed keyboard shortcuts for scrolling.
      mUi->graphicsView->verticalScrollBar()->triggerAction(
          QScrollBar::SliderSingleStepAdd);
    }
  }));
  addAction(mActionMoveDown.data());
  mActionRotateCcw.reset(cmd.rotateCcw.createAction(
      this, this, [this]() { mFsm->processRotate(Angle::deg90()); }));
  mActionRotateCw.reset(cmd.rotateCw.createAction(
      this, this, [this]() { mFsm->processRotate(-Angle::deg90()); }));
  mActionMirrorHorizontal.reset(cmd.mirrorHorizontal.createAction(
      this, this, [this]() { mFsm->processMirror(Qt::Horizontal); }));
  mActionMirrorVertical.reset(cmd.mirrorVertical.createAction(
      this, this, [this]() { mFsm->processMirror(Qt::Vertical); }));
  mActionProperties.reset(cmd.properties.createAction(
      this, mFsm.data(), &SchematicEditorFsm::processEditProperties));
  mActionRemove.reset(cmd.remove.createAction(
      this, mFsm.data(), &SchematicEditorFsm::processRemove));
  mActionAbort.reset(cmd.abort.createAction(
      this, mFsm.data(), &SchematicEditorFsm::processAbortCommand));
  mActionToolSelect.reset(cmd.toolSelect.createAction(this));
  mActionToolWire.reset(cmd.toolWire.createAction(this));
  mActionToolNetLabel.reset(cmd.toolNetLabel.createAction(this));
  mActionToolComponent.reset(cmd.toolComponent.createAction(this));
  mActionComponentResistor.reset(
      cmd.componentResistor.createAction(this, this, [this]() {
        Uuid componentUuid =
            Uuid::fromString("ef80cd5e-2689-47ee-8888-31d04fc99174");
        Uuid symbVarUuid = Uuid::fromString(
            useIeee315Symbols() ? "d16e1f44-16af-4773-a310-de370f744548"
                                : "a5995314-f535-45d4-8bd8-2d0b8a0dc42a");
        mFsm->processAddComponent(componentUuid, symbVarUuid);
      }));
  mActionComponentInductor.reset(
      cmd.componentInductor.createAction(this, this, [this]() {
        Uuid componentUuid =
            Uuid::fromString("506bd124-6062-400e-9078-b38bd7e1aaee");
        Uuid symbVarUuid = Uuid::fromString(
            useIeee315Symbols() ? "4245d515-6f6d-48cb-9958-a4ea23d0187f"
                                : "62a7598c-17fe-41cf-8fa1-4ed274c3adc2");
        mFsm->processAddComponent(componentUuid, symbVarUuid);
      }));
  mActionComponentCapacitorBipolar.reset(
      cmd.componentCapacitorBipolar.createAction(this, this, [this]() {
        Uuid componentUuid =
            Uuid::fromString("d167e0e3-6a92-4b76-b013-77b9c230e5f1");
        Uuid symbVarUuid = Uuid::fromString(
            useIeee315Symbols() ? "6e639ff1-4e81-423b-9d0e-b28b35693a61"
                                : "8cd7b37f-e5fa-4af5-a8dd-d78830bba3af");
        mFsm->processAddComponent(componentUuid, symbVarUuid);
      }));
  mActionComponentCapacitorUnipolar.reset(
      cmd.componentCapacitorUnipolar.createAction(this, this, [this]() {
        Uuid componentUuid =
            Uuid::fromString("c54375c5-7149-4ded-95c5-7462f7301ee7");
        Uuid symbVarUuid = Uuid::fromString(
            useIeee315Symbols() ? "20a01a81-506e-4fee-9dc0-8b50e6537cd4"
                                : "5412add2-af9c-44b8-876d-a0fb7c201897");
        mFsm->processAddComponent(componentUuid, symbVarUuid);
      }));
  mActionComponentGnd.reset(cmd.componentGnd.createAction(this, this, [this]() {
    Uuid componentUuid =
        Uuid::fromString("8076f6be-bfab-4fc1-9772-5d54465dd7e1");
    Uuid symbVarUuid = Uuid::fromString("f09ad258-595b-4ee9-a1fc-910804a203ae");
    mFsm->processAddComponent(componentUuid, symbVarUuid);
  }));
  mActionComponentVcc.reset(cmd.componentVcc.createAction(this, this, [this]() {
    Uuid componentUuid =
        Uuid::fromString("58c3c6cd-11eb-4557-aa3f-d3e05874afde");
    Uuid symbVarUuid = Uuid::fromString("afb86b45-68ec-47b6-8d96-153d73567228");
    mFsm->processAddComponent(componentUuid, symbVarUuid);
  }));
  mActionDockPages.reset(cmd.dockPages.createAction(this, this, [this]() {
    mDockPages->show();
    mDockPages->raise();
    mDockPages->setFocus();
  }));
  mActionDockErc.reset(cmd.dockErc.createAction(this, this, [this]() {
    mDockErc->show();
    mDockErc->raise();
    mDockErc->setFocus();
  }));

  // Widget shortcuts.
  mUi->graphicsView->addAction(cmd.commandToolBarFocus.createAction(
      this, this,
      [this]() {
        mCommandToolBarProxy->startTabFocusCycle(*mUi->graphicsView);
      },
      EditorCommand::ActionFlag::WidgetShortcut));

  // Undo stack action group.
  mUndoStackActionGroup.reset(
      new UndoStackActionGroup(*mActionUndo, *mActionRedo, nullptr,
                               &mProjectEditor.getUndoStack(), this));

  // Tools action group.
  mToolsActionGroup.reset(new ExclusiveActionGroup());
  mToolsActionGroup->addAction(SchematicEditorFsm::State::SELECT,
                               mActionToolSelect.data());
  mToolsActionGroup->addAction(SchematicEditorFsm::State::DRAW_WIRE,
                               mActionToolWire.data());
  mToolsActionGroup->addAction(SchematicEditorFsm::State::ADD_NETLABEL,
                               mActionToolNetLabel.data());
  mToolsActionGroup->addAction(SchematicEditorFsm::State::ADD_COMPONENT,
                               mActionToolComponent.data());
  mToolsActionGroup->setCurrentAction(mFsm->getCurrentState());
  connect(mFsm.data(), &SchematicEditorFsm::stateChanged,
          mToolsActionGroup.data(), &ExclusiveActionGroup::setCurrentAction);
  connect(mToolsActionGroup.data(),
          &ExclusiveActionGroup::changeRequestTriggered, this,
          &SchematicEditor::toolActionGroupChangeTriggered);
}

void SchematicEditor::createToolBars() noexcept {
  // File.
  mToolBarFile.reset(new QToolBar(tr("File"), this));
  mToolBarFile->setObjectName("toolBarFile");
  mToolBarFile->addAction(mActionCloseProject.data());
  mToolBarFile->addSeparator();
  mToolBarFile->addAction(mActionNewSheet.data());
  mToolBarFile->addAction(mActionSaveProject.data());
  mToolBarFile->addAction(mActionPrint.data());
  mToolBarFile->addAction(mActionExportPdf.data());
  mToolBarFile->addAction(mActionOrderPcb.data());
  mToolBarFile->addSeparator();
  mToolBarFile->addAction(mActionControlPanel.data());
  mToolBarFile->addAction(mActionBoardEditor.data());
  mToolBarFile->addSeparator();
  mToolBarFile->addAction(mActionUndo.data());
  mToolBarFile->addAction(mActionRedo.data());
  addToolBar(Qt::TopToolBarArea, mToolBarFile.data());

  // Edit.
  mToolBarEdit.reset(new QToolBar(tr("Edit"), this));
  mToolBarEdit->setObjectName("toolBarEdit");
  mToolBarEdit->addAction(mActionCut.data());
  mToolBarEdit->addAction(mActionCopy.data());
  mToolBarEdit->addAction(mActionPaste.data());
  mToolBarEdit->addAction(mActionRemove.data());
  mToolBarEdit->addAction(mActionRotateCcw.data());
  mToolBarEdit->addAction(mActionRotateCw.data());
  mToolBarEdit->addAction(mActionMirrorHorizontal.data());
  mToolBarEdit->addAction(mActionMirrorVertical.data());
  addToolBar(Qt::TopToolBarArea, mToolBarEdit.data());

  // View.
  mToolBarView.reset(new QToolBar(tr("View"), this));
  mToolBarView->setObjectName("toolBarView");
  mToolBarView->addAction(mActionGridProperties.data());
  mToolBarView->addAction(mActionZoomIn.data());
  mToolBarView->addAction(mActionZoomOut.data());
  mToolBarView->addAction(mActionZoomFit.data());
  addToolBar(Qt::TopToolBarArea, mToolBarView.data());

  // Search.
  mToolBarSearch.reset(new SearchToolBar(this));
  mToolBarSearch->setObjectName("toolBarSearch");
  mToolBarSearch->setPlaceholderText(tr("Find symbol..."));
  mToolBarSearch->setCompleterListFunction(
      std::bind(&SchematicEditor::getSearchToolBarCompleterList, this));
  connect(mActionFind.data(), &QAction::triggered, mToolBarSearch.data(),
          &SearchToolBar::selectAllAndSetFocus);
  connect(mActionFindNext.data(), &QAction::triggered, mToolBarSearch.data(),
          &SearchToolBar::findNext);
  connect(mActionFindPrevious.data(), &QAction::triggered,
          mToolBarSearch.data(), &SearchToolBar::findPrevious);
  addToolBar(Qt::TopToolBarArea, mToolBarSearch.data());
  connect(mToolBarSearch.data(), &SearchToolBar::goToTriggered, this,
          &SchematicEditor::goToSymbol);

  // Command.
  mToolBarCommand.reset(new QToolBar(tr("Command"), this));
  mToolBarCommand->setObjectName("toolBarCommand");
  mToolBarCommand->addAction(mActionAbort.data());
  mToolBarCommand->addSeparator();
  addToolBarBreak(Qt::TopToolBarArea);
  addToolBar(Qt::TopToolBarArea, mToolBarCommand.data());
  mCommandToolBarProxy->setToolBar(mToolBarCommand.data());

  // Tools.
  mToolBarTools.reset(new QToolBar(tr("Tools"), this));
  mToolBarTools->setObjectName("toolBarTools");
  mToolBarTools->addAction(mActionToolSelect.data());
  mToolBarTools->addAction(mActionToolWire.data());
  mToolBarTools->addAction(mActionToolNetLabel.data());
  mToolBarTools->addAction(mActionToolComponent.data());
  addToolBar(Qt::LeftToolBarArea, mToolBarTools.data());

  // Components.
  mToolBarComponents.reset(new QToolBar(tr("Components"), this));
  mToolBarComponents->setObjectName("toolBarComponents");
  mToolBarComponents->addAction(mActionComponentResistor.data());
  mToolBarComponents->addAction(mActionComponentInductor.data());
  mToolBarComponents->addAction(mActionComponentCapacitorBipolar.data());
  mToolBarComponents->addAction(mActionComponentCapacitorUnipolar.data());
  mToolBarComponents->addAction(mActionComponentGnd.data());
  mToolBarComponents->addAction(mActionComponentVcc.data());
  addToolBarBreak(Qt::LeftToolBarArea);
  addToolBar(Qt::LeftToolBarArea, mToolBarComponents.data());
  updateComponentToolbarIcons();  // Load icons according workspace settings.
  connect(&mProject.getSettings(), &ProjectSettings::settingsChanged, this,
          &SchematicEditor::updateComponentToolbarIcons);
}

void SchematicEditor::createDockWidgets() noexcept {
  // Pages.
  mDockPages.reset(new SchematicPagesDock(mProject, this));
  connect(this, &SchematicEditor::activeSchematicChanged, mDockPages.data(),
          &SchematicPagesDock::setSelectedSchematic);
  connect(mDockPages.data(), &SchematicPagesDock::selectedSchematicChanged,
          this, &SchematicEditor::setActiveSchematicIndex);
  connect(mDockPages.data(), &SchematicPagesDock::addSchematicTriggered, this,
          &SchematicEditor::addSchematic);
  connect(mDockPages.data(), &SchematicPagesDock::removeSchematicTriggered,
          this, &SchematicEditor::removeSchematic);
  connect(mDockPages.data(), &SchematicPagesDock::renameSchematicTriggered,
          this, &SchematicEditor::renameSchematic);
  addDockWidget(Qt::LeftDockWidgetArea, mDockPages.data(), Qt::Vertical);

  // ERC Messages.
  mDockErc.reset(new ErcMsgDock(mProject));
  addDockWidget(Qt::RightDockWidgetArea, mDockErc.data(), Qt::Vertical);

  // Set reasonable default dock size.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  resizeDocks(
      {
          mDockPages.data(),
          mDockErc.data(),
      },
      {
          120,
          150,
      },
      Qt::Horizontal);
#endif
}

void SchematicEditor::createMenus() noexcept {
  MenuBuilder mb(mUi->menuBar);

  // File.
  mb.newMenu(&MenuBuilder::createFileMenu);
  mb.addAction(mActionSaveProject);
  mb.addAction(mActionFileManager);
  mb.addSeparator();
  {
    MenuBuilder smb(mb.addSubMenu(&MenuBuilder::createExportMenu));
    smb.addAction(mActionExportPdf);
    smb.addAction(mActionExportImage);
    smb.addAction(mActionExportLppz);
  }
  {
    MenuBuilder smb(mb.addSubMenu(&MenuBuilder::createProductionDataMenu));
    smb.addAction(mActionGenerateBom);
  }
  mb.addSeparator();
  mb.addAction(mActionPrint);
  mb.addAction(mActionOrderPcb);
  mb.addSeparator();
  mb.addAction(mActionCloseWindow);
  mb.addAction(mActionCloseProject);
  mb.addSeparator();
  mb.addAction(mActionQuit);

  // Edit.
  mb.newMenu(&MenuBuilder::createEditMenu);
  mb.addAction(mActionUndo);
  mb.addAction(mActionRedo);
  mb.addSeparator();
  mb.addAction(mActionSelectAll);
  mb.addSeparator();
  mb.addAction(mActionCut);
  mb.addAction(mActionCopy);
  mb.addAction(mActionPaste);
  mb.addAction(mActionRemove);
  mb.addSeparator();
  mb.addAction(mActionRotateCcw);
  mb.addAction(mActionRotateCw);
  mb.addAction(mActionMirrorHorizontal);
  mb.addAction(mActionMirrorVertical);
  mb.addSeparator();
  mb.addAction(mActionFind);
  mb.addAction(mActionFindNext);
  mb.addAction(mActionFindPrevious);
  mb.addSeparator();
  mb.addAction(mActionProperties);

  // View.
  mb.newMenu(&MenuBuilder::createViewMenu);
  mb.addAction(mActionGridProperties);
  mb.addAction(mActionGridIncrease.data());
  mb.addAction(mActionGridDecrease.data());
  mb.addSeparator();
  mb.addAction(mActionZoomIn);
  mb.addAction(mActionZoomOut);
  mb.addAction(mActionZoomFit);
  mb.addSeparator();
  {
    MenuBuilder smb(mb.addSubMenu(&MenuBuilder::createGoToDockMenu));
    smb.addAction(mActionDockPages);
    smb.addAction(mActionDockErc);
  }
  {
    MenuBuilder smb(mb.addSubMenu(&MenuBuilder::createDocksVisibilityMenu));
    smb.addAction(mDockPages->toggleViewAction());
    smb.addAction(mDockErc->toggleViewAction());
  }

  // Schematic.
  mb.newMenu(&MenuBuilder::createSchematicMenu);
  mb.addAction(mActionNewSheet);
  mb.addAction(mActionRenameSheet);
  mb.addAction(mActionRemoveSheet);

  // Project.
  mb.newMenu(&MenuBuilder::createProjectMenu);
  mb.addAction(mActionNetClasses);
  mb.addAction(mActionProjectProperties);
  mb.addAction(mActionProjectSettings);
  mb.addSeparator();
  mb.addAction(mActionUpdateLibrary);

  // Tools.
  mb.newMenu(&MenuBuilder::createToolsMenu);
  mb.addAction(mActionToolSelect);
  mb.addAction(mActionToolWire);
  mb.addAction(mActionToolNetLabel);
  mb.addAction(mActionToolComponent);

  // Help.
  mb.newMenu(&MenuBuilder::createHelpMenu);
  mb.addAction(mActionOnlineDocumentation);
  mb.addAction(mActionKeyboardShortcutsReference);
  mb.addAction(mActionWebsite);
  mb.addSeparator();
  mb.addAction(mActionAboutLibrePcb);
  mb.addAction(mActionAboutQt);
}

bool SchematicEditor::graphicsViewEventHandler(QEvent* event) {
  Q_ASSERT(event);
  switch (event->type()) {
    case QEvent::GraphicsSceneMouseMove: {
      auto* e = dynamic_cast<QGraphicsSceneMouseEvent*>(event);
      Q_ASSERT(e);
      mFsm->processGraphicsSceneMouseMoved(*e);
      break;
    }

    case QEvent::GraphicsSceneMousePress: {
      auto* e = dynamic_cast<QGraphicsSceneMouseEvent*>(event);
      Q_ASSERT(e);
      switch (e->button()) {
        case Qt::LeftButton: {
          mFsm->processGraphicsSceneLeftMouseButtonPressed(*e);
          break;
        }
        default: { break; }
      }
      break;
    }

    case QEvent::GraphicsSceneMouseRelease: {
      auto* e = dynamic_cast<QGraphicsSceneMouseEvent*>(event);
      Q_ASSERT(e);
      switch (e->button()) {
        case Qt::LeftButton: {
          mFsm->processGraphicsSceneLeftMouseButtonReleased(*e);
          break;
        }
        case Qt::RightButton: {
          mFsm->processGraphicsSceneRightMouseButtonReleased(*e);
          break;
        }
        default: { break; }
      }
      break;
    }

    case QEvent::GraphicsSceneMouseDoubleClick: {
      auto* e = dynamic_cast<QGraphicsSceneMouseEvent*>(event);
      Q_ASSERT(e);
      switch (e->button()) {
        case Qt::LeftButton: {
          mFsm->processGraphicsSceneLeftMouseButtonDoubleClicked(*e);
          break;
        }
        default: { break; }
      }
      break;
    }

    case QEvent::KeyPress: {
      auto* e = dynamic_cast<QKeyEvent*>(event);
      Q_ASSERT(e);
      if (mFsm->processKeyPressed(*e)) {
        return true;
      }
      switch (e->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
          // Allow handling these keys by the graphics view for scrolling.
          return false;
        default:
          break;
      }
      break;
    }

    case QEvent::KeyRelease: {
      auto* e = dynamic_cast<QKeyEvent*>(event);
      Q_ASSERT(e);
      mFsm->processKeyReleased(*e);
      break;
    }

    default: { break; }
  }

  // Always accept graphics scene events, even if we do not react on some of
  // the events! This will give us the full control over the graphics scene.
  // Otherwise, the graphics scene can react on some events and disturb our
  // state machine. Only the wheel event is ignored because otherwise the
  // view will not allow to zoom with the mouse wheel.
  return (event->type() != QEvent::GraphicsSceneWheel);
}

void SchematicEditor::toolActionGroupChangeTriggered(
    const QVariant& newTool) noexcept {
  // Note: Converting the QVariant to SchematicEditorFsm::State doesn't work
  // with some Qt versions, thus we convert to int instead. Fixed in:
  // https://codereview.qt-project.org/c/qt/qtbase/+/159277/
  switch (newTool.value<int>()) {
    case SchematicEditorFsm::State::SELECT:
      mFsm->processSelect();
      break;
    case SchematicEditorFsm::State::DRAW_WIRE:
      mFsm->processDrawWire();
      break;
    case SchematicEditorFsm::State::ADD_NETLABEL:
      mFsm->processAddNetLabel();
      break;
    case SchematicEditorFsm::State::ADD_COMPONENT:
      mFsm->processAddComponent();
      break;
    default:
      Q_ASSERT(false);
      qCritical() << "Unknown tool triggered!";
      break;
  }
}

void SchematicEditor::addSchematic() noexcept {
  bool ok = false;
  QString name = QInputDialog::getText(this, tr("Add schematic page"),
                                       tr("Choose a name:"), QLineEdit::Normal,
                                       tr("New Page"), &ok);
  if (!ok) return;

  try {
    CmdSchematicAdd* cmd =
        new CmdSchematicAdd(mProject, ElementName(name));  // can throw
    mProjectEditor.getUndoStack().execCmd(cmd);
    setActiveSchematicIndex(mProject.getSchematics().count() - 1);
  } catch (Exception& e) {
    QMessageBox::critical(this, tr("Error"), e.getMsg());
  }
}

void SchematicEditor::removeSchematic(int index) noexcept {
  Schematic* schematic = mProject.getSchematicByIndex(index);
  if (!schematic) return;

  try {
    CmdSchematicRemove* cmd = new CmdSchematicRemove(mProject, *schematic);
    mProjectEditor.getUndoStack().execCmd(cmd);
  } catch (Exception& e) {
    QMessageBox::critical(this, tr("Error"), e.getMsg());
  }
}

void SchematicEditor::renameSchematic(int index) noexcept {
  Schematic* schematic = mProject.getSchematicByIndex(index);
  if (!schematic) return;

  bool ok = false;
  QString name =
      QInputDialog::getText(this, tr("Rename sheet"), tr("Choose new name:"),
                            QLineEdit::Normal, *schematic->getName(), &ok);
  if (!ok) return;

  try {
    QScopedPointer<CmdSchematicEdit> cmd(new CmdSchematicEdit(*schematic));
    cmd->setName(ElementName(cleanElementName(name)));  // can throw
    mProjectEditor.getUndoStack().execCmd(cmd.take());
  } catch (Exception& e) {
    QMessageBox::critical(this, tr("Error"), e.getMsg());
  }
}

QList<SI_Symbol*> SchematicEditor::getSearchCandidates() noexcept {
  QList<SI_Symbol*> candidates;
  foreach (const Schematic* schematic, mProject.getSchematics()) {
    Q_ASSERT(schematic);
    candidates += schematic->getSymbols();
  }
  return candidates;
}

QStringList SchematicEditor::getSearchToolBarCompleterList() noexcept {
  QStringList list;
  foreach (SI_Symbol* symbol, getSearchCandidates()) {
    list.append(symbol->getName());
  }
  return list;
}

void SchematicEditor::goToSymbol(const QString& name, int index) noexcept {
  QList<SI_Symbol*> symbolCandidates;
  foreach (SI_Symbol* symbol, getSearchCandidates()) {
    if (symbol->getName().startsWith(name, Qt::CaseInsensitive)) {
      symbolCandidates.append(symbol);
    }
  }

  // Sort by name for a natural order of results.
  QCollator collator;
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setIgnorePunctuation(false);
  collator.setNumericMode(true);
  std::sort(symbolCandidates.begin(), symbolCandidates.end(),
            [&collator](const SI_Symbol* a, const SI_Symbol* b) {
              return collator(a->getName(), b->getName());
            });

  if (symbolCandidates.count()) {
    while (index < 0) {
      index += symbolCandidates.count();
    }
    index %= symbolCandidates.count();
    SI_Symbol* symbol = symbolCandidates[index];
    Schematic& schematic = symbol->getSchematic();
    if (setActiveSchematicIndex(mProject.getSchematics().indexOf(&schematic))) {
      schematic.clearSelection();
      symbol->setSelected(true);
      QRectF rect = symbol->getBoundingRect();
      // Zoom to a rectangle relative to the maximum symbol dimension. The
      // symbol is 1/4th of the screen.
      qreal margin = 1.5f * std::max(rect.size().width(), rect.size().height());
      rect.adjust(-margin, -margin, margin, margin);
      mUi->graphicsView->zoomToRect(rect);
    }
  }
}

void SchematicEditor::updateComponentToolbarIcons() noexcept {
  QString suffix = useIeee315Symbols() ? "us" : "eu";
  mActionComponentResistor->setIcon(
      QIcon(":/img/library/resistor_" % suffix % ".png"));
  mActionComponentInductor->setIcon(
      QIcon(":/img/library/inductor_" % suffix % ".png"));
  mActionComponentCapacitorBipolar->setIcon(
      QIcon(":/img/library/bipolar_capacitor_" % suffix % ".png"));
  mActionComponentCapacitorUnipolar->setIcon(
      QIcon(":/img/library/unipolar_capacitor_" % suffix % ".png"));
}

void SchematicEditor::setGridProperties(const GridProperties& grid,
                                        bool applyToSchematics) noexcept {
  mUi->graphicsView->setGridProperties(grid);
  mUi->statusbar->setLengthUnit(grid.getUnit());

  if (applyToSchematics) {
    foreach (Schematic* schematic, mProject.getSchematics()) {
      schematic->setGridProperties(grid);
    }
  }
}

void SchematicEditor::execGridPropertiesDialog() noexcept {
  if (const Schematic* activeSchematic = getActiveSchematic()) {
    GridSettingsDialog dialog(activeSchematic->getGridProperties(), this);
    connect(
        &dialog, &GridSettingsDialog::gridPropertiesChanged,
        [this](const GridProperties& grid) { setGridProperties(grid, false); });
    if (dialog.exec()) {
      setGridProperties(dialog.getGrid(), true);
    }
  }
}

void SchematicEditor::execGraphicsExportDialog(
    GraphicsExportDialog::Output output, const QString& settingsKey) noexcept {
  try {
    // Determine default file path.
    QString projectName =
        FilePath::cleanFileName(*mProject.getMetadata().getName(),
                                FilePath::ReplaceSpaces | FilePath::KeepCase);
    QString projectVersion =
        FilePath::cleanFileName(mProject.getMetadata().getVersion(),
                                FilePath::ReplaceSpaces | FilePath::KeepCase);
    QString relativePath =
        QString("output/%1/%2_Schematics").arg(projectVersion, projectName);
    FilePath defaultFilePath = mProject.getPath().getPathTo(relativePath);

    // Copy all schematic pages to allow processing them in worker threads.
    const int count = mProject.getSchematics().count();
    QProgressDialog progress(tr("Preparing schematics..."), tr("Cancel"), 0,
                             count, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    QList<std::shared_ptr<GraphicsPagePainter>> pages;
    for (int i = 0; i < count; ++i) {
      pages.append(
          std::make_shared<SchematicPainter>(*mProject.getSchematicByIndex(i)));
      progress.setValue(i + 1);
      if (progress.wasCanceled()) {
        return;
      }
    }

    // Show dialog, which will do all the work.
    GraphicsExportDialog dialog(
        GraphicsExportDialog::Mode::Schematic, output, pages,
        getActiveSchematicIndex(), *mProject.getMetadata().getName(), 0,
        defaultFilePath,
        mProjectEditor.getWorkspace().getSettings().defaultLengthUnit.get(),
        "schematic_editor/" % settingsKey, this);
    connect(&dialog, &GraphicsExportDialog::requestOpenFile, this,
            [this](const FilePath& fp) {
              DesktopServices services(
                  mProjectEditor.getWorkspace().getSettings(), true);
              services.openFile(fp);
            });
    dialog.exec();
  } catch (const Exception& e) {
    QMessageBox::warning(this, tr("Error"), e.getMsg());
  }
}

bool SchematicEditor::useIeee315Symbols() const noexcept {
  foreach (const QString& norm, mProject.getSettings().getNormOrder()) {
    if (norm.toLower() == "ieee 315") {
      return true;
    } else if (norm.toLower() == "iec 60617") {
      return false;
    }
  }
  return false;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
