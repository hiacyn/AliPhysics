AliAnalysisTaskSELc2V0bachelorTMVA* AddTaskLc2V0bachpA_TMVA(TString finname="Lc2V0bachelorCuts.root",
							    Bool_t theMCon=kTRUE,
							    Bool_t onTheFly=kFALSE,
							    Bool_t keepingOnlyHIJINGbkd=kFALSE,
							    TString suffixName=""){
  
  AliAnalysisManager *mgr = AliAnalysisManager::GetAnalysisManager();
  if (!mgr) {
    ::Error("AddTaskLc2V0bachelor", "No analysis manager to connect to.");
    return NULL;
  }  
  
  // cuts are stored in a TFile generated by makeTFile4CutsLc2V0bachelor.C in ./macros/
  // set there the cuts!!!!!
  Bool_t stdcuts=kFALSE;
  TFile* filecuts;
  if( finname.EqualTo("") ) {
    stdcuts=kTRUE; 
  } else {
    filecuts=TFile::Open(finname.Data());
    if(!filecuts ||(filecuts&& !filecuts->IsOpen())){
      AliFatal("Input file not found : check your cut object");
    }
  }
  
  AliRDHFCutsLctoV0* RDHFCutsLctoV0anal = new AliRDHFCutsLctoV0();
  if (stdcuts) RDHFCutsLctoV0anal->SetStandardCutsPP2010();
  else RDHFCutsLctoV0anal = (AliRDHFCutsLctoV0*)filecuts->Get("LctoV0AnalysisCuts");
  RDHFCutsLctoV0anal->SetName("LctoV0AnalysisCuts");
  RDHFCutsLctoV0anal->SetMinPtCandidate(-1.);
  RDHFCutsLctoV0anal->SetMaxPtCandidate(10000.);
  
  
  // mm let's see if everything is ok
  if (!RDHFCutsLctoV0anal) {
    cout << "Specific AliRDHFCutsLctoV0 not found\n";
    return;
  }
  
  
  //CREATE THE TASK
  printf("CREATE TASK\n");
  AliAnalysisTaskSELc2V0bachelorTMVA *task = new AliAnalysisTaskSELc2V0bachelorTMVA("AliAnalysisTaskSELc2V0bachelorTMVA", RDHFCutsLctoV0anal, onTheFly);
  task->SetIspA(kTRUE);
  task->SetMC(theMCon);
  task->SetKeepingKeepingOnlyHIJINGBkg(keepingOnlyHIJINGbkd);
  task->SetK0sAnalysis(kTRUE);
  task->SetDebugLevel(0);
  mgr->AddTask(task);
  
  // Create and connect containers for input/output  
  //TString outputfile = AliAnalysisManager::GetCommonFileName();
  TString outputfile = Form("Lc2K0Sp_tree_pA%s.root", suffixName.Data());
  TString output1name="", output2name="", output3name="", output4name="", output5name="", output6name="", output7name="";

  output1name = Form("treeList%s", suffixName.Data());
  output2name = Form("Lc2pK0Scounter%s", suffixName.Data());
  output3name = Form("Lc2pK0SCuts%s", suffixName.Data());
  output4name = Form("treeSgn%s", suffixName.Data());
  output5name = Form("treeBkg%s", suffixName.Data());
  output6name = Form("listHistoKF%s", suffixName.Data());
  output7name = Form("weights%s", suffixName.Data());

  mgr->ConnectInput(task, 0, mgr->GetCommonInputContainer());
  AliAnalysisDataContainer *coutput1   = mgr->CreateContainer(output1name, TList::Class(), AliAnalysisManager::kOutputContainer, outputfile.Data()); // trees
  mgr->ConnectOutput(task, 1, coutput1);
  
  AliAnalysisDataContainer *coutputLc2 = mgr->CreateContainer(output2name, AliNormalizationCounter::Class(), AliAnalysisManager::kOutputContainer, outputfile.Data()); //counter
  mgr->ConnectOutput(task, 2, coutputLc2);
  
  AliAnalysisDataContainer *coutputLc3 = mgr->CreateContainer(output3name, TList::Class(), AliAnalysisManager::kOutputContainer, outputfile.Data()); // cuts
  mgr->ConnectOutput(task, 3, coutputLc3);
  
  AliAnalysisDataContainer *coutput4   = mgr->CreateContainer(output4name, TTree::Class(), AliAnalysisManager::kOutputContainer, outputfile.Data()); // trees
  mgr->ConnectOutput(task, 4, coutput4);
  
  AliAnalysisDataContainer *coutput5   = mgr->CreateContainer(output5name, TTree::Class(), AliAnalysisManager::kOutputContainer, outputfile.Data()); // trees
  mgr->ConnectOutput(task, 5, coutput5);
  
  AliAnalysisDataContainer *coutput6   = mgr->CreateContainer(output6name, TList::Class(), AliAnalysisManager::kOutputContainer, outputfile.Data()); // trees
  mgr->ConnectOutput(task, 6, coutput6);  
  
  AliAnalysisDataContainer *coutput7   = mgr->CreateContainer(output7name, TList::Class(), AliAnalysisManager::kOutputContainer, outputfile.Data()); // weights
  mgr->ConnectOutput(task, 7, coutput7);
  
  return task;
  
}
