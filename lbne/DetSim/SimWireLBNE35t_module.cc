////////////////////////////////////////////////////////////////////////
//
// SimWireLBNE10kt class designed to simulate signal on a wire in the TPC
//
// katori@fnal.gov
//
// jti3@fnal.gov
// - Revised to use sim::RawDigit instead of rawdata::RawDigit, and to
// - save the electron clusters associated with each digit.
//
////////////////////////////////////////////////////////////////////////

#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <bitset>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
}

#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Framework/Services/Optional/TFileDirectory.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "Utilities/LArFFT.h"
#include "RawData/raw.h"
#include "Utilities/LArProperties.h"
#include "Utilities/SignalShapingServiceLBNE10kt.h"
#include "Geometry/Geometry.h"

#include "Simulation/sim.h"
#include "Simulation/SimChannel.h"
#include "RawData/RawDigit.h"
#include "Utilities/DetectorProperties.h"

#include "TMath.h"
#include "TComplex.h"
#include "TString.h"
#include "TH2.h"
#include "TH1D.h"
#include "TFile.h"

#include "CLHEP/Random/RandFlat.h"
#include "CLHEP/Random/RandGaussQ.h"

///Detector simulation of raw signals on wires
namespace detsim {

  // Base class for creation of raw signals on wires. 
  class SimWireLBNE35t : public art::EDProducer {
    
  public:
        
    explicit SimWireLBNE35t(fhicl::ParameterSet const& pset); 
    virtual ~SimWireLBNE35t();
    
    // read/write access to event
    void produce (art::Event& evt);
    void beginJob();
    void endJob();
    void reconfigure(fhicl::ParameterSet const& p);

  private:

    void         GenNoise(std::vector<float>& array);

    std::string            fDriftEModuleLabel;///< module making the ionization electrons
    raw::Compress_t        fCompression;      ///< compression type to use
    float                  fNoiseFact;        ///< noise scale factor
    float                  fNoiseWidth;       ///< exponential noise width (kHz)
    float                  fLowCutoff;        ///< low frequency filter cutoff (kHz)
    float                  fNoiseFactW;        ///< noise scale factor for W (collection) plane
    float                  fNoiseWidthW;       ///< exponential noise width (kHz)  for W (collection) plane
    float                  fLowCutoffW;        ///< low frequency filter cutoff (kHz) for W (collection) plane
    float                  fNoiseFactU;        ///< noise scale factor  for U plane
    float                  fNoiseWidthU;       ///< exponential noise width (kHz)   for U plane
    float                  fLowCutoffU;        ///< low frequency filter cutoff (kHz)  for U plane
    float                  fNoiseFactV;        ///< noise scale factor   for V plane
    float                  fNoiseWidthV;       ///< exponential noise width (kHz)   for V plane
    float                  fLowCutoffV;        ///< low frequency filter cutoff (kHz)  for V plane
    unsigned int           fZeroThreshold;    ///< Zero suppression threshold
    int                    fNTicks;           ///< number of ticks of the clock
    double                 fSampleRate;       ///< sampling rate in ns
    unsigned int           fNSamplesReadout;  ///< number of ADC readout samples in 1 readout frame
    unsigned int           fNTimeSamples;     ///< number of ADC readout samples in all readout frames (per event)
    unsigned int           fNoiseArrayPoints; ///< number of  points in randomly generated noise array
  
    std::vector<double>    fChargeWork;
    //std::vector< std::vector<float> > fNoise;///< noise on each channel for each time
    std::vector< std::vector<float> > fNoiseW;///< noise on each channel for each time for W (collection) plane
    std::vector< std::vector<float> > fNoiseU;///< noise on each channel for each time for U plane
    std::vector< std::vector<float> > fNoiseV;///< noise on each channel for each time for V plane
    
    TH1D*                fNoiseDist;          ///< distribution of noise counts

  }; // class SimWireLBNE35t

  DEFINE_ART_MODULE(SimWireLBNE35t);

  //-------------------------------------------------
  SimWireLBNE35t::SimWireLBNE35t(fhicl::ParameterSet const& pset)
  {
    this->reconfigure(pset);

    produces< std::vector<raw::RawDigit>   >();
    if(fNSamplesReadout != fNTimeSamples ) {
      produces< std::vector<raw::RawDigit>   >("preSpill");
      produces< std::vector<raw::RawDigit>   >("postSpill");
    } 

    fCompression = raw::kNone;
    TString compression(pset.get< std::string >("CompressionType"));
    if(compression.Contains("Huffman",TString::kIgnoreCase)) fCompression = raw::kHuffman;    
    if(compression.Contains("ZeroSuppression",TString::kIgnoreCase)) fCompression = raw::kZeroSuppression;    

    // get the random number seed, use a random default if not specified    
    // in the configuration file.  
    unsigned int seed = pset.get< unsigned int >("Seed", sim::GetRandomNumberSeed());

    createEngine(seed);
  }

