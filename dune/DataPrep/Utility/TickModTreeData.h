// TickModTreeData.h

// David Adams
// July 2018
//
// Class to describe the data in a tickmod tree.
// See dunetpc/dune/DataPRep/Tool/AdcTickModViewer.
//
// If the layout of this class is changed, then the dictionary
// cxx and pcm files in this directory must be regenerated with
//   rm TickModTreeData_Dict.cxx; rootcint TickModTreeData_Dict.cxx TickModTreeData.h
// after incrementing the version in the ClassDef macro below.

#ifndef TickModTreeData_H
#define TickModTreeData_H

#include "Rtypes.h"

class StickyCodeMetrics;

class TickModTreeData {

public:

  using UShort = unsigned short;
  using Index = unsigned int;
  using Float = float;

  static Index badIndex() { return 999999; }

  // Identifiers that specify the tickmod and
  // run conditions.
  Index run      =badIndex();
  Index chan     =badIndex();
  Index femb     =badIndex();
  Index fembChan =badIndex();
  Index itkm     =badIndex();
  // Sticky code data for one tickmod.
  Index nsample;
  Index maxAdc;
  Index maxAdc2;
  Float meanAdc;
  Float meanAdc2;
  Float maxFraction;
  Float zeroFraction;
  Float oneFraction;
  Float highFraction;
  Float fitMean;
  Float fitSigma;
  Float fitExcess;

  // Ctor.
  TickModTreeData();

  // Clear the data.
  void clear();

  // Fill the sticky code data.
  void fill(const StickyCodeMetrics& scm);

  ClassDefNV(TickModTreeData, 1);

};


#endif