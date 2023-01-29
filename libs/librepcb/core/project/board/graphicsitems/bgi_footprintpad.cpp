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
#include "bgi_footprintpad.h"

#include "../../../application.h"
#include "../../../library/pkg/footprint.h"
#include "../../../library/pkg/package.h"
#include "../../circuit/netsignal.h"
#include "../../project.h"
#include "../../projectsettings.h"
#include "../board.h"
#include "../boarddesignrules.h"
#include "../boardlayerstack.h"
#include "../items/bi_device.h"
#include "../items/bi_footprintpad.h"

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

BGI_FootprintPad::BGI_FootprintPad(BI_FootprintPad& pad) noexcept
  : BGI_Base(),
    mPad(pad),
    mLibPad(pad.getLibPad()),
    mMainLayer(nullptr),
    mContent(),
    mOnLayerEditedSlot(*this, &BGI_FootprintPad::layerEdited) {
  mFont = qApp->getDefaultSansSerifFont();
  mFont.setPixelSize(1);

  updateCacheAndRepaint();
}

BGI_FootprintPad::~BGI_FootprintPad() noexcept {
}

/*******************************************************************************
 *  Getters
 ******************************************************************************/

bool BGI_FootprintPad::isSelectable() const noexcept {
  return mMainLayer && mMainLayer->isVisible();
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void BGI_FootprintPad::updateCacheAndRepaint() noexcept {
  prepareGeometryChange();

  setToolTip(mPad.getDisplayText());

  // set Z value
  if ((mLibPad.getComponentSide() == FootprintPad::ComponentSide::Bottom) !=
      mPad.getMirrored()) {
    setZValue(Board::ZValue_FootprintPadsBottom);
  } else {
    setZValue(Board::ZValue_FootprintPadsTop);
  }

  // set layers
  disconnectLayerEditedSlots();
  mMainLayer = getLayer(mLibPad.isTht() ? GraphicsLayer::sBoardPadsTht
                                        : mLibPad.getComponentSideLayerName());
  mContent.clear();
  mBoundingRect = QRectF();
  foreach (GraphicsLayer* layer,
           mPad.getBoard().getLayerStack().getAllLayers()) {
    const QPainterPath p = mPad.toQPainterPath(layer->getName());
    if (!p.isEmpty()) {
      mContent.append(std::make_pair(layer, p));
      mBoundingRect |= p.boundingRect();
    }
  }
  std::reverse(mContent.begin(), mContent.end());
  connectLayerEditedSlots();
  updateVisibility();

  mShape = mLibPad.getOutline().toQPainterPathPx();

  update();
}

/*******************************************************************************
 *  Inherited from QGraphicsItem
 ******************************************************************************/

void BGI_FootprintPad::paint(QPainter* painter,
                             const QStyleOptionGraphicsItem* option,
                             QWidget* widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  const NetSignal* netsignal = mPad.getCompSigInstNetSignal();
  bool highlight =
      mPad.isSelected() || (netsignal && netsignal->isHighlighted());

  if (mMainLayer && mMainLayer->isVisible()) {
    // draw areas for each layer
    foreach (const auto& content, mContent) {
      if (content.first->isEnabled()) {
        const bool isCopper = content.first->isCopperLayer();
        GraphicsLayer* layer = isCopper ? mMainLayer : content.first;
        if (content.first->isVisible()) {
          painter->setPen(Qt::NoPen);
          painter->setBrush(layer->getColor(highlight));
          painter->drawPath(content.second);
        } else if (isCopper) {
          painter->setPen(QPen(layer->getColor(highlight), 0));
          painter->setBrush(Qt::NoBrush);
          painter->drawPath(content.second);
        }
      }
    }
    // draw pad text
    painter->setFont(mFont);
    painter->setPen(mMainLayer->getColor(highlight).lighter(150));
    painter->drawText(mShape.boundingRect(), Qt::AlignCenter,
                      mPad.getDisplayText());
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

GraphicsLayer* BGI_FootprintPad::getLayer(QString name) const noexcept {
  if (mPad.getMirrored()) name = GraphicsLayer::getMirroredLayerName(name);
  return mPad.getDevice().getBoard().getLayerStack().getLayer(name);
}

void BGI_FootprintPad::connectLayerEditedSlots() noexcept {
  foreach (const auto& content, mContent) {
    content.first->onEdited.attach(mOnLayerEditedSlot);
  }
}

void BGI_FootprintPad::disconnectLayerEditedSlots() noexcept {
  foreach (const auto& content, mContent) {
    content.first->onEdited.detach(mOnLayerEditedSlot);
  }
}

void BGI_FootprintPad::layerEdited(const GraphicsLayer& layer,
                                   GraphicsLayer::Event event) noexcept {
  Q_UNUSED(layer);

  switch (event) {
    case GraphicsLayer::Event::ColorChanged:
      update();
      break;
    case GraphicsLayer::Event::HighlightColorChanged:
      update();
      break;
    case GraphicsLayer::Event::VisibleChanged:
    case GraphicsLayer::Event::EnabledChanged:
      updateVisibility();
      break;
    default:
      break;
  }
}

void BGI_FootprintPad::updateVisibility() noexcept {
  bool visible = false;
  foreach (const auto& content, mContent) {
    if (content.first->isVisible()) {
      visible = true;
      break;
    }
  }
  setVisible(visible);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
