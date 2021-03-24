#include "Combinations.h"
#include "Component.h"
#include <iostream>
#include <pugixml.hpp>
#include <vector>
#include <ctime>
#include <algorithm>
#include <climits>

#define OK std::cout << "okay" << std::endl;
#define ERR std::cout << "err" << std::endl;

constexpr int FIXED = 0;
constexpr int MULTIPLE = 1;
constexpr int MORE = 2;
constexpr int MINUS = INT_MIN;
constexpr int PLUS = INT_MAX;
constexpr int UNDEF = -1;

namespace {
    std::tm NULLTIME = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    bool first = true;

    bool NextSet(std::vector<int> &a, int n) {
        if (first) {
            first = false;
            return true;
        }
        int j = n - 2;
        while (j != -1 && a[j] >= a[j + 1]) j--;
        if (j == -1)
            return false;
        int k = n - 1;
        while (a[j] >= a[k]) k--;
        std::swap(a[j], a[k]);
        int l = j + 1, r = n - 1;
        while (l < r)
            std::swap(a[l++], a[r--]);
        return true;
    }

    bool equals_component(Component comp1, Component comp2) {
        return ((comp1.type == comp2.type) &&
                (comp1.ratio == comp2.ratio) &&
                (comp1.strike == comp2.strike) &&
                (mktime(&comp1.expiration) == mktime(&comp2.expiration)));
    }
}

bool Combinations::load(const std::filesystem::path & resource) {
    pugi::xml_parse_result result = doc.load_file(resource.c_str());

    if (!result) {
        return false;
    }

    pugi::xml_node tools = doc.child("combinations");

    for (pugi::xml_node comb: tools.children("combination")) {
        std::string name = comb.attribute("name").value();
        int cardinality, mincount;
        std::vector <Leg> legs;
        std::string str_cardinality = comb.child("legs").attribute("cardinality").value();
        if (str_cardinality == "fixed") {
            cardinality = FIXED;
        }
        if (str_cardinality == "multiple") {
            cardinality = MULTIPLE;
        }
        if (str_cardinality == "more") {
            cardinality = MORE;
            mincount = std::stoi(comb.child("legs").attribute("mincount").value());
        }
        for (pugi::xml_node leg : comb.child("legs").children("leg")) {
            InstrumentType type;
            double ratio;
            int strike, strike_offset = 0, expiration, expiration_offset = 0;
            std::string str_ratio, str_strike_offset, str_expiration_offset, str_strike, str_expiration;
            // type
            type = static_cast<InstrumentType>(leg.attribute("type").value()[0]);
            // ratio
            str_ratio = leg.attribute("ratio").value();
            if (str_ratio == "+") {
                ratio = PLUS;
            } else if (str_ratio == "-") {
                ratio = MINUS;
            } else {
                ratio = std::stoi(str_ratio);
            }
            // strike
            str_strike = leg.attribute("strike").value();
            strike = UNDEF;
            if (!str_strike.empty()) {
                strike = str_strike[0] - 'A';
            }
            // strike_offset
            str_strike_offset = leg.attribute("strike_offset").value();
            for (char c : str_strike_offset) {
                c == '+' ? strike_offset++ : strike_offset--;
            }
            // expiration
            str_expiration = leg.attribute("expiration").value();
            expiration = UNDEF;
            if (!str_expiration.empty()) {
                expiration = str_expiration[0] - 'A';
            }
            // expiration_offset
            str_expiration_offset = leg.attribute("expiration_offset").value();
            if (!str_expiration_offset.empty()) {
                if ((str_expiration_offset[0] != '+') && (str_expiration_offset[0] != '-')) {
                    expiration_offset = (str_expiration_offset[0] - '0') * 1000;
                } else {
                    for (char c : str_expiration_offset) {
                        c == '+' ? expiration_offset++ : expiration_offset--;
                    }
                }
            }
            Leg comp {type, ratio, strike, strike_offset, expiration, expiration_offset};
            legs.push_back(comp);
        }
        Combination combination {name, cardinality, mincount, legs};
        combinations.push_back(combination);
    }
    return true;
}

