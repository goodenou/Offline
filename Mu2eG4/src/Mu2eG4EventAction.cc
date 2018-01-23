//
// G4 begin and end of event actions for Mu2e.
//
// Author: Lisa Goodeough
// Date: 2017/05/04
//

// Mu2e includes
#include "Mu2eG4/inc/Mu2eG4EventAction.hh"
#include "Mu2eG4/inc/TrackingAction.hh"
#include "Mu2eG4/inc/Mu2eG4SteppingAction.hh"
#include "Mu2eG4/inc/SensitiveDetectorHelper.hh"
#include "Mu2eG4/inc/ExtMonFNALPixelSD.hh"
#include "Mu2eG4/inc/SimParticleHelper.hh"
#include "Mu2eG4/inc/SimParticlePrimaryHelper.hh"
#include "MCDataProducts/inc/ExtMonFNALSimHitCollection.hh"
#include "MCDataProducts/inc/SimParticleRemapping.hh"
#include "Mu2eG4/inc/IMu2eG4Cut.hh"
#include "MCDataProducts/inc/StatusG4.hh"
#include "Mu2eG4/inc/GenEventBroker.hh"
#include "Mu2eG4/inc/PerEventObjectsManager.hh"
#include "Mu2eG4/inc/EventStash.hh"


#include "G4RunManagerKernel.hh"
#include "G4SDManager.hh"


//G4 includes
#include "G4Timer.hh"


//art includes
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Principal/Event.h"

//C++ includes
#include <iostream>

using namespace std;

namespace mu2e {

    Mu2eG4EventAction::Mu2eG4EventAction(const fhicl::ParameterSet& pset,
        TrackingAction* tracking_action,
        Mu2eG4SteppingAction* stepping_action,
        ExtMonFNALPixelSD* extmon_FNAL_pixelSD,
        SensitiveDetectorHelper* sensitive_detectorhelper,
        IMu2eG4Cut &stacking_cuts,
        IMu2eG4Cut &stepping_cuts,
        IMu2eG4Cut &common_cuts,
        GenEventBroker *gen_eventbroker,
        PerEventObjectsManager *per_evtobjmanager,
        PhysicsProcessInfo* phys_process_info)
        :
    
        G4UserEventAction(),
        trajectoryControl_(pset.get<fhicl::ParameterSet>("TrajectoryControl")),
        simParticlePrinter_(pset.get<fhicl::ParameterSet>("SimParticlePrinter", SimParticleCollectionPrinter::defaultPSet())),
        timeVDtimes_(pset.get<std::vector<double> >("SDConfig.TimeVD.times")),
        multiStagePars_(pset.get<fhicl::ParameterSet>("MultiStageParameters")),
        _trackingAction(tracking_action),
        _steppingAction(stepping_action),
        _extMonFNALPixelSD(extmon_FNAL_pixelSD),
        _sensitiveDetectorHelper(sensitive_detectorhelper),
        _stackingCuts(stacking_cuts),
        _steppingCuts(stepping_cuts),
        _commonCuts(common_cuts),
        _genEventBroker(gen_eventbroker),
        perEvtObjManager(per_evtobjmanager),
        _tvdOutputName(StepInstanceName::timeVD),
        _timer(std::make_unique<G4Timer>()),
    
