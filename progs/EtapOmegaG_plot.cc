#include "base/Logger.h"

#include "analysis/plot/root_draw.h"
#include "analysis/physics/etaprime/etaprime_omega_gamma.h"

#include "base/CmdLine.h"
#include "base/interval.h"
#include "base/printable.h"
#include "base/WrapTFile.h"
#include "base/std_ext/string.h"
#include "base/std_ext/system.h"

#include "TH1D.h"
#include "TSystem.h"
#include "TRint.h"
#include "THStack.h"

#include <functional>
#include <string>
#include <list>

using namespace ant;
using namespace ant::analysis;
using namespace std;

volatile bool interrupt = false;

class MyTInterruptHandler : public TSignalHandler {
public:
    MyTInterruptHandler() : TSignalHandler(kSigInterrupt, kFALSE) { }

    Bool_t  Notify() {
        if (fDelay) {
            fDelay++;
            return kTRUE;
        }
        interrupt = true;
        cout << " >>> Interrupted! " << endl;
        return kTRUE;
    }
};


using CommonTree_t = physics::EtapOmegaG::TreeCommon;

template<typename SigRefTree_t>
struct Cut_t {
    using Passes_t = function<bool(const CommonTree_t&, const SigRefTree_t&)>;
    Cut_t(const std::string& name,
          Passes_t passes = [] (const CommonTree_t&, const SigRefTree_t&) { return true; }) : Name(name), Passes(passes) {}
    const std::string Name;
    const Passes_t Passes;
};


template<typename Hist_t, typename SigRefTree_t = typename Hist_t::Tree_t>
struct Node_t {
    SmartHistFactory HistFac; // directory for cuts (and H)
    SmartHistFactory H;       // subdir for mctrue splitted hists
    struct WrappedHist_t {
        using histmod_t = std::function<void(TH1*)>;
        Hist_t Hist;
        histmod_t Modify;
        WrappedHist_t(const SmartHistFactory& histFac, histmod_t mod) : Hist(histFac), Modify(mod) {}
    };

    map<unsigned, WrappedHist_t> Hists;
    typename Cut_t<SigRefTree_t>::Passes_t PassesCut;
    vector<ant::hstack> Stacks;

    Node_t(const SmartHistFactory& parentHistFac, Cut_t<SigRefTree_t> cut) :
        HistFac(cut.Name, parentHistFac, cut.Name),
        H("h", HistFac),
        Hists(MakeHists(H, HistFac.GetTitlePrefix())),
        PassesCut(cut.Passes)
    {
        for(const auto& it_hist : Hists) {
            AddToStack(it_hist.second);
        }
    }


    void AddToStack(const WrappedHist_t& hist) {
        const auto histptrs = hist.Hist.GetHists();
        for(TH1* h : histptrs) {
            h->SetLineWidth(2);
            hist.Modify(h);
        }
        if(Stacks.empty()) {
            static unsigned nStacksCreated = 0;
            for(auto h : histptrs) {
                const string name = h->GetName();
                Stacks.emplace_back(name+to_string(nStacksCreated), HistFac.GetTitlePrefix()+": "+name,
                                    true, // use intelliLegend
                                    true  // ignore empty histograms
                                    );
                Stacks.back() << h;
            }
            nStacksCreated++;
        }
        else {
            assert(histptrs.size() == Stacks.size());
            for(size_t i=0;i<Stacks.size();i++)
                Stacks[i] << histptrs[i];
        }
    }

    static map<unsigned, WrappedHist_t> MakeHists(SmartHistFactory& h, const std::string& prefix)
    {
        h.SetTitlePrefix(prefix);
        map<unsigned, WrappedHist_t> m;
        // 0=Data, 1=Sig, 2=Ref, 3=AllMC, 9=Unknown_Bkg, >=10 Known Bkg
        // create them here already to get them in the right order
        m.emplace(0, WrappedHist_t{SmartHistFactory("Data",   h, "Data"),   MakeColorHistMod(kBlack)});
        m.emplace(1, WrappedHist_t{SmartHistFactory("Sig",    h, "Sig"),    MakeColorHistMod(kRed)});
        m.emplace(2, WrappedHist_t{SmartHistFactory("Ref",    h, "Ref"),    MakeColorHistMod(kRed)});
        // mctrue is never >=3 (and <9) in tree, use this to sum up all MC and all bkg MC
        m.emplace(3, WrappedHist_t{SmartHistFactory("Sum_MC", h, "Sum_MC"), MakeColorHistMod(kBlack)});
        m.emplace(4, WrappedHist_t{SmartHistFactory("Bkg_MC", h, "Bkg_MC"), MakeColorHistMod(kGray)});

        return m;
    }

