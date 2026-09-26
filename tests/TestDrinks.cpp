#include "TestHelper.h"

TEST(TestDrinks, LoadNonExistentDirectory) {
  DrinkDB db;
  EXPECT_FALSE(db.load("/non_existent_dir_jamoke_test_12345"));
  EXPECT_TRUE(db.drinks.empty());
  EXPECT_TRUE(db.customers.empty());
  EXPECT_TRUE(db.byId.empty());
  EXPECT_EQ(db.find("anything"), nullptr);
}

TEST(TestDrinks, LoadEmptyDirectory) {
  TempDirGuard tempDir;
  DrinkDB db;
  EXPECT_FALSE(db.load(tempDir.path.string()));
  EXPECT_TRUE(db.drinks.empty());
}

TEST(TestDrinks, ParseSingleDrinkAllFields) {
  TempDirGuard tempDir;
  std::string drinkMenu =
      "// Sample Drink Menu\n"
      "drink:\n"
      "  id=L01\n"
      "  desc=\"Caramel Latte\"\n"
      "  type=Latte\n"
      "  shots=\"double regular\"\n"
      "  size=Grande\n"
      "  milk=Nonfat\n"
      "  flavor=Vanilla\n"
      "  difficulty=Hard\n";
  tempDir.writeFile("drinkmenu.txt", drinkMenu);

  DrinkDB db;
  EXPECT_TRUE(db.load(tempDir.path.string()));
  ASSERT_EQ(db.drinks.size(), 1u);

  const Drink& d = db.drinks[0];
  EXPECT_EQ(d.id, "L01");
  EXPECT_EQ(d.desc, "Caramel Latte");
  EXPECT_EQ(d.type, "latte");
  EXPECT_EQ(d.shots, 2);
  EXPECT_EQ(d.bean, "regular");
  EXPECT_EQ(d.size, "grande");
  EXPECT_EQ(d.milk, "nonfat");
  EXPECT_EQ(d.flavor, "vanilla");
  EXPECT_EQ(d.difficulty, "hard");

  const Drink* found = db.find("L01");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->desc, "Caramel Latte");
}

TEST(TestDrinks, ParseShotsAndBeansLogic) {
  TempDirGuard tempDir;
  std::string drinkMenu =
      "drink:\n"
      "  id=D1\n"
      "  shots=single\n"
      "drink:\n"
      "  id=D2\n"
      "  shots=double\n"
      "drink:\n"
      "  id=D3\n"
      "  shots=triple\n"
      "drink:\n"
      "  id=D4\n"
      "  shots=\"triple double\"\n" // triple takes precedence
      "drink:\n"
      "  id=D5\n"
      "  shots=decaf\n"
      "drink:\n"
      "  id=D6\n"
      "  shots=split\n"
      "drink:\n"
      "  id=D7\n"
      "  shots=regular\n"
      "drink:\n"
      "  id=D8\n"
      "  shots=\"decaf split\"\n" // decaf takes precedence
      "drink:\n"
      "  id=D9\n"
      "  shots=\"triple|decaf\"\n"
      "drink:\n"
      "  id=D10\n"
      "  shots=\"double split\"\n";
  tempDir.writeFile("drinkmenu.txt", drinkMenu);

  DrinkDB db;
  EXPECT_TRUE(db.load(tempDir.path.string()));
  ASSERT_EQ(db.drinks.size(), 10u);

  EXPECT_EQ(db.find("D1")->shots, 1);
  EXPECT_EQ(db.find("D1")->bean, "regular");

  EXPECT_EQ(db.find("D2")->shots, 2);
  EXPECT_EQ(db.find("D2")->bean, "regular");

  EXPECT_EQ(db.find("D3")->shots, 3);
  EXPECT_EQ(db.find("D3")->bean, "regular");

  EXPECT_EQ(db.find("D4")->shots, 3);

  EXPECT_EQ(db.find("D5")->bean, "decaf");
  EXPECT_EQ(db.find("D6")->bean, "split");
  EXPECT_EQ(db.find("D7")->bean, "regular");
  EXPECT_EQ(db.find("D8")->bean, "decaf");

  EXPECT_EQ(db.find("D9")->shots, 3);
  EXPECT_EQ(db.find("D9")->bean, "decaf");

  EXPECT_EQ(db.find("D10")->shots, 2);
  EXPECT_EQ(db.find("D10")->bean, "split");
}

