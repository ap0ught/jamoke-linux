#include "Drinks.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

static std::string trim(std::string s) {
  s.erase(0, s.find_first_not_of(" \t\r\n"));
  s.erase(s.find_last_not_of(" \t\r\n") + 1);
  return s;
}

static std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

// A sprite line names a customer image either as a plain number ("3") or as
// a LibV3 macro token ("ID_BMP_CUSTOMER_10"). Extract the numeric index either
// way; the macro name itself is not defined in our build.
static int spriteNum(const std::string& token) {
  const char* t = token.c_str();
  if (std::isdigit((unsigned char)*t)) return std::atoi(t);
  size_t p = token.find_last_of('_');
  if (p == std::string::npos) return 0;
  const char* d = token.c_str() + p + 1;
  if (!std::isdigit((unsigned char)*d)) return 0;
  return std::atoi(d);
}

// Parses a key=value line, trimming quotes off the value.
static void setKey(Drink& d, const std::string& key, std::string val) {
  val = trim(val);
  if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
    val = val.substr(1, val.size() - 2);

  // Normalize free-text values to canonical enum strings (case-insensitive),
  // so sloppy spellings in the DATA file still map onto the model's lists.
  auto oneOf = [&](const std::vector<std::string>& opts) -> std::string {
    for (auto& o : opts)
      if (lower(val) == o) return o;
    return "";
  };

  if (key == "id") d.id = val;
  else if (key == "desc") d.desc = val;
  else if (key == "type") d.type = oneOf({"latte", "mocha", "cap"});
  else if (key == "size") d.size = oneOf({"short", "tall", "grande"});
  else if (key == "milk") d.milk = oneOf({"whole", "nonfat"});
  else if (key == "difficulty") d.difficulty = oneOf({"easy", "med", "hard"});
  else if (key == "flavor") {
    std::string f = oneOf({"chocolate", "almond", "hazelnut", "vanilla",
                           "raspberry", "coconut", "orange", "mint"});
    if (!f.empty()) d.flavor = f;
  } else if (key == "shots") {
    // One token packs both dimensions, e.g. "single|regular". Substring
    // probes hash it without depending on order or exact separators; triple
    // wins over double, decaf wins over split/regular.
    std::string v = lower(val);
    if (v.find("triple") != std::string::npos) d.shots = 3;
    else if (v.find("double") != std::string::npos) d.shots = 2;
    else d.shots = 1;
    if (v.find("decaf") != std::string::npos) d.bean = "decaf";
    else if (v.find("split") != std::string::npos) d.bean = "split";
    else d.bean = "regular";
  }
}

bool DrinkDB::load(const std::string& dataDir) {
  // drinkmenu.txt is block-structured: a "drink:" line opens a new record
  // and the following "key=value" lines fill it until the next marker.
  std::ifstream f(dataDir + "/drinkmenu.txt");
  if (!f.is_open()) return false;
  std::vector<std::pair<std::string, std::string>> kv;
  Drink cur;
  bool inDrink = false;
  std::string line;
  while (std::getline(f, line)) {
    std::string t = line;
    // strip // comments (but not inside quoted strings) - crude but fine
    size_t c = t.find("//");
    if (c != std::string::npos) t = t.substr(0, c);
    t = trim(t);
    if (t.empty()) continue;
    if (t.rfind("drink:", 0) == 0) {
      if (inDrink && !cur.id.empty()) drinks.push_back(cur);
      cur = Drink();
      inDrink = true;
      continue;
    }
    if (!inDrink) continue;
    size_t eq = t.find('=');
    if (eq == std::string::npos) continue;
    setKey(cur, trim(t.substr(0, eq)), t.substr(eq + 1));
  }
  if (inDrink && !cur.id.empty()) drinks.push_back(cur);
  // Populate byId only after parsing: push_back can reallocate the vector,
  // which would invalidate any pointer into it captured earlier.
  byId.clear();
  for (size_t i = 0; i < drinks.size(); ++i) byId[drinks[i].id] = &drinks[i];

  // Same block format, for customers: "customer:" opens a record and the
  // sprite/gender/drink/shift lines populate it. Gender is lowercased so it
  // matches the head-art lookup in Game::newRound.
  std::ifstream cf(dataDir + "/CustomerList.txt");
  Customer cc;
  bool inCust = false;
  while (cf && std::getline(cf, line)) {
    std::string t = line;
    size_t c = t.find("//");
    if (c != std::string::npos) t = t.substr(0, c);
    t = trim(t);
    if (t.empty()) continue;
    if (t.rfind("customer:", 0) == 0) {
      if (inCust && !cc.drinkId.empty()) customers.push_back(cc);
      cc = Customer();
      inCust = true;
      continue;
    }
    if (!inCust) continue;
    size_t eq = t.find('=');
    if (eq == std::string::npos) continue;
    std::string k = trim(t.substr(0, eq));
    std::string v = t.substr(eq + 1);
    if (k == "sprite") cc.sprite = spriteNum(v);
    else if (k == "gender") cc.gender = lower(trim(v));
    else if (k == "drink") cc.drinkId = trim(v);
    else if (k == "shift") cc.shift = trim(v);
  }
  if (inCust && !cc.drinkId.empty()) customers.push_back(cc);
  return !drinks.empty();
}

const Drink* DrinkDB::find(const std::string& id) const {
  auto it = byId.find(id);
  return it == byId.end() ? nullptr : it->second;
}