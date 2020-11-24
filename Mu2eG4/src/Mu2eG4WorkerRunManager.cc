//
// Override the G4RunManager class so that the Mu2e framework can drive
// the event loop.
//
// Original author Lisa Goodenough
//
//
// Notes:
//
// Implementation file for Mu2eG4WorkerRunManager

//Framework includes
#include "cetlib_except/exception.h"
#include "cstddef"

#include "/mu2e/app/users/goodenou/cryptopp/sha3.h"
#include "/mu2e/app/users/goodenou/cryptopp/hex.h"
#include "/mu2e/app/users/goodenou/cryptopp/files.h"
//#include "sha.h"
//#include "hex.h"
//#include "files.h"

#include <string>

//Mu2e includes
#include "Mu2eG4/inc/Mu2eG4WorkerRunManager.hh"
#include "Mu2eG4/inc/Mu2eG4PerThreadStorage.hh"
#include "Mu2eG4/inc/Mu2eG4MTRunManager.hh"
#include "Mu2eG4/inc/SteppingVerbose.hh"
#include "Mu2eG4/inc/WorldMaker.hh"
#include "Mu2eG4/inc/physicsListDecider.hh"
#include "Mu2eG4/inc/PrimaryGeneratorAction.hh"
#include "Mu2eG4/inc/Mu2eG4SteppingAction.hh"
#include "Mu2eG4/inc/Mu2eG4StackingAction.hh"
#include "Mu2eG4/inc/TrackingAction.hh"
#include "Mu2eG4/inc/Mu2eG4RunAction.hh"
#include "Mu2eG4/inc/Mu2eG4EventAction.hh"
#include "Mu2eG4/inc/Mu2eG4MultiStageParameters.hh"
#include "Mu2eG4/inc/ExtMonFNALPixelSD.hh"

//G4 includes
#include "G4WorkerThread.hh"
#include "G4StateManager.hh"
#include "G4UserWorkerThreadInitialization.hh"
#include "G4VPhysicalVolume.hh"
#include "G4TransportationManager.hh"
#include "G4VUserPhysicsList.hh"
#include "G4ParallelWorldProcessStore.hh"
#include "G4ParticleHPManager.hh"
#include "G4HadronicProcessStore.hh"

//Other includes
#include "CLHEP/Random/JamesRandom.h"
#include <tbb/atomic.h>


using namespace std;

namespace {
  tbb::atomic<int> thread_counter{0};
  int get_new_thread_index() { return thread_counter++; }
  thread_local int s_thread_index = get_new_thread_index();
  int getThreadIndex() { return s_thread_index; }
  
  
  string CryptoPP_Hash(const string msg){
    
    CryptoPP::SHA3_224 hash;
    std::string digest, str_hex;
    
    //digest is 56 hex numbers, or 28 pairs of hex numbers = 28 bytes
    //compute the digest and keep 8 bytes of it
    CryptoPP::StringSource(msg, true, new CryptoPP::HashFilter(hash, new CryptoPP::StringSink(digest), false, 8));
    CryptoPP::CRYPTOPP_DLL HexEncoder encoder_out(new CryptoPP::StringSink(str_hex));
    CryptoPP::StringSource(digest, true, new CryptoPP::Redirector(encoder_out));
    return str_hex;
  }
  
}

namespace mu2e {


  // If the c'tor is called a second time, the c'tor of base will
  // generate an exception.
  Mu2eG4WorkerRunManager::Mu2eG4WorkerRunManager(const Mu2eG4Config::Top& conf, std::thread::id worker_ID):
    G4WorkerRunManager(),
    conf_(conf),
    m_managerInitialized(false),
    m_steppingVerbose(true),
    m_mtDebugOutput(conf.debug().mtDebugOutput()),
    rmvlevel_(conf.debug().diagLevel()),
    salt_(conf.salt()),
    perThreadObjects_(std::make_unique<Mu2eG4PerThreadStorage>()),
    masterRM(nullptr),
    workerID_(worker_ID),
    mu2elimits_(conf.ResourceLimits()),
    trajectoryControl_(conf.TrajectoryControl()),
    multiStagePars_(conf),

