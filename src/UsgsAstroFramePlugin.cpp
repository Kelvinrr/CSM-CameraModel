#include "UsgsAstroFramePlugin.h"
#include "UsgsAstroFrameSensorModel.h"

#include <cstdlib>
#include <string>

#include <csm.h>
#include <Error.h>
#include <Plugin.h>
#include <Warning.h>
#include <Version.h>

#include <json.hpp>


using json = nlohmann::json;

#ifdef _WIN32
# define DIR_DELIMITER_STR "\\"
#else
# define DIR_DELIMITER_STR  "/"
#endif


// Declaration of static variables
const std::string UsgsAstroFramePlugin::_PLUGIN_NAME = "UsgsAstroFramePluginCSM";
const std::string UsgsAstroFramePlugin::_MANUFACTURER_NAME = "UsgsAstrogeology";
const std::string UsgsAstroFramePlugin::_RELEASE_DATE = "20170425";
const int         UsgsAstroFramePlugin::_N_SENSOR_MODELS = 1;

const int         UsgsAstroFramePlugin::_NUM_ISD_KEYWORDS = 36;
const std::string UsgsAstroFramePlugin::_ISD_KEYWORD[] =
{
  "center_ephemeris_time",
   "dt_ephemeris",
   "focal2pixel_lines",
   "focal2pixel_samples",
   "focal_length_model_focal_length",
   "focal_length_model_focal_length_epsilon",
   "image_lines",
   "image_samples",
   "interpolation_method",
   "number_of_ephemerides",
   "optical_distortion_x",
   "optical_distortion_y",
   "radii_semimajor",
   "radii_semiminor",
   "reference_height_maxheight",
   "reference_height_minheight",
   "reference_height_unit",
   "sensor_location_unit",
   "sensor_location_x",
   "sensor_location_y",
   "sensor_location_z",
   "sensor_orientation",
   "sensor_velocity_unit",
   "sensor_velocity_x",
   "sensor_velocity_y",
   "sensor_velocity_z",
   "starting_detector_line",
   "starting_detector_sample",
   "starting_ephemeris_time",
   "sun_position_x",
   "sun_position_y",
   "sun_position_z",
   "sun_velocity_x",
   "sun_velocity_y",
   "sun_velocity_z"
};

const int         UsgsAstroFramePlugin::_NUM_STATE_KEYWORDS = 32;
const std::string UsgsAstroFramePlugin::_STATE_KEYWORD[] =
{
    "m_focalLength",
    "m_iTransS",
    "m_iTransL",
    "m_boresight",
    "m_transX",
    "m_transY",
    "m_majorAxis",
    "m_minorAxis",
    "m_spacecraftVelocity",
    "m_sunPosition",
    "m_startingDetectorSample",
    "m_startingDetectorLine",
    "m_targetName",
    "m_ifov",
    "m_instrumentID",
    "m_focalLengthEpsilon",
    "m_ccdCenter",
    "m_line_pp",
    "m_sample_pp",
    "m_minElevation",
    "m_maxElevation",
    "m_odtX",
    "m_odtY",
    "m_originalHalfLines",
    "m_originalHalfSamples",
    "m_spacecraftName",
    "m_pixelPitch",
    "m_ephemerisTime",
    "m_nLines",
    "m_nSamples",
    "m_currentParameterValue",
    "m_currentParameterCovariance"
};


// Static Instance of itself
const UsgsAstroFramePlugin UsgsAstroFramePlugin::m_registeredPlugin;

UsgsAstroFramePlugin::UsgsAstroFramePlugin() {
}


UsgsAstroFramePlugin::~UsgsAstroFramePlugin() {
}


std::string UsgsAstroFramePlugin::getPluginName() const {
  return _PLUGIN_NAME;
}


std::string UsgsAstroFramePlugin::getManufacturer() const {
  return _MANUFACTURER_NAME;
}


std::string UsgsAstroFramePlugin::getReleaseDate() const {
  return _RELEASE_DATE;
}


csm::Version UsgsAstroFramePlugin::getCsmVersion() const {
  return CURRENT_CSM_VERSION;
}


size_t UsgsAstroFramePlugin::getNumModels() const {
  return _N_SENSOR_MODELS;
}


std::string UsgsAstroFramePlugin::getModelName(size_t modelIndex) const {

  return UsgsAstroFrameSensorModel::_SENSOR_MODEL_NAME;
}


