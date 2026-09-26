#ifndef JAMOKE_DRINKS_H
#define JAMOKE_DRINKS_H

#include <string>
#include <vector>
#include <unordered_map>

// A drink as the customer orders it: matches the drinkmenu.txt model.
struct Drink {
  std::string id;
  std::string desc;
  std::string type;            // latte / mocha / cap
  int shots = 1;               // 1..3
  std::string bean = "regular";// regular / decaf / split
  std::string size = "short";  // short / tall / grande
  std::string milk = "whole";  // whole / nonfat
  std::string flavor = "none"; // none + 8 syrup flavors
  std::string difficulty = "med";
};

struct Customer {
  int sprite = 0;
  std::string gender = "male"; // male / female
  std::string drinkId;
  std::string shift = "early-morning";
};

struct DrinkDB {
  std::vector<Drink> drinks;
  std::vector<Customer> customers;
  // id -> Drink, built once after load() finishes; see load() for why it is
  // rebuilt after parsing rather than populated during it.
  std::unordered_map<std::string, const Drink*> byId;

  bool load(const std::string& dataDir);
  const Drink* find(const std::string& id) const;
};

struct RecipeOfBuild {
  std::string size;
  int shots;
  std::string bean;
  std::string milk;
  bool steamed;
  std::string flavor;
};

#endif