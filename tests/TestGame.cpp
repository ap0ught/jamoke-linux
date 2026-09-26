#include "TestHelper.h"

#include <set>

class GameTestFixture : public HeadlessSDLTest {
 protected:
  TempDirGuard tempDir;
  std::unique_ptr<Assets> assets;
  DrinkDB db;

  void SetUp() override {
    HeadlessSDLTest::SetUp();
    assets = std::make_unique<Assets>(renderer);

    std::string drinkMenu =
        "drink:\n"
        "  id=D_LATTE\n"
        "  desc=\"Latte\"\n"
        "  type=latte\n"
        "  shots=double\n"
        "  size=tall\n"
        "  milk=nonfat\n"
        "  flavor=hazelnut\n"
        "  difficulty=easy\n"
        "drink:\n"
        "  id=D_MOCHA\n"
        "  desc=\"Mocha\"\n"
        "  type=mocha\n"
        "  shots=triple\n"
        "  size=grande\n"
        "  milk=whole\n"
        "  flavor=chocolate\n"
        "  difficulty=hard\n"
        "drink:\n"
        "  id=D_CAP\n"
        "  desc=\"Cappuccino\"\n"
        "  type=cap\n"
        "  shots=single\n"
        "  size=short\n"
        "  milk=whole\n"
        "  flavor=none\n"
        "  difficulty=med\n";

    std::string customerList =
        "customer:\n"
        "  sprite=1\n"
        "  gender=female\n"
        "  drink=D_LATTE\n"
        "  shift=morning\n"
        "customer:\n"
        "  sprite=2\n"
        "  gender=male\n"
        "  drink=D_MOCHA\n"
        "  shift=morning\n"
        "customer:\n"
        "  sprite=3\n"
        "  gender=female\n"
        "  drink=D_CAP\n"
        "  shift=afternoon\n";

    tempDir.writeFile("drinkmenu.txt", drinkMenu);
    tempDir.writeFile("CustomerList.txt", customerList);

    ASSERT_TRUE(db.load(tempDir.path.string()));
  }
};

TEST_F(GameTestFixture, InitAndResetState) {
  Game game(*assets, db);

  // Boots on the title menu (like the original) so no order timer is
  // running before the player chooses to start.
  EXPECT_EQ(game.scene, Game::Scene::Title);
  EXPECT_FALSE(game.paused);

  game.startRun();
  EXPECT_EQ(game.scene, Game::Scene::Play);
  EXPECT_EQ(game.score, 0);
  EXPECT_EQ(game.served, 0);
  EXPECT_EQ(game.idx, 0u);
  EXPECT_EQ(game.resolveT, 0.0);

  // Initial build state
  EXPECT_EQ(game.size, "short");
  EXPECT_EQ(game.shots, 1);
  EXPECT_EQ(game.bean, "regular");
  EXPECT_EQ(game.milk, "whole");
  EXPECT_FALSE(game.steamed);
  EXPECT_EQ(game.flavor, "none");

  // Roster should have 10 customer slots
  EXPECT_EQ(game.roster.size(), 10u);
  ASSERT_NE(game.target, nullptr);
  EXPECT_EQ(game.target->id, "D_LATTE");

  // Easy difficulty sets timeTotal and timeLeft to 55.0
  EXPECT_DOUBLE_EQ(game.timeTotal, 55.0);
  EXPECT_DOUBLE_EQ(game.timeLeft, 55.0);
}

TEST_F(GameTestFixture, TargetDifficultyTimeLimits) {
  Game game(*assets, db);
  game.startRun();

  // Customer 0 is D_LATTE (easy) -> 55.0
  EXPECT_DOUBLE_EQ(game.timeLeft, 55.0);

  // Advance to Customer 1: D_MOCHA (hard)
  game.idx = 1;
  game.newRound();
  EXPECT_EQ(game.target->id, "D_MOCHA");
  EXPECT_DOUBLE_EQ(game.timeTotal, 42.0);
  EXPECT_DOUBLE_EQ(game.timeLeft, 42.0);

  // Advance to Customer 2: D_CAP (med)
  game.idx = 2;
  game.newRound();
  EXPECT_EQ(game.target->id, "D_CAP");
  EXPECT_DOUBLE_EQ(game.timeTotal, 50.0);
  EXPECT_DOUBLE_EQ(game.timeLeft, 50.0);
}

