#include <iostream>
#include <fstream>
#include <vector>
#include <initializer_list>
#include <memory>
#include <string>
#include <regex>

#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

typedef std::vector<std::string> StringList;

typedef struct {
  int time;
  StringList contents;
} Snapshot;

class MassifFile {
public:
  StringList headers;
  std::vector<std::shared_ptr<Snapshot>> snapshots;

public:
  MassifFile() {
  }

  MassifFile(std::initializer_list<std::string> paths) {
    add(paths);
  }

  /**
   * @brief Add a massif files to this class
   * 
   * @param paths path to massif files
   * @return int 0 if success, or fails
   */
  int add(std::initializer_list<std::string> &paths) {
    return add(paths.begin(), paths.end());
  }

  /**
   * @brief Add a massif files to this class
   * 
   * @param paths path to massif files
   * @return int 0 if success, or fails
   */
  int add(std::vector<std::string> &paths) {
    return add(paths.begin(), paths.end());
  }

  template <class It>
  int add(It first, It last) {
    int ret = 0, r;
    for (auto it = first; it != last; ++it) {
      if ((r = add(*it)) != 0) {
        ret = r;
      };
    }

    return ret;
  }

  /**
   * @brief Add a massif file to this class
   * 
   * @param path path to massif file
   * @return int 0 if success, or fails
   */
  int add(const std::string path) {
    return appendFile(path, headers.size() > 0);
  }

  /**
   * @brief Write whole content to a new massif file
   * 
   * @param path new massif file path
   * @return int 0 if success, or fails
   */
  int write(const std::string path) {
    if (headers.size() <= 0 && snapshots.size() <=0) {
      std::cerr << "WARN: No content, exit" << std::endl;
      return -1;
    }

    // Sort snapshots
    std::sort(snapshots.begin(), snapshots.end(), 
          [](auto &a, auto &b) -> bool {
            return a->time < b->time; 
    });

    std::ofstream file(path);
    if (!file) return 1; // Error open file

    // Write header
    writeList(file, headers);
    if (!file) return 2; // Error write file

    // Write snapshot
    for (int i = 0; i < snapshots.size(); i++) {
      writeSnapshot(file, i, snapshots[i]);
      if (!file) return 2; // Error write file
    }
    file.close();
    if (!file) return 3; // Error close file
    return 0;
  }

private:
  // Write a string list to stream
  void writeList(std::ofstream &stream, StringList &list) {
    std::ostream_iterator<std::string> output_iterator(stream, "\n");
    std::copy(std::begin(list), std::end(list), output_iterator);
  }

  // Write snapshot header and content
  void writeSnapshot(std::ofstream &stream, int index, std::shared_ptr<Snapshot> &snapshot) {
    std::vector<std::string> titles = {"#-----------",
                                       "snapshot=" + std::to_string(index),
                                       "#-----------"};
    writeList(stream, titles);
    writeList(stream, snapshot->contents);
  }

  // Read massif output file and append snapshot
  int appendFile(std::string path, bool ignore_header = true) {
    typedef enum {
      HEADER,
      SNAPSHOT_MARK,
      SNAPSHOT_NAME,
      SNAPSHOT_CONTENT,
      NONE,
    } LastLine;

    // const variable
    std::regex const keyword_regex = std::regex("^(desc|cmd|time_unit):");
    std::string const snapshot_mark = "#-----------";
    std::regex const snapshot_regex = std::regex("snapshot=\\d+");
    std::regex const snapshot_time_regex = std::regex("time=(\\d+)");

    std::ifstream file(path);
    std::string str;
    LastLine status = LastLine::NONE;

    if (!file) return 1; // error open file

    std::smatch match;
    std::shared_ptr<Snapshot> snapshot = nullptr;
    while (std::getline(file, str)) {
      if (std::regex_search(str, keyword_regex)) {
        status = LastLine::HEADER;
        if (!ignore_header) {
          this->headers.push_back(str);
        }

        continue;
      }

      if ((status == LastLine::HEADER || status == LastLine::SNAPSHOT_CONTENT) && str == snapshot_mark) {
        status = LastLine::SNAPSHOT_MARK;
        if (snapshot != nullptr && snapshot->contents.size() > 0) {
          this->snapshots.push_back(snapshot);
        }
        snapshot = nullptr;
        continue;
      }

      if (status == LastLine::SNAPSHOT_MARK && std::regex_search(str, snapshot_regex)) {
        status = LastLine::SNAPSHOT_NAME;
        continue;
      }

      if (status == LastLine::SNAPSHOT_NAME && str == snapshot_mark) {
        status = LastLine::SNAPSHOT_CONTENT;
        if (snapshot == nullptr) {
          snapshot = std::make_shared<Snapshot>();
        } else {
          std::cerr << "WARN: found new snapshot but existing another snapshot" << std::endl;
          return 2; // error when handling file
        }
        continue;
      }

      if (status == LastLine::SNAPSHOT_CONTENT) {
        if (snapshot == nullptr) {
          std::cerr << "WARN: snapshot should not empty now" << std::endl;
          return 2; // error when handling file
        } else {
          snapshot->contents.push_back(str);
          if (std::regex_search(str, match, snapshot_time_regex)) {
            snapshot->time = std::stoi(match[1]);
          }
        }
      }
    }

    if (snapshot != nullptr && snapshot->contents.size() > 0) {
      this->snapshots.push_back(snapshot);
    }

    return 0;
  }
};

