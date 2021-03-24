#pragma once

#include "Component.h"
#include <pugixml.hpp>
#include <filesystem>
#include <string>
#include <vector>

struct Component;

class Combinations
{
public:
    Combinations() = default;

    bool load(const std::filesystem::path & resource);

    std::string classify(const std::vector<Component> & components, std::vector<int> & order) const;

private:
    struct Leg {
        InstrumentType type;
        double ratio;
        int strike;
        int strike_offset;
        int expiration;
        int expiration_offset;
    };

    struct Combination {
        std::string name;
        int cardinality;
        int mincount;
        std::vector <Leg> legs;
    };

    pugi::xml_document doc;

    std::vector <Combination> combinations;
};

