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
#include "bi_footprintpad.h"

#include "../../../graphics/graphicslayer.h"
#include "../../../library/dev/device.h"
#include "../../../library/pkg/footprint.h"
#include "../../../library/pkg/package.h"
#include "../../../utils/transform.h"
#include "../../circuit/componentinstance.h"
#include "../../circuit/componentsignalinstance.h"
#include "../../circuit/netsignal.h"
#include "../boarddesignrules.h"
#include "bi_device.h"
#include "bi_netsegment.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

BI_FootprintPad::BI_FootprintPad(BI_Device& device, const Uuid& padUuid)
  : BI_Base(device.getBoard()),
    mDevice(device),
    mFootprintPad(nullptr),
    mPackagePad(nullptr),
    mComponentSignalInstance(nullptr) {
  mFootprintPad =
      mDevice.getLibFootprint().getPads().get(padUuid).get();  // can throw
  if (mFootprintPad->getPackagePadUuid()) {
    mPackagePad = mDevice.getLibPackage()
                      .getPads()
                      .get(*mFootprintPad->getPackagePadUuid())
                      .get();  // can throw

    tl::optional<Uuid> cmpSignalUuid =
        mDevice.getLibDevice()
            .getPadSignalMap()
            .get(*mFootprintPad->getPackagePadUuid())
            ->getSignalUuid();  // can throw
    if (cmpSignalUuid) {
      mComponentSignalInstance =
          mDevice.getComponentInstance().getSignalInstance(*cmpSignalUuid);
      connect(mComponentSignalInstance,
              &ComponentSignalInstance::netSignalChanged, this,
              &BI_FootprintPad::componentSignalInstanceNetSignalChanged);
    }
  }

  if (NetSignal* netsignal = getCompSigInstNetSignal()) {
    mHighlightChangedConnection =
        connect(netsignal, &NetSignal::highlightedChanged,
                [this]() { mGraphicsItem->update(); });
    mNetSignalNameChangedConnection =
        connect(netsignal, &NetSignal::nameChanged,
                [this]() { mGraphicsItem->updateCacheAndRepaint(); });
  }

  mGraphicsItem.reset(new BGI_FootprintPad(*this));
  updatePosition();

  // connect to the "attributes changed" signal of the footprint
  connect(&mDevice, &BI_Device::attributesChanged, this,
          &BI_FootprintPad::deviceAttributesChanged);
}

BI_FootprintPad::~BI_FootprintPad() {
  Q_ASSERT(!isUsed());
  mGraphicsItem.reset();
}

/*******************************************************************************
 *  Getters
 ******************************************************************************/

bool BI_FootprintPad::getMirrored() const noexcept {
  return mDevice.getMirrored();
}

const Uuid& BI_FootprintPad::getLibPadUuid() const noexcept {
  return mFootprintPad->getUuid();
}

QString BI_FootprintPad::getDisplayText() const noexcept {
  NetSignal* signal = getCompSigInstNetSignal();
  if (signal && mPackagePad) {
    return QString("%1:\n%2").arg(*mPackagePad->getName(), *signal->getName());
  } else if (mPackagePad) {
    return *mPackagePad->getName();
  } else {
    return QString();  // Unconnected pad, no name to display...
  }
}

FootprintPad::ComponentSide BI_FootprintPad::getComponentSide() const noexcept {
  if (getMirrored()) {
    return (mFootprintPad->getComponentSide() ==
            FootprintPad::ComponentSide::Top)
        ? FootprintPad::ComponentSide::Bottom
        : FootprintPad::ComponentSide::Top;
  } else {
    return mFootprintPad->getComponentSide();
  }
}

QString BI_FootprintPad::getComponentSideLayerName() const noexcept {
  if (getMirrored())
    return GraphicsLayer::getMirroredLayerName(
        mFootprintPad->getComponentSideLayerName());
  else
    return mFootprintPad->getComponentSideLayerName();
}

bool BI_FootprintPad::isOnLayer(const QString& layerName) const noexcept {
  if (getMirrored()) {
    return mFootprintPad->isOnLayer(
        GraphicsLayer::getMirroredLayerName(layerName));
  } else {
    return mFootprintPad->isOnLayer(layerName);
  }
}

