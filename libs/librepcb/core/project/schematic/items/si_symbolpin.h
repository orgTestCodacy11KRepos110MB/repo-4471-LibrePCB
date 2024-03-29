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

#ifndef LIBREPCB_CORE_SI_SYMBOLPIN_H
#define LIBREPCB_CORE_SI_SYMBOLPIN_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../erc/if_ercmsgprovider.h"
#include "../graphicsitems/sgi_symbolpin.h"
#include "./si_netline.h"
#include "si_base.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Circuit;
class ComponentPinSignalMapItem;
class ComponentSignal;
class ComponentSignalInstance;
class ErcMsg;
class SI_Symbol;
class SymbolPin;

/*******************************************************************************
 *  Class SI_SymbolPin
 ******************************************************************************/

/**
 * @brief The SI_SymbolPin class
 */
class SI_SymbolPin final : public SI_Base,
                           public SI_NetLineAnchor,
                           public IF_ErcMsgProvider {
  Q_OBJECT
  DECLARE_ERC_MSG_CLASS_NAME(SI_SymbolPin)

public:
  // Constructors / Destructor
  SI_SymbolPin() = delete;
  SI_SymbolPin(const SI_SymbolPin& other) = delete;
  explicit SI_SymbolPin(SI_Symbol& symbol, const Uuid& pinUuid);
  ~SI_SymbolPin();

  // Getters

  /**
   * @brief Get the absolute position of the pin (scene coordinates)
   *
   * @return Absolute pin position
   */
  const Point& getPosition() const noexcept override { return mPosition; }

  /**
   * @brief Get the absolute rotation of the pin (scene coordinates)
   *
   * @return Absolute pin rotation
   */
  const Angle& getRotation() const noexcept { return mRotation; }

  const Uuid& getLibPinUuid() const noexcept;
  QString getDisplayText(bool returnCmpSignalNameIfEmpty = false,
                         bool returnPinNameIfEmpty = false) const noexcept;
  SI_Symbol& getSymbol() const noexcept { return mSymbol; }
  const SymbolPin& getLibPin() const noexcept { return *mSymbolPin; }
  ComponentSignalInstance* getComponentSignalInstance() const noexcept {
    return mComponentSignalInstance;
  }
  NetSignal* getCompSigInstNetSignal() const noexcept;
  SI_NetSegment* getNetSegmentOfLines() const noexcept;
  bool isRequired() const noexcept;
  bool isUsed() const noexcept { return (mRegisteredNetLines.count() > 0); }
  bool isVisibleJunction() const noexcept;
  NetLineAnchor toNetLineAnchor() const noexcept override;

  // General Methods
  void addToSchematic() override;
  void removeFromSchematic() override;
  void updatePosition(bool mirroredOrRotated) noexcept;

  // Inherited from SI_Base
  Type_t getType() const noexcept override {
    return SI_Base::Type_t::SymbolPin;
  }
  QPainterPath getGrabAreaScenePx() const noexcept override;
  void setSelected(bool selected) noexcept override;

  // Inherited from SI_NetLineAnchor
  void registerNetLine(SI_NetLine& netline) override;
  void unregisterNetLine(SI_NetLine& netline) override;
  const QSet<SI_NetLine*>& getNetLines() const noexcept override {
    return mRegisteredNetLines;
  }

  // Operator Overloadings
  SI_SymbolPin& operator=(const SI_SymbolPin& rhs) = delete;

private slots:

  void updateErcMessages() noexcept;

private:
  void netSignalChanged(NetSignal* from, NetSignal* to) noexcept;
  QString getLibraryComponentName() const noexcept;
  QString getComponentSignalNameOrPinUuid() const noexcept;
  QString getNetSignalName() const noexcept;

  // General
  SI_Symbol& mSymbol;
  const SymbolPin* mSymbolPin;
  const ComponentPinSignalMapItem* mPinSignalMapItem;
  ComponentSignalInstance* mComponentSignalInstance;
  QMetaObject::Connection mNetSignalChangedConnection;
  QMetaObject::Connection mNetSignalRenamedConnection;
  QMetaObject::Connection mHighlightChangedConnection;

  // Misc
  Point mPosition;
  Angle mRotation;
  QScopedPointer<SGI_SymbolPin> mGraphicsItem;

  // Registered Elements
  QSet<SI_NetLine*> mRegisteredNetLines;  ///< all registered netlines

  /// @brief The ERC message for unconnected required pins
  QScopedPointer<ErcMsg> mErcMsgUnconnectedRequiredPin;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
