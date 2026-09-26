#include "TestHelper.h"

TEST_F(HeadlessSDLTest, LoadRootNonExistent) {
  Assets assets(renderer);
  EXPECT_FALSE(assets.loadRoot("/non_existent_path_jamoke_assets_12345"));
  EXPECT_TRUE(assets.files.empty());
}

TEST_F(HeadlessSDLTest, IndexDirectoryCaseInsensitive) {
  TempDirGuard tempDir;
  tempDir.writeFile("UiArt/SubFolder/TestFile.txt", "content");
  tempDir.writeFile("Sounds/BEEP.WAV", "fake_wav");

  Assets assets(renderer);
  EXPECT_TRUE(assets.loadRoot(tempDir.path.string()));
  EXPECT_EQ(assets.files.size(), 2u);

  // Checks lowercase relative paths
  EXPECT_NE(assets.files.find("uiart/subfolder/testfile.txt"), assets.files.end());
  EXPECT_NE(assets.files.find("sounds/beep.wav"), assets.files.end());
}

TEST_F(HeadlessSDLTest, TexAndQueryMissing) {
  Assets assets(renderer);
  EXPECT_EQ(assets.tex("missing_texture.png"), nullptr);
  EXPECT_EQ(assets.texW("missing_texture.png"), 0);
  EXPECT_EQ(assets.texH("missing_texture.png"), 0);
}

TEST_F(HeadlessSDLTest, PlayMissingAudioGraceful) {
  Assets assets(renderer);
  // Should not crash or throw
  assets.play("missing_sound.wav");
}

TEST_F(HeadlessSDLTest, RealAssetsLoadingAndTextures) {
  std::string root = getAssetRoot();
  if (!fs::is_directory(root)) {
    GTEST_SKIP() << "Asset root not available at: " << root;
  }

  Assets assets(renderer);
  ASSERT_TRUE(assets.loadRoot(root));
  EXPECT_FALSE(assets.files.empty());

  // Check background image loading
  SDL_Texture* bg = assets.tex("uiart/background.jpg");
  EXPECT_NE(bg, nullptr);
  EXPECT_EQ(assets.texW("uiart/background.jpg"), 800);
  EXPECT_EQ(assets.texH("uiart/background.jpg"), 600);

  // Check caching: second call returns same pointer
  SDL_Texture* bg2 = assets.tex("uiart/background.jpg");
  EXPECT_EQ(bg, bg2);

  // Check sound playback
  assets.play("sounds/cashreg.wav");
}
