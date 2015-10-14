#pragma once

#include "analysis/physics/Physics.h"

namespace ant {
namespace analysis {
namespace physics {


class MCSmearing : public Physics {
protected:
    TH3D* energies;
    TH3D* angles;
public:
    MCSmearing(PhysOptPtr opts=nullptr);
    virtual ~MCSmearing();

    virtual void ProcessEvent(const data::Event& event) override;
    virtual void Finish() override;
    virtual void ShowResult() override;
};

}}} // namespace ant::analysis::physics