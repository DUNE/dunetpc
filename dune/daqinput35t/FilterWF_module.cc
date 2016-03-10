#ifndef FILTERWF_H
#define FILTERWF_H

////////////////////////////////////////////////////////////////////////
//
// FilterWF_module
//
// bkirby@bnl.gov
//
// Updated by m.wallbank@sheffield.ac.uk
//
////////////////////////////////////////////////////////////////////////

// framework
#include "art/Framework/Principal/Handle.h" 
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "messagefacility/MessageLogger/MessageLogger.h" 
#include "art/Framework/Services/Optional/detail/TH1AddDirectorySentry.h"
//#include "art/Framework/Services/Optional/detail/RootDirectorySentry.h"
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

// larsoft
#include "lardata/RawData/RawDigit.h"
#include "larcore/Geometry/Geometry.h"

//#include "DetectorInfoServices/DetectorPropertiesService.h"

// lbne-raw-data
#include "lbne-raw-data/Services/ChannelMap/ChannelMapService.h"

// C++
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <fstream>
#include <sstream>

// ROOT
#include <TProfile.h>
#include <TTree.h>
#include <TMath.h>
#include <TFile.h>
//#include <TFileDirectory.h>

namespace lbne {
  class FilterWF;
}

class lbne::FilterWF : public art::EDProducer {

public:

  explicit FilterWF(fhicl::ParameterSet const& pset);
  virtual ~FilterWF();

  virtual void produce(art::Event& evt);
  void reconfigure(fhicl::ParameterSet const& pset);
  void beginRun(art::Run const& run);
  void endRun(art::Run const& run);

private:

  std::string fRawDigitModuleLabel, fRawDigitModuleInstance;
  //std::string fOutputModuleLabel;
  bool fSkipStuckCodes;

  int  fMethod; //0: group by regulator
                //1: group by ASIC

  art::ServiceHandle<geo::Geometry> fGeom;
  art::ServiceHandle<lbne::ChannelMapService> fChannelMap;

};

//-------------------------------------------------------------------
lbne::FilterWF::FilterWF(fhicl::ParameterSet const& pset) {
  this->reconfigure(pset);
  produces<std::vector<raw::RawDigit> >();
}

//-------------------------------------------------------------------
lbne::FilterWF::~FilterWF(){
}

//-------------------------------------------------------------------
void lbne::FilterWF::reconfigure(fhicl::ParameterSet const& pset) {
  fRawDigitModuleLabel = pset.get<std::string>("RawDigitModuleLabel");
  fRawDigitModuleInstance = pset.get<std::string>("RawDigitModuleInstance");
  //fOutputModuleLabel = "filterwf";
  fSkipStuckCodes = pset.get<bool>("SkipStuckCodes",true);
  fMethod         = pset.get<int>("Method",0);  //0: group by regulator
                                                //1: group by ASIC
}

//-------------------------------------------------------------------
void lbne::FilterWF::beginRun(art::Run const& run) {
  return;
}

//-------------------------------------------------------------------
void lbne::FilterWF::endRun(art::Run const& run) {
  return;
}
  
