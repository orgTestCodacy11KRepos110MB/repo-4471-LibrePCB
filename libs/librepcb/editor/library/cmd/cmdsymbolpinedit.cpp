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
#include "cmdsymbolpinedit.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdSymbolPinEdit::CmdSymbolPinEdit(std::shared_ptr<SymbolPin> pin) noexcept
  : UndoCommand(tr("Edit pin")),
    mPin(pin),
    mOldName(pin->getName()),
    mNewName(mOldName),
    mOldLength(pin->getLength()),
    mNewLength(mOldLength),
    mOldPos(pin->getPosition()),
    mNewPos(mOldPos),
    mOldRotation(pin->getRotation()),
    mNewRotation(mOldRotation),
    mOldNamePosition(pin->getNamePosition()),
    mNewNamePosition(mOldNamePosition),
    mOldNameRotation(pin->getNameRotation()),
    mNewNameRotation(mOldNameRotation),
    mOldNameHeight(pin->getNameHeight()),
    mNewNameHeight(mOldNameHeight),
    mOldNameAlignment(pin->getNameAlignment()),
    mNewNameAlignment(mOldNameAlignment) {
  Q_ASSERT(mPin);
}

CmdSymbolPinEdit::~CmdSymbolPinEdit() noexcept {
  if (!wasEverExecuted()) {
    try {
      performUndo();
    } catch (...) {
      qCritical() << "Undo failed in CmdSymbolPinEdit destructor!";
    }
  }
}

/*******************************************************************************
 *  Setters
 ******************************************************************************/

void CmdSymbolPinEdit::setName(const CircuitIdentifier& name,
                               bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewName = name;
  if (immediate) mPin->setName(mNewName);
}

void CmdSymbolPinEdit::setLength(const UnsignedLength& length,
                                 bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewLength = length;
  if (immediate) mPin->setLength(mNewLength);
}

void CmdSymbolPinEdit::setNamePosition(const Point& position,
                                       bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewNamePosition = position;
  if (immediate) mPin->setNamePosition(mNewNamePosition);
}

void CmdSymbolPinEdit::setNameRotation(const Angle& rotation,
                                       bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewNameRotation = rotation;
  if (immediate) mPin->setNameRotation(mNewNameRotation);
}

void CmdSymbolPinEdit::setNameHeight(const PositiveLength& height,
                                     bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewNameHeight = height;
  if (immediate) mPin->setNameHeight(mNewNameHeight);
}

void CmdSymbolPinEdit::setNameAlignment(const Alignment& align,
                                        bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewNameAlignment = align;
  if (immediate) mPin->setNameAlignment(mNewNameAlignment);
}

void CmdSymbolPinEdit::setPosition(const Point& pos, bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos = pos;
  if (immediate) mPin->setPosition(mNewPos);
}

void CmdSymbolPinEdit::translate(const Point& deltaPos,
                                 bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos += deltaPos;
  if (immediate) mPin->setPosition(mNewPos);
}

void CmdSymbolPinEdit::snapToGrid(const PositiveLength& gridInterval,
                                  bool immediate) noexcept {
  setPosition(mNewPos.mappedToGrid(gridInterval), immediate);
}

void CmdSymbolPinEdit::setRotation(const Angle& angle,
                                   bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewRotation = angle;
  if (immediate) mPin->setRotation(mNewRotation);
}

void CmdSymbolPinEdit::rotate(const Angle& angle, const Point& center,
                              bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos.rotate(angle, center);
  mNewRotation += angle;
  if (immediate) {
    mPin->setPosition(mNewPos);
    mPin->setRotation(mNewRotation);
  }
}

void CmdSymbolPinEdit::mirror(Qt::Orientation orientation, const Point& center,
                              bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos.mirror(orientation, center);
  if (orientation == Qt::Horizontal) {
    mNewRotation = Angle::deg180() - mNewRotation;
  } else {
    mNewRotation = -mNewRotation;
  }
  if (immediate) {
    mPin->setPosition(mNewPos);
    mPin->setRotation(mNewRotation);
  }
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdSymbolPinEdit::performExecute() {
  performRedo();  // can throw

  if (mNewName != mOldName) return true;
  if (mNewLength != mOldLength) return true;
  if (mNewPos != mOldPos) return true;
  if (mNewRotation != mOldRotation) return true;
  if (mNewNamePosition != mOldNamePosition) return true;
  if (mNewNameRotation != mOldNameRotation) return true;
  if (mNewNameHeight != mOldNameHeight) return true;
  if (mNewNameAlignment != mOldNameAlignment) return true;
  return false;
}

void CmdSymbolPinEdit::performUndo() {
  mPin->setName(mOldName);
  mPin->setLength(mOldLength);
  mPin->setPosition(mOldPos);
  mPin->setRotation(mOldRotation);
  mPin->setNamePosition(mOldNamePosition);
  mPin->setNameRotation(mOldNameRotation);
  mPin->setNameHeight(mOldNameHeight);
  mPin->setNameAlignment(mOldNameAlignment);
}

void CmdSymbolPinEdit::performRedo() {
  mPin->setName(mNewName);
  mPin->setLength(mNewLength);
  mPin->setPosition(mNewPos);
  mPin->setRotation(mNewRotation);
  mPin->setNamePosition(mNewNamePosition);
  mPin->setNameRotation(mNewNameRotation);
  mPin->setNameHeight(mNewNameHeight);
  mPin->setNameAlignment(mNewNameAlignment);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