    physicsProcessInfo_(),
    sensitiveDetectorHelper_(conf.SDConfig()),
    extMonFNALPixelSD_(),
    stackingCuts_(createMu2eG4Cuts(conf.Mu2eG4StackingOnlyCut.get<fhicl::ParameterSet>(), mu2elimits_)),
    steppingCuts_(createMu2eG4Cuts(conf.Mu2eG4SteppingOnlyCut.get<fhicl::ParameterSet>(), mu2elimits_)),
    commonCuts_(createMu2eG4Cuts(conf.Mu2eG4CommonCut.get<fhicl::ParameterSet>(), mu2elimits_))
  {

    if (m_mtDebugOutput > 0) {
      G4cout << "WorkerRM on thread " << workerID_ << " is being created\n!";
      //to see random number seeds for each event and other verbosity, uncomment this
      //SetPrintProgress(1);
    }
  }

  // Destructor of base is called automatically.  No need to do anything.
  Mu2eG4WorkerRunManager::~Mu2eG4WorkerRunManager(){
    
    if (m_mtDebugOutput > 0) {
      G4cout << "WorkerRM on thread " << workerID_ << " is being destroyed\n!";
    }
  }


  void Mu2eG4WorkerRunManager::initializeThread(Mu2eG4MTRunManager* mRM, const G4ThreeVector& origin_in_world){

    masterRM = mRM;

    if (m_mtDebugOutput > 0) {
      G4cout << "starting WorkerRM::initializeThread on thread: " << workerID_ << G4endl;
    }

    G4Threading::G4SetThreadId(getThreadIndex());

    const CLHEP::HepRandomEngine* masterEngine = masterRM->getMasterRandomEngine();
    masterRM->GetUserWorkerThreadInitialization()->SetupRNGEngine(masterEngine);

    // Initialize worker part of shared resources (geometry, physics)
    G4WorkerThread::BuildGeometryAndPhysicsVector();

    // Set the geometry for the worker, share from master
    G4VPhysicalVolume* worldPV = G4MTRunManager::GetMasterRunManagerKernel()->GetCurrentWorld();
    kernel->WorkerDefineWorldVolume(worldPV);

    G4TransportationManager::GetTransportationManager()->SetWorldForTracking(worldPV);

    const G4VUserDetectorConstruction* detector = masterRM->GetUserDetectorConstruction();
    G4RunManager::SetUserInitialization( const_cast<G4VUserDetectorConstruction*>(detector) );
    const_cast<G4VUserDetectorConstruction*>(detector)->ConstructSDandField();

    // Set the physics list for the worker, share from master
    physicsList = const_cast<G4VUserPhysicsList*>(masterRM->GetUserPhysicsList());
    SetUserInitialization(physicsList);

    physicsList->SetVerboseLevel(rmvlevel_);
    SetVerboseLevel(rmvlevel_);
    G4ParticleHPManager::GetInstance()->SetVerboseLevel(rmvlevel_);
    G4HadronicProcessStore::Instance()->SetVerbose(rmvlevel_);

    //these called in G4RunManager::InitializePhysics()
    G4StateManager::GetStateManager()->SetNewState(G4State_Init);
    kernel->InitializePhysics();

    const bool kernelInit = kernel->RunInitialization();
    if (!kernelInit) {
      throw cet::exception("WorkerRUNMANAGER")
        << "Error: WorkerRunManager Geant4 kernel initialization failed!\n";
    }

    initializeUserActions(origin_in_world);

    G4StateManager* stateManager = G4StateManager::GetStateManager();
    G4String currentState =  stateManager->GetStateString(stateManager->GetCurrentState());

    //we have to do this so that the state is correct for RunInitialization
    G4StateManager::GetStateManager()->SetNewState(G4State_Idle);
    if (m_mtDebugOutput > 0) {
      G4cout << "completed WorkerRM::initializeThread on thread " << workerID_ << G4endl;
    }
  }


