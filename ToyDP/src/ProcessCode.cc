//
// An enum-matched-to-names class used to indicate why a SimParticle was
// created and why it stopped. The class contains enum entries for all
// physics processes known in G4; it also contains an enum entry to indicate
// that the particle is a primary particle and other enum entries to
// indicate that a particle was killed in one of the user actions written by G4.
//
// $Id: ProcessCode.cc,v 1.1 2011/01/04 22:04:53 kutschke Exp $
// $Author: kutschke $ 
// $Date: 2011/01/04 22:04:53 $
//
// Original author Rob Kutschke
//

#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include "ToyDP/inc/ProcessCode.hh"

#include <boost/static_assert.hpp>

using namespace std;

namespace mu2e {
  
  const char* ProcessCode::_name[] = { PROCESSCODE_NAMES };

  BOOST_STATIC_ASSERT(sizeof(ProcessCode::_name)/sizeof(char*) == ProcessCode::lastEnum);

  void ProcessCode::printAll( std::ostream& ost){
    ost << "List of known process codes: " << endl;
    for ( int i=0; i<lastEnum; ++i){
      ost << setw(3) << i << " " << _name[i] << std::endl;
    }
  }

  ProcessCode::ProcessCode( int id ):
    _id(static_cast<enum_type>(id)){
    if ( !isValid() ){
      ostringstream os;
      os << "Invalid ProcessCode::enum_type: " << id;
      throw std::logic_error ( os.str());
    }
  }
  
  ProcessCode ProcessCode::findByName ( std::string const& name){

    // Size must be at least 2 (for unknown and lastEnum).
    for ( size_t i=0; i<size(); ++i ){
      if ( _name[i] == name ){
        return ProcessCode(i);
      }
    }
    return ProcessCode(unknown);
  }

  // Return a vector of the codes that are mu2e specific.
  std::vector<ProcessCode> ProcessCode::mu2eCodes(){
    std::vector<ProcessCode> codes;
    for ( size_t i=0; i<size(); ++i ){
      string name = _name[i];
      if ( name.substr(0,4) == "mu2e" ){
        codes.push_back(ProcessCode(i));
      }
    }
    return codes;
  }


}
