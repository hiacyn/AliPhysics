/**************************************************************************
* Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
*                                                                        *
* Author: The ALICE Off-line Project.                                    *
* Contributors are mentioned in the code where appropriate.              *
*                                                                        *
* Permission to use, copy, modify and distribute this software and its   *
* documentation strictly for non-commercial purposes is hereby granted   *
* without fee, provided that the above copyright notice appears in all   *
* copies and that both the copyright notice and this permission notice   *
* appear in the supporting documentation. The authors make no claims     *
* about the suitability of this software for any purpose. It is          *
* provided "as is" without express or implied warranty.                  *
**************************************************************************/

/* $Id: AliTRDinfoGen.cxx 27496 2008-07-22 08:35:45Z cblume $ */

////////////////////////////////////////////////////////////////////////////
//
//  Tender wagon for TRD performance/calibration train
//
// In this wagon the information from
//   - ESD
//   - Friends [if available]
//   - MC [if available]
// are grouped into AliTRDtrackInfo objects and fed to worker tasks
//
//  Authors:
//    Markus Fasel <M.Fasel@gsi.de>
//    Alexandru Bercuci <A.Bercuci@gsi.de>
//
////////////////////////////////////////////////////////////////////////////

#include <TClonesArray.h>
#include <TObjArray.h>
#include <TObject.h>
#include <TString.h>
#include <TH1S.h>
#include <TPad.h>
#include <TFile.h>
#include <TTree.h>
#include <TROOT.h>
#include <TChain.h>
#include <TParticle.h>

#include "AliLog.h"
#include "AliAnalysisManager.h"
#include "AliGeomManager.h"
#include "AliCDBManager.h"
#include "AliCDBEntry.h"
#include "AliCDBPath.h"
#include "AliESDEvent.h"
#include "AliMCEvent.h"
#include "AliESDInputHandler.h"
#include "AliMCEventHandler.h"

#include "AliESDfriend.h"
#include "AliESDfriendTrack.h"
#include "AliESDHeader.h"
#include "AliESDtrack.h"
#include "AliESDv0.h"
#include "AliESDtrackCuts.h"
#include "AliMCParticle.h"
#include "AliPID.h"
#include "AliStack.h"
#include "AliTrackReference.h"
#include "TTreeStream.h"

#include <cstdio>
#include <climits>
#include <cstring>
#include <iostream>
#include <memory>

#include "AliTRDReconstructor.h"
#include "AliTRDrecoParam.h"
#include "AliTRDcalibDB.h"
#include "AliTRDtrackerV1.h"
#include "AliTRDgeometry.h"
#include "AliTRDtrackV1.h"
#include "AliTRDseedV1.h"
#include "AliTRDcluster.h"
#include "AliTRDinfoGen.h"
#include "AliTRDpwg1Helper.h"
#include "info/AliTRDtrackInfo.h"
#include "info/AliTRDeventInfo.h"
#include "info/AliTRDv0Info.h"
#include "info/AliTRDeventCuts.h"

ClassImp(AliTRDinfoGen)

const Float_t AliTRDinfoGen::fgkITS = 100.; // to be checked
const Float_t AliTRDinfoGen::fgkTPC = 290.;
const Float_t AliTRDinfoGen::fgkTRD = 365.;

const Float_t AliTRDinfoGen::fgkEvVertexZ = 15.;
const Int_t   AliTRDinfoGen::fgkEvVertexN = 1;