  void Mu2eG4WorkerRunManager::initializeUserActions(const G4ThreeVector& origin_in_world){

    userPrimaryGeneratorAction = new PrimaryGeneratorAction(conf_.debug(), perThreadObjects_.get());
    SetUserAction(userPrimaryGeneratorAction);

    steppingAction_ = new Mu2eG4SteppingAction(conf_.debug(),
                                               conf_.SDConfig().TimeVD().times(),
                                               *steppingCuts_.get(),
                                               *commonCuts_.get(),
                                               trajectoryControl_,
                                               mu2elimits_);
    SetUserAction(steppingAction_);

    SetUserAction( new Mu2eG4StackingAction(*stackingCuts_.get(),
                                            *commonCuts_.get()) );

    trackingAction_ = new TrackingAction(conf_,
                                         steppingAction_,
                                         multiStagePars_.simParticleNumberOffset(),
                                         trajectoryControl_,
                                         mu2elimits_);
    SetUserAction(trackingAction_);

    SetUserAction( new Mu2eG4RunAction(conf_.debug(),
                                       origin_in_world,
                                       masterRM->getPhysVolumeHelper(),
                                       &physicsProcessInfo_,
                                       trackingAction_,
                                       steppingAction_,
                                       &sensitiveDetectorHelper_) );

    SetUserAction( new Mu2eG4EventAction(conf_,
                                         trackingAction_,
                                         steppingAction_,
                                         &sensitiveDetectorHelper_,
                                         *stackingCuts_.get(),
                                         *steppingCuts_.get(),
                                         *commonCuts_.get(),
                                         perThreadObjects_.get(),
                                         &physicsProcessInfo_,
                                         origin_in_world) );

  }

  void Mu2eG4WorkerRunManager::initializeRun(art::Event* art_event){

    perThreadObjects_->currentRunNumber = art_event->id().run();
      
    ConstructScoringWorlds();

    //following taken from G4WorkerRunManager::RunInitialization()
    fakeRun = false;
    if(!(kernel->RunInitialization(fakeRun))) return;

    CleanUpPreviousEvents();
    if(currentRun) delete currentRun;
    currentRun = 0;

    if(fGeometryHasBeenDestroyed) G4ParallelWorldProcessStore::GetInstance()->UpdateWorlds();
    if(userRunAction) currentRun = userRunAction->GenerateRun();

    if(!currentRun) currentRun = new G4Run();
    currentRun->SetRunID(runIDCounter);
    currentRun->SetNumberOfEventToBeProcessed(numberOfEventToBeProcessed);
    currentRun->SetDCtable(DCtable);

    G4SDManager* fSDM = G4SDManager::GetSDMpointerIfExist();

    if(fSDM) currentRun->SetHCtable(fSDM->GetHCtable());

    std::ostringstream oss;
    G4Random::saveFullState(oss);
    randomNumberStatusForThisRun = oss.str();
    currentRun->SetRandomNumberStatus(randomNumberStatusForThisRun);

    for(G4int i_prev=0;i_prev<n_perviousEventsToBeStored;i_prev++) {
      previousEvents->push_back((G4Event*)0);
    }

    if(printModulo>=0 || verboseLevel>0) {
      G4cout << "### Run " << currentRun->GetRunID() << " starts." << G4endl;
    }

    if(userRunAction) userRunAction->BeginOfRunAction(currentRun);

    if(storeRandomNumberStatus) {
      G4String fileN = "currentRun";
      if ( rngStatusEventsFlag ) {
        std::ostringstream os;
        os << "run" << currentRun->GetRunID();
        fileN = os.str();
      }
      StoreRNGStatus(fileN);
    }

    runAborted = false;
    numberOfEventProcessed = 0;
    m_managerInitialized = true;
    
  }