    static Color_t GetColor(unsigned i) {
        const std::vector<Color_t> colors = {kGreen+1, kBlue, kYellow+1, kMagenta, kCyan, kOrange, kSpring+10,};
        return colors[i % colors.size()];
    }

    static typename WrappedHist_t::histmod_t MakeColorHistMod(const Color_t color) {
        return [color] (TH1* h) { h->SetLineColor(color); };
    }


    bool Fill(const CommonTree_t& treeCommon, const SigRefTree_t& treeSigRef) {
       if(PassesCut(treeCommon, treeSigRef)) {

           const auto mctrue = treeCommon.MCTrue();
           auto it_hist = Hists.lower_bound(mctrue);
           if(it_hist == Hists.end() || it_hist->first != mctrue) {
               // mctrue should be 9 or higher here...
               const string& name = mctrue>=10 ?
                                        physics::EtapOmegaG::ptreeBackgrounds[mctrue-10].Name
                                    : "Other";
               it_hist = Hists.emplace_hint(it_hist, mctrue, WrappedHist_t{
                                                SmartHistFactory("Bkg_"+name, H, name),
                                                MakeColorHistMod(GetColor(mctrue-9))
                                            });

               AddToStack(it_hist->second);
           }

           it_hist->second.Hist.Fill(treeCommon, treeSigRef);
           // handle MC_all and MC_bkg
           if(mctrue>0) {
               Hists.at(3).Hist.Fill(treeCommon, treeSigRef);
               if(mctrue >= 9)
                   Hists.at(4).Hist.Fill(treeCommon, treeSigRef);
           }

           return true;
       }
       return false;
    }
};


template<typename SigRefTree_t>
using MultiCut_t = std::vector<Cut_t<SigRefTree_t>>;

template<typename SigRefTree_t>
using Cuts_t = std::list<MultiCut_t<SigRefTree_t>>;

template<typename SigRefTree_t>
using CutsIterator_t = typename Cuts_t<SigRefTree_t>::const_iterator;


template<typename SigRefTree_t, typename Hist_t>
using CutTree_t = typename Tree<Node_t<Hist_t, SigRefTree_t>>::node_t;

template<typename SigRefTree_t, typename Hist_t>
void BuildCutTree(CutTree_t<SigRefTree_t, Hist_t> cuttree, CutsIterator_t<SigRefTree_t> first, CutsIterator_t<SigRefTree_t> last) {
    if(first == last)
        return;
    const MultiCut_t<SigRefTree_t>& multicut = *first;
    for(const Cut_t<SigRefTree_t>& cut : multicut) {
        auto daughter = cuttree->CreateDaughter(cuttree->Get().HistFac, cut);
        BuildCutTree<SigRefTree_t, Hist_t>(daughter, std::next(first), last);
    }
}

template<typename Hist_t, typename SigRefTree_t = typename Hist_t::Tree_t>
CutTree_t<SigRefTree_t, Hist_t> MakeCutTree(SmartHistFactory histFac, const string& name) {
    const auto& cuts = Hist_t::GetCuts();
    auto cuttree = Tree<Node_t<Hist_t, SigRefTree_t>>::MakeNode(histFac, Cut_t<SigRefTree_t>{name});
    BuildCutTree<SigRefTree_t, Hist_t>(cuttree, cuts.begin(), cuts.end());
    return cuttree;
}

