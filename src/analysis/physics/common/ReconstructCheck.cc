#include "physics/common/ReconstructCheck.h"
#include "plot/root_draw.h"
#include "data/Particle.h"
#include "base/Detector_t.h"
#include "TH1D.h"
#include "TLorentzVector.h"
#include <cmath>
#include <iostream>
#include "base/Logger.h"
#include "base/cbtaps_display/TH2TAPS.h"
#include "expconfig/ExpConfig.h"
#include "base/std_ext/math.h"

using namespace std;
using namespace ant;
using namespace ant::analysis;
using namespace ant::analysis::physics;
using namespace ant::analysis::data;


ReconstructCheck::ReconstructCheck(PhysOptPtr opts):
    Physics("ReconstructCheck",opts),
    cb_group(HistFac, "CB", histgroup::detectortype::CB),
    taps_group(HistFac,"TAPS",histgroup::detectortype::TAPS),
    all_group(HistFac,"All",histgroup::detectortype::All),
    tapsveto(HistFac)
{
    const BinSettings e(max(1000.0, atof(GetOption("Emax").c_str())));
    EnergyRec_cb = HistFac.makeTH2D("Energry Reconstruction CB","E_{true} [MeV]","E_{rec} [MeV]", e, e, "Energy_rec_cb");
    EnergyRec_taps = HistFac.makeTH2D("Energry Reconstruction TAPS","E_{true} [MeV]","E_{rec} [MeV]", e, e, "Energy_rec_taps");
}

Detector_t::Any_t GetCommonDetector(const CandidateList& cands) {
    Detector_t::Any_t common_detetctor = Detector_t::Any_t::None;
    for(const auto& cand : cands) {
        common_detetctor |= cand->Detector();
    }
    return common_detetctor;
}

void ReconstructCheck::ProcessEvent(const Event &event)
{
    if(event.MCTrue().Particles().GetAll().size() == 1) {

        const auto& mctrue_particle = event.MCTrue().Particles().GetAll().at(0);

        const auto common_detector = GetCommonDetector(event.Reconstructed().Candidates());

        if(common_detector & Detector_t::Any_t::CB) {
            cb_group.Fill(mctrue_particle, event.Reconstructed().Candidates(), event.Reconstructed().InsaneClusters());
        }

        if(common_detector & Detector_t::Any_t::TAPS) {
            taps_group.Fill(mctrue_particle, event.Reconstructed().Candidates(), event.Reconstructed().InsaneClusters());
        }

        all_group.Fill(mctrue_particle, event.Reconstructed().Candidates(), event.Reconstructed().InsaneClusters());

        tapsveto.Fill(event.Reconstructed().Candidates(), event.Reconstructed().InsaneClusters());

    }
}


void ReconstructCheck::Finish()
{
    cb_group.Finish();
    taps_group.Finish();
    all_group.Finish();
    tapsveto.Finish();
    LOG(INFO) << "ReconstructCheck Finish";
}

void ReconstructCheck::ShowResult()
{
    cb_group.ShowResult();
    taps_group.ShowResult();
    all_group.ShowResult();
    tapsveto.ShowResult();
}

void LabelBins(TAxis* x) {

    for(Int_t i=1; i<=x->GetNbins(); ++i) {
        const auto a = x->GetBinLowEdge(i);
        x->SetBinLabel(i,to_string(int(a)).c_str());
    }
}

std::unique_ptr<ReconstructCheck::PositionMap> ReconstructCheck::histgroup::makePosMap(SmartHistFactory& f, ReconstructCheck::histgroup::detectortype d, const string& name, const string title)
{
    std::unique_ptr<PositionMap> ptr;
    switch (d) {
    case detectortype::All:
    case detectortype::CB:
        ptr = std_ext::make_unique<PositionMapCB>(f,name,title);
        break;
    case detectortype::TAPS:
        ptr = std_ext::make_unique<PositionMapTAPS>(f,name,title);
    }

    return move(ptr);
}

