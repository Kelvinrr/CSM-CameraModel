// csm_translate: convert a CSM camera model / state between file formats. The
// pivot is always a CSM model. See printUsage() below for the interface.

#include <UsgsAstroPlugin.h>
#include <RasterGM.h>
#include <UsgsAstroLsSensorModel.h>
#include <Utilities.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

void printUsage(const std::string &progName) {
  std::cout <<
"Usage: " << progName << " <from> <to> [options]\n"
"\n"
"Convert a CSM camera model/state between file formats. The input is loaded\n"
"into a CSM model and written back out in the output format. Formats are\n"
"inferred from the file extensions unless overridden.\n"
"\n"
"Formats (by extension):\n"
"  .json           CSM model state as JSON (a JSON ISD is also accepted as\n"
"                  input and constructed into a model state)\n"
"  .isd, .msgpack  CSM model state in binary MessagePack\n"
#ifdef USGSCSM_ENABLE_STARDS
"  .stards         CSM model state in the STARDS binary format\n"
#endif
"\n"
"Options:\n"
"  -i <fmt>        Override the input format (json, msgpack, stards).\n"
"  -o <fmt>        Override the output format (json, msgpack, stards).\n"
"  --set key=val   Set an advanced, format-specific option (repeatable).\n"
"                  STARDS output supports:\n"
"                    compression=<none|gzip|zstd|lz4|gzip-shuffle|lz4-shuffle>\n"
"                    block-size=<bytes>\n"
"  -h, --help      Show this help message and exit.\n"
"\n"
"Examples:\n"
"  " << progName << " image.json state.msgpack\n"
#ifdef USGSCSM_ENABLE_STARDS
"  " << progName << " state.json state.stards\n"
"  " << progName << " state.json state.stards --set compression=gzip-shuffle\n"
"  " << progName << " state.stards state.json\n"
#endif
;
}

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::string fileExtension(const std::string &path) {
  size_t slash = path.find_last_of("/\\");
  size_t dot = path.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
    return "";
  }
  return toLower(path.substr(dot + 1));
}

// Map a file extension to a format name. Empty string if unknown.
std::string formatFromExtension(const std::string &ext) {
  if (ext == "json") return "json";
  if (ext == "isd" || ext == "msgpack" || ext == "mp") return "msgpack";
#ifdef USGSCSM_ENABLE_STARDS
  if (ext == "stards") return "stards";
#endif
  return "";
}

bool isKnownFormat(const std::string &fmt) {
  return fmt == "json" || fmt == "msgpack"
#ifdef USGSCSM_ENABLE_STARDS
         || fmt == "stards"
#endif
      ;
}

// Returns nullptr on failure. inputFormat picks the reader; an ISD is still
// distinguished from a model state by content.
std::shared_ptr<csm::RasterGM> loadModel(const std::string &path,
                                         const std::string &inputFormat) {
  // Force libusgscsm's plugin to register (as usgscsm_cam_test does).
  UsgsAstroLsSensorModel lsModel;

#ifdef USGSCSM_ENABLE_STARDS
  if (inputFormat == "stards") {
    csm::Model *m = getUsgsCsmModelFromStards(path, NULL);
    return std::shared_ptr<csm::RasterGM>(dynamic_cast<csm::RasterGM *>(m));
  }
#endif

  if (inputFormat == "msgpack") {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      std::cerr << "Could not open input file: " << path << "\n";
      return nullptr;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
    const char *dataPtr = reinterpret_cast<const char *>(data.data());
    json j = json::from_msgpack(dataPtr, dataPtr + data.size());
    std::string modelName = j.at("m_modelName").get<std::string>();
    csm::Model *m = getUsgsCsmModelFromJsonState(j.dump(), modelName, NULL);
    return std::shared_ptr<csm::RasterGM>(dynamic_cast<csm::RasterGM *>(m));
  }

  // json: either a CSM model state or a JSON ISD (detected by content).
  std::string contents;
  if (!readFileInString(path, contents)) {
    std::cerr << "Could not read input file: " << path << "\n";
    return nullptr;
  }

  std::string modelName;
  UsgsAstroPlugin plugin;
  if (isUsgsCsmIsd(contents, modelName)) {
    csm::Isd isd(path);
    csm::Model *m = plugin.constructModelFromISD(isd, modelName, NULL);
    return std::shared_ptr<csm::RasterGM>(dynamic_cast<csm::RasterGM *>(m));
  }
  if (isUsgsCsmState(contents, modelName)) {
    csm::Model *m = plugin.constructModelFromState(contents, NULL);
    return std::shared_ptr<csm::RasterGM>(dynamic_cast<csm::RasterGM *>(m));
  }

  std::cerr << "Input file is not a recognized CSM ISD or model state: " << path
            << "\n";
  return nullptr;
}

