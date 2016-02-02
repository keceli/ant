#include "catch.hpp"
#include "catch_config.h"
#include "expconfig_helpers.h"

#include "analysis/physics/PhysicsManager.h"
#include "analysis/input/ant/AntReader.h"
#include "analysis/input/pluto/PlutoReader.h"

#include "unpacker/Unpacker.h"
#include "reconstruct/Reconstruct.h"
#include "tree/TAntHeader.h"

#include "base/Logger.h"
#include "base/tmpfile_t.h"
#include "base/WrapTFile.h"

#include "TTree.h"


#include <iostream>
#include <list>

using namespace std;
using namespace ant;
using namespace ant::analysis;

void dotest_raw();
void dotest_raw_nowrite();
void dotest_plutogeant();


TEST_CASE("PhysicsManager: Raw Input", "[analysis]") {
    test::EnsureSetup();
    dotest_raw();
}

TEST_CASE("PhysicsManager: Raw Input without TEvent writing", "[analysis]") {
    test::EnsureSetup();
    dotest_raw_nowrite();
}

TEST_CASE("PhysicsManager: Pluto/Geant Input", "[analysis]") {
    test::EnsureSetup();
    dotest_plutogeant();
}

struct TestPhysics : Physics
{
    bool finishCalled = false;
    bool initCalled = false;
    bool showCalled = false;
    bool nowrite    = false;
    unsigned seenEvents = 0;
    unsigned seenCandidates = 0;
    unsigned seenMCTrue = 0;


    TestPhysics(bool nowrite_ = false) :
        Physics("TestPhysics", nullptr),
        nowrite(nowrite_)
    {
        HistFac.makeTH1D("test","test","test",BinSettings(10));
    }

    virtual void ProcessEvent(const TEvent& event, manager_t& manager) override
    {
        seenEvents++;
        seenCandidates += event.Reconstructed->Candidates.size();
        seenMCTrue += event.MCTrue->Particles.GetAll().size();

        // request to save every third event
        if(!nowrite && seenEvents % 3 == 0)
            manager.SaveEvent();
    }
    virtual void Finish() override
    {
        finishCalled = true;
    }
    virtual void ShowResult() override
    {
        showCalled = true;
    }
    virtual void Initialize(slowcontrol::SlowControl&) override
    {
        initCalled = true;
    }
};

struct PhysicsManagerTester : PhysicsManager
{
    using PhysicsManager::PhysicsManager;

    shared_ptr<TestPhysics> GetTestPhysicsModule() {
        // bit ugly to obtain the physics module back
        auto module = move(physics.back());
        physics.pop_back();
        return dynamic_pointer_cast<TestPhysics, Physics>(
                    std::shared_ptr<Physics>(module.release()));
    }
};

void dotest_raw()
{
    const unsigned expectedEvents = 221;

    tmpfile_t tmpfile;

    // write out some file
    {
        WrapTFileOutput outfile(tmpfile.filename, WrapTFileOutput::mode_t::recreate, true);
        PhysicsManagerTester pm;
        pm.AddPhysics<TestPhysics>();

        // make some meaningful input for the physics manager
        auto unpacker = Unpacker::Get(string(TEST_BLOBS_DIRECTORY)+"/Acqu_oneevent-big.dat.xz");
        auto reconstruct = std_ext::make_unique<Reconstruct>();
        list< unique_ptr<analysis::input::DataReader> > readers;
        readers.emplace_back(std_ext::make_unique<input::AntReader>(nullptr, move(unpacker), move(reconstruct)));
        pm.ReadFrom(move(readers), numeric_limits<long long>::max());
        TAntHeader header;
        pm.SetAntHeader(header);

        const std::uint32_t timestamp = 1408221194;
        REQUIRE(header.FirstID == TID(timestamp, 0u));
        REQUIRE(header.LastID == TID(timestamp, expectedEvents-1));

        std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

        REQUIRE(physics->finishCalled);
        REQUIRE_FALSE(physics->showCalled);
        REQUIRE(physics->initCalled);

        REQUIRE(physics->seenEvents == expectedEvents);
        REQUIRE(physics->seenCandidates == 822);

        // quick check if TTree was there...
        auto tree = outfile.GetSharedClone<TTree>("treeEvents");
        REQUIRE(tree != nullptr);
        REQUIRE(tree->GetEntries() == expectedEvents/3);
    }

    // read in file with AntReader
    {
        auto inputfiles = make_shared<WrapTFileInput>(tmpfile.filename);

        PhysicsManagerTester pm;
        pm.AddPhysics<TestPhysics>();

        // make some meaningful input for the physics manager

        auto reconstruct = std_ext::make_unique<Reconstruct>();
        list< unique_ptr<analysis::input::DataReader> > readers;

        readers.emplace_back(std_ext::make_unique<input::AntReader>(inputfiles, nullptr, move(reconstruct)));
        pm.ReadFrom(move(readers), numeric_limits<long long>::max());
        TAntHeader header;
        pm.SetAntHeader(header);

        // note that we actually requested every third event to be saved in the physics class
        const std::uint32_t timestamp = 1408221194;
        REQUIRE(header.FirstID == TID(timestamp, 2u));
        REQUIRE(header.LastID == TID(timestamp, 3*unsigned((expectedEvents-1)/3)-1));

        std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

        REQUIRE(physics->seenEvents == expectedEvents/3);
        // make sure the reconstruction wasn't applied twice!
        REQUIRE(physics->seenCandidates == 273);

    }

    // read in file with AntReader
    {
        auto inputfiles = make_shared<WrapTFileInput>(tmpfile.filename);

        PhysicsManagerTester pm;
        pm.AddPhysics<TestPhysics>();

        // make some meaningful input for the physics manager

        list< unique_ptr<analysis::input::DataReader> > readers;
        readers.emplace_back(std_ext::make_unique<input::AntReader>(inputfiles, nullptr, nullptr));
        pm.ReadFrom(move(readers), numeric_limits<long long>::max());
        TAntHeader header;
        pm.SetAntHeader(header);

        // note that we actually requested every third event to be saved in the physics class
        const std::uint32_t timestamp = 1408221194;
        REQUIRE(header.FirstID == TID(timestamp, 2u));
        REQUIRE(header.LastID == TID(timestamp, 3*unsigned((expectedEvents-1)/3)-1));

        std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

        REQUIRE(physics->seenEvents == expectedEvents/3);
        REQUIRE(physics->seenCandidates == 273);

    }

}