  void Mu2eG4WorkerRunManager::processEvent(art::Event* event){

    numberOfEventToBeProcessed = 1;
    
    runIsSeeded = false;
    eventLoopOnGoing = true;
    std::string i_event = to_string(event->id().event());
    std::string i_subrun = to_string(event->id().subRun());
    std::string i_run = to_string(event->id().run());
    
    // below code is from ProcessOneEvent(i_event);
     currentEvent = generateEvt(i_event, i_subrun, i_run);
    
     if(eventLoopOnGoing) {
     eventManager->ProcessOneEvent(currentEvent);
     AnalyzeEvent(currentEvent);
     UpdateScoring();
     }
  }
  
  
  G4Event* Mu2eG4WorkerRunManager::generateEvt(std::string i_event, std::string i_subrun, std::string i_run){
    
    G4Event* anEvent = new G4Event(std::stoi(i_event));
    long s1 = 0;
    long s2 = 0;
    G4bool eventHasToBeSeeded = true;
    
    eventLoopOnGoing = masterRM->SetUpEvent();
    runIsSeeded = true;
    
    if(!eventLoopOnGoing)
    {
      delete anEvent;
      return 0;
    }
    
    if(eventHasToBeSeeded)
    {
      std::string msg = "r" + i_run + "s" + i_subrun + "e" + i_event + salt_;

      std::string hash_out = CryptoPP_Hash(msg);
      std::vector<std::string> randnumstrings = {hash_out.substr(0,8), hash_out.substr(8,8)};
 
      long rn1 = std::stol(randnumstrings[0],nullptr,16);
      long rn2 = std::stol(randnumstrings[1],nullptr,16);
      long seeds[3] = { rn1, rn2, 0 };
      G4Random::setTheSeeds(seeds,-1);
      runIsSeeded = true;
    }
    
    //This is the filename base constructed from run and event
    const auto filename = [&] {
      std::ostringstream os;
      os << "run" << currentRun->GetRunID() << "evt" << anEvent->GetEventID();
      return os.str();
    };
    
    G4bool RNGstatusReadFromFile = false;
    if ( readStatusFromFile ) {
      //Build full path of RNG status file for this event
      std::ostringstream os;
      os << filename() << ".rndm";
      const G4String& randomStatusFile = os.str();
      std::ifstream ifile(randomStatusFile.c_str());
      if ( ifile ) { //File valid and readable
        RNGstatusReadFromFile = true;
        G4Random::restoreEngineStatus(randomStatusFile.c_str());
      }
    }
    
    
    if(storeRandomNumberStatusToG4Event==1 || storeRandomNumberStatusToG4Event==3) {
      std::ostringstream oss;
      G4Random::saveFullState(oss);
      randomNumberStatusForThisEvent = oss.str();
      anEvent->SetRandomNumberStatus(randomNumberStatusForThisEvent);
    }
    
    if(storeRandomNumberStatus && ! RNGstatusReadFromFile ) { //If reading from file, avoid to rewrite the same
      G4String fileN = "currentEvent";
      if ( rngStatusEventsFlag ) {
        fileN = filename();
      }
      StoreRNGStatus(fileN);
    }
    
    if(printModulo > 0 && anEvent->GetEventID()%printModulo == 0 ) {
      G4cout << "--> Event " << anEvent->GetEventID() << " starts";
      if(eventHasToBeSeeded && m_mtDebugOutput > 1) {
        G4cout << " with initial seeds (" << s1 << "," << s2 << ")";
      }
      G4cout << "." << G4endl;
    }
    
    userPrimaryGeneratorAction->GeneratePrimaries(anEvent);
    return anEvent;
    
  }

} // end namespace mu2e