const Float_t AliTRDinfoGen::fgkTrkDCAxy  = 3.;
const Float_t AliTRDinfoGen::fgkTrkDCAz   = 10.;
const Int_t   AliTRDinfoGen::fgkNclTPC    = 70;
const Float_t AliTRDinfoGen::fgkPt        = 0.2;
const Float_t AliTRDinfoGen::fgkEta       = 0.9;
AliTRDReconstructor* AliTRDinfoGen::fgReconstructor(NULL);
AliTRDgeometry* AliTRDinfoGen::fgGeo(NULL);
//____________________________________________________________________
AliTRDinfoGen::AliTRDinfoGen()
  :AliAnalysisTaskSE()
  ,fEvTrigger(NULL)
  ,fESDev(NULL)
  ,fMCev(NULL)
  ,fEventCut(NULL)
  ,fTrackCut(NULL)
  ,fV0Cut(NULL)
  ,fOCDB("local://$ALICE_ROOT/OCDB")
  ,fTrackInfo(NULL)
  ,fEventInfo(NULL)
  ,fV0Info(NULL)
  ,fTracksBarrel(NULL)
  ,fTracksSA(NULL)
  ,fTracksKink(NULL)
  ,fV0List(NULL)
  ,fContainer(NULL)
  ,fDebugStream(NULL)
{
  //
  // Default constructor
  //
  SetNameTitle("TRDinfoGen", "MC-REC TRD-track list generator");
}

//____________________________________________________________________
AliTRDinfoGen::AliTRDinfoGen(char* name)
  :AliAnalysisTaskSE(name)
  ,fEvTrigger(NULL)
  ,fESDev(NULL)
  ,fMCev(NULL)
  ,fEventCut(NULL)
  ,fTrackCut(NULL)
  ,fV0Cut(NULL)
  ,fOCDB("local://$ALICE_ROOT/OCDB")
  ,fTrackInfo(NULL)
  ,fEventInfo(NULL)
  ,fV0Info(NULL)
  ,fTracksBarrel(NULL)
  ,fTracksSA(NULL)
  ,fTracksKink(NULL)
  ,fV0List(NULL)
  ,fContainer(NULL)
  ,fDebugStream(NULL)
{
  //
  // Default constructor
  //
  SetTitle("MC-REC TRD-track list generator");
  DefineOutput(AliTRDpwg1Helper::kTracksBarrel, TObjArray::Class());
  DefineOutput(AliTRDpwg1Helper::kTracksSA, TObjArray::Class());
  DefineOutput(AliTRDpwg1Helper::kTracksKink, TObjArray::Class());
  DefineOutput(AliTRDpwg1Helper::kEventInfo, AliTRDeventInfo::Class());
  DefineOutput(AliTRDpwg1Helper::kV0List, TObjArray::Class());
  DefineOutput(AliTRDpwg1Helper::kMonitor, TObjArray::Class()); // histogram list
}

//____________________________________________________________________
AliTRDinfoGen::~AliTRDinfoGen()
{
// Destructor
  if(fgGeo) delete fgGeo;
  if(fgReconstructor) delete fgReconstructor;
  if(fDebugStream) delete fDebugStream;
  if(fEvTrigger) delete fEvTrigger;
  if(fV0Cut) delete fV0Cut;
  if(fTrackCut) delete fTrackCut;
  if(fEventCut) delete fEventCut;
  if(fTrackInfo) delete fTrackInfo; fTrackInfo = NULL;
  if(fEventInfo) delete fEventInfo; fEventInfo = NULL;
  if(fV0Info) delete fV0Info; fV0Info = NULL;
  if(fTracksBarrel){ 
    fTracksBarrel->Delete(); delete fTracksBarrel;
    fTracksBarrel = NULL;
  }
  if(fTracksSA){ 
    fTracksSA->Delete(); delete fTracksSA;
    fTracksSA = NULL;
  }
  if(fTracksKink){ 
    fTracksKink->Delete(); delete fTracksKink;
    fTracksKink = NULL;
  }
  if(fV0List){ 
    fV0List->Delete(); 
    delete fV0List;
    fV0List = NULL;
  }
  if(fContainer){ 
    fContainer->Delete(); 
    delete fContainer;
    fContainer = NULL;
  }
}

//____________________________________________________________________
Bool_t AliTRDinfoGen::GetRefFigure(Int_t)
{
// General graphs for PWG1/TRD train
  if(!gPad){
    AliWarning("Please provide a canvas to draw results.");
    return kFALSE;
  }
  fContainer->At(0)->Draw("bar");
  return kTRUE;
}