std::string UsgsAstroFramePlugin::getModelFamily(size_t modelIndex) const {
  return CSM_RASTER_FAMILY;
}


csm::Version UsgsAstroFramePlugin::getModelVersion(const std::string &modelName) const {
  return csm::Version(1, 0, 0);
}


bool UsgsAstroFramePlugin::canModelBeConstructedFromState(const std::string &modelName,
                                                const std::string &modelState,
                                                csm::WarningList *warnings) const {
  bool constructible = true;

  // Get the model name from the model state
  std::string model_name_from_state;
  try {
    model_name_from_state = getModelNameFromModelState(modelState, warnings);
  }
  catch(...) {
    return false;
  }

  // Check that the plugin supports the model
  if (modelName != model_name_from_state ||
      modelName != UsgsAstroFrameSensorModel::_SENSOR_MODEL_NAME){
          constructible = false;
      }
  // Check that the necessary keys are there (this does not chek values at all.)
  auto state = json::parse(modelState);
  for(auto &key : _STATE_KEYWORD){
      if (state.find(key) == state.end()){
          constructible = false;
          break;
      }
  }
  return constructible;
}


bool UsgsAstroFramePlugin::canModelBeConstructedFromISD(const csm::Isd &imageSupportData,
                                              const std::string &modelName,
                                              csm::WarningList *warnings) const {
  return canISDBeConvertedToModelState(imageSupportData, modelName, warnings);
}


csm::Model *UsgsAstroFramePlugin::constructModelFromState(const std::string& modelState,
                                                csm::WarningList *warnings) const {
    csm::Model *sensor_model = 0;

    // Get the sensor model name from the sensor model state
    std::string model_name_from_state = getModelNameFromModelState(modelState);

    if (model_name_from_state != UsgsAstroFrameSensorModel::_SENSOR_MODEL_NAME){
        csm::Error::ErrorType aErrorType = csm::Error::INVALID_SENSOR_MODEL_STATE;
        std::string aMessage = "Model name from state is not recognized.";
        std::string aFunction = "UsgsAstroFramePlugin::constructModelFromState()";
        throw csm::Error(aErrorType, aMessage, aFunction);
    }

    if (!canModelBeConstructedFromState(model_name_from_state, modelState)){
        csm::Error::ErrorType aErrorType = csm::Error::INVALID_SENSOR_MODEL_STATE;
        std::string aMessage = "Model state is not valid.";
        std::string aFunction = "UsgsAstroFramePlugin::constructModelFromState()";
        throw csm::Error(aErrorType, aMessage, aFunction);
    }

    // Create the model from the state
    UsgsAstroFrameSensorModel* mdsensor_model = new UsgsAstroFrameSensorModel();

    auto state = json::parse(modelState);

    mdsensor_model->m_ccdCenter[0] = state["m_ccdCenter"][0];
    mdsensor_model->m_ccdCenter[1] = state["m_ccdCenter"][1];
    mdsensor_model->m_ephemerisTime = state["m_ephemerisTime"];
    mdsensor_model->m_focalLength = state["m_focalLength"];
    mdsensor_model->m_focalLengthEpsilon = state["m_focalLengthEpsilon"];
    mdsensor_model->m_ifov = state["m_ifov"];
    mdsensor_model->m_instrumentID = state["m_instrumentID"];

    mdsensor_model->m_majorAxis = state["m_majorAxis"];
    mdsensor_model->m_minorAxis = state["m_minorAxis"];
    mdsensor_model->m_startingDetectorLine = state["m_startingDetectorLine"];
    mdsensor_model->m_startingDetectorSample = state["m_startingDetectorSample"];
    mdsensor_model->m_line_pp = state["m_line_pp"];
    mdsensor_model->m_sample_pp = state["m_sample_pp"];
    mdsensor_model->m_originalHalfLines = state["m_originalHalfLines"];
    mdsensor_model->m_originalHalfSamples = state["m_originalHalfSamples"];
    mdsensor_model->m_spacecraftName = state["m_spacecraftName"];
    mdsensor_model->m_pixelPitch = state["m_pixelPitch"];
    mdsensor_model->m_nLines = state["m_nLines"];
    mdsensor_model->m_nSamples = state["m_nSamples"];
    mdsensor_model->m_minElevation = state["m_minElevation"];
    mdsensor_model->m_maxElevation = state["m_maxElevation"];

    for (int i=0;i<3;i++){
        mdsensor_model->m_boresight[i] = state["m_boresight"][i];
        mdsensor_model->m_iTransL[i] = state["m_iTransL"][i];
        mdsensor_model->m_iTransS[i] = state["m_iTransS"][i];

        mdsensor_model->m_transX[i] = state["m_transX"][i];
        mdsensor_model->m_transY[i] = state["m_transY"][i];
        mdsensor_model->m_spacecraftVelocity[i] = state["m_spacecraftVelocity"][i];
        mdsensor_model->m_sunPosition[i] = state["m_sunPosition"][i];
    }

    // Having types as vectors, instead of arrays makes interoperability with
    // the JSON library very easy.
    mdsensor_model->m_currentParameterValue = state["m_currentParameterValue"].get<std::vector<double>>();
    mdsensor_model->m_odtX = state["m_odtX"].get<std::vector<double>>();
    mdsensor_model->m_odtY = state["m_odtY"].get<std::vector<double>>();

    mdsensor_model->m_currentParameterCovariance = state["m_currentParameterCovariance"].get<std::vector<double>>();

sensor_model = mdsensor_model;
return sensor_model;
}