TEST_F(GameTestFixture, BeanSelectionCycling) {
  Game game(*assets, db);
  game.startRun();

  EXPECT_EQ(game.bean, "regular");

  game.selectBean("regular");
  EXPECT_EQ(game.bean, "regular");

  // Clicking decaf cycles: regular -> decaf -> split -> regular -> decaf
  game.selectBean("decaf");
  EXPECT_EQ(game.bean, "decaf");

  game.selectBean("decaf");
  EXPECT_EQ(game.bean, "split");

  game.selectBean("decaf");
  EXPECT_EQ(game.bean, "regular");

  game.selectBean("decaf");
  EXPECT_EQ(game.bean, "decaf");

  // Direct selection of split
  game.selectBean("split");
  EXPECT_EQ(game.bean, "split");
}

TEST_F(GameTestFixture, MilkAndSizeSelection) {
  Game game(*assets, db);
  game.startRun();

  // Milk
  game.selectMilk("nonfat");
  EXPECT_EQ(game.milk, "nonfat");
  game.selectMilk("whole");
  EXPECT_EQ(game.milk, "whole");

  // Size
  game.selectSize("tall");
  EXPECT_EQ(game.size, "tall");
  game.selectSize("grande");
  EXPECT_EQ(game.size, "grande");
  game.selectSize("short");
  EXPECT_EQ(game.size, "short");
}

TEST_F(GameTestFixture, FlavorSelectionToggle) {
  Game game(*assets, db);
  game.startRun();

  EXPECT_EQ(game.flavor, "none");

  game.selectFlavor("vanilla");
  EXPECT_EQ(game.flavor, "vanilla");

  // Tapping the same active flavor bottle resets to "none"
  game.selectFlavor("vanilla");
  EXPECT_EQ(game.flavor, "none");

  // Selecting a different flavor
  game.selectFlavor("hazelnut");
  EXPECT_EQ(game.flavor, "hazelnut");
  game.selectFlavor("chocolate");
  EXPECT_EQ(game.flavor, "chocolate");
}

TEST_F(GameTestFixture, ServeValidationLatte) {
  Game game(*assets, db);
  game.startRun();

  // Target is D_LATTE: tall, double, regular, nonfat, hazelnut, latte (wants steam)
  game.selectSize("tall");
  game.shots = 2;
  game.selectBean("regular");
  game.selectMilk("nonfat");
  game.selectFlavor("hazelnut");
  game.steamed = true;

  game.timeLeft = 25.0; // > 10.0 gives +25 bonus tip
  game.serve();

  EXPECT_TRUE(game.resolveResult);
  EXPECT_DOUBLE_EQ(game.resolveT, 1.6);
  EXPECT_EQ(game.score, 125);
  EXPECT_NE(game.resolveMsg.find("+125"), std::string::npos);
  EXPECT_NE(game.resolveMsg.find("CORRECT!"), std::string::npos);
}

TEST_F(GameTestFixture, ServeValidationLatteRequiresSteam) {
  Game game(*assets, db);
  game.startRun();

  // Target is D_LATTE
  game.selectSize("tall");
  game.shots = 2;
  game.selectBean("regular");
  game.selectMilk("nonfat");
  game.selectFlavor("hazelnut");
  game.steamed = false; // missing steam for latte

  game.timeLeft = 25.0;
  game.serve();

  EXPECT_FALSE(game.resolveResult);
  EXPECT_DOUBLE_EQ(game.resolveT, 1.6);
  EXPECT_EQ(game.score, 0);
  EXPECT_EQ(game.resolveMsg, "WRONG - NO TIP");
}

TEST_F(GameTestFixture, ServeValidationMochaDoesNotWantSteam) {
  Game game(*assets, db);
  game.startRun();

  // Switch to Mocha target
  game.idx = 1;
  game.newRound();
  EXPECT_EQ(game.target->id, "D_MOCHA");

  // D_MOCHA: grande, triple, regular, whole, chocolate, mocha (no steam)
  game.selectSize("grande");
  game.shots = 3;
  game.selectBean("regular");
  game.selectMilk("whole");
  game.selectFlavor("chocolate");

  // If steamed is erroneously true
  game.steamed = true;
  game.serve();
  EXPECT_FALSE(game.resolveResult);

  // Reset resolve timer to test correct build
  game.resolveT = 0.0;
  game.steamed = false;
  game.timeLeft = 5.0; // <= 10.0, so no bonus tip (100 base)
  game.serve();

  EXPECT_TRUE(game.resolveResult);
  EXPECT_EQ(game.score, 100);
  EXPECT_NE(game.resolveMsg.find("+100"), std::string::npos);
}

