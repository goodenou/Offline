#ifndef PhysicsProcessInfo_H
#define PhysicsProcessInfo_H 
//
// Information about physics processes.
//
// $Id: PhysicsProcessInfo.hh,v 1.2 2011/01/04 22:07:20 kutschke Exp $
// $Author: kutschke $
// $Date: 2011/01/04 22:07:20 $
//
// Original author Rob Kutschke
//

#include <iosfwd>
#include <set>
#include <map>
#include <string>

#include "ToyDP/inc/ProcessCode.hh"

#include "G4String.hh"

namespace mu2e {

  class PhysicsProcessInfo {

  public:

    PhysicsProcessInfo();

    // Accept compiler written d'tor, copy c'tor an assignment operator.

    // Bulid _allProcesses.  Valid until the end of the run.
    void beginRun();

    // Clear information at end of run and write summary printout.
    void endRun();

    // Locate a process by its name, return the corresponding process code and
    // increment the counter.
    ProcessCode findAndCount( G4String const& name );
    
    void printAll ( std::ostream& os) const;
    void printSummary ( std::ostream& os) const;

    // Information about one physics process.
    struct ProcInfo{
      ProcInfo():procName(""),particleNames(),code(),count(0){}
      ProcInfo( std::string aprocName,
             ProcessCode acode ):
        procName(aprocName),
        particleNames(),
        code(acode),
        count(0){}
      
      std::string procName;
      std::vector<std::string> particleNames;
      ProcessCode code;
      size_t count;
    };

  private:

    // The information about all of the physics processes.
    typedef std::map<G4String,ProcInfo> map_type;
    map_type _allProcesses;

    // The length of the longest name; for formatting printed output.
    size_t _longestName;

  };

} // end namespace mu2e

#endif

