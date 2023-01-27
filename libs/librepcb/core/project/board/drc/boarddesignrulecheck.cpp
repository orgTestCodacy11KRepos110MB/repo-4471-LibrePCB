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
#include "boarddesignrulecheck.h"

#include "../../../geometry/hole.h"
#include "../../../geometry/stroketext.h"
#include "../../../library/pkg/footprint.h"
#include "../../../library/pkg/footprintpad.h"
#include "../../../utils/clipperhelpers.h"
#include "../../../utils/toolbox.h"
#include "../../../utils/transform.h"
#include "../../circuit/circuit.h"
#include "../../circuit/componentinstance.h"
#include "../../circuit/netsignal.h"
#include "../../project.h"
#include "../board.h"
#include "../boardlayerstack.h"
#include "../items/bi_airwire.h"
#include "../items/bi_device.h"
#include "../items/bi_footprintpad.h"
#include "../items/bi_hole.h"
#include "../items/bi_netline.h"
#include "../items/bi_netsegment.h"
#include "../items/bi_plane.h"
#include "../items/bi_stroketext.h"
#include "../items/bi_via.h"
#include "boardclipperpathgenerator.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

BoardDesignRuleCheck::BoardDesignRuleCheck(Board& board, const Options& options,
                                           QObject* parent) noexcept
  : QObject(parent),
    mBoard(board),
    mOptions(options),
    mProgressStatus(),
    mMessages() {
}