TEST_F(GameTestFixture, ServeMismatchesAttributeTests) {
  Game game(*assets, db);
  game.startRun();
  // Target: tall, double, regular, nonfat, hazelnut, steamed=true

  auto setupCorrect = [&]() {
    game.resolveT = 0.0;
    game.score = 0;
    game.selectSize("tall");
    game.shots = 2;
    game.selectBean("regular");
    game.selectMilk("nonfat");
    game.selectFlavor("hazelnut");
    game.steamed = true;
  };

  // Wrong size
  setupCorrect();
  game.selectSize("short");
  game.serve();
  EXPECT_FALSE(game.resolveResult);

  // Wrong shots
  setupCorrect();
  game.shots = 1;
  game.serve();
  EXPECT_FALSE(game.resolveResult);

  // Wrong bean
  setupCorrect();
  game.selectBean("decaf");
  game.serve();
  EXPECT_FALSE(game.resolveResult);

  // Wrong milk
  setupCorrect();
  game.selectMilk("whole");
  game.serve();
  EXPECT_FALSE(game.resolveResult);

  // Wrong flavor
  setupCorrect();
  game.selectFlavor("vanilla");
  game.serve();
  EXPECT_FALSE(game.resolveResult);
}

TEST_F(GameTestFixture, ServeIgnoredWhileResolving) {
  Game game(*assets, db);
  game.startRun();
  game.resolveT = 1.0;
  game.resolveResult = true;
  game.score = 125;

  // Attempting to serve during resolve should be ignored
  game.serve();
  EXPECT_TRUE(game.resolveResult);
  EXPECT_EQ(game.score, 125);
  EXPECT_DOUBLE_EQ(game.resolveT, 1.0);
}

TEST_F(GameTestFixture, UpdateTimerAndTimeout) {
  Game game(*assets, db);
  game.startRun();
  EXPECT_DOUBLE_EQ(game.timeLeft, 55.0);

  // Small update decreases timeLeft
  game.update(5.0);
  EXPECT_DOUBLE_EQ(game.timeLeft, 50.0);

  // Update exceeding remaining time triggers timeout
  game.update(60.0);
  EXPECT_DOUBLE_EQ(game.timeLeft, 0.0);
  EXPECT_FALSE(game.resolveResult);
  EXPECT_EQ(game.resolveMsg, "TOO SLOW");
  EXPECT_DOUBLE_EQ(game.resolveT, 1.6);
}

TEST_F(GameTestFixture, UpdateResolveFeedbackProgression) {
  Game game(*assets, db);
  game.startRun();
  game.resolveT = 1.6;
  game.idx = 0;
  game.served = 0;

  game.update(1.0);
  EXPECT_DOUBLE_EQ(game.resolveT, 0.6);
  EXPECT_EQ(game.idx, 0u);
  EXPECT_EQ(game.served, 0);

  // Remaining dt completes resolve timer and triggers newRound
  game.update(0.7);
  EXPECT_DOUBLE_EQ(game.resolveT, 0.0);
  EXPECT_EQ(game.served, 1);
  EXPECT_EQ(game.idx, 1u);
  EXPECT_EQ(game.target->id, "D_MOCHA");
}

TEST_F(GameTestFixture, FullRosterProgressionToGameOver) {
  Game game(*assets, db);
  game.startRun();

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(game.scene, Game::Scene::Play);
    EXPECT_EQ(game.idx, (size_t)i);
    game.serve();
    game.update(2.0); // resolveT finishes and advances round
  }

  EXPECT_EQ(game.idx, 10u);
  EXPECT_EQ(game.served, 10);
  EXPECT_EQ(game.scene, Game::Scene::Over);
}

TEST_F(GameTestFixture, EscPausesInPlaceAndPreservesTheRun) {
  Game game(*assets, db);
  game.startRun();
  EXPECT_EQ(game.scene, Game::Scene::Play);

  game.score = 250;
  game.idx = 4;
  game.timeLeft = 30.0;

  // Esc opens the menu overlay; the run is untouched.
  game.togglePause();
  EXPECT_TRUE(game.paused);
  EXPECT_EQ(game.scene, Game::Scene::Play);
  EXPECT_EQ(game.score, 250);
  EXPECT_EQ(game.idx, 4u);

  // A paused game must not drain the customer timer or advance the round.
  game.update(5.0);
  EXPECT_DOUBLE_EQ(game.timeLeft, 30.0);
  EXPECT_EQ(game.idx, 4u);

  // Clicks are swallowed while the overlay is up (no free build/serve).
  game.selectBean("decaf");
  game.handleClick(100, 160);
  EXPECT_EQ(game.bean, "decaf");

  // Esc again resumes in place.
  game.togglePause();
  EXPECT_FALSE(game.paused);
  EXPECT_EQ(game.score, 250);
  game.update(5.0);
  EXPECT_DOUBLE_EQ(game.timeLeft, 25.0);
}