//-------------------------------------------------------------------
void lbne::FilterWF::produce(art::Event& evt) {

  art::Handle<std::vector<raw::RawDigit> > rawDigitHandle;
  evt.getByLabel(fRawDigitModuleLabel, fRawDigitModuleInstance, rawDigitHandle);
  std::vector<raw::RawDigit> const& rawDigitVector(*rawDigitHandle);
  
  // define temporary vector to hold filtered waveforms
  const unsigned int n_channels = fGeom->Nchannels();
  //const unsigned int n_channels = fGeom->rawDigitVector.size();
  std::vector<std::vector<short> > filterWf(n_channels); // MW: I had to make this a vector of shorts to make it compile (the RawDigit constructor does not accept ADC as float)
  
  // raw digit map
  std::map<int,raw::RawDigit> rawDigitMap;
  std::map<int,float> pedestalMap;
  unsigned int maxNumBins = 0;
  for (std::vector<raw::RawDigit>::const_iterator digitIt = rawDigitVector.begin(); digitIt != rawDigitVector.end(); ++digitIt) {
    rawDigitMap[digitIt->Channel()] = *digitIt;
    pedestalMap[digitIt->Channel()] = digitIt->GetPedestal();
    if (digitIt->NADC() > maxNumBins) maxNumBins = digitIt->NADC();
  }
  
  // define set of induction and collection channels in each regulator group
  std::vector<std::vector<std::vector<unsigned int> > > Chs;  //Chs[plane id][group id][channel id]

  if (fMethod == 0){ //group by regulators
    Chs.resize(3);
    for (size_t i = 0; i<3; ++i){
      Chs[i].resize(32);
    }
    for (unsigned int i = 0; i<n_channels; ++i){//online channels
      unsigned int plane     = fChannelMap->PlaneFromOnlineChannel(i);
      unsigned int rce       = fChannelMap->RCEFromOnlineChannel(i);
      unsigned int regulator = fChannelMap->RegulatorFromOnlineChannel(i);
      Chs[plane][rce*2+regulator].push_back(i);
    }
  }
  
  // derive correction factors - require raw adc waveform and pedestal for each channel
  std::vector<Double_t> corrVals;
  // loop through time slices
  for (unsigned int s = 0; s < maxNumBins; s++) {
    for (size_t i = 0; i<Chs.size(); ++i){
      for (size_t j = 0; j<Chs[i].size(); ++j){
	corrVals.clear();
	for (size_t k = 0; k<Chs[i][j].size(); ++k){
	  
	  unsigned int offlineChan = fChannelMap->Offline(Chs[i][j][k]);
	  if (rawDigitMap.count(offlineChan) == 0)
	    continue;
	  int adc = rawDigitMap.at(offlineChan).ADC(s);
	  if ( fSkipStuckCodes && ( (adc & 0x3F) == 0x0 || (adc & 0x3F) == 0x3F ) ) 
	    continue;
	  if (adc < 10) //skip "sample dropping" problem
	    continue;
	  double mean = pedestalMap.at(offlineChan);
	  if (mean < 10)
	    continue;
	  corrVals.push_back(adc - mean);
	}

	unsigned int corrValSize = corrVals.size();
	sort(corrVals.begin(),corrVals.end());
	double correction = 0;
	if (corrValSize < 2)
	  correction = 0.0;
	else if ((corrValSize % 2) == 0)
	  correction = (corrVals[corrValSize/2] + corrVals[(corrValSize/2)-1])/2.0;
	else
	  correction = corrVals[(corrValSize-1)/2];

	for (size_t k = 0; k<Chs[i][j].size(); ++k){
		  
	  unsigned int offlineChan = fChannelMap->Offline(Chs[i][j][k]);
	  if (rawDigitMap.count(offlineChan) == 0)
	    continue;
	  int adc = rawDigitMap.at(offlineChan).ADC(s);
	  double newAdc = adc - correction;
	  if ( fSkipStuckCodes && ( (adc & 0x3F) == 0x0 || (adc & 0x3F) == 0x3F ) )
	    newAdc = adc; //don't do anything about stuck code, will run stuck code removal later
	  // if the code unsticker is run first, then don't skip the stuck codes.
	  //	  if( adc < 10  ) //skip "sample dropping" problem
	  //	    newAdc = 0;
	  filterWf.at(offlineChan).push_back(newAdc);
	}
      }//all groups
    }//all planes
  }// all ticks

  // loop over channels - save filtered waveforms into digits
  std::vector<raw::RawDigit> filterRawDigitVector;
  for (unsigned int ich = 0; ich < n_channels; ich++) {
    if (rawDigitMap.count(ich) == 0)
      continue;
    raw::RawDigit theRawDigit(ich, filterWf.at(ich).size(), filterWf.at(ich));
    theRawDigit.SetPedestal(rawDigitMap[ich].GetPedestal(),
			    rawDigitMap[ich].GetSigma());
    filterRawDigitVector.push_back(theRawDigit);
  }

  // save filtered waveforms to event
  evt.put(std::make_unique<decltype(filterRawDigitVector)>(std::move(filterRawDigitVector)));

}

DEFINE_ART_MODULE(lbne::FilterWF)

#endif //FILTERWF_H