template<typename Hist_t, typename SigRefTree_t = typename Hist_t::Tree_t>
void FillCutTree(CutTree_t<SigRefTree_t, Hist_t> cuttree, const CommonTree_t& commonTree, const SigRefTree_t& tree) {
    if(cuttree->Get().Fill(commonTree, tree)) {
        for(const auto& d : cuttree->Daughters()) {
            FillCutTree<Hist_t, SigRefTree_t>(d, commonTree, tree);
        }
    }
}

template<typename Hist_t, typename SigRefTree_t = typename Hist_t::Tree_t>
void DrawCutTree(CutTree_t<SigRefTree_t, Hist_t> cuttree, bool leaves_only = true) {
    if(!leaves_only || cuttree->IsLeaf()) {
        canvas c(cuttree->Get().HistFac.GetTitlePrefix());
        for(auto& stack : cuttree->Get().Stacks)
            c << drawoption("nostack") << stack;
        c << endc;
    }
    for(const auto& d : cuttree->Daughters()) {
        DrawCutTree<Hist_t, SigRefTree_t>(d, leaves_only);
    }
}



// define the structs containing the histograms
// and the cuts. for simple branch variables, that could
// be combined...


struct CommonHist_t {
    TH1D* h_KinFitChi2;

    CommonHist_t(SmartHistFactory HistFac) {
        h_KinFitChi2 = HistFac.makeTH1D("KinFitChi2","#chi^{2}","",BinSettings(200,0,100),"h_KinFitChi2");
    }
    void Fill(const CommonTree_t& treeCommon) {
        h_KinFitChi2->Fill(treeCommon.KinFitChi2, treeCommon.TaggW);
    }
    std::vector<TH1*> GetHists() const {
        return {h_KinFitChi2};
    }

    // Sig and Ref channel share some cuts...
    template<typename Tree_t>
    static Cuts_t<Tree_t> GetCuts() {
        Cuts_t<Tree_t> cuts;
        cuts.emplace_back(MultiCut_t<Tree_t>{
                                 {"CBSumVeto=0", [] (const CommonTree_t& treeCommon, const Tree_t&) { return treeCommon.CBSumVetoE==0; } },
                                 {"CBSumVeto<0.25", [] (const CommonTree_t& treeCommon, const Tree_t&) { return treeCommon.CBSumVetoE<0.25; } },
                                 {"PIDSumE=0", [] (const CommonTree_t& treeCommon, const Tree_t&) { return treeCommon.PIDSumE==0; } },
                                 {"PIDSumE<0.25", [] (const CommonTree_t& treeCommon, const Tree_t&) { return treeCommon.PIDSumE<0.25; } },
                             });
        cuts.emplace_back(MultiCut_t<Tree_t>{
                                 {"KinFitChi2<10", [] (const CommonTree_t& treeCommon, const Tree_t&) { return treeCommon.KinFitChi2<10; } },
                                 {"KinFitChi2<20", [] (const CommonTree_t& treeCommon, const Tree_t&) { return treeCommon.KinFitChi2<20; } },
                             });
        return cuts;
    }


};

struct SigHist_t : CommonHist_t {
    using Tree_t = physics::EtapOmegaG::Sig_t::Tree_t;

    TH2D* h_IM_gg_gg;     // Goldhaber plot
    TH1D* h_TreeFitChi2;
    TH1D* h_Bachelor_E;

    SigHist_t(SmartHistFactory HistFac) : CommonHist_t(HistFac) {
        BinSettings bins_goldhaber(400, 0, 900);
        const string axislabel_goldhaber("2#gamma IM / MeV");

        h_IM_gg_gg = HistFac.makeTH2D("IM 2#gamma-2#gamma",
                                    axislabel_goldhaber, axislabel_goldhaber,
                                    bins_goldhaber, bins_goldhaber,
                                    "h_IM_gg_gg"
                                    );
        h_TreeFitChi2 = HistFac.makeTH1D("TreeFitChi2", "#chi^{2}","",BinSettings(200,0,100),"h_TreeFitChi2");
        h_Bachelor_E = HistFac.makeTH1D("E_#gamma in #eta' frame","E_{#gamma}","",BinSettings(400,0,400),"h_Bachelor_E");
    }

