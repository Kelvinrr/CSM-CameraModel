#include "UsgsAstroFramePlugin.h"
#include "UsgsAstroFrameSensorModel.h"

#include <sstream>
#include <fstream>

#include <gtest/gtest.h>

<<<<<<< HEAD:tests/TestyMcTestFace.cpp
using json = nlohmann::json;

class FrameIsdTest : public ::testing::Test {
   protected:

      csm::Isd isd;
      std::string isdstring;

   virtual void SetUp() {
      std::ifstream isdFile("/Users/krodriguez-pr/repos/CSM-CameraModel/tests/data/simpleFramerISD.json");
      std::stringstream buffer;
      buffer << isdFile.rdbuf();
      isdFile.close();
      isdstring = buffer.str();
      json jsonIsd = json::parse(buffer.str());
      for (json::iterator it = jsonIsd.begin(); it != jsonIsd.end(); ++it) {
         json jsonValue = it.value();
         if (jsonValue.size() > 1) {
            for (int i = 0; i < jsonValue.size(); i++) {
               isd.addParam(it.key(), jsonValue[i].dump());
            }
         }
         else {
            isd.addParam(it.key(), jsonValue.dump());
         }
      }
      isdFile.close();
   }
};
=======
#include "Fixtures.h"
>>>>>>> jayupdates:tests/FramePluginTests.cpp

TEST(FramePluginTests, PluginName) {
   UsgsAstroFramePlugin testPlugin;
   EXPECT_EQ("UsgsAstroFramePluginCSM", testPlugin.getPluginName());;
}

TEST(FramePluginTests, ManufacturerName) {
   UsgsAstroFramePlugin testPlugin;
   EXPECT_EQ("UsgsAstrogeology", testPlugin.getManufacturer());;
}

TEST(FramePluginTests, ReleaseDate) {
   UsgsAstroFramePlugin testPlugin;
   EXPECT_EQ("20170425", testPlugin.getReleaseDate());;
}

TEST(FramePluginTests, NumModels) {
   UsgsAstroFramePlugin testPlugin;
   EXPECT_EQ(1, testPlugin.getNumModels());;
}

TEST(FramePluginTests, NoStateName) {
   UsgsAstroFramePlugin testPlugin;
   std::string badState = "{\"not_a_name\":\"bad_name\"}";
   EXPECT_FALSE(testPlugin.canModelBeConstructedFromState(
         "USGS_ASTRO_FRAME_SENSOR_MODEL",
         badState));;
}

TEST(FramePluginTests, BadStateName) {
   UsgsAstroFramePlugin testPlugin;
   std::string badState = "{\"model_name\":\"bad_name\"}";
   EXPECT_FALSE(testPlugin.canModelBeConstructedFromState(
         "USGS_ASTRO_FRAME_SENSOR_MODEL",
         badState));;
}

TEST(FramePluginTests, BadStateValue) {
   UsgsAstroFramePlugin testPlugin;
   std::string badState = "{"
         "\"model_name\":\"USGS_ASTRO_FRAME_SENSOR_MODEL\","
         "\"bad_param\":\"bad_value\"}";
   EXPECT_FALSE(testPlugin.canModelBeConstructedFromState(
         "USGS_ASTRO_FRAME_SENSOR_MODEL",
         badState));;
}

TEST(FramePluginTests, MissingStateValue) {
   UsgsAstroFramePlugin testPlugin;
   std::string badState = "{"
         "\"model_name\":\"USGS_ASTRO_FRAME_SENSOR_MODEL\"}";
   EXPECT_FALSE(testPlugin.canModelBeConstructedFromState(
         "USGS_ASTRO_FRAME_SENSOR_MODEL",
         badState));;
}

TEST_F(FrameIsdTest, Constructible) {
   UsgsAstroFramePlugin testPlugin;
   EXPECT_TRUE(testPlugin.canModelBeConstructedFromISD(
               isd,
               "USGS_ASTRO_FRAME_SENSOR_MODEL"));
}

TEST_F(FrameIsdTest, ConstructValidCamera) {
   testing::internal::CaptureStdout();
   UsgsAstroFramePlugin testPlugin;
   csm::Model *cameraModel = NULL;
   EXPECT_NO_THROW(
         cameraModel = testPlugin.constructModelFromISD(
               isd,
               "USGS_ASTRO_FRAME_SENSOR_MODEL",
               NULL)
   );
   std::string capturedStdout = ::testing::internal::GetCapturedStdout().c_str();
   EXPECT_STREQ(isdstring.c_str(), capturedStdout.c_str());
   // UsgsAstroFrameSensorModel *frameModel = dynamic_cast<UsgsAstroFrameSensorModel *>(cameraModel);
   // EXPECT_NE(frameModel, nullptr);
   // if (cameraModel) {
   //    delete cameraModel;
   // }
}

TEST_F(FrameIsdTest, ConstructInValidCamera) {
   UsgsAstroFramePlugin testPlugin;
   // Remove the model_name keyword from the ISD to make it invalid
   isd.clearParams("model_name");
   csm::Model *cameraModel = NULL;
   try {
      testPlugin.constructModelFromISD(
            isd,
            "USGS_ASTRO_FRAME_SENSOR_MODEL",
            NULL);
   }
   catch(csm::Error &e) {
      EXPECT_EQ(e.getError(), csm::Error::ISD_NOT_SUPPORTED);
   }
   catch(...) {
      FAIL() << "Expected csm ISD_NOT_SUPPORTED error";
   }
   if (cameraModel) {
      delete cameraModel;
   }
}

int main(int argc, char **argv) {
   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