void dotest_raw_nowrite()
{
    tmpfile_t tmpfile;
    WrapTFileOutput outfile(tmpfile.filename, WrapTFileOutput::mode_t::recreate, true);

    PhysicsManagerTester pm;
    pm.AddPhysics<TestPhysics>(true);

    // make some meaningful input for the physics manager
    auto unpacker = Unpacker::Get(string(TEST_BLOBS_DIRECTORY)+"/Acqu_oneevent-big.dat.xz");
    auto reconstruct = std_ext::make_unique<Reconstruct>();
    list< unique_ptr<analysis::input::DataReader> > readers;
    readers.emplace_back(std_ext::make_unique<input::AntReader>(nullptr, move(unpacker), move(reconstruct)));
    pm.ReadFrom(move(readers), numeric_limits<long long>::max());
    TAntHeader header;
    pm.SetAntHeader(header);

    const std::uint32_t timestamp = 1408221194;
    const unsigned expectedEvents = 221;
    REQUIRE(header.FirstID == TID(timestamp, 0u));
    REQUIRE(header.LastID == TID(timestamp, expectedEvents-1));

    std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

    REQUIRE(physics->finishCalled);
    REQUIRE_FALSE(physics->showCalled);
    REQUIRE(physics->initCalled);


    REQUIRE(physics->seenEvents == expectedEvents);
    REQUIRE(physics->seenCandidates == 822);

    // the PhysicsManager should not create a TTree...
    REQUIRE(outfile.GetSharedClone<TTree>("treeEvents") == nullptr);
}

void dotest_plutogeant()
{
    tmpfile_t tmpfile;
    WrapTFileOutput outfile(tmpfile.filename, WrapTFileOutput::mode_t::recreate, true);

    PhysicsManagerTester pm;
    pm.AddPhysics<TestPhysics>();

    // make some meaningful input for the physics manager
    auto unpacker = Unpacker::Get(string(TEST_BLOBS_DIRECTORY)+"/Geant_with_TID.root");
    auto reconstruct = std_ext::make_unique<Reconstruct>();
    list< unique_ptr<analysis::input::DataReader> > readers;
    readers.emplace_back(std_ext::make_unique<input::AntReader>(nullptr, move(unpacker), move(reconstruct)));

    auto plutofile = std::make_shared<WrapTFileInput>(string(TEST_BLOBS_DIRECTORY)+"/Pluto_with_TID.root");
    readers.push_back(std_ext::make_unique<analysis::input::PlutoReader>(plutofile));

    pm.ReadFrom(move(readers), numeric_limits<long long>::max());

    std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

    REQUIRE(physics->seenEvents == 10);
    REQUIRE(physics->seenCandidates == 10);
    REQUIRE(physics->seenMCTrue == 10);

}