        _spHelper(),
        _parentHelper(),
        _processInfo(phys_process_info),
        _artEvent(),
        _stashForEventData(),
        eventNumberInProcess(-1)
        {
            if (G4Threading::G4GetThreadId()<= 0){
                
                std::cout << "From EventAction" << std::endl;
                G4SDManager* SDman = G4SDManager::GetSDMpointer();
                SDman->ListTree();
            }
        
        }
    
Mu2eG4EventAction::~Mu2eG4EventAction()
    {
        //std::cout << "AT Mu2eG4EventAction destructor" << std::endl;
    }


void Mu2eG4EventAction::BeginOfEventAction(const G4Event *evt)
    {
    
//        if (G4Threading::G4GetThreadId()<= 0)
//        {
//            std::cout << "***** AT EA::BeginOfEventAction *****" << std::endl;
//        }
        
        //G4RunManagerKernel const * rmk = G4RunManagerKernel::GetRunManagerKernel();
        //G4TrackingManager* tm  = rmk->GetTrackingManager();
        //tm->SetVerboseLevel(2);
    
        setEventData();
        
        _spHelper = perEvtObjManager->getSimParticleHelper();
        _parentHelper = perEvtObjManager->getSimParticlePrimaryHelper();
        
        // local Mu2e timer, almost equal to time of G4EventManager::ProcessOneEvent()
        _timer->Start();
    
        simParticles = unique_ptr<SimParticleCollection>( new SimParticleCollection );
        tvdHits = unique_ptr<StepPointMCCollection>( new StepPointMCCollection );
        mcTrajectories = unique_ptr<MCTrajectoryCollection>( new MCTrajectoryCollection );
        simsRemap = unique_ptr<SimParticleRemapping>( new SimParticleRemapping );
        extMonFNALHits = unique_ptr<ExtMonFNALSimHitCollection>( new ExtMonFNALSimHitCollection );
    

        //these will NEVER be run in MT mode, so we don't need a mutex and lock
        //on the _artEvent->getByLabel call, even though the call is not thread-safe
        art::Handle<SimParticleCollection> inputSimHandle;
        if(art::InputTag() != multiStagePars_.inputSimParticles()) {
            _artEvent->getByLabel(multiStagePars_.inputSimParticles(), inputSimHandle);
            
            if(!inputSimHandle.isValid()) {
                throw cet::exception("CONFIG")
                << "Error retrieving inputSimParticles for "
                << multiStagePars_.inputSimParticles() <<"\n";
            }
        }

        art::Handle<MCTrajectoryCollection> inputMCTrajectoryHandle;
        if(art::InputTag() != multiStagePars_.inputMCTrajectories()) {
            _artEvent->getByLabel(multiStagePars_.inputMCTrajectories(), inputMCTrajectoryHandle);

            if(!inputMCTrajectoryHandle.isValid()) {
                throw cet::exception("CONFIG")
                << "Error retrieving inputMCTrajectories for "
                << multiStagePars_.inputMCTrajectories() <<"\n";
            }
        }
        
        
        // NEW!  moved from module in order to make SDs that are accessible to the thread
        // the ones associated with the SDH
        //01/10/18: moved to RunAction, so that it is only called once per run,
        //not once per event
        //_sensitiveDetectorHelper->registerSensitiveDetectors();
        
        //also moved to RunAction
/* THIS NEEDS TO BE ACTIVATED ONCE EVERYTHING IS WORKING
        if (standardMu2eDetector_) _extMonFNALPixelSD =
            dynamic_cast<ExtMonFNALPixelSD*>(G4SDManager::GetSDMpointer()
                                             ->FindSensitiveDetector(SensitiveDetectorName::ExtMonFNAL()));
*/
        

        //these are OK, nothing put into or defined for event
        _sensitiveDetectorHelper->createProducts(*_artEvent, *_spHelper);
        _stackingCuts.beginEvent(*_artEvent, *_spHelper);
        _steppingCuts.beginEvent(*_artEvent, *_spHelper);
        _commonCuts.beginEvent(*_artEvent, *_spHelper);
        
        _trackingAction->beginEvent(inputSimHandle, inputMCTrajectoryHandle, *_spHelper,
                                *_parentHelper, *mcTrajectories, *simsRemap);

        _steppingAction->BeginOfEvent(*tvdHits, *_spHelper);
        
        _sensitiveDetectorHelper->updateSensitiveDetectors(*_processInfo, *_spHelper);
        
        if(_extMonFNALPixelSD) {
            _extMonFNALPixelSD->beforeG4Event(extMonFNALHits.get(), *_spHelper);
        }
        
        
//       if (G4Threading::G4GetThreadId()<= 0){

//          std::cout << "From EventAction" << std::endl;
//            G4SDManager* SDman = G4SDManager::GetSDMpointer();
//            SDman->ListTree();
//        }
    }

    
void Mu2eG4EventAction::EndOfEventAction(const G4Event *evt)
    {
        
        // Run self consistency checks if enabled.
        _trackingAction->endEvent(*simParticles);
        
        _timer->Stop();
    
        // Populate the output data products.
    
        // Fill the status object.
        float cpuTime  = _timer->GetSystemElapsed() + _timer->GetUserElapsed();
    
        int status(0);
        if ( _steppingAction->nKilledStepLimit() > 0 ) status =  1;
        if ( _trackingAction->overflowSimParticles() ) status = 10;
    
        unique_ptr<StatusG4> g4stat(new StatusG4( status,
                                                 _trackingAction->nG4Tracks(),
                                                 _trackingAction->overflowSimParticles(),
                                                 _steppingAction->nKilledStepLimit(),
                                                 cpuTime,
                                                 _timer->GetRealElapsed()
                                                 )
                                    );
        
        //NEED TO PUT THIS BACK IN ONCE EVERYTHING IS DONE
        //std::cout << "PRINTING INFO ABOUT SIMS from EventAction" << std::endl;
        //simParticlePrinter_.print(std::cout, *simParticles);
        
        // Add data products to the Stash
        
        //this will become the instance# corresponding to the generated particle
        //don't think this is needed in current design
        int instance_number = G4Threading::G4GetThreadId()+20;
        
/*        if (G4Threading::G4GetThreadId()<= 0)
        {
            std::cout << "completed art::event #" << _artEvent->id()
            << " in thread #" << G4Threading::G4GetThreadId() << std::endl;

            std::cout << "in EA::EndOfEventAction, the event instance # is " << eventNumberInProcess << std::endl;
            std::cout << "---------------------------------------------------" << std::endl;

        }
*/
        
        if (eventNumberInProcess == -1) {
            throw cet::exception("EVENTACTION")
            << "Invalid event number from generator stash being processed." << "\n"
            << "Event number was not incremented by PerEventObjectManager!" << "\n";

        }
        else
        {
            //we don't need a lock here because each thread accesses a different element of the stash
            _stashForEventData->insertData(eventNumberInProcess, instance_number,
                                           std::move(g4stat),
                                           std::move(simParticles));
        
        
            if(!timeVDtimes_.empty()) {
                std::cout << "timeVDtimes not EMPTY" << std::endl;
                _stashForEventData->insertTVDHits(eventNumberInProcess, std::move(tvdHits), _tvdOutputName.name());
            }
        
            if(trajectoryControl_.produce()) {
                _stashForEventData->insertMCTrajectoryCollection(eventNumberInProcess, std::move(mcTrajectories));
            }
        
        
            if(multiStagePars_.multiStage()) {
                _stashForEventData->insertSimsRemapping(eventNumberInProcess, std::move(simsRemap));
            }
        
        
            if(_sensitiveDetectorHelper->extMonPixelsEnabled()) {
                _stashForEventData->insertExtMonFNALSimHits(eventNumberInProcess, std::move(extMonFNALHits));
            }
        
            _sensitiveDetectorHelper->insertSDDataIntoStash(eventNumberInProcess,_stashForEventData);
    
            _stackingCuts.insertCutsDataIntoStash(eventNumberInProcess,_stashForEventData);
            _steppingCuts.insertCutsDataIntoStash(eventNumberInProcess,_stashForEventData);
            _commonCuts.insertCutsDataIntoStash(eventNumberInProcess,_stashForEventData);
            
        
        }
    
    }
    
    
void Mu2eG4EventAction::setEventData()
    {
        _artEvent = _genEventBroker->getartEvent();
        _stashForEventData = _genEventBroker->getEventStash();
        eventNumberInProcess = perEvtObjManager->getEventInstanceNumber();
    }

} // end namespace mu2e