    void Fill(const CommonTree_t& treeCommon, const Tree_t& tree) {
        CommonHist_t::Fill(treeCommon);
        for(unsigned i=0;i<tree.gg_gg1().size();i++){
            h_IM_gg_gg->Fill(tree.gg_gg1()[i], tree.gg_gg2()[i]);
            h_IM_gg_gg->Fill(tree.gg_gg2()[i], tree.gg_gg1()[i]);
        }
        h_TreeFitChi2->Fill(tree.TreeFitChi2, treeCommon.TaggW);
        h_Bachelor_E->Fill(tree.Bachelor_best_best, treeCommon.TaggW);
    }
    std::vector<TH1*> GetHists() const {
        auto hists = CommonHist_t::GetHists();
        hists.insert(hists.end(), {h_IM_gg_gg, h_TreeFitChi2, h_Bachelor_E});
        return hists;
    }

    static Cuts_t<Tree_t> GetCuts() {
        auto cuts = CommonHist_t::GetCuts<Tree_t>();

        // reduces pi0pi0 and pi0eta backgrounds
        auto goldhaber_cut = [] (const CommonTree_t&, const Tree_t& tree) {
            const auto& Pi0 = ParticleTypeDatabase::Pi0.GetWindow(40);
            const auto& Eta = ParticleTypeDatabase::Eta.GetWindow(30);

            for(unsigned i=0;i<tree.gg_gg1().size();i++) {
                const double im1 = tree.gg_gg1()[i];
                const double im2 = tree.gg_gg2()[i];
                if(   im1 < Pi0.Stop()
                   && im2 < Pi0.Stop())
                    return false;
                if(   Eta.Contains(im1)
                   && Pi0.Contains(im2))
                    return false;
                if(   Pi0.Contains(im1)
                   && Eta.Contains(im2))
                    return false;
            }
            return true;
        };

        cuts.emplace_back(MultiCut_t<Tree_t>{
                              {"Goldhaber", goldhaber_cut },
                          });

        cuts.emplace_back(MultiCut_t<Tree_t>{
                              {"TreeFitChi2<20", [] (const CommonTree_t&, const Tree_t& tree) { return tree.TreeFitChi2<20; } },
                              {"TreeFitChi2<50", [] (const CommonTree_t&, const Tree_t& tree) { return tree.TreeFitChi2<50; } },
                          });


        return cuts;
    }
};

struct RefHist_t : CommonHist_t {
    using Tree_t = physics::EtapOmegaG::Ref_t::Tree_t;

    TH1D* h_IM_2g;

    RefHist_t(SmartHistFactory HistFac) : CommonHist_t(HistFac) {
        h_IM_2g = HistFac.makeTH1D("IM 2g","IM / MeV","",BinSettings(1100,0,1100),"h_IM_2g");
    }

    void Fill(const CommonTree_t& treeCommon, const Tree_t& tree) {
        CommonHist_t::Fill(treeCommon);
        h_IM_2g->Fill(tree.IM_2g, treeCommon.TaggW);
    }
    std::vector<TH1*> GetHists() const {
        auto hists = CommonHist_t::GetHists();
        hists.insert(hists.end(), {h_IM_2g});
        return hists;
    }

    static Cuts_t<Tree_t> GetCuts() {
        // reference does not have more to cut than common stuff...
        return CommonHist_t::GetCuts<Tree_t>();
    }
};

