# Configuration file to test RandomNumberGenerator
#
#
# $Id: test01.py,v 1.2 2010/03/07 22:01:00 kutschke Exp $
# $Author: kutschke $
# $Date: 2010/03/07 22:01:00 $
#
# Save random numbers at start of each event.
# Test fire instances of Randflat and RandGaussQ every event.
#
# Original author Rob Kutschke
#

# Define the default configuratio for the framework.
import FWCore.ParameterSet.python.Config as mu2e

# Give this job a name.  
process = mu2e.Process("RNGTest01")

# Maximum number of events to do.
process.maxEvents = mu2e.untracked.PSet(
    input = mu2e.untracked.int32(5)
)

# Load the standard message logger configuration.
# Threshold=Info. Limit of 5 per category; then exponential backoff.
process.load("Config/MessageLogger_cfi")

# Initialize the random number sequences.
# This just changes the seed for the global CLHEP random engine.
process.add_(mu2e.Service("RandomNumberService",
                          globalSeed=mu2e.untracked.int32(9877),
                          debug=mu2e.untracked.bool(True)
))

# Define and configure some modules to do work on each event.
# Modules are just defined for now, the are scheduled later.

process.source      = mu2e.Source("EmptySource")
process.rngtest     = mu2e.EDAnalyzer("RNGTest")
process.randomsaver = mu2e.EDAnalyzer("RandomNumberSaver",
                                      debug=mu2e.untracked.bool(True)
                                      )
process.outfile     = mu2e.OutputModule("PoolOutputModule",
                      fileName = mu2e.untracked.string('file:randomtest_01.root'),
)

# Tell the system to execute all paths.
process.output = mu2e.EndPath(  process.rngtest*process.randomsaver*process.outfile );