//____________________________________________________________________
void AliTRDinfoGen::UserCreateOutputObjects()
{	
  //
  // Create Output Containers (TObjectArray containing 1D histograms)
  //
 
  fTrackInfo = new AliTRDtrackInfo();
  fEventInfo = new AliTRDeventInfo();
  fV0Info    = new AliTRDv0Info();
  fTracksBarrel = new TObjArray(200); fTracksBarrel->SetOwner(kTRUE);
  fTracksSA = new TObjArray(20); fTracksSA->SetOwner(kTRUE);
  fTracksKink = new TObjArray(20); fTracksKink->SetOwner(kTRUE);
  fV0List = new TObjArray(10); fV0List->SetOwner(kTRUE);

  // define general monitor
  fContainer = new TObjArray(1); fContainer->SetOwner(kTRUE);
  TH1 *h=new TH1S("hStat", "Run statistics;Observable;Entries", 12, -0.5, 11.5);
  TAxis *ax(h->GetXaxis());
  ax->SetBinLabel( 1, "ESD");
  ax->SetBinLabel( 2, "MC");
  ax->SetBinLabel( 3, "V0");
  ax->SetBinLabel( 4, "TPC");
  ax->SetBinLabel( 5, "TRDin");
  ax->SetBinLabel( 6, "TRDout");
  ax->SetBinLabel( 7, "Barrel");
  ax->SetBinLabel( 8, "BarrelMC");
  ax->SetBinLabel( 9, "SA");
  ax->SetBinLabel(10, "SAMC");
  ax->SetBinLabel(11, "Kink");
  ax->SetBinLabel(12, "KinkMC");
  fContainer->AddAt(h, 0);
  PostData(AliTRDpwg1Helper::kMonitor, fContainer);
}

//____________________________________________________________________
Bool_t AliTRDinfoGen::Load(const Char_t *file, const Char_t *dir, const Char_t *name)
{
// Load data from performance file

  if(!TFile::Open(file)){
    AliWarning(Form("Couldn't open file %s.", file));
    return kFALSE;
  }
  if(dir){
    if(!gFile->cd(dir)){
      AliWarning(Form("Couldn't cd to %s in %s.", dir, file));
      return kFALSE;
    }
  }
  TObjArray *o(NULL);
  const Char_t *tn=(name ? name : GetName());
  if(!(o = (TObjArray*)gDirectory->Get(tn))){
    AliWarning(Form("Missing histogram container %s.", tn));
    return kFALSE;
  }
  fContainer = (TObjArray*)o->Clone(GetName());
  gFile->Close();
  return kTRUE;
}