TEST_F(GameTestFixture, QuittingToTitleIsConfirmedAndDestructive) {
  Game game(*assets, db);
  game.startRun();
  game.score = 250;
  game.idx = 4;
  game.togglePause();
  ASSERT_TRUE(game.paused);

  // First Q only arms the confirmation.
  game.requestQuitToTitle();
  EXPECT_TRUE(game.quitPending);
  EXPECT_EQ(game.scene, Game::Scene::Play);
  EXPECT_EQ(game.score, 250);

  // Second Q abandons the run for the title menu.
  game.requestQuitToTitle();
  EXPECT_EQ(game.scene, Game::Scene::Title);
  EXPECT_FALSE(game.paused);
  EXPECT_EQ(game.idx, 0u);
  EXPECT_EQ(game.score, 0);

  // The session record must survive an abandoned run: bank a 900 first, then
  // quit a later run to the title and confirm the record is still there.
  game.startRun();
  for (int i = 0; i < 9; ++i) {
    game.serve();
    game.update(2.0);
  }
  game.score = 900;
  game.serve();
  game.update(2.0);
  ASSERT_EQ(game.scene, Game::Scene::Over);
  ASSERT_EQ(game.bestScore, 900);

  game.startRun();
  game.togglePause();
  game.requestQuitToTitle();
  game.requestQuitToTitle();
  EXPECT_EQ(game.scene, Game::Scene::Title);
  EXPECT_EQ(game.bestScore, 900);
}

TEST_F(GameTestFixture, ClickingTheTitleStartsARun) {
  Game game(*assets, db);
  ASSERT_EQ(game.scene, Game::Scene::Title);

  // The title screen advertises clicking the order board; that must work.
  game.handleClick(650, 200);
  EXPECT_EQ(game.scene, Game::Scene::Play);
  EXPECT_EQ(game.idx, 0u);
  EXPECT_EQ(game.score, 0);
  ASSERT_NE(game.target, nullptr);
}

TEST_F(GameTestFixture, ToTitleReturnsToMenu) {
  Game game(*assets, db);
  game.startRun();
  EXPECT_EQ(game.scene, Game::Scene::Play);

  game.score = 50;
  game.idx = 4;
  game.toTitle();

  // Abandoning a run lands on the title menu with the round state cleared so
  // the next start is a fresh run.
  EXPECT_EQ(game.scene, Game::Scene::Title);
  EXPECT_EQ(game.idx, 0u);
  EXPECT_EQ(game.score, 0);
}

TEST_F(GameTestFixture, BestScoreTracksRecordAndClearScores) {
  Game game(*assets, db);
  game.startRun();

  // F9 toggles the fps readout flag.
  EXPECT_FALSE(game.showFps);
  game.toggleFps();
  EXPECT_TRUE(game.showFps);
  game.toggleFps();
  EXPECT_FALSE(game.showFps);

  // Nine rounds of "served" customers (wrong build, score 0), then a score
  // bump before the 10th round ends the run on Scene::Over.
  for (int i = 0; i < 9; ++i) {
    game.serve();
    game.update(2.0);
  }
  game.score = 555;
  game.serve();
  game.update(2.0);
  EXPECT_EQ(game.scene, Game::Scene::Over);
  EXPECT_EQ(game.bestScore, 555);
  int record = game.bestScore;

  // A lower-scoring run must not overwrite the record.
  game.reset();
  for (int i = 0; i < 9; ++i) {
    game.serve();
    game.update(2.0);
  }
  game.score = 120;
  game.serve();
  game.update(2.0);
  EXPECT_EQ(game.scene, Game::Scene::Over);
  EXPECT_EQ(game.bestScore, record);

  // Ctrl-Alt-C clears the high score.
  game.clearScores();
  EXPECT_EQ(game.bestScore, 0);
}