csm::Model *UsgsAstroFramePlugin::constructModelFromISD(const csm::Isd &imageSupportData,
                                              const std::string &modelName,
                                              csm::WarningList *warnings) const {

  // // Check if the sensor model can be constructed from ISD given the model name
  // if (!canModelBeConstructedFromISD(imageSupportData, modelName)) {
  //   throw csm::Error(csm::Error::ISD_NOT_SUPPORTED,
  //                    "Sensor model support data provided is not supported by this plugin",
  //                    "UsgsAstroFramePlugin::constructModelFromISD");
  // }
  UsgsAstroFrameSensorModel *sensorModel = new UsgsAstroFrameSensorModel();

  // Keep track of necessary keywords that are missing from the ISD.
  std::vector<std::string> missingKeywords;

  sensorModel->m_startingDetectorSample =
      atof(imageSupportData.param("starting_detector_sample").c_str());
  sensorModel->m_startingDetectorLine =
      atof(imageSupportData.param("starting_detector_line").c_str());

  std::cout << imageSupportData.param("radii_semiminor") << std::endl;

  sensorModel->m_focalLength = atof(imageSupportData.param("focal_length_model_focal_length").c_str());
  if (imageSupportData.param("focal_length_model_focal_length") == "") {
    missingKeywords.push_back("focal_length_model_focal_length");
  }
  sensorModel->m_focalLengthEpsilon =
      atof(imageSupportData.param("focal_length_model_focal_epsilon").c_str());

  sensorModel->m_currentParameterValue[0] =
      atof(imageSupportData.param("sensor_location_x").c_str());
  sensorModel->m_currentParameterValue[1] =
      atof(imageSupportData.param("sensor_location_y").c_str());
  sensorModel->m_currentParameterValue[2] =
      atof(imageSupportData.param("sensor_location_z").c_str());
  if (imageSupportData.param("sensor_location_x") == "") {
    missingKeywords.push_back("sensor_location_x");
  }
  if (imageSupportData.param("sensor_location_y") == "") {
    missingKeywords.push_back("sensor_location_y");
  }
  if (imageSupportData.param("sensor_location_z") == "") {
    missingKeywords.push_back("sensor_location_z");
  }

  sensorModel->m_spacecraftVelocity[0] =
      atof(imageSupportData.param("sensor_velocity_x").c_str());
  sensorModel->m_spacecraftVelocity[1] =
      atof(imageSupportData.param("sensor_velocity_y").c_str());
  sensorModel->m_spacecraftVelocity[2] =
      atof(imageSupportData.param("sensor_velocity_z").c_str());
  // sensor velocity not strictly necessary?

  sensorModel->m_sunPosition[0] =
      atof(imageSupportData.param("sun_position_x").c_str());
  sensorModel->m_sunPosition[1] =
      atof(imageSupportData.param("sun_position_y").c_str());
  sensorModel->m_sunPosition[2] =
      atof(imageSupportData.param("sun_position_z").c_str());
  // sun position is not strictly necessary, but is required for getIlluminationDirection.

  sensorModel->m_currentParameterValue[3] = atof(imageSupportData.param("sensor_orientation", 0).c_str());
  sensorModel->m_currentParameterValue[4] = atof(imageSupportData.param("sensor_orientation", 1).c_str());
  sensorModel->m_currentParameterValue[5] = atof(imageSupportData.param("sensor_orientation", 2).c_str());
  sensorModel->m_currentParameterValue[6] = atof(imageSupportData.param("sensor_orientation", 3).c_str());

  if (imageSupportData.param("sensor_orientation", 0) == "") {
    missingKeywords.push_back("sensor_orientation");
  }

  sensorModel->m_odtX[0] = atof(imageSupportData.param("optical_distortion_x", 0).c_str());
  sensorModel->m_odtX[1] = atof(imageSupportData.param("optical_distortion_x", 1).c_str());
  sensorModel->m_odtX[2] = atof(imageSupportData.param("optical_distortion_x", 2).c_str());
  sensorModel->m_odtX[3] = atof(imageSupportData.param("optical_distortion_x", 3).c_str());
  sensorModel->m_odtX[4] = atof(imageSupportData.param("optical_distortion_x", 4).c_str());
  sensorModel->m_odtX[5] = atof(imageSupportData.param("optical_distortion_x", 5).c_str());
  sensorModel->m_odtX[6] = atof(imageSupportData.param("optical_distortion_x", 6).c_str());
  sensorModel->m_odtX[7] = atof(imageSupportData.param("optical_distortion_x", 7).c_str());
  sensorModel->m_odtX[8] = atof(imageSupportData.param("optical_distortion_x", 8).c_str());
  sensorModel->m_odtX[9] = atof(imageSupportData.param("optical_distortion_x", 9).c_str());

  sensorModel->m_odtY[0] = atof(imageSupportData.param("optical_distortion_y", 0).c_str());
  sensorModel->m_odtY[1] = atof(imageSupportData.param("optical_distortion_y", 1).c_str());
  sensorModel->m_odtY[2] = atof(imageSupportData.param("optical_distortion_y", 2).c_str());
  sensorModel->m_odtY[3] = atof(imageSupportData.param("optical_distortion_y", 3).c_str());
  sensorModel->m_odtY[4] = atof(imageSupportData.param("optical_distortion_y", 4).c_str());
  sensorModel->m_odtY[5] = atof(imageSupportData.param("optical_distortion_y", 5).c_str());
  sensorModel->m_odtY[6] = atof(imageSupportData.param("optical_distortion_y", 6).c_str());
  sensorModel->m_odtY[7] = atof(imageSupportData.param("optical_distortion_y", 7).c_str());
  sensorModel->m_odtY[8] = atof(imageSupportData.param("optical_distortion_y", 8).c_str());
  sensorModel->m_odtY[9] = atof(imageSupportData.param("optical_distortion_y", 9).c_str());

  sensorModel->m_ephemerisTime = atof(imageSupportData.param("center_ephemeris_time").c_str());
  if (imageSupportData.param("center_ephemeris_time") == "") {
    missingKeywords.push_back("center_ephemeris_time");
  }

  sensorModel->m_nLines = atoi(imageSupportData.param("image_lines").c_str());
  sensorModel->m_nSamples = atoi(imageSupportData.param("image_samples").c_str());
  if (imageSupportData.param("image_lines") == "") {
    missingKeywords.push_back("image_lines");
  }
  if (imageSupportData.param("image_samples") == "") {
    missingKeywords.push_back("image_samples");
  }

  sensorModel->m_transY[0] = atof(imageSupportData.param("focal2pixel_lines", 0).c_str());
  sensorModel->m_transY[1] = atof(imageSupportData.param("focal2pixel_lines", 1).c_str());
  sensorModel->m_transY[2] = atof(imageSupportData.param("focal2pixel_lines", 2).c_str());
  if (imageSupportData.param("focal2pixel_lines", 0) == "") {
    missingKeywords.push_back("focal2pixel_lines 0");
  }
  else if (imageSupportData.param("focal2pixel_lines", 1) == "") {
    missingKeywords.push_back("focal2pixel_lines 1");
  }
  else if (imageSupportData.param("focal2pixel_lines", 2) == "") {
    missingKeywords.push_back("focal2pixel_lines 2");
  }

  sensorModel->m_transX[0] = atof(imageSupportData.param("focal2pixel_samples", 0).c_str());
  sensorModel->m_transX[1] = atof(imageSupportData.param("focal2pixel_samples", 1).c_str());
  sensorModel->m_transX[2] = atof(imageSupportData.param("focal2pixel_samples", 2).c_str());
  if (imageSupportData.param("focal2pixel_samples", 0) == "") {
    missingKeywords.push_back("focal2pixel_samples 0");
  }
  else if (imageSupportData.param("focal2pixel_samples", 1) == "") {
    missingKeywords.push_back("focal2pixel_samples 1");
  }
  else if (imageSupportData.param("focal2pixel_samples", 2) == "") {
    missingKeywords.push_back("focal2pixel_samples 2");
  }

  sensorModel->m_majorAxis = 1000 * atof(imageSupportData.param("radii_semimajor").c_str());
  if (imageSupportData.param("radii_semimajor") == "") {
    missingKeywords.push_back("radii_semimajor");
  }
  // Do we assume that if we do not have a semi-minor axis, then the body is a sphere?
  if (imageSupportData.param("radii_semiminor") == "") {
    sensorModel->m_minorAxis = sensorModel->m_majorAxis;
  }
  else {
    sensorModel->m_minorAxis = 1000 * atof(imageSupportData.param("radii_semiminor").c_str());
  }

  sensorModel->m_minElevation = atof(imageSupportData.param("reference_height_minheight").c_str());
  sensorModel->m_maxElevation = atof(imageSupportData.param("reference_height_maxheight").c_str());
  if (imageSupportData.param("reference_height_minheight") == ""){
      missingKeywords.push_back("reference_height_minheight");
  }
  if (imageSupportData.param("reference_height_maxheight") == ""){
      missingKeywords.push_back("reference_height_maxheight");
  }

  // If we are missing necessary keywords from ISD, we cannot create a valid sensor model.
  if (missingKeywords.size() != 0) {

    std::string errorMessage = "ISD is missing the necessary keywords: [";

    for (int i = 0; i < (int)missingKeywords.size(); i++) {
      if (i == (int)missingKeywords.size() - 1) {
        errorMessage += missingKeywords[i] + "]";
      }
      else {
        errorMessage += missingKeywords[i] + ", ";
      }
    }

    throw csm::Error(csm::Error::SENSOR_MODEL_NOT_CONSTRUCTIBLE,
                     errorMessage,
                     "UsgsAstroFramePlugin::constructModelFromISD");
  }

  return sensorModel;
}


