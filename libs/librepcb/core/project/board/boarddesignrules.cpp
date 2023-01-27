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
#include "boarddesignrules.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

BoardDesignRules::BoardDesignRules() noexcept
  :  // general attributes
    mName(tr("LibrePCB Default Design Rules")),
    mDescription(),
    // stop mask
    mStopMaskClearanceRatio(Ratio::percent0()),  // 0%
    mStopMaskClearanceMin(100000),  // 0.1mm
    mStopMaskClearanceMax(100000),  // 0.1mm
    mStopMaskMaxViaDrillDiameter(500000),  // 0.5mm
    // cream mask
    mCreamMaskClearanceRatio(Ratio::percent100() / 10),  // 10%
    mCreamMaskClearanceMin(0),  // 0.0mm
    mCreamMaskClearanceMax(1000000),  // 1.0mm
    // pad annular ring
    mPadAnnularRingRatio(Ratio::percent100() / 4),  // 25%
    mPadAnnularRingMin(250000),  // 0.25mm
    mPadAnnularRingMax(2000000),  // 2.0mm
    mViaAnnularRingRatio(Ratio::percent100() / 4),  // 25%
    mViaAnnularRingMin(200000),  // 0.2mm
    mViaAnnularRingMax(2000000)  // 2.0mm
{
}

BoardDesignRules::BoardDesignRules(const BoardDesignRules& other)
  : BoardDesignRules() {
  *this = other;
}

BoardDesignRules::BoardDesignRules(const SExpression& node)
  :  // general attributes
    mName(deserialize<ElementName>(node.getChild("name/@0"))),
    mDescription(node.getChild("description/@0").getValue()),
    // stop mask
    mStopMaskClearanceRatio(deserialize<UnsignedRatio>(
        node.getChild("stopmask_clearance_ratio/@0"))),
    mStopMaskClearanceMin(deserialize<UnsignedLength>(
        node.getChild("stopmask_clearance_min/@0"))),
    mStopMaskClearanceMax(deserialize<UnsignedLength>(
        node.getChild("stopmask_clearance_max/@0"))),
    mStopMaskMaxViaDrillDiameter(deserialize<UnsignedLength>(
        node.getChild("stopmask_max_via_drill_diameter/@0"))),
    // cream mask
    mCreamMaskClearanceRatio(deserialize<UnsignedRatio>(
        node.getChild("creammask_clearance_ratio/@0"))),
    mCreamMaskClearanceMin(deserialize<UnsignedLength>(
        node.getChild("creammask_clearance_min/@0"))),
    mCreamMaskClearanceMax(deserialize<UnsignedLength>(
        node.getChild("creammask_clearance_max/@0"))),
    // pad annular ring
    mPadAnnularRingRatio(
        deserialize<UnsignedRatio>(node.getChild("pad_annular_ring_ratio/@0"))),
    mPadAnnularRingMin(
        deserialize<UnsignedLength>(node.getChild("pad_annular_ring_min/@0"))),
    mPadAnnularRingMax(
        deserialize<UnsignedLength>(node.getChild("pad_annular_ring_max/@0"))),
    // via annular ring
    mViaAnnularRingRatio(
        deserialize<UnsignedRatio>(node.getChild("via_annular_ring_ratio/@0"))),
    mViaAnnularRingMin(
        deserialize<UnsignedLength>(node.getChild("via_annular_ring_min/@0"))),
    mViaAnnularRingMax(
        deserialize<UnsignedLength>(node.getChild("via_annular_ring_max/@0"))) {
  // force validating properties, throw exception on error
  try {
    setStopMaskClearanceBounds(mStopMaskClearanceMin, mStopMaskClearanceMax);
    setCreamMaskClearanceBounds(mCreamMaskClearanceMin, mCreamMaskClearanceMax);
    setPadAnnularRingBounds(mPadAnnularRingMin, mPadAnnularRingMax);
    setViaAnnularRingBounds(mViaAnnularRingMin, mViaAnnularRingMax);
  } catch (const Exception& e) {
    throw RuntimeError(__FILE__, __LINE__,
                       tr("Invalid design rules: %1").arg(e.getMsg()));
  }
}

BoardDesignRules::~BoardDesignRules() noexcept {
}

/*******************************************************************************
 *  Setters
 ******************************************************************************/

void BoardDesignRules::setStopMaskClearanceBounds(const UnsignedLength& min,
                                                  const UnsignedLength& max) {
  if (max >= min) {
    mStopMaskClearanceMin = min;
    mStopMaskClearanceMax = max;
  } else {
    throw RuntimeError(__FILE__, __LINE__,
                       tr("Stop mask clearance: MAX must be >= MIN"));
  }
}

void BoardDesignRules::setCreamMaskClearanceBounds(const UnsignedLength& min,
                                                   const UnsignedLength& max) {
  if (max >= min) {
    mCreamMaskClearanceMin = min;
    mCreamMaskClearanceMax = max;
  } else {
    throw RuntimeError(__FILE__, __LINE__,
                       tr("Cream mask clearance: MAX must be >= MIN"));
  }
}

void BoardDesignRules::setPadAnnularRingBounds(const UnsignedLength& min,
                                               const UnsignedLength& max) {
  if (max >= min) {
    mPadAnnularRingMin = min;
    mPadAnnularRingMax = max;
  } else {
    throw RuntimeError(__FILE__, __LINE__,
                       tr("Pads annular ring: MAX must be >= MIN"));
  }
}