TEST_F(GameTestFixture, HandleClickUIElements) {
  Game game(*assets, db);
  game.startRun();

  // Grinder regular: {92, 150, 64, 70}
  game.bean = "decaf";
  game.handleClick(100, 160);
  EXPECT_EQ(game.bean, "regular");

  // Grinder decaf: {166, 150, 64, 70}
  game.handleClick(170, 160);
  EXPECT_EQ(game.bean, "decaf");

  // Milk whole: {92, 250, 68, 121}
  game.milk = "nonfat";
  game.handleClick(100, 260);
  EXPECT_EQ(game.milk, "whole");

  // Milk nonfat: {172, 250, 68, 121}
  game.handleClick(180, 260);
  EXPECT_EQ(game.milk, "nonfat");

  // Cup size stacks: {620, 480, 48, 110}, {672, 460, 54, 130}, {730, 440, 60, 150}
  game.handleClick(690, 500);
  EXPECT_EQ(game.size, "tall");
  game.handleClick(750, 490);
  EXPECT_EQ(game.size, "grande");
  game.handleClick(640, 520);
  EXPECT_EQ(game.size, "short");

  // Shots (spigots 1, 2, 3): {285, 230, 110, 80}, {400, 230, 110, 80}, {510, 230, 110, 80}
  game.handleClick(450, 260);
  EXPECT_EQ(game.shots, 2);
  game.handleClick(550, 260);
  EXPECT_EQ(game.shots, 3);
  game.handleClick(340, 260);
  EXPECT_EQ(game.shots, 1);

  // Steamer: {580, 115, 60, 60}
  EXPECT_FALSE(game.steamed);
  game.handleClick(595, 135);
  EXPECT_TRUE(game.steamed);
  game.handleClick(595, 135);
  EXPECT_FALSE(game.steamed);

  // Flavor: top shelf bottles (chocolate at {280, 44, 44, 115})
  game.handleClick(300, 70);
  EXPECT_EQ(game.flavor, "chocolate");

  // Clicking active bottle toggles back to none
  game.handleClick(300, 70);
  EXPECT_EQ(game.flavor, "none");

  // Serve button: {600, 140, 177, 252}
  EXPECT_EQ(game.resolveT, 0.0);
  game.handleClick(650, 200);
  EXPECT_DOUBLE_EQ(game.resolveT, 1.6);

  // Click during resolve should do nothing
  game.handleClick(100, 460);
  EXPECT_FALSE(game.steamed);

  // Transition to Scene::Over and click to reset
  game.scene = Game::Scene::Over;
  game.handleClick(100, 100);
  EXPECT_EQ(game.scene, Game::Scene::Play);
  EXPECT_EQ(game.idx, 0u);
}

TEST_F(GameTestFixture, RenderAllScenesWithoutCrash) {
  ASSERT_NE(renderer, nullptr);
  Game game(*assets, db);
  game.startRun();

  // Title scene
  game.scene = Game::Scene::Title;
  game.render(renderer);

  // Play scene
  game.scene = Game::Scene::Play;
  game.render(renderer);

  // Play scene with resolve feedback
  game.resolveT = 1.0;
  game.resolveResult = true;
  game.resolveMsg = "+125 CORRECT!";
  game.render(renderer);

  // Over scene
  game.scene = Game::Scene::Over;
  game.render(renderer);
}

// Faces must advance with the customer's own index in the roster: each of the
// 10 slots gets a distinct face, matching the sprite order in CustomerList
// rather than a global serve counter. Uses the real asset DB, so it needs the
// original DATA tree (same guard as TestDrinks.RealAssetDatabaseIntegrity).
TEST(RealDataFaces, EachRosterCustomerGetsOwnFaceByIndex) {
  std::string root = getAssetRoot();
  if (!fs::is_directory(root)) {
    GTEST_SKIP() << "Asset root not available at: " << root;
  }

  SDL_Window* w = SDL_CreateWindow("faces", 0, 0, 800, 600, SDL_WINDOW_HIDDEN);
  ASSERT_NE(w, nullptr);
  SDL_Renderer* r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
  ASSERT_NE(r, nullptr);
  Assets a(r);
  DrinkDB db;
  ASSERT_TRUE(db.load(root));

  Game game(a, db);
  game.startRun();

  // Every customer in the 10-slot roster must show a different face.
  std::set<std::string> seen;
  for (int i = 0; i < 10; ++i) {
    ASSERT_FALSE(game.head.empty()) << "no head key set for roster slot " << i;
    EXPECT_TRUE(seen.insert(game.head).second)
        << "roster slot " << i << " reuses face " << game.head;
    // Serve (deliberately unbuilt = wrong) and run the feedback pause so the
    // next round picks a new customer and with it their own face.
    game.serve();
    game.resolveT = 1.6;
    game.update(2.0);
  }
  EXPECT_EQ(game.scene, Game::Scene::Over);

  SDL_DestroyRenderer(r);
  SDL_DestroyWindow(w);
}