ReconstructCheck::histgroup::histgroup(SmartHistFactory& f, const string& prefix, detectortype d): Prefix(prefix)
{
    const BinSettings energy(1000);
    const BinSettings vetoEnergy(100,0,25);
    const BinSettings clusersize(19,1,20);
    const BinSettings costheta(360,-1,1);
    const BinSettings phi(360,-180,180);

    nPerEvent     = f.makeTH1D(prefix+" Candidates/Event", "Candidates/Event","",BinSettings(10),prefix+"candEvent");
    LabelBins(nPerEvent->GetXaxis());
    nPerEvent->SetFillColor(kGray);

    nPerEventPerE = f.makeTH2D(prefix+" Candidates/Event/Energy","MC True Energy [MeV]","Candidates/Event",BinSettings(1000),BinSettings(10),prefix+"candEventEnergy");
    LabelBins(nPerEventPerE->GetYaxis());

    splitPerEvent = f.makeTH1D(prefix+" Split-flagged Clusters/Event", "# split clusters/Event","",BinSettings(20),prefix+"splits");
    LabelBins(splitPerEvent->GetXaxis());
    splitPerEvent->SetFillColor(kGray);

    splitPos = makePosMap(f,d, prefix+"_splitpos",prefix+" Pos of split-flagged clusters");
    posCharged = makePosMap(f,d, prefix+"_chargedpos",prefix+" Pos of charged cands");

    multiplicity_map      = f.makeTH3D(prefix+" Multitplicity Map","cos(#theta_{True})","#phi_{True}","Mult",costheta,phi,BinSettings(10),prefix+"mults");
    multiplicity_map->SetStats(false);

    mult2_split_angles.resize(3);
    for(size_t i=0;i<mult2_split_angles.size();++i) {
        mult2_split_angles[i] = f.makeTH1D(prefix+"Mult==2 cluster angle "+to_string(i),"#alpha [#circ]","",BinSettings(180,0,90),prefix+"mult2_"+to_string(i));
    }

    cluserSize = f.makeTH2D(prefix+" Cluster Size TAPS","E_{True} [MeV]","Elements",energy, clusersize,prefix+"_clustersize");
    LabelBins(cluserSize->GetYaxis());

    dEE = f.makeTH2D(prefix+" dEE TAPS", "E [MeV]","VetoEnergy [MeV]",energy, vetoEnergy,prefix+"_dEE");
    dEE_true = f.makeTH2D(prefix+" dEE TAPS (true E)", "E_{True} [MeV]","VetoEnergy [MeV]",energy, vetoEnergy,prefix+"_dEE_true");

    nCharged        = f.makeTH1D(prefix+" N Charged (VetoEnergy > 0) CB", "# charged candidates", "", BinSettings(10),prefix+"_ncharged");
    LabelBins(nCharged->GetXaxis());
    nCharged->SetFillColor(kGray);

    unmatched_veto = f.makeTH1D(prefix+" Unmatched Veto Clusters","# unmatched veto clusters","",BinSettings(6),prefix+"_unmatched_veto");
    LabelBins(unmatched_veto->GetXaxis());
    unmatched_veto->SetFillColor(kGray);

    edge_flag_pos = f.makeTH2D(prefix+" Edge flagged Clusters","cos(#theta_{True})","#phi_{True}", costheta,phi,prefix+"_edge_flag");

    veto_cand_phi_diff = f.makeTH1D(prefix+" Angle unmatched Veto - Cand","# unmatched veto clusters","",BinSettings(6),prefix+"_veto_cand_phi_diff");

    energy_recov  = makePosMap(f,d,prefix+"_Erecov",prefix+" Energy Recovery Average");
    energy_recov->maphist->SetStats(false);

    energy_recov_norm  = makePosMap(f,d,prefix+"_Erecov_norm",prefix+" Energy Recovery Average Norm");
    energy_recov_norm->maphist->SetStats(false);
}

void ReconstructCheck::histgroup::ShowResult() const
{
    hstack splitstack("");
    for(const auto& h : mult2_split_angles) { splitstack << h; }

    canvas c(Prefix);

    c << drawoption("colz") << nPerEvent << nPerEventPerE << splitPerEvent
      << *splitPos << edge_flag_pos << multiplicity_map
      << cluserSize << dEE << dEE_true << nCharged << *posCharged << unmatched_veto
      << drawoption("nostack") << padoption::set(padoption_t::Legend) << splitstack
      << padoption::unset(padoption_t::Legend)
      << drawoption("colz") << *energy_recov
      << endc;
}

void Norm(TH1* hist) {
    hist->Scale(1.0/hist->GetEntries());
}

void ReconstructCheck::histgroup::Finish()
{
    Norm(nPerEvent);
    Norm(splitPerEvent);
    Norm(nCharged);
    Norm(unmatched_veto);
    energy_recov->maphist->Divide(energy_recov_norm->maphist);
}

double angle(const data::Candidate& c1, const data::Candidate& c2) {
    TVector3 v1(1,0,0);
    v1.SetTheta(c1.Theta());
    v1.SetPhi(c1.Phi());

    TVector3 v2(1,0,0);
    v2.SetTheta(c2.Theta());
    v2.SetPhi(c2.Phi());

    return v1.Angle(v2);
}

std::list<CandidatePtr> CandidatesByDetector(const Detector_t::Any_t& detector, const CandidateList& candidates) {
    std::list<CandidatePtr> cands;
    for(const auto& c : candidates) {
        if(c->Detector() & detector) {
            cands.emplace_back(c);
        }
    }
    return cands;
}