std::string UsgsAstroFramePlugin::getModelNameFromModelState(const std::string &modelState,
                                                   csm::WarningList *warnings) const {
  std::string name;
  auto state = json::parse(modelState);
  if(state.find("model_name") != state.end()){
      name = state["model_name"];
  }
  else{
      csm::Error::ErrorType aErrorType = csm::Error::INVALID_SENSOR_MODEL_STATE;
      std::string aMessage = "No 'model_name' key in the model state object.";
      std::string aFunction = "UsgsAstroFramePlugin::getModelNameFromModelState";
      csm::Error csmErr(aErrorType, aMessage, aFunction);
      throw(csmErr);
  }

  if (name != UsgsAstroFrameSensorModel::_SENSOR_MODEL_NAME){
      csm::Error::ErrorType aErrorType = csm::Error::SENSOR_MODEL_NOT_SUPPORTED;
      std::string aMessage = "Sensor model not supported.";
      std::string aFunction = "UsgsAstroFramePlugin::getModelNameFromModelState()";
      csm::Error csmErr(aErrorType, aMessage, aFunction);
      throw(csmErr);
  }


  return UsgsAstroFrameSensorModel::_SENSOR_MODEL_NAME;
}


bool UsgsAstroFramePlugin::canISDBeConvertedToModelState(const csm::Isd &imageSupportData,
                                               const std::string &modelName,
                                               csm::WarningList *warnings) const {
  bool convertible = true;
  if (modelName !=UsgsAstroFrameSensorModel::_SENSOR_MODEL_NAME){
      convertible = false;
  }

  std::string value;
  for(auto &key : _ISD_KEYWORD){
      value = imageSupportData.param(key);
      if (value.empty()){
          convertible = false;
      }
  }
  return convertible;
}


std::string UsgsAstroFramePlugin::convertISDToModelState(const csm::Isd &imageSupportData,
                                               const std::string &modelName,
                                               csm::WarningList *warnings) const {
  csm::Model* sensor_model = constructModelFromISD(
                             imageSupportData, modelName);

  if (sensor_model == 0){
      csm::Error::ErrorType aErrorType = csm::Error::ISD_NOT_SUPPORTED;
      std::string aMessage = "ISD not supported: ";
      std::string aFunction = "UsgsAstroFramePlugin::convertISDToModelState()";
      throw csm::Error(aErrorType, aMessage, aFunction);
  }
  return sensor_model->getModelState();
}
