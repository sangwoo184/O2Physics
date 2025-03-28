// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file f0980pbpbanalysis.cxx
/// \brief f0980 resonance analysis in PbPb collisions
/// \author Junlee Kim (jikim1290@gmail.com)

#include <CommonConstants/MathConstants.h>
#include <Framework/Configurable.h>
#include <cmath>
#include <array>
#include <cstdlib>
#include <chrono>
// #include <iostream>
#include <iostream>
#include <string>

#include "TLorentzVector.h"
#include "TRandom3.h"
#include "TF1.h"
#include "TVector2.h"
#include "Math/Vector3D.h"
#include "Math/Vector4D.h"
#include "Math/GenVector/Boost.h"
#include <TMath.h>

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/StepTHn.h"
#include "Framework/O2DatabasePDGPlugin.h"
#include "Framework/ASoAHelpers.h"
#include "Framework/StaticFor.h"

#include "Common/DataModel/PIDResponse.h"
#include "Common/DataModel/Multiplicity.h"
#include "Common/DataModel/Centrality.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/Qvectors.h"

#include "Common/Core/trackUtilities.h"
#include "Common/Core/TrackSelection.h"

#include "CommonConstants/PhysicsConstants.h"

#include "ReconstructionDataFormats/Track.h"

#include "DataFormatsParameters/GRPObject.h"
#include "DataFormatsParameters/GRPMagField.h"

#include "CCDB/CcdbApi.h"
#include "CCDB/BasicCCDBManager.h"

//from phi
#include "PWGLF/DataModel/EPCalibrationTables.h"
#include "Common/DataModel/PIDResponseITS.h"


using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using namespace o2::soa;
using namespace o2::constants::physics;

struct f0980pbpbanalysis {
  HistogramRegistry histos{
    "histos",
    {},
    OutputObjHandlingPolicy::AnalysisObject};

  Service<o2::ccdb::BasicCCDBManager> ccdb;
  o2::ccdb::CcdbApi ccdbApi;

