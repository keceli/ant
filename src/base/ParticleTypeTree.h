#pragma once

#include "ParticleType.h"
#include "Tree.h"

#include <map>

namespace ant {

using ParticleTypeTree = std::shared_ptr<Tree<const ParticleTypeDatabase::Type&>>;

class ParticleTypeTreeDatabase {
public:
    enum class Channel {
        Direct3Pi0_6g,
        Direct2Pi0_4g,
        Direct1Pi0_2g,
        Omega_gEta_3g,
        Omega_gPi0_3g,
        EtaPrime_2g,
        EtaPrime_3Pi0_6g,
        EtaPrime_2Pi0Eta_6g,
        EtaPrime_gOmega_ggPi0_4g
    };


    static ParticleTypeTree Get(Channel channel);

protected:
    using database_t = std::map<Channel, ParticleTypeTree>;
    static database_t database;

    static database_t CreateDatabase();

    static ParticleTypeTree GetBaseTree();

};

}