void BoardDesignRules::setViaAnnularRingBounds(const UnsignedLength& min,
                                               const UnsignedLength& max) {
  if (max >= min) {
    mViaAnnularRingMin = min;
    mViaAnnularRingMax = max;
  } else {
    throw RuntimeError(__FILE__, __LINE__,
                       tr("Vias annular ring: MAX must be >= MIN"));
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void BoardDesignRules::restoreDefaults() noexcept {
  *this = BoardDesignRules();
}

void BoardDesignRules::serialize(SExpression& root) const {
  // general attributes
  root.ensureLineBreak();
  root.appendChild("name", mName);
  root.ensureLineBreak();
  root.appendChild("description", mDescription);
  root.ensureLineBreak();
  // stop mask
  root.appendChild("stopmask_clearance_ratio", mStopMaskClearanceRatio);
  root.ensureLineBreak();
  root.appendChild("stopmask_clearance_min", mStopMaskClearanceMin);
  root.ensureLineBreak();
  root.appendChild("stopmask_clearance_max", mStopMaskClearanceMax);
  root.ensureLineBreak();
  root.appendChild("stopmask_max_via_drill_diameter",
                   mStopMaskMaxViaDrillDiameter);
  root.ensureLineBreak();
  // cream mask
  root.appendChild("creammask_clearance_ratio", mCreamMaskClearanceRatio);
  root.ensureLineBreak();
  root.appendChild("creammask_clearance_min", mCreamMaskClearanceMin);
  root.ensureLineBreak();
  root.appendChild("creammask_clearance_max", mCreamMaskClearanceMax);
  root.ensureLineBreak();
  // pads annular ring
  root.appendChild("pad_annular_ring_ratio", mPadAnnularRingRatio);
  root.ensureLineBreak();
  root.appendChild("pad_annular_ring_min", mPadAnnularRingMin);
  root.ensureLineBreak();
  root.appendChild("pad_annular_ring_max", mPadAnnularRingMax);
  root.ensureLineBreak();
  // vias annular ring
  root.appendChild("via_annular_ring_ratio", mViaAnnularRingRatio);
  root.ensureLineBreak();
  root.appendChild("via_annular_ring_min", mViaAnnularRingMin);
  root.ensureLineBreak();
  root.appendChild("via_annular_ring_max", mViaAnnularRingMax);
  root.ensureLineBreak();
}

/*******************************************************************************
 *  Helper Methods
 ******************************************************************************/

bool BoardDesignRules::doesViaRequireStopMask(const Length& drillDia) const
    noexcept {
  return (drillDia > *mStopMaskMaxViaDrillDiameter ? true : false);
}

UnsignedLength BoardDesignRules::calcStopMaskClearance(
    const Length& padSize) const noexcept {
  return UnsignedLength(
      qBound(*mStopMaskClearanceMin,
             padSize.scaled(mStopMaskClearanceRatio->toNormalized()),
             *mStopMaskClearanceMax));
}

UnsignedLength BoardDesignRules::calcCreamMaskClearance(
    const Length& padSize) const noexcept {
  return UnsignedLength(
      qBound(*mCreamMaskClearanceMin,
             padSize.scaled(mCreamMaskClearanceRatio->toNormalized()),
             *mCreamMaskClearanceMax));
}

UnsignedLength BoardDesignRules::calcPadAnnularRing(
    const Length& drillDia) const noexcept {
  return UnsignedLength(
      qBound(*mPadAnnularRingMin,
             drillDia.scaled(mPadAnnularRingRatio->toNormalized()),
             *mPadAnnularRingMax));
}

UnsignedLength BoardDesignRules::calcViaAnnularRing(
    const Length& drillDia) const noexcept {
  return UnsignedLength(
      qBound(*mViaAnnularRingMin,
             drillDia.scaled(mViaAnnularRingRatio->toNormalized()),
             *mViaAnnularRingMax));
}

/*******************************************************************************
 *  Operator Overloadings
 ******************************************************************************/

BoardDesignRules& BoardDesignRules::operator=(
    const BoardDesignRules& rhs) noexcept {
  // general attributes
  mName = rhs.mName;
  mDescription = rhs.mDescription;
  // stop mask
  mStopMaskClearanceRatio = rhs.mStopMaskClearanceRatio;
  mStopMaskClearanceMin = rhs.mStopMaskClearanceMin;
  mStopMaskClearanceMax = rhs.mStopMaskClearanceMax;
  mStopMaskMaxViaDrillDiameter = rhs.mStopMaskMaxViaDrillDiameter;
  // cream mask
  mCreamMaskClearanceRatio = rhs.mCreamMaskClearanceRatio;
  mCreamMaskClearanceMin = rhs.mCreamMaskClearanceMin;
  mCreamMaskClearanceMax = rhs.mCreamMaskClearanceMax;
  // pads annular ring
  mPadAnnularRingRatio = rhs.mPadAnnularRingRatio;
  mPadAnnularRingMin = rhs.mPadAnnularRingMin;
  mPadAnnularRingMax = rhs.mPadAnnularRingMax;
  // vias annular ring
  mViaAnnularRingRatio = rhs.mViaAnnularRingRatio;
  mViaAnnularRingMin = rhs.mViaAnnularRingMin;
  mViaAnnularRingMax = rhs.mViaAnnularRingMax;
  return *this;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