TEST(TestDrinks, ParseDrinkTypesAndFlavors) {
  TempDirGuard tempDir;
  std::string drinkMenu =
      "drink:\n"
      "  id=T1\n"
      "  type=latte\n"
      "  flavor=chocolate\n"
      "  size=short\n"
      "  milk=whole\n"
      "  difficulty=easy\n"
      "drink:\n"
      "  id=T2\n"
      "  type=MOCHA\n"
      "  flavor=ALMOND\n"
      "  size=TALL\n"
      "  milk=NONFAT\n"
      "  difficulty=MED\n"
      "drink:\n"
      "  id=T3\n"
      "  type=Cap\n"
      "  flavor=hazelnut\n"
      "  size=grande\n"
      "  difficulty=HARD\n"
      "drink:\n"
      "  id=T4\n"
      "  flavor=raspberry\n"
      "drink:\n"
      "  id=T5\n"
      "  flavor=coconut\n"
      "drink:\n"
      "  id=T6\n"
      "  flavor=orange\n"
      "drink:\n"
      "  id=T7\n"
      "  flavor=mint\n"
      "drink:\n"
      "  id=T8\n"
      "  flavor=invalid_flavor\n" // invalid should remain default "none"
      "  type=invalid_type\n"     // invalid type should remain empty
      "  size=extra_large\n"      // invalid size should remain empty
      "  milk=oat\n"              // invalid milk should remain empty
      "  difficulty=expert\n";    // invalid difficulty should remain empty
  tempDir.writeFile("drinkmenu.txt", drinkMenu);

  DrinkDB db;
  EXPECT_TRUE(db.load(tempDir.path.string()));
  ASSERT_EQ(db.drinks.size(), 8u);

  EXPECT_EQ(db.find("T1")->type, "latte");
  EXPECT_EQ(db.find("T1")->flavor, "chocolate");
  EXPECT_EQ(db.find("T1")->size, "short");
  EXPECT_EQ(db.find("T1")->milk, "whole");
  EXPECT_EQ(db.find("T1")->difficulty, "easy");

  EXPECT_EQ(db.find("T2")->type, "mocha");
  EXPECT_EQ(db.find("T2")->flavor, "almond");
  EXPECT_EQ(db.find("T2")->size, "tall");
  EXPECT_EQ(db.find("T2")->milk, "nonfat");
  EXPECT_EQ(db.find("T2")->difficulty, "med");

  EXPECT_EQ(db.find("T3")->type, "cap");
  EXPECT_EQ(db.find("T3")->flavor, "hazelnut");
  EXPECT_EQ(db.find("T3")->size, "grande");
  EXPECT_EQ(db.find("T3")->difficulty, "hard");

  EXPECT_EQ(db.find("T4")->flavor, "raspberry");
  EXPECT_EQ(db.find("T5")->flavor, "coconut");
  EXPECT_EQ(db.find("T6")->flavor, "orange");
  EXPECT_EQ(db.find("T7")->flavor, "mint");

  EXPECT_EQ(db.find("T8")->flavor, "none");
  EXPECT_EQ(db.find("T8")->type, "");
  EXPECT_EQ(db.find("T8")->size, "");
  EXPECT_EQ(db.find("T8")->milk, "");
  EXPECT_EQ(db.find("T8")->difficulty, "");
}