void ReconstructCheck::histgroup::Fill(const ParticlePtr& mctrue, const CandidateList& cand, const ClusterList& insane)
{
    const auto mc_phi = mctrue->Phi()*TMath::RadToDeg();
    const auto mc_theta = mctrue->Theta();
    const auto mc_cos_theta = cos(mc_theta);
    const auto mc_energy = mctrue->Ek();


    nPerEvent->Fill(cand.size());
    nPerEventPerE->Fill(mctrue->Ek(), cand.size());

    unsigned nsplit(0);
    unsigned n(0);
    unsigned ncharged(0);

    for(const CandidatePtr& c : cand) {
        if(c->VetoEnergy() > 0.0) {
            posCharged->Fill(mc_theta,mc_phi);
        }

            cluserSize->Fill(mc_energy, c->ClusterSize());
            dEE->Fill(c->ClusterEnergy(), c->VetoEnergy());
            dEE_true->Fill(mc_energy, c->VetoEnergy());

            if(c->VetoEnergy() > 0.0)
                ncharged++;
            ++n;

        for(const Cluster& cl : c->Clusters) {
            if(cl.flags.isChecked(Cluster::Flag::Split)) {
                ++nsplit;
                splitPos->Fill(mc_theta, mc_phi);

            }

            if(cl.flags.isChecked(Cluster::Flag::TouchesHole)) {
                edge_flag_pos->Fill(mc_cos_theta, mc_phi);
            }
        }
    }

    unsigned nunmatched_veto(0);
    for(const Cluster& ic : insane) {
        if(ic.Detector & Detector_t::Any_t::Veto) {
            nunmatched_veto++;
        }
    }
    unmatched_veto->Fill(nunmatched_veto);

    if(n>0)
        nCharged->Fill(ncharged);

    splitPerEvent->Fill(nsplit);
    multiplicity_map->Fill(mc_cos_theta, mc_phi, cand.size());

    if(cand.size() == 2) {
        if(nsplit<3) {
            mult2_split_angles[nsplit]->Fill(angle(*cand.at(0), *cand.at(1))*TMath::RadToDeg());
        }
    }

    if(cand.size()==1) {
        const auto& c = cand.at(0);
        const auto rec = c->ClusterEnergy() / mc_energy;
        energy_recov->Fill(mc_theta, mc_phi, rec);
        energy_recov_norm->Fill(mc_theta,mc_phi);
    }

}



void ReconstructCheck::PositionMapCB::Fill(const double theta, const double phi, const double v)
{
    maphist->Fill(cos(theta),phi,v);
}

void ReconstructCheck::PositionMap::Draw(const string&) const
{
    maphist->Draw("colz");
}

ReconstructCheck::PositionMapCB::PositionMapCB(SmartHistFactory &f, const string &name, const string &title)
{
    const BinSettings costheta(360,-1,1);
    const BinSettings phi(360,-180,180);
    maphist = f.makeTH2D(title,"cos(#theta_{True})","#phi [#circ]",costheta,phi,name);
}

ReconstructCheck::PositionMapTAPS::PositionMapTAPS(SmartHistFactory &f, const string &name, const string& title)
{
    const auto l   = 70.0; //cm
    const auto res =   .5; //cm

    const BinSettings bins(2*l/res,-l,l);

    maphist = f.makeTH2D(title,"x [cm]","y [cm]",bins, bins, name);
}

void ReconstructCheck::PositionMapTAPS::Fill(const double theta, const double phi, const double v)
{
    constexpr const auto tapsdist = 145.0; //cm
    constexpr const auto max_theta = std_ext::degree_to_radian(25.0);

    if(theta < max_theta) {
        const auto r = tan(theta) * tapsdist; //cm
        const auto x = r * cos(phi);
        const auto y = r * sin(phi);
        maphist->Fill(x,y,v);
    }
}

void ReconstructCheck::PositionMapTAPS::Draw(const string&) const
{
    maphist->Draw("colz");
    auto taps = new TH2TAPS();
    taps->Draw("same text");
}



ReconstructCheck::TAPSVetoMatch::TAPSVetoMatch(SmartHistFactory& f)
{
    vetoElement_dist = f.makeTH2D("Veto TAPS Cluster dist","Veto Element","Dist [cm]",BinSettings(348),BinSettings(100,0,20),"tapsveto_cluster_dist");
}

void ReconstructCheck::TAPSVetoMatch::ShowResult()
{
    canvas c("TAPS Veto");
    c << vetoElement_dist << endc;
}

void ReconstructCheck::TAPSVetoMatch::Fill(const CandidateList& cands, const ClusterList& instane)
{
    using namespace ant::std_ext;

    auto clusterLoop = [this] (const Cluster* vCluster, const CandidateList& cands) {
        if(vCluster && vCluster->Detector & Detector_t::Type_t::TAPSVeto) {
            for(const CandidatePtr& cCand : cands) {
                const auto cCluster = cCand->FindCaloCluster();
                if(cCluster && cCluster->Detector & Detector_t::Type_t::TAPS) {
                    const auto dx = vCluster->pos.X() - cCluster->pos.X();
                    const auto dy = vCluster->pos.Y() - cCluster->pos.Y();
                    const auto d  = sqrt(sqr(dx)+sqr(dy));
                    this->vetoElement_dist->Fill(vCluster->CentralElement, d);
                }
            }
        }
    };


    for(const CandidatePtr& vCand : cands) {
        const auto vCluster = vCand->FindVetoCluster();
        clusterLoop(vCluster, cands);
    }

    for(const Cluster& iCluster : instane) {
        clusterLoop(addressof(iCluster), cands);
    }

}

AUTO_REGISTER_PHYSICS(ReconstructCheck, "ReconstructCheck")