void usage(const char *app) {
  std::cout << "Usage: %s [-o output] [-d] [-v] <file-pattern>..." << std::endl;
  std::cout << "\t\t-o output: specify output file path" << std::endl;
  std::cout << "\t\t-d: after combining, delete input files" << std::endl;
  std::cout << "\t\t-v: verbose processing" << std::endl;
  std::cout << "\t\tfile-pattern: input file list, can include * character" << std::endl;
}

bool fileExists(const std::string file) {
  struct stat buf;
  return (stat(file.c_str(), &buf) == 0);
}

bool deleteFiles(std::vector<std::string>& files, bool verbose = false) {
  bool ret = true;
  for (auto& file : files) {
    if (verbose) {
      std::cout << "Deleting file " << file << std::endl;
    }
    // Delete file
    if (remove(file.c_str()) < 0) {
      std::cerr << "[" << errno << "]" << "Error removing file " << file << std::endl;
      ret = false;
    }
  }

  return ret;
}

int listFile(std::vector<std::string>& files, std::string pattern) {
  char buffer[PATH_MAX];
  snprintf(buffer, PATH_MAX, "ls -1 %s", pattern.c_str());

  int len;
  FILE* pipe = popen(buffer, "r");
  if (!pipe) return 1; // exec fail
  try {
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
      len = strlen(buffer);
      if (len >= 1 && buffer[len-1] == '\n') {
        if (len >= 2 && buffer[len-2] == '\r') {
          buffer[len-2] = '\0';
        } else {
          buffer[len-1] = '\0';
        }
      }
      if (fileExists(buffer)) {
        files.push_back(buffer);
      }
    }
  } catch (...) {
    pclose(pipe);
    throw;
  }
  pclose(pipe);
  return 0;
}

#define DEFAULT_OUTPUTNAME  "massif.out.combine"
class InputArgs {
public:
  bool deleteSuccess;
  bool verbose;
  std::string outputFile;
  std::vector<std::string> inputFiles;

public:
  InputArgs() : 
    deleteSuccess(false),
    verbose(false),
    outputFile(DEFAULT_OUTPUTNAME) {
  }

  InputArgs(int argc, char * const* argv): InputArgs() {
    parse(argc, argv);
  }

  ~InputArgs() {}

  void parse(int argc, char * const* argv) {
    // Retrieve the options:
    int opt;
    while ((opt = getopt(argc, argv, "vdo:")) != -1) {
      // for each option...
      switch (opt) {
      case 'v':
        verbose = true;
        break;
      case 'o':
        outputFile = optarg;
        break;
      case 'd':
        deleteSuccess = true;
        break;
      default: // unknown option...
        break;
      }
    }

    for (int i = optind; i < argc; i++) {
      if (fileExists(argv[i])) {
        inputFiles.push_back(argv[i]);
      } else {
        std::vector<std::string> files;
        if (listFile(files, argv[i]) == 0 && files.size() > 0) {
          inputFiles.insert(inputFiles.end(), files.begin(), files.end());
        }
      }
    }
  }
};

int main(int argc, char * const* argv) {
  if (argc <= 1) {
    usage(argv[0]);
    return -1;
  }

  InputArgs args(argc, argv);
  MassifFile massifFile;
  for (auto& file : args.inputFiles) {
    massifFile.add(file);
    if (args.verbose) {
      std::cout << "Input: " << file;
      std::cout << "  Size: " << massifFile.snapshots.size() << std::endl;
    }
  }

  if (massifFile.write(args.outputFile) == 0 && args.deleteSuccess) {
    // Delete input file
    deleteFiles(args.inputFiles, args.verbose);
  }

  return 0;
}