BoardDesignRuleCheck::~BoardDesignRuleCheck() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void BoardDesignRuleCheck::execute() {
  emit started();
  emit progressPercent(5);

  mProgressStatus.clear();
  mMessages.clear();

  if (mOptions.rebuildPlanes) {
    rebuildPlanes(5, 15);
  }
  if (mOptions.checkCopperBoardClearance || mOptions.checkCopperNpthClearance) {
    checkCopperBoardClearances(15, 40);
  }
  if (mOptions.checkCopperCopperClearance) {
    checkCopperCopperClearances(40, 70);
  }
  if (mOptions.checkCopperWidth) {
    checkMinimumCopperWidth(70, 72);
  }
  if (mOptions.checkPthAnnularRing) {
    checkMinimumPthAnnularRing(72, 74);
  }
  if (mOptions.checkNpthDrillDiameter) {
    checkMinimumNpthDrillDiameter(74, 76);
  }
  if (mOptions.checkNpthSlotWidth) {
    checkMinimumNpthSlotWidth(76, 78);
  }
  if (mOptions.checkPthDrillDiameter) {
    checkMinimumPthDrillDiameter(78, 80);
  }
  if (mOptions.checkPthSlotWidth) {
    checkMinimumPthSlotWidth(80, 82);
  }
  if (mOptions.checkNpthSlotsWarning) {
    checkWarnNpthSlots(82, 83);
  }
  if (mOptions.checkPthSlotsWarning) {
    checkWarnPthSlots(83, 84);
  }
  if (mOptions.checkCourtyardClearance) {
    checkCourtyardClearances(84, 88);
  }
  if (mOptions.checkMissingConnections) {
    checkForMissingConnections(88, 90);
  }

  emitStatus(
      tr("Finished with %1 message(s)!", "Count of messages", mMessages.count())
          .arg(mMessages.count()));
  emit progressPercent(100);
  emit finished();
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void BoardDesignRuleCheck::rebuildPlanes(int progressStart, int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Rebuild planes..."));
  mBoard.rebuildAllPlanes();
  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::checkForMissingConnections(int progressStart,
                                                      int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Check for missing connections..."));

  // No check based on copper paths implemented yet -> return existing airwires
  // instead.
  mBoard.forceAirWiresRebuild();
  foreach (const BI_AirWire* airwire, mBoard.getAirWires()) {
    QString msg = tr("Missing connection: '%1'", "Placeholder is net name")
                      .arg(*airwire->getNetSignal().getName());
    Path location = Path::obround(airwire->getP1(), airwire->getP2(),
                                  PositiveLength(50000));
    emitMessage(BoardDesignRuleCheckMessage(msg, location));
  }

  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::checkCopperBoardClearances(int progressStart,
                                                      int progressEnd) {
  emitStatus(tr("Check board clearances..."));

  qreal progressSpan = progressEnd - progressStart;
  QList<NetSignal*> netsignals =
      mBoard.getProject().getCircuit().getNetSignals().values();
  netsignals.append(nullptr);  // also check unconnected copper objects

  // Board outline
  ClipperLib::Paths outlineRestrictedArea;
  if (mOptions.checkCopperBoardClearance) {
    BoardClipperPathGenerator gen(mBoard, maxArcTolerance());
    gen.addBoardOutline();
    outlineRestrictedArea = gen.getPaths();
    ClipperLib::Paths outlinePathsInner = gen.getPaths();
    ClipperHelpers::offset(
        outlinePathsInner,
        *maxArcTolerance() - *mOptions.minCopperBoardClearance,
        maxArcTolerance());
    ClipperHelpers::subtract(outlineRestrictedArea, outlinePathsInner);
  }

  // Holes
  if (mOptions.checkCopperNpthClearance) {
    BoardClipperPathGenerator gen(mBoard, maxArcTolerance());
    gen.addHoles(*mOptions.minCopperNpthClearance - *maxArcTolerance());
    ClipperHelpers::unite(outlineRestrictedArea, gen.getPaths());
  }

  auto layers = mBoard.getLayerStack().getAllLayers();
  for (int layerIndex = 0; layerIndex < layers.count(); ++layerIndex) {
    const GraphicsLayer* layer = layers[layerIndex];
    if ((!layer->isCopperLayer()) || (!layer->isEnabled())) {
      continue;
    }
    for (int i = 0; i < netsignals.count(); ++i) {
      std::unique_ptr<ClipperLib::PolyTree> intersections =
          ClipperHelpers::intersect(outlineRestrictedArea,
                                    getCopperPaths(*layer, {netsignals[i]}));
      for (const ClipperLib::Path& path :
           ClipperHelpers::flattenTree(*intersections)) {
        QString name1 = netsignals[i] ? *netsignals[i]->getName() : "";
        QString msg = tr("Clearance (%1): '%2' <-> Board Outline",
                         "Placeholders are layer name + net name")
                          .arg(layer->getNameTr(), name1);
        Path location = ClipperHelpers::convert(path);
        emitMessage(BoardDesignRuleCheckMessage(msg, location));
      }
      qreal progress = progressSpan *
          qreal((layerIndex + 1) * netsignals.count() + i + 1) /
          qreal(layers.count() * netsignals.count());
      emit progressPercent(progressStart + static_cast<int>(progress));
    }
  }
}

void BoardDesignRuleCheck::checkCopperCopperClearances(int progressStart,
                                                       int progressEnd) {
  emitStatus(tr("Check copper clearances..."));

  qreal progressSpan = progressEnd - progressStart;
  QList<NetSignal*> netsignals =
      mBoard.getProject().getCircuit().getNetSignals().values();
  netsignals.append(nullptr);  // also check unconnected copper objects

  auto layers = mBoard.getLayerStack().getAllLayers();
  for (int layerIndex = 0; layerIndex < layers.count(); ++layerIndex) {
    const GraphicsLayer* layer = layers[layerIndex];
    if ((!layer->isCopperLayer()) || (!layer->isEnabled())) {
      continue;
    }
    for (int i = 0; i < netsignals.count(); ++i) {
      ClipperLib::Paths paths1 = getCopperPaths(*layer, {netsignals[i]});
      ClipperHelpers::offset(
          paths1, (*mOptions.minCopperCopperClearance - *maxArcTolerance()) / 2,
          maxArcTolerance());
      for (int k = i + 1; k < netsignals.count(); ++k) {
        ClipperLib::Paths paths2 = getCopperPaths(*layer, {netsignals[k]});
        ClipperHelpers::offset(
            paths2,
            (*mOptions.minCopperCopperClearance - *maxArcTolerance()) / 2,
            maxArcTolerance());
        std::unique_ptr<ClipperLib::PolyTree> intersections =
            ClipperHelpers::intersect(paths1, paths2);
        for (const ClipperLib::Path& path :
             ClipperHelpers::flattenTree(*intersections)) {
          QString name1 = netsignals[i] ? *netsignals[i]->getName() : "";
          QString name2 = netsignals[k] ? *netsignals[k]->getName() : "";
          QString msg = tr("Clearance (%1): '%2' <-> '%3'",
                           "Placeholders are layer name + net names")
                            .arg(layer->getNameTr(), name1, name2);
          Path location = ClipperHelpers::convert(path);
          emitMessage(BoardDesignRuleCheckMessage(msg, location));
        }
      }
      qreal progress = progressSpan *
          qreal((layerIndex + 1) * netsignals.count() + i + 1) /
          qreal(layers.count() * netsignals.count());
      emit progressPercent(progressStart + static_cast<int>(progress));
    }
  }
}

void BoardDesignRuleCheck::checkCourtyardClearances(int progressStart,
                                                    int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Check courtyard clearances..."));

  auto layers = mBoard.getLayerStack().getLayers(
      {GraphicsLayer::sTopCourtyard, GraphicsLayer::sBotCourtyard});
  foreach (const GraphicsLayer* layer, layers) {
    // determine device courtyard areas
    QMap<const BI_Device*, ClipperLib::Paths> deviceCourtyards;
    foreach (const BI_Device* device, mBoard.getDeviceInstances()) {
      ClipperLib::Paths paths = getDeviceCourtyardPaths(*device, layer);
      ClipperHelpers::offset(paths, mOptions.courtyardOffset,
                             maxArcTolerance());
      deviceCourtyards.insert(device, paths);
    }

    // check clearances
    for (int i = 0; i < deviceCourtyards.count(); ++i) {
      const BI_Device* dev1 = deviceCourtyards.keys()[i];
      Q_ASSERT(dev1);
      const ClipperLib::Paths& paths1 = deviceCourtyards[dev1];
      for (int k = i + 1; k < deviceCourtyards.count(); ++k) {
        const BI_Device* dev2 = deviceCourtyards.keys()[k];
        Q_ASSERT(dev2);
        const ClipperLib::Paths& paths2 = deviceCourtyards[dev2];
        std::unique_ptr<ClipperLib::PolyTree> intersections =
            ClipperHelpers::intersect(paths1, paths2);
        for (const ClipperLib::Path& path :
             ClipperHelpers::flattenTree(*intersections)) {
          QString name1 = *dev1->getComponentInstance().getName();
          QString name2 = *dev2->getComponentInstance().getName();
          QString msg = tr("Clearance (%1): '%2' <-> '%3'",
                           "Placeholders are layer name + component names")
                            .arg(layer->getNameTr(), name1, name2);
          Path location = ClipperHelpers::convert(path);
          emitMessage(BoardDesignRuleCheckMessage(msg, location));
        }
      }
    }
  }

  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::checkMinimumCopperWidth(int progressStart,
                                                   int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Check minimum copper width..."));

  // stroke texts
  foreach (const BI_StrokeText* text, mBoard.getStrokeTexts()) {
    const GraphicsLayer* layer =
        mBoard.getLayerStack().getLayer(*text->getText().getLayerName());
    if ((!layer) || (!layer->isCopperLayer()) || (!layer->isEnabled())) {
      continue;
    }
    if (text->getText().getStrokeWidth() < mOptions.minCopperWidth) {
      QString msg = tr("Min. copper width (%1) of text: %2",
                       "Placeholders are layer name + width")
                        .arg(layer->getNameTr(),
                             formatLength(*text->getText().getStrokeWidth()));
      QVector<Path> locations;
      Transform transform(text->getText());
      foreach (Path path, transform.map(text->generatePaths())) {
        locations += path.toOutlineStrokes(PositiveLength(
            qMax(*text->getText().getStrokeWidth(), Length(50000))));
      }
      emitMessage(BoardDesignRuleCheckMessage(msg, locations));
    }
  }

  // planes
  foreach (const BI_Plane* plane, mBoard.getPlanes()) {
    const GraphicsLayer* layer =
        mBoard.getLayerStack().getLayer(*plane->getLayerName());
    if ((!layer) || (!layer->isCopperLayer()) || (!layer->isEnabled())) {
      continue;
    }
    if (plane->getMinWidth() < mOptions.minCopperWidth) {
      QString msg =
          tr("Min. copper width (%1) of plane: %2",
             "Placeholders are layer name + width")
              .arg(layer->getNameTr(), formatLength(*plane->getMinWidth()));
      QVector<Path> locations =
          plane->getOutline().toClosedPath().toOutlineStrokes(
              PositiveLength(200000));
      emitMessage(BoardDesignRuleCheckMessage(msg, locations));
    }
  }

  // devices
  foreach (const BI_Device* device, mBoard.getDeviceInstances()) {
    foreach (const BI_StrokeText* text, device->getStrokeTexts()) {
      // Do *not* mirror layer since it is independent of the device!
      const GraphicsLayer* layer =
          mBoard.getLayerStack().getLayer(*text->getText().getLayerName());
      if ((!layer) || (!layer->isCopperLayer()) || (!layer->isEnabled())) {
        continue;
      }
      if (text->getText().getStrokeWidth() < mOptions.minCopperWidth) {
        QString msg = tr("Min. copper width (%1) of text: %2",
                         "Placeholders are layer name + width")
                          .arg(layer->getNameTr(),
                               formatLength(*text->getText().getStrokeWidth()));
        QVector<Path> locations;
        Transform transform(text->getText());
        foreach (Path path, transform.map(text->generatePaths())) {
          locations += path.toOutlineStrokes(PositiveLength(
              qMax(*text->getText().getStrokeWidth(), Length(50000))));
        }
        emitMessage(BoardDesignRuleCheckMessage(msg, locations));
      }
    }
  }

  // netlines
  foreach (const BI_NetSegment* netsegment, mBoard.getNetSegments()) {
    foreach (const BI_NetLine* netline, netsegment->getNetLines()) {
      if ((!netline->getLayer().isCopperLayer()) ||
          (!netline->getLayer().isEnabled())) {
        continue;
      }
      if (netline->getWidth() < mOptions.minCopperWidth) {
        QString msg = tr("Min. copper width (%1) of trace: %2",
                         "Placeholders are layer name + width")
                          .arg(netline->getLayer().getNameTr(),
                               formatLength(*netline->getWidth()));
        Path location = Path::obround(netline->getStartPoint().getPosition(),
                                      netline->getEndPoint().getPosition(),
                                      netline->getWidth());
        emitMessage(BoardDesignRuleCheckMessage(msg, location));
      }
    }
  }

  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::checkMinimumPthAnnularRing(int progressStart,
                                                      int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Check minimum PTH annular rings..."));

  // Determine tha areas where copper is available on *all* layers.
  QList<ClipperLib::Paths> thtCopperAreas;
  for (const GraphicsLayer* l : mBoard.getLayerStack().getAllLayers()) {
    if (l->isCopperLayer() && l->isEnabled()) {
      thtCopperAreas.append(getCopperPaths(*l, {}));
    }
  }
  std::unique_ptr<ClipperLib::PolyTree> thtCopperAreaIntersections =
      ClipperHelpers::intersect(thtCopperAreas);
  const ClipperLib::Paths thtCopperAreaPaths =
      ClipperHelpers::treeToPaths(*thtCopperAreaIntersections);

  // Check via annular rings.
  foreach (const BI_NetSegment* netsegment, mBoard.getNetSegments()) {
    foreach (const BI_Via* via, netsegment->getVias()) {
      // Determine via area including minimum annular ring.
      const Length diameter =
          via->getDrillDiameter() + (*mOptions.minPthAnnularRing * 2) - 1;
      if (diameter <= 0) {
        continue;
      }
      const ClipperLib::Paths areas{ClipperHelpers::convert(
          Path::circle(PositiveLength(diameter)).translated(via->getPosition()),
          maxArcTolerance())};

      // Check if there's not a 100% overlap.
      const std::unique_ptr<ClipperLib::PolyTree> remainingAreasTree =
          ClipperHelpers::subtractToTree(areas, thtCopperAreaPaths);
      const ClipperLib::Paths remainingAreas =
          ClipperHelpers::flattenTree(*remainingAreasTree);
      if (!remainingAreas.empty()) {
        QString msg = tr("Annular ring of via '%1' < %2",
                         "Placeholders are net name + annular ring width")
                          .arg(netsegment->getNetNameToDisplay(true),
                               formatLength(*mOptions.minPthAnnularRing));
        const QVector<Path> location = ClipperHelpers::convert(remainingAreas);
        emitMessage(BoardDesignRuleCheckMessage(msg, location));
      }
    }
  }

  // Check pad annular rings.
  foreach (const BI_Device* device, mBoard.getDeviceInstances()) {
    foreach (const BI_FootprintPad* pad, device->getPads()) {
      // Determine hole areas including minimum annular ring.
      const Transform transform(*pad);
      ClipperLib::Paths areas;
      for (const Hole& hole : pad->getLibPad().getHoles()) {
        const Length diameter =
            hole.getDiameter() + (*mOptions.minPthAnnularRing * 2) - 1;
        if (diameter <= 0) {
          continue;
        }
        foreach (const Path& area,
                 hole.getPath()->toOutlineStrokes(PositiveLength(diameter))) {
          ClipperHelpers::unite(
              areas,
              ClipperHelpers::convert(transform.map(area), maxArcTolerance()));
        }
      }

      // Check if there's not a 100% overlap.
      const std::unique_ptr<ClipperLib::PolyTree> remainingAreasTree =
          ClipperHelpers::subtractToTree(areas, thtCopperAreaPaths);
      const ClipperLib::Paths remainingAreas =
          ClipperHelpers::flattenTree(*remainingAreasTree);
      if (!remainingAreas.empty()) {
        QString msg = tr("Annular ring of pad '%1' < %2",
                         "Placeholders are pad name + annular ring width")
                          .arg(pad->getDisplayText().simplified(),
                               formatLength(*mOptions.minPthAnnularRing));
        const QVector<Path> location = ClipperHelpers::convert(remainingAreas);
        emitMessage(BoardDesignRuleCheckMessage(msg, location));
      }
    }
  }

  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::checkMinimumNpthDrillDiameter(int progressStart,
                                                         int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Check minimum NPTH drill diameters..."));

  const QString msgTr =
      tr("Min. hole diameter: %1 < %2", "The '<' means 'smaller than'.");

  // Board holes.
  foreach (const BI_Hole* hole, mBoard.getHoles()) {
    if ((!hole->getHole().isSlot()) &&
        (hole->getHole().getDiameter() < mOptions.minNpthDrillDiameter)) {
      emitMessage(BoardDesignRuleCheckMessage(
          msgTr.arg(formatLength(*hole->getHole().getDiameter()),
                    formatLength(*mOptions.minNpthDrillDiameter)),
          getHoleLocation(hole->getHole())));
    }
  }

  // Package holes.
  foreach (const BI_Device* device, mBoard.getDeviceInstances()) {
    Transform transform(*device);
    for (const Hole& hole : device->getLibFootprint().getHoles()) {
      if ((!hole.isSlot()) &&
          (hole.getDiameter() < *mOptions.minNpthDrillDiameter)) {
        emitMessage(BoardDesignRuleCheckMessage(
            msgTr.arg(formatLength(*hole.getDiameter()),
                      formatLength(*mOptions.minNpthDrillDiameter)),
            getHoleLocation(hole, transform)));
      }
    }
  }

  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::checkMinimumNpthSlotWidth(int progressStart,
                                                     int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Check minimum NPTH slot width..."));

  const QString msgTr =
      tr("Min. NPTH slot width: %1 < %2", "The '<' means 'smaller than'.");

  // Board holes.
  foreach (const BI_Hole* hole, mBoard.getHoles()) {
    if ((hole->getHole().isSlot()) &&
        (hole->getHole().getDiameter() < mOptions.minNpthSlotWidth)) {
      emitMessage(BoardDesignRuleCheckMessage(
          msgTr.arg(formatLength(*hole->getHole().getDiameter()),
                    formatLength(*mOptions.minNpthSlotWidth)),
          getHoleLocation(hole->getHole())));
    }
  }

  // Package holes.
  foreach (const BI_Device* device, mBoard.getDeviceInstances()) {
    Transform transform(*device);
    for (const Hole& hole : device->getLibFootprint().getHoles()) {
      if ((hole.isSlot()) &&
          (hole.getDiameter() < *mOptions.minNpthSlotWidth)) {
        emitMessage(BoardDesignRuleCheckMessage(
            msgTr.arg(formatLength(*hole.getDiameter()),
                      formatLength(*mOptions.minNpthSlotWidth)),
            getHoleLocation(hole, transform)));
      }
    }
  }

  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::checkMinimumPthDrillDiameter(int progressStart,
                                                        int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Check minimum PTH drill diameters..."));

  // Vias.
  foreach (const BI_NetSegment* netsegment, mBoard.getNetSegments()) {
    foreach (const BI_Via* via, netsegment->getVias()) {
      if (via->getDrillDiameter() < mOptions.minPthDrillDiameter) {
        QString msg = tr("Min. via drill diameter ('%1'): %2",
                         "Placeholders are net name + drill diameter")
                          .arg(netsegment->getNetNameToDisplay(true),
                               formatLength(*via->getDrillDiameter()));
        Path location = Path::circle(via->getDrillDiameter())
                            .translated(via->getPosition());
        emitMessage(BoardDesignRuleCheckMessage(msg, location));
      }
    }
  }

  // Pads.
  foreach (const BI_Device* device, mBoard.getDeviceInstances()) {
    foreach (const BI_FootprintPad* pad, device->getPads()) {
      for (const Hole& hole : pad->getLibPad().getHoles()) {
        if (hole.getDiameter() < *mOptions.minPthDrillDiameter) {
          QString msg = tr("Min. pad drill diameter ('%1'): %2",
                           "Placeholders are pad name + drill diameter")
                            .arg(pad->getDisplayText().simplified(),
                                 formatLength(*hole.getDiameter()));
          PositiveLength diameter(qMax(*hole.getDiameter(), Length(50000)));
          Path location = Path::circle(diameter).translated(pad->getPosition());
          emitMessage(BoardDesignRuleCheckMessage(msg, location));
        }
      }
    }
  }

  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::checkMinimumPthSlotWidth(int progressStart,
                                                    int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Check minimum PTH slot width..."));

  const QString msgTr =
      tr("Min. PTH slot width: %1 < %2", "The '<' means 'smaller than'.");

  // Pads.
  foreach (const BI_Device* device, mBoard.getDeviceInstances()) {
    Transform devTransform(*device);
    foreach (const BI_FootprintPad* pad, device->getPads()) {
      Transform padTransform(pad->getLibPad().getPosition(),
                             pad->getLibPad().getRotation());
      for (const Hole& hole : pad->getLibPad().getHoles()) {
        if ((hole.isSlot()) &&
            (hole.getDiameter() < *mOptions.minPthSlotWidth)) {
          emitMessage(BoardDesignRuleCheckMessage(
              msgTr.arg(formatLength(*hole.getDiameter()),
                        formatLength(*mOptions.minPthSlotWidth)),
              getHoleLocation(hole, padTransform, devTransform)));
        }
      }
    }
  }

  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::checkWarnNpthSlots(int progressStart,
                                              int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Check NPTH slots..."));

  // Board holes.
  foreach (const BI_Hole* hole, mBoard.getHoles()) {
    processHoleSlotWarning(hole->getHole(), mOptions.npthSlotsWarning);
  }

  // Package holes.
  foreach (const BI_Device* device, mBoard.getDeviceInstances()) {
    Transform transform(*device);
    for (const Hole& hole : device->getLibFootprint().getHoles()) {
      processHoleSlotWarning(hole, mOptions.npthSlotsWarning, transform);
    }
  }

  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::checkWarnPthSlots(int progressStart,
                                             int progressEnd) {
  Q_UNUSED(progressStart);
  emitStatus(tr("Check PTH slots..."));

  // Pads.
  foreach (const BI_Device* device, mBoard.getDeviceInstances()) {
    Transform devTransform(*device);
    foreach (const BI_FootprintPad* pad, device->getPads()) {
      Transform padTransform(pad->getLibPad().getPosition(),
                             pad->getLibPad().getRotation());
      for (const Hole& hole : pad->getLibPad().getHoles()) {
        processHoleSlotWarning(hole, mOptions.pthSlotsWarning, padTransform,
                               devTransform);
      }
    }
  }

  emit progressPercent(progressEnd);
}

void BoardDesignRuleCheck::processHoleSlotWarning(const Hole& hole,
                                                  SlotsWarningLevel level,
                                                  const Transform& transform1,
                                                  const Transform& transform2) {
  const QString suggestion = "\n" %
      tr("Either avoid them or check if your PCB manufacturer supports "
         "them.");
  const QString checkSlotMode = "\n" %
      tr("Choose the desired Excellon slot mode when generating the "
         "production data (G85 vs. G00..G03).");
  const QString g85NotAvailable = "\n" %
      tr("The drilled slot mode (G85) will not be available when generating "
         "production data.");
  if ((level >= SlotsWarningLevel::Curved) && hole.isCurvedSlot()) {
    emitMessage(BoardDesignRuleCheckMessage(
        tr("Hole is a slot with curves"),
        getHoleLocation(hole, transform1, transform2),
        tr("Curved slots are a very unusual thing and may cause troubles "
           "with many PCB manufacturers.") %
            suggestion % g85NotAvailable));
  } else if ((level >= SlotsWarningLevel::MultiSegment) &&
             hole.isMultiSegmentSlot()) {
    emitMessage(BoardDesignRuleCheckMessage(
        tr("Hole is a multi-segment slot"),
        getHoleLocation(hole, transform1, transform2),
        tr("Multi-segment slots are a rather unusual thing and may cause "
           "troubles with some PCB manufacturers.") %
            suggestion % checkSlotMode));
  } else if ((level >= SlotsWarningLevel::All) && hole.isSlot()) {
    emitMessage(BoardDesignRuleCheckMessage(
        tr("Hole is a slot"), getHoleLocation(hole, transform1, transform2),
        tr("Slots may cause troubles with some PCB manufacturers.") %
            suggestion % checkSlotMode));
  }
}

const ClipperLib::Paths& BoardDesignRuleCheck::getCopperPaths(
    const GraphicsLayer& layer, const QSet<const NetSignal*>& netsignals) {
  const auto key = qMakePair(&layer, netsignals);
  if (!mCachedPaths.contains(key)) {
    BoardClipperPathGenerator gen(mBoard, maxArcTolerance());
    gen.addCopper(layer.getName(), netsignals);
    mCachedPaths[key] = gen.getPaths();
  }
  return mCachedPaths[key];
}

ClipperLib::Paths BoardDesignRuleCheck::getDeviceCourtyardPaths(
    const BI_Device& device, const GraphicsLayer* layer) {
  ClipperLib::Paths paths;
  Transform transform(device);
  for (const Polygon& polygon : device.getLibFootprint().getPolygons()) {
    GraphicsLayerName polygonLayer = transform.map(polygon.getLayerName());
    if (polygonLayer != layer->getName()) {
      continue;
    }
    Path path = transform.map(polygon.getPath());
    ClipperHelpers::unite(paths,
                          ClipperHelpers::convert(path, maxArcTolerance()));
  }
  for (const Circle& circle : device.getLibFootprint().getCircles()) {
    GraphicsLayerName circleLayer = transform.map(circle.getLayerName());
    if (circleLayer != layer->getName()) {
      continue;
    }
    Point absolutePos = transform.map(circle.getCenter());
    ClipperHelpers::unite(
        paths,
        ClipperHelpers::convert(Path::circle(circle.getDiameter()),
                                maxArcTolerance()));
  }
  return paths;
}

QVector<Path> BoardDesignRuleCheck::getHoleLocation(
    const Hole& hole, const Transform& transform1,
    const Transform& transform2) const noexcept {
  return transform2.map(
      transform1.map(hole.getPath())->toOutlineStrokes(hole.getDiameter()));
}

void BoardDesignRuleCheck::emitStatus(const QString& status) noexcept {
  mProgressStatus.append(status);
  emit progressStatus(status);
}

void BoardDesignRuleCheck::emitMessage(
    const BoardDesignRuleCheckMessage& msg) noexcept {
  mMessages.append(msg);
  emit progressMessage(msg.getMessage());
}

QString BoardDesignRuleCheck::formatLength(const Length& length) const
    noexcept {
  return Toolbox::floatToString(length.toMm(), 6, QLocale()) % "mm";
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
