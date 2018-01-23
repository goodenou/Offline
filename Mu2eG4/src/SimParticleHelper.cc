// Andrei Gaponenko, 2013

#include "Mu2eG4/inc/SimParticleHelper.hh"

#include "art/Framework/Principal/Event.h"
#include "G4Track.hh"
#include "G4Threading.hh"

namespace mu2e {
  SimParticleHelper::SimParticleHelper(unsigned particleNumberOffset,
                                       const art::ProductID& simID,
                                       const art::Event* event,
                                       const art::EDProductGetter* sim_prod_getter)
    : particleNumberOffset_(particleNumberOffset)
    , simID_(simID)
    , event_(event)
    , simProductGetter_(sim_prod_getter)
  {
  
      //std::cout << "in SPH::c'tor, ProcessIndex is " << simID_.processIndex() << std::endl;
      //std::cout << "in SPH::c'tor, ProductIndex is " << simID_.productIndex() << std::endl;
      
/*      if (G4Threading::G4GetThreadId() == 0) {

          std::cout << "in SPH, event_ address is " << event_ << std::endl;
          std::cout << "event # is: " << event_->event() << std::endl;
          if(simProductGetter_)
              std::cout << "in SPH, we got simProductGetter_!" << std::endl;

          if(!simProductGetter_)
              std::cout << "NO simProductGetter_!" << std::endl;
      }
 */

  }

  art::Ptr<SimParticle> SimParticleHelper::particlePtr(const G4Track *trk) const {
    return particlePtrFromG4TrackID(trk->GetTrackID());
  }

  art::Ptr<SimParticle> SimParticleHelper::particlePtrFromG4TrackID(int g4TrkID) const {
      
    return art::Ptr<SimParticle>(simID_,
                                 particleNumberOffset_ + g4TrkID,
                                 simProductGetter_);
      
  }

  SimParticleCollection::key_type SimParticleHelper::particleKeyFromG4TrackID(int g4TrkID) const {
    return SimParticleCollection::key_type(g4TrkID + particleNumberOffset_);
  }

  const art::EDProductGetter *SimParticleHelper::productGetter() const {
    return simProductGetter_;
  }

  const art::EDProductGetter *SimParticleHelper::otherProductGetter(art::ProductID otherID) const {
    return event_->productGetter(otherID);
  }
}
