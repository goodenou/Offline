#ifndef CosmicRayShieldGeom_CosmicRayShield_hh
#define CosmicRayShieldGeom_CosmicRayShield_hh

//
// Representation of CosmicRayShield
//
// $Id: CosmicRayShield.hh,v 1.8 2011/05/20 20:21:47 wb Exp $
// $Author: wb $
// $Date: 2011/05/20 20:21:47 $
//
// Original author KLG
//

// c++ includes
#include <map>
#include <string>

// clhep includes
#include "CLHEP/Vector/Rotation.h"
#include "CLHEP/Vector/ThreeVector.h"

// Includes from Mu2e
#include "CosmicRayShieldGeom/inc/CRSScintillatorShield.hh"
#include "CosmicRayShieldGeom/inc/CRSSteelShield.hh"
#include "GeometryService/inc/Detector.hh"


namespace mu2e {

  // Forward reference.
  class SimpleConfig;
  class CosmicRayShieldMaker;

  class CosmicRayShield : public Detector {

    friend class CosmicRayShieldMaker;

  public:

    CosmicRayShield():
      _name("CosmicRayShield")
    {}

    ~CosmicRayShield(){}

    std::string name() const { return _name;};

    // Get SteelShield
    CRSSteelShield         const & getCRSSteelShield(std::string name) const;

    std::map<std::string,CRSSteelShield> const & getCRSSteelShields() const {
      return _steelShields;
    }

    // Get ScintillatorShield
    CRSScintillatorShield  const & getCRSScintillatorShield(std::string name)  const;

    std::map<std::string,CRSScintillatorShield> const & getCRSScintillatorShields() const {
      return _scintillatorShields;
    }

    CRSScintillatorBarDetail const & getCRSScintillatorBarDetail() const {
      return _barDetails;
    }

    std::vector<CRSScintillatorBar> const & getAllCRSScintillatorBars() const {
      return _allCRSScintillatorBars;
    }

    const CRSScintillatorBar& getCRSScintillatorBar ( CRSScintillatorBarIndex index ) const {
      return _allCRSScintillatorBars.at(index.asInt());
    }

    CLHEP::Hep3Vector const & getGlobalOffset() const {
      return _globalOffset;
    }

    CLHEP::Hep3Vector const & getLocalOffset() const {
      return _localOffset;
    }

  private:

    std::string _name;

    // position of the center in the parent frame
    CLHEP::Hep3Vector _localOffset;

    // position of the center in  Mu2e frame
    CLHEP::Hep3Vector _globalOffset;

    std::map<std::string,CRSSteelShield>         _steelShields;

    std::map<std::string,CRSScintillatorShield>  _scintillatorShields;

    // Detailed info about scintillators etc...
    CRSScintillatorBarDetail _barDetails;

    // global holder of all scintillator bars
    std::vector<CRSScintillatorBar>  _allCRSScintillatorBars;

    // for a future reference:

    // given that the bars, layers and modules have the same rotation
    // angles as their shield, one could e.g. store the rotations as
    // HepRotation in the shields or in CosmicRayShield and point to
    // them from the lower level objects; similarly for their half
    // lengths, one could store layer and module half lengths as "details"
    // in CosmicRayShield and point to them from those objects

  };

}
#endif /* CosmicRayShieldGeom_CosmicRayShield_hh */