// Write the model in the requested output format. Returns true on success.
bool writeModel(csm::RasterGM *model, const std::string &path,
                const std::string &outputFormat,
                const std::map<std::string, std::string> &advanced) {
  if (outputFormat == "json") {
    std::ofstream ofs(path);
    if (!ofs) {
      std::cerr << "Could not open output file: " << path << "\n";
      return false;
    }
    ofs << model->getModelState() << "\n";
    return true;
  }

  if (outputFormat == "msgpack") {
    json j = json::parse(getUsgsCsmModelJson(model));
    std::vector<uint8_t> bytes = json::to_msgpack(j);
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
      std::cerr << "Could not open output file: " << path << "\n";
      return false;
    }
    ofs.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    return true;
  }

#ifdef USGSCSM_ENABLE_STARDS
  if (outputFormat == "stards") {
    std::string compression = "lz4-shuffle";
    size_t blockSize = 1024 * 1024;
    size_t arrayThreshold = 100;
    for (const auto &kv : advanced) {
      if (kv.first == "compression") {
        compression = kv.second;
      } else if (kv.first == "block-size" || kv.first == "block_size") {
        blockSize = std::stoul(kv.second);
      } else if (kv.first == "array-threshold" || kv.first == "array_threshold") {
        arrayThreshold = std::stoul(kv.second);
      } else {
        std::cerr << "Unknown STARDS option '" << kv.first << "'\n";
        return false;
      }
    }
    writeUsgsCsmModelToStards(model, path, compression, blockSize, arrayThreshold);
    return true;
  }
#else
  (void)advanced;
#endif

  std::cerr << "Unsupported output format: " << outputFormat << "\n";
  return false;
}

}  // namespace

int main(int argc, char **argv) {
  std::string inputPath, outputPath;
  std::string inputFormatOverride, outputFormatOverride;
  std::map<std::string, std::string> advanced;
  std::vector<std::string> positionals;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else if (arg == "-i" || arg == "--input-format") {
      if (i + 1 >= argc) { std::cerr << arg << " requires a value\n"; return 1; }
      inputFormatOverride = toLower(argv[++i]);
    } else if (arg == "-o" || arg == "--output-format") {
      if (i + 1 >= argc) { std::cerr << arg << " requires a value\n"; return 1; }
      outputFormatOverride = toLower(argv[++i]);
    } else if (arg == "--set") {
      if (i + 1 >= argc) { std::cerr << "--set requires key=value\n"; return 1; }
      std::string kv = argv[++i];
      size_t eq = kv.find('=');
      if (eq == std::string::npos) {
        std::cerr << "--set expects key=value, got: " << kv << "\n";
        return 1;
      }
      advanced[toLower(kv.substr(0, eq))] = kv.substr(eq + 1);
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "Unknown option: " << arg << "\n";
      printUsage(argv[0]);
      return 1;
    } else {
      positionals.push_back(arg);
    }
  }

  if (positionals.size() != 2) {
    std::cerr << "Expected exactly two file arguments (from, to).\n";
    printUsage(argv[0]);
    return 1;
  }
  inputPath = positionals[0];
  outputPath = positionals[1];

  // Resolve input/output formats: override wins, else infer from extension.
  std::string inputFormat = inputFormatOverride.empty()
                                ? formatFromExtension(fileExtension(inputPath))
                                : inputFormatOverride;
  std::string outputFormat = outputFormatOverride.empty()
                                 ? formatFromExtension(fileExtension(outputPath))
                                 : outputFormatOverride;

  if (inputFormat.empty() || !isKnownFormat(inputFormat)) {
    std::cerr << "Could not determine input format for '" << inputPath
              << "'. Use -i to specify it.\n";
    return 1;
  }
  if (outputFormat.empty() || !isKnownFormat(outputFormat)) {
    std::cerr << "Could not determine output format for '" << outputPath
              << "'. Use -o to specify it.\n";
    return 1;
  }

  try {
    std::shared_ptr<csm::RasterGM> model = loadModel(inputPath, inputFormat);
    if (!model) {
      std::cerr << "Failed to load model from: " << inputPath << "\n";
      return 1;
    }

    if (!writeModel(model.get(), outputPath, outputFormat, advanced)) {
      std::cerr << "Failed to write: " << outputPath << "\n";
      return 1;
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  std::cout << "Converted " << inputPath << " (" << inputFormat << ") -> "
            << outputPath << " (" << outputFormat << ")\n";
  return 0;
}