  Configurable<std::string> cfgURL{"cfgURL", "http://alice-ccdb.cern.ch", "Address of the CCDB to browse"};
  Configurable<int64_t> ccdbNoLaterThan{"ccdbNoLaterThan", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), "Latest acceptable timestamp of creation for the object"};

  Configurable<float> cfgCutVertex{"cfgCutVertex", 10.0, "PV selection"};
  Configurable<bool> cfgQvecSel{"cfgQvecSel", true, "Reject events when no QVector"};
  Configurable<bool> cfgOccupancySel{"cfgOccupancySel", false, "Occupancy selection"};
  Configurable<int> cfgMaxOccupancy{"cfgMaxOccupancy", 999999, "maximum occupancy of tracks in neighbouring collisions in a given time range"};
  Configurable<int> cfgMinOccupancy{"cfgMinOccupancy", -100, "maximum occupancy of tracks in neighbouring collisions in a given time range"};
  Configurable<bool> cfgNCollinTR{"cfgNCollinTR", false, "Additional selection for the number of coll in time range"};
  Configurable<bool> cfgPVSel{"cfgPVSel", false, "Additional PV selection flag for syst"};
  Configurable<float> cfgPV{"cfgPV", 8.0, "Additional PV selection range for syst"};

  Configurable<float> cfgCentSel{"cfgCentSel", 80., "Centrality selection"};
  Configurable<int> cfgCentEst{"cfgCentEst", 1, "Centrality estimator, 1: FT0C, 2: FT0M"};

  Configurable<float> cfgMinPt{"cfgMinPt", 0.15, "Minimum transverse momentum for charged track"};
  Configurable<float> cfgMaxEta{"cfgMaxEta", 0.8, "Maximum pseudorapidiy for charged track"};
  Configurable<float> cfgMaxDCArToPVcut{"cfgMaxDCArToPVcut", 0.5, "Maximum transverse DCA"};
  Configurable<float> cfgMaxDCAzToPVcut{"cfgMaxDCAzToPVcut", 2.0, "Maximum longitudinal DCA"};
  Configurable<int> cfgTPCcluster{"cfgTPCcluster", 70, "Number of TPC cluster"};
  Configurable<float> cfgRatioTPCRowsOverFindableCls{"cfgRatioTPCRowsOverFindableCls", 0.8, "TPC Crossed Rows to Findable Clusters"};

  Configurable<float> cfgMinRap{"cfgMinRap", -0.5, "Minimum rapidity for pair"};
  Configurable<float> cfgMaxRap{"cfgMaxRap", 0.5, "Maximum rapidity for pair"};

  Configurable<bool> cfgPrimaryTrack{"cfgPrimaryTrack", true, "Primary track selection"};
  Configurable<bool> cfgGlobalWoDCATrack{"cfgGlobalWoDCATrack", true, "Global track selection without DCA"};
  Configurable<bool> cfgPVContributor{"cfgPVContributor", true, "PV contributor track selection"};

  Configurable<double> cMaxTOFnSigmaPion{"cMaxTOFnSigmaPion", 3.0, "TOF nSigma cut for Pion"}; // TOF
  Configurable<double> cMaxTPCnSigmaPion{"cMaxTPCnSigmaPion", 5.0, "TPC nSigma cut for Pion"}; // TPC
  Configurable<double> cMaxTPCnSigmaPionS{"cMaxTPCnSigmaPionS", 3.0, "TPC nSigma cut for Pion as a standalone"};
  Configurable<bool> cfgUSETOF{"cfgUSETOF", false, "TPC usage"};
  Configurable<int> cfgSelectPID{"cfgSelectPID", 0, "PID selection type"};
  Configurable<int> cfgSelectPtl{"cfgSelectPtl", 0, "Particle selection type"};

  Configurable<int> cfgnMods{"cfgnMods", 1, "The number of modulations of interest starting from 2"};
  Configurable<int> cfgNQvec{"cfgNQvec", 7, "The number of total Qvectors for looping over the task"};

  Configurable<std::string> cfgQvecDetName{"cfgQvecDetName", "FT0C", "The name of detector to be analyzed"};
  Configurable<std::string> cfgQvecRefAName{"cfgQvecRefAName", "TPCpos", "The name of detector for reference A"};
  Configurable<std::string> cfgQvecRefBName{"cfgQvecRefBName", "TPCneg", "The name of detector for reference B"};

  Configurable<bool> cfgRotBkg{"cfgRotBkg", true, "flag to construct rotational backgrounds"};
  Configurable<int> cfgNRotBkg{"cfgNRotBkg", 10, "the number of rotational backgrounds"};

  //for phi test
  Configurable<int> cfgCutOccupancy{"cfgCutOccupancy", 3000, "Occupancy cut"};
  Configurable<bool> cfgEvselITS{"cfgEvselITS", true, "Additional event selcection for ITS"};
  Configurable<float> cfgTPCSharedcluster{"cfgTPCSharedcluster", 0.4, "Maximum Number of TPC shared cluster"};
  Configurable<int> cfgITScluster{"cfgITScluster", 0, "Number of ITS cluster"};
  Configurable<bool> isDeepAngle{"isDeepAngle", false, "Deep Angle cut"};
  Configurable<double> cfgDeepAngle{"cfgDeepAngle", 0.04, "Deep Angle cut value"};
  Configurable<float> ConfFakeKaonCut{"ConfFakeKaonCut", 0.1, "Cut based on track from momentum difference"};
  Configurable<float> cfgCutTOFBeta{"cfgCutTOFBeta", 0.0, "cut TOF beta"};
  Configurable<bool> ispTdepPID{"ispTdepPID", true, "pT dependent PID"};
  Configurable<bool> isTOFOnly{"isTOFOnly", false, "use TOF only PID"};
  Configurable<bool> useGlobalTrack{"useGlobalTrack", true, "use Global track"};
  Configurable<bool> removefaketrack{"removefaketrack", true, "Remove fake track from momentum difference"};
  Configurable<float> nsigmaKaCutTPC{"nsigmacutKaTPC", 3.0, "Value of the TPC Nsigma Kaon cut"};


  ConfigurableAxis massAxis{"massAxis", {400, 0.2, 2.2}, "Invariant mass axis"};
  ConfigurableAxis ptAxis{"ptAxis", {VARIABLE_WIDTH, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 6.0, 7.0, 8.0, 10.0, 13.0, 20.0}, "Transverse momentum Binning"};
  ConfigurableAxis centAxis{"centAxis", {VARIABLE_WIDTH, 0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 100}, "Centrality interval"};

  TF1* fMultPVCutLow = nullptr;
  TF1* fMultPVCutHigh = nullptr;

  int detId;
  int refAId;
  int refBId;

  int qVecDetInd;
  int qVecRefAInd;
  int qVecRefBInd;

  float centrality;

  double angle;
  double relPhi;
  double relPhiRot;

  // double massPi = o2::constants::physics::MassPionCharged;
  double massPtl;

  TRandom* rn = new TRandom();
  // float theta2;

  Filter collisionFilter = nabs(aod::collision::posZ) < cfgCutVertex;
  Filter acceptanceFilter = (nabs(aod::track::eta) < cfgMaxEta && nabs(aod::track::pt) > cfgMinPt);
  Filter cutDCAFilter = (nabs(aod::track::dcaXY) < cfgMaxDCArToPVcut) && (nabs(aod::track::dcaZ) < cfgMaxDCAzToPVcut);
  //from phi
  Filter centralityFilter = nabs(aod::cent::centFT0C) < cfgCentSel;
  Filter PIDcutFilter = nabs(aod::pidtpc::tpcNSigmaKa) < nsigmaKaCutTPC;
  // Filter PIDcutFilter = nabs(aod::pidTPCFullKa::tpcNSigmaKa) < cMaxTPCnSigmaPion;

  using EventCandidates = soa::Filtered<soa::Join<aod::Collisions, aod::EvSels, aod::FT0Mults, aod::FV0Mults, aod::TPCMults, aod::CentFV0As, aod::CentFT0Ms, aod::CentFT0Cs, aod::CentFT0As, aod::Mults, aod::Qvectors, aod::EPCalibrationTables>>;
  // aod::EPCalibrationTables 추가됨
  using TrackCandidates = soa::Filtered<soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksDCA, aod::TrackSelection, aod::pidTPCFullPi, aod::pidTOFFullPi, aod::pidTOFFullKa, aod::pidTPCFullKa, aod::pidTOFbeta>>;
  // aod::pidTOFbeta 추가됨

  template <typename T>
  int getDetId(const T& name)
  {
    if (name.value == "FT0C") {
      return 0;
    } else if (name.value == "FT0A") {
      return 1;
    } else if (name.value == "FT0M") {
      return 2;
    } else if (name.value == "FV0A") {
      return 3;
    } else if (name.value == "TPCpos") {
      return 4;
    } else if (name.value == "TPCneg") {
      return 5;
    } else {
      return 0;
    }
  }

  template <typename TCollision>
  bool eventSelected(TCollision collision)
  {
    if (cfgSelectPID == 2) {
      // if (collision.alias_bit(kTVXinTRD)) {
      //   // return 0;
      // }
      // auto multNTracksPV = collision.multNTracksPV();
      // if (multNTracksPV < fMultPVCutLow->Eval(centrality)) {
      //   return 0;
      // }
      // if (multNTracksPV > fMultPVCutHigh->Eval(centrality)) {
      //   return 0;
      // }
      if (!collision.sel8()) {
        return 0;
      }
      if (!collision.triggereventep()) {
        return 0;
      }
      if (!collision.selection_bit(aod::evsel::kNoSameBunchPileup)) {
        return 0;
      }
      int occupancy = collision.trackOccupancyInTimeRange();
      if (occupancy > cfgCutOccupancy) {
        return 0;
      }
      if (cfgNCollinTR && !collision.selection_bit(o2::aod::evsel::kNoCollInTimeRangeStandard)) {
        return 0;
      }
      if (cfgEvselITS && !collision.selection_bit(o2::aod::evsel::kIsGoodITSLayersAll)) {
        return 0;
      }
      return 1;
    } else {
      if (!collision.sel8()) {
        return 0;
      }

      if (cfgCentSel < centrality) {
        return 0;
      }
      /*
          auto multNTracksPV = collision.multNTracksPV();
          if (multNTracksPV < fMultPVCutLow->Eval(centrality)) {
            return 0;
          }
          if (multNTracksPV > fMultPVCutHigh->Eval(centrality)) {
            return 0;
          }
      */
      if (!collision.selection_bit(aod::evsel::kIsGoodZvtxFT0vsPV)) {
        return 0;
      }
      if (!collision.selection_bit(aod::evsel::kNoSameBunchPileup)) {
        return 0;
      }
      if (cfgQvecSel && (collision.qvecAmp()[detId] < 1e-4 || collision.qvecAmp()[refAId] < 1e-4 || collision.qvecAmp()[refBId] < 1e-4)) {
        return 0;
      }
      if (cfgOccupancySel && (collision.trackOccupancyInTimeRange() > cfgMaxOccupancy || collision.trackOccupancyInTimeRange() < cfgMinOccupancy)) {
        return 0;
      }
      if (cfgNCollinTR && !collision.selection_bit(o2::aod::evsel::kNoCollInTimeRangeStandard)) {
        return 0;
      }
      if (cfgPVSel && std::abs(collision.posZ()) > cfgPV) {
        return 0;
      }
      return 1;
    }
  } // event selection

  template <typename TrackType>
  bool trackSelected(const TrackType track)
  {
    if (cfgSelectPID == 2){
      if (useGlobalTrack && !(track.isGlobalTrack() && track.isPVContributor() && track.itsNCls() > cfgITScluster && track.tpcNClsCrossedRows() > cfgTPCcluster && track.tpcFractionSharedCls() < cfgTPCSharedcluster)) {
      return 0;
      }
      if (!useGlobalTrack && !(track.tpcNClsFound() > cfgTPCcluster)) {
        return 0;
      }
      return 1;
    } else {
      if (std::abs(track.pt()) < cfgMinPt) {
        return 0;
      }
      if (std::fabs(track.eta()) > cfgMaxEta) {
        return 0;
      }
      if (std::fabs(track.dcaXY()) > cfgMaxDCArToPVcut) {
        return 0;
      }
      if (std::fabs(track.dcaZ()) > cfgMaxDCAzToPVcut) {
        return 0;
      }
      if (track.tpcNClsFound() < cfgTPCcluster) {
        return 0;
      }
      if (cfgPVContributor && !track.isPVContributor()) {
        return 0;
      }
      if (cfgPrimaryTrack && !track.isPrimaryTrack()) {
        return 0;
      }
      if (cfgGlobalWoDCATrack && !track.isGlobalTrackWoDCA()) {
        return 0;
      }
      if (track.tpcCrossedRowsOverFindableCls() < cfgRatioTPCRowsOverFindableCls) {
        return 0;
      }
      return 1;
    }
  }

  template <typename TrackType>
  bool selectionPID(const TrackType track)
  {
    if (cfgSelectPID == 0) {
      if (cfgUSETOF) {
        if (std::fabs(track.tofNSigmaPi()) > cMaxTOFnSigmaPion) {
          return 0;
        }
        if (std::fabs(track.tpcNSigmaPi()) > cMaxTPCnSigmaPion) {
          return 0;
        }
      }
      if (std::fabs(track.tpcNSigmaPi()) > cMaxTPCnSigmaPionS) {
        return 0;
      }
      return 1;
    } else if (cfgSelectPID == 1) {
      if (cfgUSETOF) {
        if (track.hasTOF()) {
          if (std::fabs(track.tofNSigmaPi()) > cMaxTOFnSigmaPion) {
            return 0;
          }
          if (std::fabs(track.tpcNSigmaPi()) > cMaxTPCnSigmaPion) {
            return 0;
          }
        } else {
          if (std::fabs(track.tpcNSigmaPi()) > cMaxTPCnSigmaPionS) {
            return 0;
          }
        }
      } else {
        if (std::fabs(track.tpcNSigmaPi()) > cMaxTPCnSigmaPionS) {
          return 0;
        }
      }
      return 1;
    } else if (cfgSelectPID == 2) {
      if (cfgSelectPtl == 0) {
        if (!track.hasTOF() && TMath::Abs(track.tpcNSigmaPi()) < cMaxTPCnSigmaPionS) {
          return 1;
        }
        if (track.hasTOF() && track.beta() > cfgCutTOFBeta && TMath::Abs(track.tpcNSigmaPi()) < cMaxTPCnSigmaPion && TMath::Abs(track.tofNSigmaPi()) < cMaxTOFnSigmaPion) {
          return 1;
        }
        return 0;
      } else {
        if (!track.hasTOF() && TMath::Abs(track.tpcNSigmaKa()) < 3) {
          return 1;
        }
        if (track.hasTOF() && track.beta() > cfgCutTOFBeta && TMath::Abs(track.tpcNSigmaKa()) < 3 && TMath::Abs(track.tofNSigmaKa()) < 3) {
          return 1;
        }
        return 0;
      }
    }
    return 0;
  }

  template <typename TrackType>
  bool selectionPIDpTdependent(const TrackType track)
  {
    if (track.pt() < 0.5 && TMath::Abs(track.tpcNSigmaKa()) < cMaxTPCnSigmaPionS) {
      return 0;
    }
    if (track.pt() >= 0.5 && track.hasTOF() && track.beta() > cfgCutTOFBeta && TMath::Abs(track.tpcNSigmaKa()) < cMaxTPCnSigmaPion && TMath::Abs(track.tofNSigmaKa()) < cMaxTOFnSigmaPion) {
      return 1;
    }
    if (!useGlobalTrack && !track.hasTPC()) {
      return 1;
    }
    return 0;
  }

  template <typename TrackType>
  bool selectionPID2(const TrackType track)
  {
    if (track.hasTOF() && track.beta() > cfgCutTOFBeta && TMath::Abs(track.tofNSigmaKa()) < cMaxTOFnSigmaPion) {
      return 1;
    }
    return 0;
  }

  template <typename TrackType1, typename TrackType2>
  bool selectionPair(const TrackType1 track1, const TrackType2 track2)
  {
    double pt1, pt2, pz1, pz2, p1, p2, angle;
    pt1 = track1.pt();
    pt2 = track2.pt();
    pz1 = track1.pz();
    pz2 = track2.pz();
    p1 = track1.p();
    p2 = track2.p();
    angle = TMath::ACos((pt1 * pt2 + pz1 * pz2) / (p1 * p2));
    if (isDeepAngle && angle < cfgDeepAngle) {
      return 0;
    }
    return 1;
  }
  double GetPhiInRange(double phi)
  {
    double result = phi;
    while (result < 0) {
      result = result + 2. * TMath::Pi() / 2;
    }
    while (result > 2. * TMath::Pi() / 2) {
      result = result - 2. * TMath::Pi() / 2;
    }
    return result;
  }
  double GetDeltaPsiSubInRange(double psi1, double psi2)
  {
    double delta = psi1 - psi2;
    if (TMath::Abs(delta) > TMath::Pi() / 2) {
      if (delta > 0.)
        delta -= 2. * TMath::Pi() / 2;
      else
        delta += 2. * TMath::Pi() / 2;
    }
    return delta;
  }

  template <typename TrackType>
  bool isFakeKaon(const TrackType track)
  {
    const auto pglobal = track.p();
    const auto ptpc = track.tpcInnerParam();
    if (TMath::Abs(pglobal - ptpc) > ConfFakeKaonCut) {
      return 1;
    }
    return 0;
  }

  template <typename TrackType>
  float getTpcNSigma(const TrackType track)
  {
    if (cfgSelectPtl == 0) {
      return track.tpcNSigmaPi();
    } else {
      return track.tpcNSigmaKa();
    }
  }

  template <typename TrackType>
  float getTofNSigma(const TrackType track)
  {
    if (cfgSelectPtl == 0) {
      return track.tofNSigmaPi();
    } else {
      return track.tofNSigmaKa();
    }
  }

  template<typename TrackType>
  double getITSResponse(const TrackType track)
  {
    o2::aod::ITSResponse itsResponse;
    if (cfgSelectPtl == 0) {
      return itsResponse.nSigmaITS<o2::track::PID::Pion>(track);
    } else {
      return itsResponse.nSigmaITS<o2::track::PID::Kaon>(track);
    }
  }

  template <bool IsMC, typename CollisionType, typename TracksType>
  void fillHistograms(const CollisionType& collision,
                      const TracksType& dTracks, int nmode)
  {
    qVecDetInd = detId * 4 + 3 + (nmode - 2) * cfgNQvec * 4;
    qVecRefAInd = refAId * 4 + 3 + (nmode - 2) * cfgNQvec * 4;
    qVecRefBInd = refBId * 4 + 3 + (nmode - 2) * cfgNQvec * 4;

    double eventPlaneDet = std::atan2(collision.qvecIm()[qVecDetInd], collision.qvecRe()[qVecDetInd]) / static_cast<float>(nmode);
    double eventPlaneRefA = std::atan2(collision.qvecIm()[qVecRefAInd], collision.qvecRe()[qVecRefAInd]) / static_cast<float>(nmode);
    double eventPlaneRefB = std::atan2(collision.qvecIm()[qVecRefBInd], collision.qvecRe()[qVecRefBInd]) / static_cast<float>(nmode);

    histos.fill(HIST("QA/EPhist"), centrality, eventPlaneDet);
    histos.fill(HIST("QA/EPResAB"), centrality, std::cos(static_cast<float>(nmode) * (eventPlaneDet - eventPlaneRefA)));
    histos.fill(HIST("QA/EPResAC"), centrality, std::cos(static_cast<float>(nmode) * (eventPlaneDet - eventPlaneRefB)));
    histos.fill(HIST("QA/EPResBC"), centrality, std::cos(static_cast<float>(nmode) * (eventPlaneRefA - eventPlaneRefB)));

    int test = 0;

    TLorentzVector pion1, pion2, pion2Rot, reco, recoRot;
    for (const auto& trk1 : dTracks) {
      if (!trackSelected(trk1)) {
        continue;
      }

      // if (!selectionPID(trk1)) {
      //     continue;
      // } //original

      if (!ispTdepPID && !selectionPID(trk1)) {
        continue;
      }
      if (ispTdepPID && !isTOFOnly && !selectionPIDpTdependent(trk1)) {
      continue;
      }
      if (isTOFOnly && !selectionPID2(trk1)) {
        continue;
      }
      if (useGlobalTrack && trk1.p() < 1.0 && !(getITSResponse(trk1) > -2.5 && getITSResponse(trk1) < 2.5)) {
        continue;
      }

      histos.fill(HIST("QA/Nsigma_TPC"), trk1.pt(), getTpcNSigma(trk1));
      histos.fill(HIST("QA/Nsigma_TOF"), trk1.pt(), getTofNSigma(trk1));
      histos.fill(HIST("QA/TPC_TOF"), getTpcNSigma(trk1), getTofNSigma(trk1));

      test++;
      if (test < 10) {
        std::cout << "index : " << trk1.index() << "global index :" << trk1.globalIndex() << std::endl;
      }

      for (const auto& trk2 : dTracks) {
        if (!trackSelected(trk2)) {
          continue;
        }

        // // PID
        // if (!selectionPID(trk2)) {
        //   continue;
        // } //original

        if (!ispTdepPID && !selectionPID(trk2)) {
          continue;
        }
        if (ispTdepPID && !isTOFOnly && !selectionPIDpTdependent(trk2)) {
          continue;
        }
        if (isTOFOnly && !selectionPID2(trk2)) {
          continue;
        }

        if (trk1.index() == trk2.index()) {
          histos.fill(HIST("QA/Nsigma_TPC_selected"), trk1.pt(), getTpcNSigma(trk2));
          histos.fill(HIST("QA/Nsigma_TOF_selected"), trk1.pt(), getTofNSigma(trk2));
          histos.fill(HIST("QA/TPC_TOF_selected"), getTpcNSigma(trk2), getTofNSigma(trk2));
        }

        if (trk2.globalIndex() == trk1.globalIndex()) {
          continue;
        }
        if (!selectionPair(trk1, trk2)) {
          continue;
        }
        if (removefaketrack && isFakeKaon(trk1)) {
          continue;
        }
        if (removefaketrack && isFakeKaon(trk2)) {
          continue;
        }
        if (useGlobalTrack && trk2.p() < 1.0 && !(getITSResponse(trk2) > -2.5 && getITSResponse(trk2) < 2.5)) {
          continue;
        }

        pion1.SetXYZM(trk1.px(), trk1.py(), trk1.pz(), massPtl);
        pion2.SetXYZM(trk2.px(), trk2.py(), trk2.pz(), massPtl);
        reco = pion1 + pion2;

        if (reco.Rapidity() > cfgMaxRap || reco.Rapidity() < cfgMinRap) {
          continue;
        }

        relPhi = TVector2::Phi_0_2pi((reco.Phi() - eventPlaneDet) * static_cast<float>(nmode));

        if (trk1.sign() * trk2.sign() < 0) {
          histos.fill(HIST("hInvMass_f0980_US_EPA"), reco.M(), reco.Pt(), centrality, relPhi);
        } else if (trk1.sign() > 0 && trk2.sign() > 0) {
          histos.fill(HIST("hInvMass_f0980_LSpp_EPA"), reco.M(), reco.Pt(), centrality, relPhi);
        } else if (trk1.sign() < 0 && trk2.sign() < 0) {
          histos.fill(HIST("hInvMass_f0980_LSmm_EPA"), reco.M(), reco.Pt(), centrality, relPhi);
        }

        if (cfgRotBkg && trk1.sign() * trk2.sign() < 0) {
          for (int nr = 0; nr < cfgNRotBkg; nr++) {
            auto randomPhi = rn->Uniform(o2::constants::math::PI * 5.0 / 6.0, o2::constants::math::PI * 7.0 / 6.0);
            randomPhi += pion2.Phi();
            pion2Rot.SetXYZM(pion2.Pt() * std::cos(randomPhi), pion2.Pt() * std::sin(randomPhi), trk2.pz(), massPtl);
            recoRot = pion1 + pion2Rot;
            relPhiRot = TVector2::Phi_0_2pi((recoRot.Phi() - eventPlaneDet) * static_cast<float>(nmode));
            histos.fill(HIST("hInvMass_f0980_USRot_EPA"), recoRot.M(), recoRot.Pt(), centrality, relPhiRot);
          }
        }
      }
    }
  }

  void init(o2::framework::InitContext&)
  {
    AxisSpec epAxis = {6, 0.0, 2.0 * o2::constants::math::PI};
    AxisSpec qaCentAxis = {110, 0, 110};
    AxisSpec qaVzAxis = {100, -20, 20};
    AxisSpec qaPIDAxis = {100, -10, 10};
    AxisSpec qaPtAxis = {200, 0, 20};
    AxisSpec qaEpAxis = {100, -1.0 * o2::constants::math::PI, o2::constants::math::PI};
    AxisSpec epresAxis = {102, -1.02, 1.02};

    histos.add("QA/CentDist", "", {HistType::kTH1F, {qaCentAxis}});
    histos.add("QA/Vz", "", {HistType::kTH1F, {qaVzAxis}});

    histos.add("QA/Nsigma_TPC", "", {HistType::kTH2F, {qaPtAxis, qaPIDAxis}});
    histos.add("QA/Nsigma_TOF", "", {HistType::kTH2F, {qaPtAxis, qaPIDAxis}});
    histos.add("QA/TPC_TOF", "", {HistType::kTH2F, {qaPIDAxis, qaPIDAxis}});

    histos.add("QA/Nsigma_TPC_selected", "", {HistType::kTH2F, {qaPtAxis, qaPIDAxis}});
    histos.add("QA/Nsigma_TOF_selected", "", {HistType::kTH2F, {qaPtAxis, qaPIDAxis}});
    histos.add("QA/TPC_TOF_selected", "", {HistType::kTH2F, {qaPIDAxis, qaPIDAxis}});

    histos.add("QA/EPhist", "", {HistType::kTH2F, {qaCentAxis, qaEpAxis}});
    histos.add("QA/EPResAB", "", {HistType::kTH2F, {qaCentAxis, epresAxis}});
    histos.add("QA/EPResAC", "", {HistType::kTH2F, {qaCentAxis, epresAxis}});
    histos.add("QA/EPResBC", "", {HistType::kTH2F, {qaCentAxis, epresAxis}});

    histos.add("hInvMass_f0980_US_EPA", "unlike invariant mass",
               {HistType::kTHnSparseF, {massAxis, ptAxis, centAxis, epAxis}});
    histos.add("hInvMass_f0980_LSpp_EPA", "++ invariant mass",
               {HistType::kTHnSparseF, {massAxis, ptAxis, centAxis, epAxis}});
    histos.add("hInvMass_f0980_LSmm_EPA", "-- invariant mass",
               {HistType::kTHnSparseF, {massAxis, ptAxis, centAxis, epAxis}});
    histos.add("hInvMass_f0980_USRot_EPA", "unlike invariant mass Rotation",
               {HistType::kTHnSparseF, {massAxis, ptAxis, centAxis, epAxis}});
    //    if (doprocessMCLight) {
    //      histos.add("MCL/hpT_f0980_GEN", "generated f0 signals", HistType::kTH1F, {qaPtAxis});
    //      histos.add("MCL/hpT_f0980_REC", "reconstructed f0 signals", HistType::kTH3F, {massAxis, qaPtAxis, centAxis});
    //    }

    detId = getDetId(cfgQvecDetName);
    refAId = getDetId(cfgQvecRefAName);
    refBId = getDetId(cfgQvecRefBName);

    if (detId == refAId || detId == refBId || refAId == refBId) {
      LOGF(info, "Wrong detector configuration \n The FT0C will be used to get Q-Vector \n The TPCpos and TPCneg will be used as reference systems");
      detId = 0;
      refAId = 4;
      refBId = 5;
    }

    if (cfgSelectPtl == 0) {
      massPtl = o2::constants::physics::MassPionCharged;
    } else {
      massPtl = o2::constants::physics::MassKaonCharged;
    }

    fMultPVCutLow = new TF1("fMultPVCutLow", "[0]+[1]*x+[2]*x*x+[3]*x*x*x - 2.5*([4]+[5]*x+[6]*x*x+[7]*x*x*x+[8]*x*x*x*x)", 0, 100);
    fMultPVCutLow->SetParameters(2834.66, -87.0127, 0.915126, -0.00330136, 332.513, -12.3476, 0.251663, -0.00272819, 1.12242e-05);
    fMultPVCutHigh = new TF1("fMultPVCutHigh", "[0]+[1]*x+[2]*x*x+[3]*x*x*x + 2.5*([4]+[5]*x+[6]*x*x+[7]*x*x*x+[8]*x*x*x*x)", 0, 100);
    fMultPVCutHigh->SetParameters(2834.66, -87.0127, 0.915126, -0.00330136, 332.513, -12.3476, 0.251663, -0.00272819, 1.12242e-05);

    ccdb->setURL(cfgURL);
    ccdbApi.init("http://alice-ccdb.cern.ch");
    ccdb->setCaching(true);
    ccdb->setLocalObjectValidityChecking();
    ccdb->setCreatedNotAfter(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  }

  void processData(EventCandidates::iterator const& collision,
                   TrackCandidates const& tracks, aod::BCsWithTimestamps const&)
  {
    if (cfgCentEst == 1) {
      centrality = collision.centFT0C();
    } else if (cfgCentEst == 2) {
      centrality = collision.centFT0M();
    }
    if (!eventSelected(collision)) {
      return;
    }
    histos.fill(HIST("QA/CentDist"), centrality, 1.0);
    histos.fill(HIST("QA/Vz"), collision.posZ(), 1.0);

    fillHistograms<false>(collision, tracks, 2); // second order
  };
  PROCESS_SWITCH(f0980pbpbanalysis, processData, "Process Event for data", true);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<f0980pbpbanalysis>(cfgc, TaskName{"lf-f0980pbpbanalysis"})};
}