  //-------------------------------------------------
  SimWireLBNE35t::~SimWireLBNE35t()
  {

    fChargeWork.clear();
 
    for(unsigned int i = 0; i < fNoiseW.size(); ++i) fNoiseW[i].clear();
    fNoiseW.clear();
   
    for(unsigned int i = 0; i < fNoiseU.size(); ++i) fNoiseU[i].clear();
    fNoiseU.clear();
   
    for(unsigned int i = 0; i < fNoiseV.size(); ++i) fNoiseV[i].clear();
    fNoiseV.clear();

  }

  //-------------------------------------------------
  void SimWireLBNE35t::reconfigure(fhicl::ParameterSet const& p) 
  {
    fDriftEModuleLabel= p.get< std::string         >("DriftEModuleLabel");


    fNoiseFactW        = p.get< double              >("NoiseFactW");
    fNoiseWidthW       = p.get< double              >("NoiseWidthW");
    fLowCutoffW        = p.get< double              >("LowCutoffW");
    fNoiseFactU        = p.get< double              >("NoiseFactU");
    fNoiseWidthU       = p.get< double              >("NoiseWidthU");
    fLowCutoffU        = p.get< double              >("LowCutoffU");
    fNoiseFactV        = p.get< double              >("NoiseFactV");
    fNoiseWidthV       = p.get< double              >("NoiseWidthV");
    fLowCutoffV        = p.get< double              >("LowCutoffV");
    fZeroThreshold    = p.get< unsigned int        >("ZeroThreshold");
    fNoiseArrayPoints = p.get< unsigned int        >("NoiseArrayPoints");

    art::ServiceHandle<util::DetectorProperties> detprop;
    fSampleRate       = detprop->SamplingRate();
    fNSamplesReadout  = detprop->ReadOutWindowSize();
    fNTimeSamples  = detprop->NumberTimeSamples();
    
    return;
  }

  //-------------------------------------------------
  void SimWireLBNE35t::beginJob() 
  { 

    // get access to the TFile service
    art::ServiceHandle<art::TFileService> tfs;

    fNoiseDist  = tfs->make<TH1D>("Noise", ";Noise  (ADC);", 1000,   -10., 10.);

    art::ServiceHandle<util::LArFFT> fFFT;
    fNTicks = fFFT->FFTSize();

    if ( fNTicks%2 != 0 ) 
      LOG_DEBUG("SimWireLBNE35t") << "Warning: FFTSize not a power of 2. "
				     << "May cause issues in (de)convolution.\n";

    if ( (int)fNSamplesReadout > fNTicks ) 
      mf::LogError("SimWireLBNE35t") << "Cannot have number of readout samples "
					<< "greater than FFTSize!";

    fChargeWork.resize(fNTicks, 0.);
    art::ServiceHandle<geo::Geometry> geo;
    
    //fNoise.resize(geo->Nchannels());
    fNoiseW.resize(fNoiseArrayPoints);
    fNoiseU.resize(fNoiseArrayPoints);
    fNoiseV.resize(fNoiseArrayPoints);

    // GenNoise() will further resize each channel's 
    // fNoise vector to fNoiseArrayPoints long.

    //for(unsigned int p = 0; p < geo->Nchannels(); ++p){
    for(unsigned int p = 0; p < fNoiseArrayPoints; ++p){

      fNoiseFact = fNoiseFactW;
      fNoiseWidth = fNoiseWidthW;
      fLowCutoff = fLowCutoffW;
      GenNoise(fNoiseW[p]);

      fNoiseFact = fNoiseFactU;
      fNoiseWidth = fNoiseWidthU;
      fLowCutoff = fLowCutoffU;
      GenNoise(fNoiseU[p]);

      fNoiseFact = fNoiseFactV;
      fNoiseWidth = fNoiseWidthV;
      fLowCutoff = fLowCutoffV;
      GenNoise(fNoiseV[p]);

      for(int i = 0; i < fNTicks; ++i){
	fNoiseDist->Fill(fNoiseW[p][i]);
	fNoiseDist->Fill(fNoiseU[p][i]);
	fNoiseDist->Fill(fNoiseV[p][i]);
      }
    }// end loop over wires

    return;

  }

  //-------------------------------------------------
  void SimWireLBNE35t::endJob() 
  {
  }