//____________________________________________________________________
void AliTRDinfoGen::UserExec(Option_t *){
  //
  // Run the Analysis
  //

  fTracksBarrel->Delete();
  fTracksSA->Delete();
  fTracksKink->Delete();
  fV0List->Delete();
  fEventInfo->Delete("");

  fESDev = dynamic_cast<AliESDEvent*>(InputEvent());
  if(!fESDev){
    AliError("Failed retrieving ESD event");
    return;
  }
  // WARNING
  // This part may conflict with other detectors !!
  if(!IsInitOCDB()){ 
    AliInfo("Initializing OCDB ...");
    // prepare OCDB access
    AliCDBManager* ocdb = AliCDBManager::Instance();
    ocdb->SetDefaultStorage(fOCDB.Data());
    ocdb->SetRun(fESDev->GetRunNumber());
    // create geo manager
    AliCDBEntry* obj = ocdb->Get(AliCDBPath("GRP", "Geometry", "Data"));
    AliGeomManager::SetGeometry((TGeoManager*)obj->GetObject());
    AliGeomManager::GetNalignable("TRD");
    AliGeomManager::ApplyAlignObjsFromCDB("TRD");
    fgGeo = new AliTRDgeometry;

    // set no of time bins
    AliTRDtrackerV1::SetNTimeBins(AliTRDcalibDB::Instance()->GetNumberOfTimeBinsDCS());
    AliInfo(Form("OCDB :  Loc[%s] Run[%d] TB[%d]", fOCDB.Data(), ocdb->GetRun(), AliTRDtrackerV1::GetNTimeBins()));

    AliInfo(Form("Initializing TRD reco params for EventSpecie[%d]...",
      fESDev->GetEventSpecie()));
    fgReconstructor = new AliTRDReconstructor();
    obj = ocdb->Get(AliCDBPath("TRD", "Calib", "RecoParam"));
    obj->PrintMetaData();
    TObjArray *recos((TObjArray*)obj->GetObject());
    for(Int_t ireco(0); ireco<recos->GetEntriesFast(); ireco++){
      AliTRDrecoParam *reco((AliTRDrecoParam*)recos->At(ireco));
      Int_t es(reco->GetEventSpecie());
      if(!(es&fESDev->GetEventSpecie())) continue;
      fgReconstructor->SetRecoParam(reco);
      TString s;
      if(es&AliRecoParam::kLowMult) s="LowMult";
      else if(es&AliRecoParam::kHighMult) s="HighMult";
      else if(es&AliRecoParam::kCosmic) s="Cosmic";
      else if(es&AliRecoParam::kCalib) s="Calib";
      else s="Unknown";
      AliInfo(Form("Using reco params for %s", s.Data()));
      break;
    }
    SetInitOCDB();
  }

  // link MC if available
  fMCev = MCEvent();

  // event selection : trigger cut
  if(UseLocalEvSelection() && fEvTrigger){ 
    Bool_t kTRIGGERED(kFALSE);
    std::auto_ptr<TObjArray> trig(fEvTrigger->Tokenize(" "));
    for(Int_t itrig=trig->GetEntriesFast(); itrig--;){
      const Char_t *trigClass(((TObjString*)(*trig)[itrig])->GetName());
      if(fESDev->IsTriggerClassFired(trigClass)) {
        AliDebug(2, Form("Ev[%4d] Trigger[%s]", fESDev->GetEventNumberInFile(), trigClass));
        kTRIGGERED = kTRUE;
        break; 
      }
    }
    if(!kTRIGGERED){ 
      AliDebug(2, Form("Reject Ev[%4d] Trigger", fESDev->GetEventNumberInFile()));
      return;
    }
    // select only physical events
    if(fESDev->GetEventType() != 7){ 
      AliDebug(2, Form("Reject Ev[%4d] EvType[%d]", fESDev->GetEventNumberInFile(), fESDev->GetEventType()));
      return;
    }
  }

  // if the required trigger is a collision trigger then apply event vertex cut
  if(UseLocalEvSelection() && IsCollision()){
    const AliESDVertex *vertex = fESDev->GetPrimaryVertex();
    if(TMath::Abs(vertex->GetZv())<1.e-10 || 
       TMath::Abs(vertex->GetZv())>fgkEvVertexZ || 
       vertex->GetNContributors()<fgkEvVertexN) {
      AliDebug(2, Form("Reject Ev[%4d] Vertex Zv[%f] Nv[%d]", fESDev->GetEventNumberInFile(), TMath::Abs(vertex->GetZv()), vertex->GetNContributors()));
      return;
    }
  }

  if(fEventCut && !fEventCut->IsSelected(fESDev, IsCollision())) return;

  if(!fESDfriend){
    AliError("Failed retrieving ESD friend event");
    return;
  }
  if(HasMCdata() && !fMCev){
    AliError("Failed retrieving MC event");
    return;
  }

  new(fEventInfo)AliTRDeventInfo(fESDev->GetHeader(), const_cast<AliESDRun *>(fESDev->GetESDRun()));
  
  Bool_t *trackMap(NULL);
  AliStack * mStack(NULL);
  Int_t nTracksMC = HasMCdata() ? fMCev->GetNumberOfTracks() : 0, nTracksESD = fESDev->GetNumberOfTracks();
  if(HasMCdata()){
    mStack = fMCev->Stack();
    if(!mStack){
      AliError("Failed retrieving MC Stack");
      return;
    }
    trackMap = new Bool_t[nTracksMC];
    memset(trackMap, 0, sizeof(Bool_t) * nTracksMC);
  }
  
  Double32_t dedx[100]; Int_t nSlices(0);
  Int_t nTRDout(0), nTRDin(0), nTPC(0)
       ,nclsTrklt
       ,nBarrel(0), nSA(0), nKink(0)
       ,nBarrelMC(0), nSAMC(0), nKinkMC(0);
  AliESDtrack *esdTrack = NULL;
  AliESDfriendTrack *esdFriendTrack = NULL;
  TObject *calObject = NULL;
  AliTRDtrackV1 *track = NULL;
  AliTRDseedV1 *tracklet = NULL;
  AliTRDcluster *cl = NULL;


  // LOOP 0 - over ESD v0s
  Float_t bField(fESDev->GetMagneticField());
  AliESDv0 *v0(NULL);
  Int_t v0pid[AliPID::kSPECIES];
  for(Int_t iv0(0); iv0<fESDev->GetNumberOfV0s(); iv0++){
    if(!(v0 = fESDev->GetV0(iv0))) continue;
    // register v0
    if(fV0Cut) new(fV0Info) AliTRDv0Info(*fV0Cut);
    else new(fV0Info) AliTRDv0Info();
    fV0Info->SetMagField(bField);
    fV0Info->SetV0tracks(fESDev->GetTrack(v0->GetPindex()), fESDev->GetTrack(v0->GetNindex()));
    fV0Info->SetV0Info(v0);
    fV0List->Add(new AliTRDv0Info(*fV0Info));//  kFOUND=kFALSE;
  }


  // LOOP 1 - over ESD tracks
  AliTRDv0Info *v0info=NULL;
  for(Int_t itrk = 0; itrk < nTracksESD; itrk++){
    new(fTrackInfo) AliTRDtrackInfo();
    esdTrack = fESDev->GetTrack(itrk);
    AliDebug(3, Form("\n%3d ITS[%d] TPC[%d] TRD[%d]\n", itrk, esdTrack->GetNcls(0), esdTrack->GetNcls(1), esdTrack->GetNcls(2)));

    if(esdTrack->GetStatus()&AliESDtrack::kTPCout) nTPC++;
    if(esdTrack->GetStatus()&AliESDtrack::kTRDout) nTRDout++;
    if(esdTrack->GetStatus()&AliESDtrack::kTRDin) nTRDin++;

    // look at external track param
    const AliExternalTrackParam *op = esdTrack->GetOuterParam();
    Double_t xyz[3];
    if(op){
      op->GetXYZ(xyz);
      op->Global2LocalPosition(xyz, op->GetAlpha());
      AliDebug(3, Form("op @ X[%7.3f]\n", xyz[0]));
    }

    // read MC info
    Int_t fPdg = -1;
    Int_t label = -1; UInt_t alab=UINT_MAX;
    if(HasMCdata()){
      label = esdTrack->GetLabel(); 
      alab = TMath::Abs(label);
      // register the track
      if(alab < UInt_t(nTracksMC)){ 
        trackMap[alab] = kTRUE; 
      } else { 
        AliError(Form("MC label[%d] outside scope for Ev[%d] Trk[%d].", label, (Int_t)AliAnalysisManager::GetAnalysisManager()->GetCurrentEntry(), itrk));
        continue; 
      }
      AliMCParticle *mcParticle = NULL; 
      if(!(mcParticle = (AliMCParticle*) fMCev->GetTrack(alab))){
        AliError(Form("MC particle label[%d] missing for Ev[%d] Trk[%d].", label, (Int_t)AliAnalysisManager::GetAnalysisManager()->GetCurrentEntry(), itrk));
        continue;
      }
      fPdg = mcParticle->Particle()->GetPdgCode();
      Int_t nRefs = mcParticle->GetNumberOfTrackReferences();
      Int_t iref = 0; AliTrackReference *ref = NULL; 
      while(iref<nRefs){
        ref = mcParticle->GetTrackReference(iref);
        if(ref->LocalX() > fgkTPC) break;
        iref++;
      }

      fTrackInfo->SetMC();
      fTrackInfo->SetPDG(fPdg);
      fTrackInfo->SetPrimary(mcParticle->Particle()->IsPrimary());
      fTrackInfo->SetLabel(label);
      Int_t jref = iref;//, kref = 0;
      while(jref<nRefs){
        ref = mcParticle->GetTrackReference(jref);
        if(ref->LocalX() > fgkTRD) break;
        AliDebug(4, Form("  trackRef[%2d (%2d)] @ %7.3f OK", jref-iref, jref, ref->LocalX()));
        fTrackInfo->AddTrackRef(ref);
        jref++;
      }
      AliDebug(3, Form("NtrackRefs[%d(%d)]", fTrackInfo->GetNTrackRefs(), nRefs));
    }

    // copy some relevant info to TRD track info
    fTrackInfo->SetStatus(esdTrack->GetStatus());
    fTrackInfo->SetTrackId(esdTrack->GetID());
    Double_t p[AliPID::kSPECIES]; esdTrack->GetTRDpid(p);
    fTrackInfo->SetESDpid(p);
    fTrackInfo->SetESDpidQuality(esdTrack->GetTRDntrackletsPID());
    if(!nSlices) nSlices = esdTrack->GetNumberOfTRDslices();
    memset(dedx, 0, 100*sizeof(Double32_t));
    Int_t in(0);
    for(Int_t il=0; il<AliTRDgeometry::kNlayer; il++)
      for(Int_t is=0; is<nSlices; is++) 
        dedx[in++]=esdTrack->GetTRDslice(il, is);
    for(Int_t il=0; il<AliTRDgeometry::kNlayer; il++) dedx[in++]=esdTrack->GetTRDmomentum(il);
    fTrackInfo->SetSlices(in, dedx);
    fTrackInfo->SetNumberOfClustersRefit(esdTrack->GetNcls(2));
    // some other Informations which we may wish to store in order to find problematic cases
    fTrackInfo->SetKinkIndex(esdTrack->GetKinkIndex(0));
    fTrackInfo->SetTPCncls(static_cast<UShort_t>(esdTrack->GetNcls(1)));
    nclsTrklt = 0;
  
    // set V0pid info
    for(Int_t iv(0); iv<fV0List->GetEntriesFast(); iv++){
      if(!(v0info = (AliTRDv0Info*)fV0List->At(iv))) continue;
      if(!v0info->fTrackP && !v0info->fTrackN) continue;
      if(!v0info->HasTrack(fTrackInfo)) continue;
      memset(v0pid, 0, AliPID::kSPECIES*sizeof(Int_t));
      fTrackInfo->SetV0();
      for(Int_t is=AliPID::kSPECIES; is--;){v0pid[is] = v0info->GetPID(is, fTrackInfo);}
      fTrackInfo->SetV0pid(v0pid);
      fTrackInfo->SetV0();
      //const AliTRDtrackInfo::AliESDinfo *ei = fTrackInfo->GetESDinfo();
      break;
    }

    // read REC info
    esdFriendTrack = fESDfriend->GetTrack(itrk);
    if(esdFriendTrack){
      Int_t icalib = 0;
      while((calObject = esdFriendTrack->GetCalibObject(icalib++))){
        if(strcmp(calObject->IsA()->GetName(),"AliTRDtrackV1") != 0) continue; // Look for the TRDtrack
        if(!(track = dynamic_cast<AliTRDtrackV1*>(calObject))) break;
        AliDebug(4, Form("TRD track OK"));
        // Set the clusters to unused
        for(Int_t ipl = 0; ipl < AliTRDgeometry::kNlayer; ipl++){
          if(!(tracklet = track->GetTracklet(ipl))) continue;
          tracklet->ResetClusterIter();
          while((cl = tracklet->NextCluster())) cl->Use(0);
        }
        fTrackInfo->SetTrack(track);
        break;
      }
      AliDebug(3, Form("Ntracklets[%d]\n", fTrackInfo->GetNTracklets()));
    } else AliDebug(3, "No ESD friends");
    if(op) fTrackInfo->SetOuterParam(op);

    if(DebugLevel() >= 1){
      AliTRDtrackInfo info(*fTrackInfo);
      (*DebugStream()) << "trackInfo"
      << "TrackInfo.=" << &info
      << "\n";
      info.Delete("");
    }

    ULong_t status(esdTrack->GetStatus());
    if((status&AliESDtrack::kTPCout)){
      if(!esdTrack->GetKinkIndex(0)){ // Barrel  Track Selection
        Bool_t selected(kTRUE);
        if(UseLocalTrkSelection()){
          if(esdTrack->Pt() < fgkPt){ 
            AliDebug(2, Form("Reject Ev[%4d] Trk[%3d] Pt[%5.2f]", fESDev->GetEventNumberInFile(), itrk, esdTrack->Pt()));
            selected = kFALSE;
          }
          if(selected && TMath::Abs(esdTrack->Eta()) > fgkEta){
            AliDebug(2, Form("Reject Ev[%4d] Trk[%3d] Eta[%5.2f]", fESDev->GetEventNumberInFile(), itrk, TMath::Abs(esdTrack->Eta())));
            selected = kFALSE;
          }
          if(selected && esdTrack->GetTPCNcls() < fgkNclTPC){ 
            AliDebug(2, Form("Reject Ev[%4d] Trk[%3d] NclTPC[%d]", fESDev->GetEventNumberInFile(), itrk, esdTrack->GetTPCNcls()));
            selected = kFALSE;
          }
          Float_t par[2], cov[3];
          esdTrack->GetImpactParameters(par, cov);
          if(IsCollision()){ // cuts on DCA
            if(selected && TMath::Abs(par[0]) > fgkTrkDCAxy){ 
              AliDebug(2, Form("Reject Ev[%4d] Trk[%3d] DCAxy[%f]", fESDev->GetEventNumberInFile(), itrk, TMath::Abs(par[0])));
              selected = kFALSE;
            }
            if(selected && TMath::Abs(par[1]) > fgkTrkDCAz){ 
              AliDebug(2, Form("Reject Ev[%4d] Trk[%3d] DCAz[%f]", fESDev->GetEventNumberInFile(), itrk, TMath::Abs(par[1])));
              selected = kFALSE;
            }
          } else if(selected && fMCev && !fMCev->IsPhysicalPrimary(alab)){;
            AliDebug(2, Form("Reject Ev[%4d] Trk[%3d] Primary", fESDev->GetEventNumberInFile(), itrk));
            selected = kFALSE;
          }
        }
        if(fTrackCut && !fTrackCut->IsSelected(esdTrack)) selected = kFALSE;
        if(selected){ 
          fTracksBarrel->Add(new AliTRDtrackInfo(*fTrackInfo));
          nBarrel++;
        }
      } else {
        fTracksKink->Add(new AliTRDtrackInfo(*fTrackInfo));
        nKink++;
      }
    } else if((status&AliESDtrack::kTRDout) && !(status&AliESDtrack::kTRDin)){ 
      fTracksSA->Add(new AliTRDtrackInfo(*fTrackInfo));
      nSA++;
    }
    fTrackInfo->Delete("");
  }

  // LOOP 2 - over MC tracks which are passing TRD where the track is not reconstructed
  if(HasMCdata()){
    AliDebug(10, "Output of the MC track map:");
    for(Int_t itk = 0; itk < nTracksMC;  itk++) AliDebug(10, Form("trackMap[%d] = %s", itk, trackMap[itk] == kTRUE ? "TRUE" : "kFALSE"));
  
    for(Int_t itk = 0; itk < nTracksMC; itk++){
      if(trackMap[itk]) continue;
      AliMCParticle *mcParticle =  (AliMCParticle*) fMCev->GetTrack(TMath::Abs(itk));
      Int_t fPdg = mcParticle->Particle()->GetPdgCode();
      Int_t nRefs = mcParticle->GetNumberOfTrackReferences();
      Int_t iref = 0; AliTrackReference *ref = NULL; 
      Int_t nRefsTRD = 0;
      new(fTrackInfo) AliTRDtrackInfo();
      fTrackInfo->SetMC();
      fTrackInfo->SetPDG(fPdg);
      while(iref<nRefs){ // count TRD TR
        Bool_t kIN(kFALSE);
        ref = mcParticle->GetTrackReference(iref);
        if(ref->LocalX() > fgkTPC && ref->LocalX() < fgkTRD){
          fTrackInfo->AddTrackRef(ref);
          nRefsTRD++;kIN=kTRUE;
        }
        AliDebug(4, Form("  trackRef[%2d] @ x[%7.3f] %s", iref, ref->LocalX(), kIN?"IN":"OUT"));
        iref++;
      }
      if(!nRefsTRD){
        // In this stage we at least require 1 hit inside TRD. What will be done with this tracks is a task for the 
        // analysis job
        fTrackInfo->Delete("");
        continue;
      }
      fTrackInfo->SetPrimary(mcParticle->Particle()->IsPrimary());
      fTrackInfo->SetLabel(itk);
      if(DebugLevel() >= 1){
        AliTRDtrackInfo info(*fTrackInfo);
        (*DebugStream()) << "trackInfo"
        << "TrackInfo.=" << &info
        << "\n";
        info.Delete("");
      }
      AliDebug(3, Form("Add MC track @ label[%d] nTRDrefs[%d].", itk, nRefsTRD));
      // check where the track starts
      ref = mcParticle->GetTrackReference(0);
      if(ref->LocalX() < fgkITS){ 
        fTracksBarrel->Add(new AliTRDtrackInfo(*fTrackInfo));
        nBarrelMC++;
      } else if(ref->LocalX() < fgkTPC) {
        fTracksKink->Add(new AliTRDtrackInfo(*fTrackInfo));
        nKinkMC++;
      } else if(nRefsTRD>6){
        fTracksSA->Add(new AliTRDtrackInfo(*fTrackInfo));
        nSAMC++;
      }
      fTrackInfo->Delete("");
    }
    delete[] trackMap;
  }
  AliDebug(1, Form(
    "\nEv[%3d] Tracks: ESD[%d] MC[%d] V0[%d]\n"
    "        TPCout[%d] TRDin[%d] TRDout[%d]\n"
    "        Barrel[%3d+%3d=%3d] SA[%2d+%2d=%2d] Kink[%2d+%2d=%2d]"
    ,(Int_t)AliAnalysisManager::GetAnalysisManager()->GetCurrentEntry(), nTracksESD, nTracksMC, fV0List->GetEntries()
    , nTPC, nTRDin, nTRDout
    ,nBarrel, nBarrelMC, fTracksBarrel->GetEntries()
    ,nSA, nSAMC, fTracksSA->GetEntries()
    ,nKink, nKinkMC, fTracksKink->GetEntries()
  ));
  // save track statistics
  TH1 *h((TH1S*)fContainer->At(0));
  h->Fill( 0., nTracksESD);
  h->Fill( 1., nTracksMC);
  h->Fill( 2., fV0List->GetEntries());
  h->Fill( 3., nTPC);
  h->Fill( 4., nTRDin);
  h->Fill( 5., nTRDout);
  h->Fill( 6., nBarrel);
  h->Fill( 7., nBarrelMC);
  h->Fill( 8., nSA);
  h->Fill( 9., nSAMC);
  h->Fill(10., nKink);
  h->Fill(11., nKinkMC);

  PostData(AliTRDpwg1Helper::kTracksBarrel, fTracksBarrel);
  PostData(AliTRDpwg1Helper::kTracksSA, fTracksSA);
  PostData(AliTRDpwg1Helper::kTracksKink, fTracksKink);
  PostData(AliTRDpwg1Helper::kEventInfo, fEventInfo);
  PostData(AliTRDpwg1Helper::kV0List, fV0List);
}

//____________________________________________________________________
void AliTRDinfoGen::SetLocalV0Selection(const AliTRDv0Info *v0)
{
// Set V0 cuts from outside

  if(!fV0Cut) fV0Cut = new AliTRDv0Info(*v0);
  else new(fV0Cut) AliTRDv0Info(*v0);
}

//____________________________________________________________________
void AliTRDinfoGen::SetTrigger(const Char_t *trigger)
{
  if(!fEvTrigger) fEvTrigger = new TString(trigger);
  else (*fEvTrigger) = trigger;
}

//____________________________________________________________________
TTreeSRedirector* AliTRDinfoGen::DebugStream()
{
// Manage debug stream for task
  if(!fDebugStream){
    TDirectory *savedir = gDirectory;
    fDebugStream = new TTreeSRedirector("TRD.DebugInfoGen.root");
    savedir->cd();
  }
  return fDebugStream;
}