TEST(TestDrinks, ParseCustomerList) {
  TempDirGuard tempDir;
  std::string drinkMenu =
      "drink:\n"
      "  id=D001\n"
      "  desc=\"Drink 1\"\n";
  std::string customerList =
      "// Customers list\n"
      "customer:\n"
      "  sprite=3\n"
      "  gender=FEMALE\n"
      "  drink=D001\n"
      "  shift=morning\n"
      "\n"
      "customer:\n"
      "  sprite=7\n"
      "  gender=Male\n"
      "  drink=D002\n"
      "  shift=afternoon\n";

  tempDir.writeFile("drinkmenu.txt", drinkMenu);
  tempDir.writeFile("CustomerList.txt", customerList);

  DrinkDB db;
  EXPECT_TRUE(db.load(tempDir.path.string()));
  ASSERT_EQ(db.customers.size(), 2u);

  EXPECT_EQ(db.customers[0].sprite, 3);
  EXPECT_EQ(db.customers[0].gender, "female");
  EXPECT_EQ(db.customers[0].drinkId, "D001");
  EXPECT_EQ(db.customers[0].shift, "morning");

  EXPECT_EQ(db.customers[1].sprite, 7);
  EXPECT_EQ(db.customers[1].gender, "male");
  EXPECT_EQ(db.customers[1].drinkId, "D002");
  EXPECT_EQ(db.customers[1].shift, "afternoon");
}

TEST(TestDrinks, ParseCustomerSpriteTokens) {
  TempDirGuard tempDir;
  tempDir.writeFile("drinkmenu.txt",
                    "drink:\n  id=D001\n  desc=\"Drink 1\"\n");
  tempDir.writeFile(
      "CustomerList.txt",
      "customer:\n  sprite=ID_BMP_CUSTOMER_10\n  gender=male\n  drink=D001\n"
      "customer:\n  sprite=ID_BMP_CUSTOMER_0\n  gender=female\n  drink=D001\n"
      "customer:\n  sprite=4\n  gender=male\n  drink=D001\n"
      "customer:\n  sprite=NOSUFFIX\n  gender=female\n  drink=D001\n");

  DrinkDB db;
  EXPECT_TRUE(db.load(tempDir.path.string()));
  ASSERT_EQ(db.customers.size(), 4u);

  EXPECT_EQ(db.customers[0].sprite, 10);
  EXPECT_EQ(db.customers[1].sprite, 0);
  EXPECT_EQ(db.customers[2].sprite, 4);
  EXPECT_EQ(db.customers[3].sprite, 0);  // no numeric suffix -> default 0
}

TEST(TestDrinks, FindMethodAndPointerValidity) {
  TempDirGuard tempDir;
  std::string drinkMenu;
  for (int i = 0; i < 50; ++i) {
    drinkMenu += "drink:\n  id=ID_" + std::to_string(i) + "\n  desc=\"Drink " + std::to_string(i) + "\"\n";
  }
  tempDir.writeFile("drinkmenu.txt", drinkMenu);

  DrinkDB db;
  EXPECT_TRUE(db.load(tempDir.path.string()));
  EXPECT_EQ(db.drinks.size(), 50u);

  for (int i = 0; i < 50; ++i) {
    std::string id = "ID_" + std::to_string(i);
    const Drink* d = db.find(id);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->id, id);
    EXPECT_EQ(d->desc, "Drink " + std::to_string(i));
  }
  EXPECT_EQ(db.find("ID_999"), nullptr);
}

TEST(TestDrinks, RealAssetDatabaseIntegrity) {
  std::string root = getAssetRoot();
  if (!fs::is_directory(root)) {
    GTEST_SKIP() << "Asset root not available at: " << root;
  }

  DrinkDB db;
  ASSERT_TRUE(db.load(root));
  EXPECT_EQ(db.drinks.size(), 179u);
  EXPECT_EQ(db.customers.size(), 11u);

  // The data file lists customers in sprite order (ID_BMP_CUSTOMER_0..10);
  // parsing those macro tokens must recover the index so Game::newRound can
  // pick faces by the customer's own number.
  for (size_t i = 0; i < db.customers.size(); ++i) {
    EXPECT_EQ(db.customers[i].sprite, (int)i)
        << "customer sprite index at data position " << i;
  }

  for (const auto& c : db.customers) {
    EXPECT_FALSE(c.drinkId.empty());
    const Drink* d = db.find(c.drinkId);
    EXPECT_NE(d, nullptr) << "Customer drink " << c.drinkId << " not found in DB";
    EXPECT_TRUE(c.gender == "male" || c.gender == "female");
  }
}