std::string Combinations::classify(const std::vector<Component> & components, std::vector<int> & order) const
{
    if (components.empty()) {
        return "Unclassified";
    }

    int num = components.size();

    order.resize(num);

    for (int i = 0; i < combinations.size(); i++) {
        bool mul = false;
        std::vector <Component> new_components;
        std::vector <Component> in_order;
        int new_num;
        std::vector <int> new_order;

        order.resize(num);

        for (int j = 0; j < num; j++) {
            order[j] = j + 1;
        }

        Combination comb = combinations[i];

        if (comb.cardinality == FIXED) {
            new_num = num;
            new_components = components;
            if (comb.legs.size() != num) {
                first = true;
                continue;
            }
        } else if (comb.cardinality == MULTIPLE) {
            mul = true;
            // uniq
            for (int j = 0; j < num; j++) {
                Component checked = components[j];
                bool uniq = true;
                for (int k = 0; k < new_components.size(); k++) {
                    if (equals_component(checked, new_components[k])) {
                        uniq = false;
                    }
                }
                if (uniq) {
                    new_components.push_back(checked);
                }
            }
            new_num = new_components.size();
            if (new_num != comb.legs.size()) {
                first = true;
                continue;
            }
        } else if (comb.cardinality == MORE) {
            if (comb.mincount > num) {
                first = true;
                continue;
            }
            bool okay = true;
            Leg leg = comb.legs[0];
            for (int k = 0; k < num; k++) {
                // type
                if ((leg.type != components[k].type) &&
                    !((leg.type == InstrumentType::U) &&
                      (components[k].type == InstrumentType::U || components[k].type == InstrumentType::F)) &&
                    !((leg.type == InstrumentType::O) &&
                      (components[k].type == InstrumentType::O || components[k].type == InstrumentType::P || components[k].type == InstrumentType::C))) {
                    okay = false;
                }
                // ratio
                if (leg.ratio == PLUS) {
                    if (components[k].ratio < 0) {
                        okay = false;
                    }
                } else if (leg.ratio == MINUS) {
                    if (components[k].ratio > 0) {
                        okay = false;
                    }
                } else if (leg.ratio != components[k].ratio) {
                    okay = false;
                }
            }
            if (okay) {
                return comb.name;
            } else {
                first = true;
                continue;
            }
        }
        // init
        new_order.resize(new_num);
        in_order.resize(new_num);
        for (int j = 0; j < new_num; j++) {
            new_order[j] = j + 1;
            in_order[j] = new_components[j];
        }
        // go
        while (NextSet(new_order, new_num)) {
            for (int j = 0; j < new_num; j++) {
                in_order[j] = new_components[new_order[j] - 1];
            }

            bool okay = true;
            //type and ratio
            if (okay) {
                for (int j = 0; j < comb.legs.size(); j++) {
                    // type
                    if ((comb.legs[j].type != in_order[j].type) &&
                        !((comb.legs[j].type == InstrumentType::U) &&
                          (in_order[j].type == InstrumentType::U || in_order[j].type == InstrumentType::F))) {
                        okay = false;
                    }

                    // ratio
                    if (comb.legs[j].ratio == PLUS) {
                        if (in_order[j].ratio < 0) {
                            okay = false;
                        }
                    } else if (comb.legs[j].ratio == MINUS) {
                        if (in_order[j].ratio > 0) {
                            okay = false;
                        }
                    } else if (comb.legs[j].ratio != in_order[j].ratio) {
                        okay = false;
                    }
                }
            }
            // strike
            if (okay) {
                std::vector <int> strikes (26, UNDEF);
                for (int j = 0; j < comb.legs.size(); j++) {
                    int type_strike = comb.legs[j].strike;
                    if (type_strike != UNDEF) {
                        if (strikes[type_strike] == UNDEF) {
                            strikes[type_strike] = in_order[j].strike;
                        } else if (strikes[type_strike] != in_order[j].strike) {
                            okay = false;
                        }
                    }
                }
            }
            //strike_offset
            if (okay) {

                std::vector <int> pluses;
                std::vector <int> minuses;
                for (int j = 0; j < comb.legs.size(); j++) {
                    if (comb.legs[j].strike_offset == 0) {
                        pluses.clear();
                        minuses.clear();
                        pluses.push_back(in_order[j].strike);
                        minuses.push_back(in_order[j].strike);
                    } else if (comb.legs[j].strike_offset > 0) {
                        int idx = comb.legs[j].strike_offset;
                        if (pluses.size() - 1 < idx) {   //UNDEF
                            pluses.push_back(in_order[j].strike);
                        } else if (pluses[idx] != in_order[j].strike) {
                            okay = false;
                        }
                        if (pluses[idx] <= pluses[idx - 1]) {
                            okay = false;
                        }
                    } else if (comb.legs[j].strike_offset < 0) {
                        int idx = comb.legs[j].strike_offset * (-1);
                        if (minuses.size() - 1 < idx) {   //UNDEF
                            minuses.push_back(in_order[j].strike);
                        } else if (minuses[idx] != in_order[j].strike) {
                            okay = false;
                        }
                        if (minuses[idx] >= pluses[idx - 1]) {
                            okay = false;
                        }
                    }
                }
            }
            //expiration
            if (okay) {
                std::vector <std::tm> expirations (26, NULLTIME);
                for (int j = 0; j < comb.legs.size(); j++) {
                    int type_expiration = comb.legs[j].expiration;
                    if (type_expiration != UNDEF) {
                        std::tm exp = in_order[j].expiration;
                        if (mktime(&expirations[type_expiration]) == mktime(&NULLTIME)) {
                            expirations[type_expiration] = exp;
                        } else if (mktime(&expirations[type_expiration]) != mktime(&exp)) {
                            okay = false;
                        }
                    }
                }
            }
            //expiration_offset
            if (okay) {
                std::vector <std::tm> pluses;
                std::vector <std::tm> minuses;
                for (int j = 0; j < comb.legs.size(); j++) {
                    std::tm exp = in_order[j].expiration;
                    if (abs(comb.legs[j].expiration_offset) < 1000) {
                        if (comb.legs[j].expiration_offset == 0) {
                            pluses.clear();
                            minuses.clear();
                            pluses.push_back(in_order[j].expiration);
                            minuses.push_back(in_order[j].expiration);
                        } else if (comb.legs[j].expiration_offset > 0) {
                            int idx = comb.legs[j].expiration_offset;
                            if (pluses.size() - 1 < idx) {   //UNDEF
                                pluses.push_back(in_order[j].expiration);
                            } else if (mktime(& pluses[idx]) != mktime(& in_order[j].expiration)) {
                                okay = false;
                            }
                            if (mktime(& pluses[idx]) <= mktime(& pluses[idx - 1])) {
                                okay = false;
                            }
                        } else if (comb.legs[j].expiration_offset < 0) {
                            int idx = comb.legs[j].expiration_offset * (-1);
                            if (minuses.size() - 1 < idx) {   //UNDEF
                                minuses.push_back(in_order[j].expiration);
                            } else if (mktime(& minuses[idx]) != mktime(& in_order[j].expiration)) {
                                okay = false;
                            }
                            if (mktime(& minuses[idx]) >= mktime(& pluses[idx - 1])) {
                                okay = false;
                            }
                        }
                    } else {    // Не нашел ничего, кроме nq
                        int r = 3;
                        r *= (comb.legs[j].expiration_offset / 1000);
                        int last_month = pluses[0].tm_mon;
                        int last_year = pluses[0].tm_year;
                        int month = exp.tm_mon;
                        int year = exp.tm_year;
                        if (last_month > month) {
                            month += 12;
                            year--;
                        }
                        if (month - last_month != r || year != last_year) {
                            okay = false;
                        }
                    }
                }
            }
            if (okay) {
                if (mul) {
                    std::vector <int> add (new_order.size(), 0);
                    for (int j = 0; j < components.size(); j++) {
                        for (int k = 0; k < in_order.size(); k++) {
                            if (equals_component(components[j], in_order[k])) {
                                order[j] = k + add[k] + 1;
                                add[k] += 4;
                            }
                        }
                    }
                } else {
                    order = new_order;
                }
                return comb.name;
            }
        }
        first = true;
    }
    order.clear();
    return "Unclassified";
}
