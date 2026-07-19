#include "usgscsm/Logging.h"
#include "UsgsAstroPlugin.h"
#include <gtest/gtest.h>
#include <sstream>

// Test fixture to capture stderr output
class LoggingTests : public ::testing::Test {
protected:
    std::stringstream buffer;
    std::streambuf* old;

    void SetUp() override {
        // Redirect std::cerr to our buffer
        old = std::cerr.rdbuf(buffer.rdbuf());
        usgscsm::logger::set_log_level(usgscsm::logger::DEBUG);
    }

    void TearDown() override {
        // Restore std::cerr
        std::cerr.rdbuf(old);
    }

    std::string getOutput() {
        return buffer.str();
    }
};

TEST_F(LoggingTests, PlaceholderFormatting) {
    LOG_DEBUG("Test {} and {}", 42, "string");
    std::string output = getOutput();

    EXPECT_NE(output.find("Test 42 and string"), std::string::npos);
    EXPECT_NE(output.find("[DEBUG]"), std::string::npos);
}

TEST_F(LoggingTests, MultiplePlaceholders) {
    int x = 100;
    double y = 3.14;
    std::string name = "test";

    LOG_INFO("Values: x={}, y={}, name={}", x, y, name);
    std::string output = getOutput();

    EXPECT_NE(output.find("x=100"), std::string::npos);
    EXPECT_NE(output.find("y=3.14"), std::string::npos);
    EXPECT_NE(output.find("name=test"), std::string::npos);
}

TEST_F(LoggingTests, SingleMessage) {
    LOG_WARN("Single message");
    std::string output = getOutput();

    EXPECT_NE(output.find("Single message"), std::string::npos);
    EXPECT_NE(output.find("[WARN]"), std::string::npos);
}

TEST_F(LoggingTests, StdStringMessage) {
    std::string msg = "Message from string variable";
    LOG_ERROR(msg);
    std::string output = getOutput();

    EXPECT_NE(output.find("Message from string variable"), std::string::npos);
    EXPECT_NE(output.find("[ERROR]"), std::string::npos);
}

TEST_F(LoggingTests, LogLevelFiltering) {
    usgscsm::logger::set_log_level(usgscsm::logger::WARN);

    LOG_DEBUG("This should not appear");
    LOG_INFO("This should not appear either");
    LOG_WARN("This should appear");

    std::string output = getOutput();

    EXPECT_EQ(output.find("This should not appear"), std::string::npos);
    EXPECT_NE(output.find("This should appear"), std::string::npos);
}

TEST_F(LoggingTests, MixedTypes) {
    LOG_DEBUG("int={}, double={}, bool={}, string={}", 42, 3.14159, true, "hello");
    std::string output = getOutput();

    EXPECT_NE(output.find("int=42"), std::string::npos);
    EXPECT_NE(output.find("double=3.14159"), std::string::npos);
    EXPECT_NE(output.find("bool=1"), std::string::npos);  // bool prints as 1/0
    EXPECT_NE(output.find("string=hello"), std::string::npos);
}

TEST_F(LoggingTests, ExtraPlaceholders) {
    // More placeholders than arguments - should just output placeholders literally
    LOG_DEBUG("Test {} and {}", 42);
    std::string output = getOutput();

    EXPECT_NE(output.find("Test 42 and {}"), std::string::npos);
}

TEST_F(LoggingTests, NoPlaceholders) {
    // Arguments but no placeholders - legacy behavior (just outputs format string)
    LOG_DEBUG("Test message", 42, "extra");
    std::string output = getOutput();

    EXPECT_NE(output.find("Test message"), std::string::npos);
}

TEST_F(LoggingTests, UnprojectedIsdLogsNoProjectedError) {
    // A frame ISD has no "geotransform", so the projected sensor model is not
    // attempted, and constructing a normal frame model logs no error. Regression
    // for the misleading "Could not construct model ... geotransform" error that
    // a plain frame or unprojected linescan load used to print at ERROR level.
    UsgsAstroPlugin testPlugin;
    csm::Isd isd("data/simpleFramerISD.img");
    csm::Model *model = testPlugin.constructModelFromISD(
        isd, "USGS_ASTRO_FRAME_SENSOR_MODEL", nullptr);
    ASSERT_NE(model, nullptr);
    std::string output = getOutput();

    EXPECT_EQ(output.find("[ERROR]"), std::string::npos);
    EXPECT_EQ(output.find("geotransform"), std::string::npos);
    delete model;
}