NetSignal* BI_FootprintPad::getCompSigInstNetSignal() const noexcept {
  if (mComponentSignalInstance) {
    return mComponentSignalInstance->getNetSignal();
  } else {
    return nullptr;
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void BI_FootprintPad::addToBoard() {
  if (isAddedToBoard() || isUsed()) {
    throw LogicError(__FILE__, __LINE__);
  }
  if (mComponentSignalInstance) {
    mComponentSignalInstance->registerFootprintPad(*this);  // can throw
  }
  componentSignalInstanceNetSignalChanged(nullptr, getCompSigInstNetSignal());
  BI_Base::addToBoard(mGraphicsItem.data());
}

void BI_FootprintPad::removeFromBoard() {
  if ((!isAddedToBoard()) || isUsed()) {
    throw LogicError(__FILE__, __LINE__);
  }
  if (mComponentSignalInstance) {
    mComponentSignalInstance->unregisterFootprintPad(*this);  // can throw
  }
  componentSignalInstanceNetSignalChanged(getCompSigInstNetSignal(), nullptr);
  BI_Base::removeFromBoard(mGraphicsItem.data());
}

void BI_FootprintPad::registerNetLine(BI_NetLine& netline) {
  if ((!isAddedToBoard()) || (mRegisteredNetLines.contains(&netline)) ||
      (netline.getBoard() != mBoard)) {
    throw LogicError(__FILE__, __LINE__);
  }
  if (netline.getNetSegment().getNetSignal() != getCompSigInstNetSignal()) {
    throw RuntimeError(
        __FILE__, __LINE__,
        QString("Trace of net \"%1\" is not allowed to be connected to "
                "pad \"%2\" of device \"%3\" (%4) since it is connected to the "
                "net \"%5\".")
            .arg(netline.getNetSegment().getNetNameToDisplay(),
                 getPadNameOrUuid(), getComponentInstanceName(),
                 getLibraryDeviceName(), getNetSignalName()));
  }
  // if (!isOnLayer(netline.getLayer().getName())) {
  //  throw RuntimeError(
  //      __FILE__, __LINE__,
  //      QString("Trace on layer \"%1\" cannot be connected to the pad \"%2\" "
  //              "of device \"%3\" (%4) since it is on layer \"%5\".")
  //          .arg(netline.getLayer().getName(), getPadNameOrUuid(),
  //               getComponentInstanceName(), getLibraryDeviceName(),
  //               getLayerName()));
  //}
  foreach (const BI_NetLine* l, mRegisteredNetLines) {
    if (&l->getNetSegment() != &netline.getNetSegment()) {
      throw RuntimeError(
          __FILE__, __LINE__,
          QString("There are traces from multiple net segments connected to "
                  "the pad \"%1\" of device \"%2\" (%3).")
              .arg(getPadNameOrUuid(), getComponentInstanceName(),
                   getLibraryDeviceName()));
    }
  }
  mRegisteredNetLines.insert(&netline);
  netline.updateLine();
}

void BI_FootprintPad::unregisterNetLine(BI_NetLine& netline) {
  if ((!isAddedToBoard()) || (!mRegisteredNetLines.contains(&netline))) {
    throw LogicError(__FILE__, __LINE__);
  }
  mRegisteredNetLines.remove(&netline);
  netline.updateLine();
}

void BI_FootprintPad::updatePosition() noexcept {
  Transform transform(mDevice);
  mPosition = transform.map(mFootprintPad->getPosition());
  mRotation = transform.map(mFootprintPad->getRotation());

  Angle rot = mRotation;
  if (mDevice.getMirrored()) {
    rot = Angle::deg180() - rot;
  }

  QTransform t;
  if (mDevice.getMirrored()) t.scale(qreal(-1), qreal(1));
  t.rotate(-rot.toDeg());
  mGraphicsItem->setTransform(t);
  mGraphicsItem->setPos(mPosition.toPxQPointF());
  mGraphicsItem->updateCacheAndRepaint();
  foreach (BI_NetLine* netline, mRegisteredNetLines) { netline->updateLine(); }
}

/*******************************************************************************
 *  Inherited from BI_Base
 ******************************************************************************/

QPainterPath BI_FootprintPad::getGrabAreaScenePx() const noexcept {
  return mGraphicsItem->sceneTransform().map(mGraphicsItem->shape());
}

bool BI_FootprintPad::isSelectable() const noexcept {
  return mGraphicsItem->isSelectable();
}

void BI_FootprintPad::setSelected(bool selected) noexcept {
  BI_Base::setSelected(selected);
  mGraphicsItem->update();
}

/*QVector<Path> BI_FootprintPad::getAnnularRingOutlines() const noexcept {
  QVector<Path> paths;
  for (const Hole& h : mFootprintPad->getHoles()) {
    const UnsignedLength annularWidth =
        mBoard.getDesignRules().calcPadAnnularRing(*h.getDiameter());
    paths.append(h.getPath()->toOutlineStrokes(
        h.getDiameter() + UnsignedLength(annularWidth * 2)));
  }
  return paths;
}*/

QVector<Path> BI_FootprintPad::getOutlinesOnLayer(
    const QString& layerName, const UnsignedLength& expansion) const noexcept {
  const bool hasTopSide = mFootprintPad->isTht() ||
      (getComponentSide() == FootprintPad::ComponentSide::Top);
  const bool hasBottomSide = mFootprintPad->isTht() ||
      (getComponentSide() == FootprintPad::ComponentSide::Bottom);

  QVector<Path> p;
  if (((layerName == GraphicsLayer::sTopCopper) && hasTopSide) ||
      ((layerName == GraphicsLayer::sBotCopper) && hasBottomSide)) {
    // Outer copper.
    p.append(mFootprintPad->getOutline(*expansion));
  } else if (GraphicsLayer::isCopperLayer(layerName) &&
             GraphicsLayer::isInnerLayer(layerName)) {
    // Inner copper (annular rings).
    for (const Hole& h : mFootprintPad->getHoles()) {
      const UnsignedLength totalExpansion =
          mBoard.getDesignRules().calcPadAnnularRing(*h.getDiameter()) +
          expansion;
      p.append(h.getPath()->toOutlineStrokes(
          h.getDiameter() + UnsignedLength(totalExpansion * 2)));
    }
  } else if (((layerName == GraphicsLayer::sTopStopMask) && hasTopSide) ||
             ((layerName == GraphicsLayer::sBotStopMask) && hasBottomSide)) {
    // Stop mask.
    const PositiveLength size =
        qMin(mFootprintPad->getWidth(), mFootprintPad->getHeight());
    const Length totalExpansion =
        *mBoard.getDesignRules().calcStopMaskClearance(*size) + expansion;
    p.append(mFootprintPad->getOutline(totalExpansion));
  } else if (((layerName == GraphicsLayer::sTopSolderPaste) && hasTopSide) ||
             ((layerName == GraphicsLayer::sBotSolderPaste) && hasBottomSide)) {
    // Solder paste.
    const PositiveLength size =
        qMin(mFootprintPad->getWidth(), mFootprintPad->getHeight());
    const Length totalExpansion =
        *mBoard.getDesignRules().calcCreamMaskClearance(*size) + expansion;
    p.append(mFootprintPad->getOutline(totalExpansion));
  }
  return p;
}

QPainterPath BI_FootprintPad::toQPainterPath(const QString& layerName) const
    noexcept {
  const QVector<Path> outlines = getOutlinesOnLayer(layerName);
  if (outlines.isEmpty()) {
    return QPainterPath();
  }

  QPainterPath p;
  p.setFillRule(Qt::OddEvenFill);  // Important to subtract the holes!
  p.addPath(Path::toQPainterPathPx(outlines, true));
  if (GraphicsLayer::isCopperLayer(layerName) ||
      (layerName == GraphicsLayer::sBoardPadsTht)) {
    QPainterPath holeAreas;
    for (const Hole& hole : mFootprintPad->getHoles()) {
      for (const Path& path :
           hole.getPath()->toOutlineStrokes(hole.getDiameter())) {
        holeAreas.addPath(path.toQPainterPathPx());
      }
    }
    p.addPath(holeAreas);
  }
  return p;
}

/*QPainterPath BI_FootprintPad::toHolesQPainterPath(bool withAnnularRing) const
    noexcept {
  QPainterPath p;
  for (const Hole& hole : mFootprintPad->getHoles()) {
    const UnsignedLength expansion(
        withAnnularRing
            ? (mBoard.getDesignRules().calcPadAnnularRing(*hole.getDiameter()) *
               2)
            : 0);
    for (const Path& path :
         hole.getPath()->toOutlineStrokes(hole.getDiameter() + expansion)) {
      p.addPath(path.toQPainterPathPx());
    }
  }
  return p;
}

QPainterPath BI_FootprintPad::toAnnularRingQPainterPath() const noexcept {
  QPainterPath p;
  p.setFillRule(Qt::OddEvenFill);  // Important to subtract the holes!
  p.addPath(toHolesQPainterPath(true));
  p.addPath(toHolesQPainterPath(false));
  return p;
}

QPainterPath BI_FootprintPad::toOuterQPainterPath() const noexcept {
  QPainterPath p;
  p.setFillRule(Qt::OddEvenFill);  // Important to subtract the holes!
  p.addPath(getOutline().toQPainterPathPx());
  p.addPath(toHolesQPainterPath(true));
  return p;
}*/

// Path BI_FootprintPad::getOutline(const Length& expansion) const noexcept {
//  return mFootprintPad->getOutline(expansion);
//}

QVector<Path> BI_FootprintPad::getSceneOutlinesOnLayer(
    const QString& layerName, const UnsignedLength& expansion) const noexcept {
  const Transform transform(mDevice.getPosition(), mDevice.getRotation(),
                            mDevice.getMirrored());

  QVector<Path> paths = getOutlinesOnLayer(layerName, expansion);
  for (Path& path : paths) {
    path = transform.map(path.rotated(mFootprintPad->getRotation())
                             .translated(mFootprintPad->getPosition()));
  }
  return paths;
}

TraceAnchor BI_FootprintPad::toTraceAnchor() const noexcept {
  return TraceAnchor::pad(mDevice.getComponentInstanceUuid(),
                          mFootprintPad->getUuid());
}

/*******************************************************************************
 *  Private Slots
 ******************************************************************************/

void BI_FootprintPad::deviceAttributesChanged() {
  mGraphicsItem->updateCacheAndRepaint();
}

void BI_FootprintPad::componentSignalInstanceNetSignalChanged(NetSignal* from,
                                                              NetSignal* to) {
  Q_ASSERT(!isUsed());  // no netlines must be connected when netsignal changes!
  if (mHighlightChangedConnection) {
    disconnect(mHighlightChangedConnection);
    mHighlightChangedConnection = QMetaObject::Connection();
  }
  if (mNetSignalNameChangedConnection) {
    disconnect(mNetSignalNameChangedConnection);
    mNetSignalNameChangedConnection = QMetaObject::Connection();
  }
  if (to) {
    mHighlightChangedConnection =
        connect(to, &NetSignal::highlightedChanged,
                [this]() { mGraphicsItem->update(); });
    mNetSignalNameChangedConnection =
        connect(to, &NetSignal::nameChanged,
                [this]() { mGraphicsItem->updateCacheAndRepaint(); });
  }
  mBoard.scheduleAirWiresRebuild(from);
  mBoard.scheduleAirWiresRebuild(to);
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

QString BI_FootprintPad::getLibraryDeviceName() const noexcept {
  return *mDevice.getLibDevice().getNames().getDefaultValue();
}

QString BI_FootprintPad::getComponentInstanceName() const noexcept {
  return *mDevice.getComponentInstance().getName();
}

QString BI_FootprintPad::getPadNameOrUuid() const noexcept {
  return mPackagePad ? *mPackagePad->getName()
                     : mFootprintPad->getUuid().toStr();
}

QString BI_FootprintPad::getNetSignalName() const noexcept {
  if (const NetSignal* signal = getCompSigInstNetSignal()) {
    return *signal->getName();
  } else {
    return QString();
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
