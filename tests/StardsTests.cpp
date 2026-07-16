#define _USE_MATH_DEFINES

#include "Utilities.h"
#include "UsgsAstroFrameSensorModel.h"
#include "UsgsAstroLsSensorModel.h"
#include "UsgsAstroPlugin.h"

#include <gtest/gtest.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

// These tests only build when STARDS support is compiled in. Without the
// option the translation unit is intentionally empty.
#ifdef USGSCSM_ENABLE_STARDS

using json = nlohmann::json;

namespace {

// Read a CSM model-state file and return the JSON body plus the model name.
// Handles both the "<MODELNAME>\n<json>" form and a bare JSON state (no
// prefix); in the latter case the model name is read from the m_modelName key.
std::string readStateBody(const std::string &path, std::string &modelName) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  std::string full = ss.str();

  size_t firstBrace = full.find_first_not_of(" \t\r\n");
  if (firstBrace != std::string::npos && full[firstBrace] == '{') {
    // Bare JSON state (no model-name prefix).
    isUsgsCsmState(full, modelName);
    return full;
  }
  // Prefixed form: "<MODELNAME>\n<json>".
  size_t nl = full.find('\n');
  modelName = full.substr(0, nl);
  return full.substr(nl + 1);
}

}  // namespace

// The VariantMap read from a STARDS state file matches the one parsed from the
// equivalent JSON state, key for key, for both scalar and array values.
TEST(Stards, VariantMapMatchesJsonState) {
  std::string modelName;
  std::string jsonBody =
      readStateBody("data/lineScanState.json.state", modelName);
  VariantMap fromJson = variantMapFromJson(stateAsJson(jsonBody));
  VariantMap fromStards = variantMapFromStards("data/lineScanState.stards");

  // Every key present in the JSON state is present in the STARDS state.
  for (const std::string &key : fromJson.keys()) {
    ASSERT_TRUE(fromStards.contains(key)) << "missing key: " << key;
  }

  // A representative scalar and a large array agree.
  EXPECT_DOUBLE_EQ(fromJson.get<double>("m_focalLength"),
                   fromStards.get<double>("m_focalLength"));
  std::vector<double> qJson = fromJson.get<std::vector<double>>("m_quaternions");
  std::vector<double> qStards =
      fromStards.get<std::vector<double>>("m_quaternions");
  ASSERT_EQ(qJson.size(), qStards.size());
  for (size_t i = 0; i < qJson.size(); ++i) {
    EXPECT_DOUBLE_EQ(qJson[i], qStards[i]) << "quaternion element " << i;
  }
}

// A line scan model built from a STARDS state produces the same projections as
// the same model built from its JSON state.
TEST(Stards, LineScanModelMatchesJsonState) {
  std::string modelName;
  std::string jsonBody =
      readStateBody("data/lineScanState.json.state", modelName);

  std::unique_ptr<csm::RasterGM> fromJson(
      getUsgsCsmModelFromJsonState(jsonBody, modelName, nullptr));
  std::unique_ptr<csm::RasterGM> fromStards(
      getUsgsCsmModelFromStards("data/lineScanState.stards", nullptr));
  ASSERT_NE(fromJson.get(), nullptr);
  ASSERT_NE(fromStards.get(), nullptr);

  csm::ImageCoord imagePt(500.0, 500.0);
  csm::EcefCoord gJson = fromJson->imageToGround(imagePt, 0.0);
  csm::EcefCoord gStards = fromStards->imageToGround(imagePt, 0.0);
  EXPECT_NEAR(gJson.x, gStards.x, 1e-6);
  EXPECT_NEAR(gJson.y, gStards.y, 1e-6);
  EXPECT_NEAR(gJson.z, gStards.z, 1e-6);
}

// A frame model built from a STARDS state produces the same projections as the
// same model built from its JSON state.
TEST(Stards, FrameModelMatchesJsonState) {
  std::string modelName;
  std::string jsonBody = readStateBody("data/frameState.json.state", modelName);

  std::unique_ptr<csm::RasterGM> fromJson(
      getUsgsCsmModelFromJsonState(jsonBody, modelName, nullptr));
  std::unique_ptr<csm::RasterGM> fromStards(
      getUsgsCsmModelFromStards("data/frameState.stards", nullptr));
  ASSERT_NE(fromJson.get(), nullptr);
  ASSERT_NE(fromStards.get(), nullptr);

  csm::ImageCoord imagePt(7.5, 7.5);
  csm::EcefCoord gJson = fromJson->imageToGround(imagePt, 0.0);
  csm::EcefCoord gStards = fromStards->imageToGround(imagePt, 0.0);
  EXPECT_NEAR(gJson.x, gStards.x, 1e-6);
  EXPECT_NEAR(gJson.y, gStards.y, 1e-6);
  EXPECT_NEAR(gJson.z, gStards.z, 1e-6);
}

// The plugin routes a .stards image file through the STARDS state path.
TEST(Stards, PluginConstructsFromStardsFile) {
  csm::Isd isd;
  isd.setFilename("data/lineScanState.stards");
  UsgsAstroPlugin plugin;
  std::unique_ptr<csm::Model> model(plugin.constructModelFromISD(
      isd, UsgsAstroLsSensorModel::_SENSOR_MODEL_NAME, nullptr));
  EXPECT_NE(dynamic_cast<UsgsAstroLsSensorModel *>(model.get()), nullptr);
}

// isStardsFile keys only off the .stards extension.
TEST(Stards, IsStardsFileExtension) {
  EXPECT_TRUE(isStardsFile("foo/bar.stards"));
  EXPECT_FALSE(isStardsFile("foo/bar.json"));
  EXPECT_FALSE(isStardsFile("stards"));
}

// A missing STARDS file raises a clear error rather than crashing.
TEST(Stards, MissingFileThrows) {
  EXPECT_THROW(variantMapFromStards("data/does_not_exist.stards"), csm::Error);
}

#endif  // USGSCSM_ENABLE_STARDS