  //-------------------------------------------------
  void SimWireLBNE35t::produce(art::Event& evt)
  {
    // get the geometry to be able to figure out signal types and chan -> plane mappings
    art::ServiceHandle<geo::Geometry> geo;
    unsigned int signalSize = fNTicks;

    // vectors for working
    std::vector<short>    adcvec(signalSize, 0);	
    std::vector<short>    adcvecPreSpill(signalSize, 0);	
    std::vector<short>    adcvecPostSpill(signalSize, 0);	
    std::vector<const sim::SimChannel*> chanHandle;
    evt.getView(fDriftEModuleLabel,chanHandle);

    //Get fIndShape and fColShape from SignalShapingService, on the fly
    art::ServiceHandle<util::SignalShapingServiceLBNE10kt> sss;

    // make a vector of const sim::SimChannel* that has same number
    // of entries as the number of channels in the detector
    // and set the entries for the channels that have signal on them
    // using the chanHandle
    std::vector<const sim::SimChannel*> channels(geo->Nchannels());
    for(size_t c = 0; c < chanHandle.size(); ++c){
      channels[chanHandle[c]->Channel()] = chanHandle[c];
    }
    
    // make an unique_ptr of sim::SimDigits that allows ownership of the produced
    // digits to be transferred to the art::Event after the put statement below
    std::unique_ptr< std::vector<raw::RawDigit>   >  digcol(new std::vector<raw::RawDigit>);
    std::unique_ptr< std::vector<raw::RawDigit>   >  digcolPreSpill(new std::vector<raw::RawDigit>);
    std::unique_ptr< std::vector<raw::RawDigit>   >  digcolPostSpill(new std::vector<raw::RawDigit>);

	  
    unsigned int chan = 0; 
    fChargeWork.clear();
    fChargeWork.resize(fNTicks, 0.);
	  
    std::vector<double> fChargeWorkPreSpill, fChargeWorkPostSpill;

    art::ServiceHandle<util::LArFFT> fFFT;

    // Add all channels  
    art::ServiceHandle<art::RandomNumberGenerator> rng;
    CLHEP::HepRandomEngine &engine = rng->getEngine();
    CLHEP::RandFlat flat(engine);

    std::map<int,double>::iterator mapIter;      
    for(chan = 0; chan < geo->Nchannels(); chan++) {    
      
      fChargeWork.clear();    
      //      fChargeWork.resize(fNTicks, 0.);    
      fChargeWork.resize(fNTimeSamples, 0.);    


      fChargeWorkPreSpill.clear();
      fChargeWorkPostSpill.clear();
      fChargeWorkPreSpill.resize(fNTicks,0);
      fChargeWorkPostSpill.resize(fNTicks,0);


      // get the sim::SimChannel for this channel
      const sim::SimChannel* sc = channels[chan];

      if( sc ){      

	// loop over the tdcs and grab the number of electrons for each
	for(size_t t = 0; t < fChargeWork.size(); ++t) 
	  fChargeWork[t] = sc->Charge(t);      

        // Convolve charge with appropriate response function 
	if(fNSamplesReadout != fNTimeSamples ) {
	  fChargeWorkPreSpill.assign(fChargeWork.begin(), fChargeWork.begin()+fNSamplesReadout);
	  fChargeWorkPostSpill.assign(fChargeWork.begin()+2*fNSamplesReadout, fChargeWork.end());

	  fChargeWork.erase( fChargeWork.begin()+2*fNSamplesReadout, fChargeWork.end() );
	  fChargeWork.erase( fChargeWork.begin(), fChargeWork.begin()+fNSamplesReadout );

	  fChargeWorkPreSpill.resize(fNTicks,0);
	  fChargeWorkPostSpill.resize(fNTicks,0);
	  sss->Convolute(chan, fChargeWorkPreSpill);
	  sss->Convolute(chan, fChargeWorkPostSpill);
	}
	fChargeWork.resize(fNTicks,0);
	sss->Convolute(chan,fChargeWork);

      }

      // noise was already generated for each wire in the event
      // raw digit vec is already in channel order
      // pick a new "noise channel" for every channel  - this makes sure    
      // the noise has the right coherent characteristics to be on one channel

      const geo::View_t plane = geo->View(chan);

      //int noisechan = TMath::Nint(flat.fire()*(1.*(geo->Nchannels()-1)+0.1));
      int noisechan = TMath::Nint(flat.fire()*(1.*(fNoiseArrayPoints-1)+0.1));
      for(unsigned int i = 0; i < signalSize; ++i){
	

	if(plane==geo::kU){
	  adcvec[i] = (short)TMath::Nint(fNoiseU[noisechan][i] + fChargeWork[i]);
	  adcvecPreSpill[i] = (short)TMath::Nint(fNoiseU[noisechan][i] + fChargeWorkPreSpill[i]);
	  adcvecPostSpill[i] = (short)TMath::Nint(fNoiseU[noisechan][i] + fChargeWorkPostSpill[i]);
	}
	else if(plane==geo::kV){
	  adcvec[i] = (short)TMath::Nint(fNoiseV[noisechan][i] + fChargeWork[i]);
	  adcvecPreSpill[i] = (short)TMath::Nint(fNoiseV[noisechan][i] + fChargeWorkPreSpill[i]);
	  adcvecPostSpill[i] = (short)TMath::Nint(fNoiseV[noisechan][i] + fChargeWorkPostSpill[i]);
	}
	else if(plane==geo::kW){
	  adcvec[i] = (short)TMath::Nint(fNoiseW[noisechan][i] + fChargeWork[i]);
	  adcvecPreSpill[i] = (short)TMath::Nint(fNoiseW[noisechan][i] + fChargeWorkPreSpill[i]);
	  adcvecPostSpill[i] = (short)TMath::Nint(fNoiseW[noisechan][i] + fChargeWorkPostSpill[i]);
	}
	else 
	  std::cout << "ERROR: CHANNEL NUMBER " << chan << " OUTSIDE OF PLANE" << std::endl;
      }// end loop over signal size

      // resize the adcvec to be the correct number of time samples, 
      // just drop the extra samples
      adcvec.resize(fNSamplesReadout);
      adcvecPreSpill.resize(fNSamplesReadout);
      adcvecPostSpill.resize(fNSamplesReadout);

      // compress the adc vector using the desired compression scheme,
      // if raw::kNone is selected nothing happens to adcvec
      // This shrinks adcvec, if fCompression is not kNone.

      
      raw::Compress(adcvec, fCompression, fZeroThreshold); 
      raw::Compress(adcvecPreSpill, fCompression, fZeroThreshold); 
      raw::Compress(adcvecPostSpill, fCompression, fZeroThreshold); 
      

      raw::RawDigit rd(chan, fNSamplesReadout, adcvec, fCompression);
      raw::RawDigit rdPreSpill(chan, fNSamplesReadout, adcvecPreSpill, fCompression);
      raw::RawDigit rdPostSpill(chan, fNSamplesReadout, adcvecPostSpill, fCompression);

      // Then, resize adcvec back to full length!
      adcvec.clear();
      adcvec.resize(signalSize,0.0);

      adcvecPreSpill.clear();
      adcvecPreSpill.resize(signalSize,0.0);
      adcvecPostSpill.clear();
      adcvecPostSpill.resize(signalSize,0.0);
      // add this digit to the collection
      digcol->push_back(rd);
      digcolPreSpill->push_back(rdPreSpill);
      digcolPostSpill->push_back(rdPostSpill);

    }// end loop over channels      

    evt.put(std::move(digcol));
    if(fNSamplesReadout != fNTimeSamples ) {
      evt.put(std::move(digcolPreSpill), "preSpill");
      evt.put(std::move(digcolPostSpill), "postSpill");
    }

    return;
  }