int main(int argc, char** argv) {
    SetupLogger();

    TCLAP::CmdLine cmd("plot", ' ', "0.1");
    auto cmd_input = cmd.add<TCLAP::ValueArg<string>>("i","input","Input file",true,"","input");
    auto cmd_batchmode = cmd.add<TCLAP::MultiSwitchArg>("b","batch","Run in batch mode (no ROOT shell afterwards)",false);
    auto cmd_maxevents = cmd.add<TCLAP::MultiArg<int>>("m","maxevents","Process only max events",false,"maxevents");
    auto cmd_output = cmd.add<TCLAP::ValueArg<string>>("o","output","Output file",false,"","filename");

    auto cmd_sigtree = cmd.add<TCLAP::ValueArg<string>>("","sigtree","Signal tree name",false,"Fitted/SigAll","treename");
    auto cmd_reftree = cmd.add<TCLAP::ValueArg<string>>("","reftree","Reference tree name",false,"Fitted/treeRef","treename");

    cmd.parse(argc, argv);

    int fake_argc=1;
    char* fake_argv[2];
    fake_argv[0] = argv[0];
    if(cmd_batchmode->isSet()) {
        fake_argv[fake_argc++] = strdup("-b");
    }
    TRint app("EtapOmegaG_plot",&fake_argc,fake_argv,nullptr,0,true);
    auto oldsig = app.GetSignalHandler();
    oldsig->Remove();
    auto mysig = new MyTInterruptHandler();
    mysig->Add();
    gSystem->AddSignalHandler(mysig);

    WrapTFileInput input(cmd_input->getValue());

    auto link_branches = [&input] (const string treename, WrapTTree* wraptree, long long expected_entries) {
        TTree* t;
        if(!input.GetObject(treename,t))
            throw runtime_error("Cannot find tree "+treename+" in input file");
        if(expected_entries>=0 && t->GetEntries() != expected_entries)
            throw runtime_error("Tree "+treename+" does not have entries=="+to_string(expected_entries));
        wraptree->LinkBranches(t);
    };


    CommonTree_t treeCommon;
    link_branches("EtapOmegaG/treeCommon", addressof(treeCommon), -1);
    auto entries = treeCommon.Tree->GetEntries();

    SigHist_t::Tree_t treeSig;
    link_branches("EtapOmegaG/"+cmd_sigtree->getValue(), addressof(treeSig), entries);
    RefHist_t::Tree_t treeRef;
    link_branches("EtapOmegaG/"+cmd_reftree->getValue(), addressof(treeRef), entries);


    unique_ptr<WrapTFileOutput> masterFile;
    if(cmd_output->isSet()) {
        masterFile = std_ext::make_unique<WrapTFileOutput>(cmd_output->getValue(),
                                                    WrapTFileOutput::mode_t::recreate,
                                                     true); // cd into masterFile upon creation
    }


    SmartHistFactory HistFac("EtapOmegaG");

    auto cuttreeSig = MakeCutTree<SigHist_t>(HistFac, std_ext::replace_str(cmd_sigtree->getValue(),"/","_"));
    auto cuttreeRef = MakeCutTree<RefHist_t>(HistFac, std_ext::replace_str(cmd_reftree->getValue(),"/","_"));

    LOG(INFO) << "Tree entries=" << entries;
    auto max_entries = entries;
    if(cmd_maxevents->isSet() && cmd_maxevents->getValue().back()<entries) {
        max_entries = cmd_maxevents->getValue().back();
        LOG(INFO) << "Running until " << max_entries;
    }



    for(long long entry=0;entry<max_entries;entry++) {
        if(interrupt)
            break;

        treeCommon.Tree->GetEntry(entry);

        // we handle the Ref/Sig cut here to save some reading work
        if(treeCommon.IsSignal) {
            treeSig.Tree->GetEntry(entry);
            FillCutTree<SigHist_t>(cuttreeSig, treeCommon, treeSig);
        }
        else {
            treeRef.Tree->GetEntry(entry);
            FillCutTree<RefHist_t>(cuttreeRef, treeCommon, treeRef);
        }
        if(entry % 100000 == 0)
            LOG(INFO) << "Processed " << 100.0*entry/entries << " %";
    }

    if(!cmd_batchmode->isSet()) {
        if(!std_ext::system::isInteractive()) {
            LOG(INFO) << "No TTY attached. Not starting ROOT shell.";
        }
        else {

            mysig->Remove();
            oldsig->Add();
            gSystem->AddSignalHandler(oldsig);
            delete mysig;

            if(masterFile)
                LOG(INFO) << "Stopped running, but close ROOT properly to write data to disk.";

            DrawCutTree<SigHist_t>(cuttreeSig);
            DrawCutTree<RefHist_t>(cuttreeRef);

            app.Run(kTRUE); // really important to return...
            if(masterFile)
                LOG(INFO) << "Writing output file...";
            masterFile = nullptr;   // and to destroy the master WrapTFile before TRint is destroyed
        }
    }


    return 0;
}