  //-------------------------------------------------
  void SimWireLBNE35t::GenNoise(std::vector<float>& noise)
  {
    art::ServiceHandle<art::RandomNumberGenerator> rng;
    CLHEP::HepRandomEngine &engine = rng->getEngine();
    CLHEP::RandFlat flat(engine);

    noise.clear();
    noise.resize(fNTicks, 0.);
    // noise in frequency space
    std::vector<TComplex> noiseFrequency(fNTicks/2+1, 0.);

    double pval = 0.; 
    double lofilter = 0.;
    double phase = 0.;
    double rnd[2] = {0.};

    // width of frequencyBin in kHz
    double binWidth = 1.0/(fNTicks*fSampleRate*1.0e-6);
    for(int i=0; i< fNTicks/2+1; ++i){
      // exponential noise spectrum 
      pval = fNoiseFact*exp(-(double)i*binWidth/fNoiseWidth);
      // low frequency cutoff     
      lofilter = 1.0/(1.0+exp(-(i-fLowCutoff/binWidth)/0.5));
      // randomize 10%
      flat.fireArray(2,rnd,0,1);
      pval *= lofilter*(0.9+0.2*rnd[0]);
      // random pahse angle
      phase = rnd[1]*2.*TMath::Pi();

      TComplex tc(pval*cos(phase),pval*sin(phase));
      noiseFrequency[i] += tc;
    }
  
    // std::cout << "filled noise freq" << std::endl;

    // inverse FFT MCSignal
    art::ServiceHandle<util::LArFFT> fFFT;
    fFFT->DoInvFFT(noiseFrequency, noise);

    noiseFrequency.clear();

    // multiply each noise value by fNTicks as the InvFFT 
    // divides each bin by fNTicks assuming that a forward FFT
    // has already been done.
    for(unsigned int i = 0; i < noise.size(); ++i) noise[i] *= 1.*fNTicks;

    return;
  }
}
