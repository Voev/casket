#include "generator.hpp"
#include "gen-cpp.hpp"
#include "objectmodel.hpp"

#include <casket/utils/string.hpp>

#include <iostream>

using namespace gen;

using std::vector;
using std::string;

const char* help =
    "Usage: gen [options] <my-database.xml>\n\n"
    "Options:\n"
    " -t, --target=TARGET                generate code for TARGET (default: "
    "c++)\n"
    " -v, --verbose                      verbosely report code generation\n"
    " --help                             print help\n"

    " -t, --target=TARGET                generate code for TARGET (default: "
    "c++)\n"
    " --output-dir=/path/to/src          output all files to directory \n"
    " --output-sources=/path/to/src      output sources to directory \n"
    " --output-include=/path/to/include  output includes to directory\n"
    " --refresh                          refresh code of target\n"
    " --overwrite                        overwrite code on generation\n"
    "\n"
    "Supported targets:\n"
    "  'c++'        C++ target (.cpp,.hpp)\n"
    "\n\n";

struct options_t
{
    string output_dir;
    string output_sources;
    string output_includes;
    bool refresh;
    bool printHelp;
    vector<string> targets;
};

options_t options = {"", "", "", true, false, vector<string>()};

static int parseArgs(int argc, char** argv)
{
    if (argc == 1)
        return -1;

    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];
        if (arg == "-v" || arg == "--verbose")
        {
            continue;
        }
        else if (arg == "-t" || arg == "--target")
        {
            if (i + 1 >= argc)
            {
                std::cout << "Error: missing target" << std::endl;
                return -1;
            }
            options.targets.push_back(argv[i + 1]);
            i++;
            continue;
        }
        else if (arg.find_first_of("--target=") == 0)
        {
            vector<string> lang = casket::utils::split(arg, "=");
            options.targets.push_back(lang[1]);
            continue;
        }
        else if (arg.find_first_of("--output-dir") == 0)
        {
            vector<string> lang = casket::utils::split(arg, "=");
            options.output_dir = lang[1];
            continue;
        }
        else if (arg.find_first_of("--output-sources") == 0)
        {
            vector<string> lang = casket::utils::split(arg, "=");
            options.output_sources = lang[1];
            continue;
        }
        else if (arg.find_first_of("--output-include") == 0)
        {
            vector<string> lang = casket::utils::split(arg, "=");
            options.output_includes = lang[1];
            continue;
        }
        else if (arg == "--help")
        {
            options.printHelp = true;
            continue;
        }
        else if (i < argc - 1)
        {
            std::cout << "Error: invalid argument " << arg << std::endl;
            return -1;
        }
    }
    return 0;
}

static int generateCode(ObjectModel& model)
{
    CompositeGenerator generator;

    generator.setOutputDirectory(options.output_dir);

    for (vector<string>::const_iterator target = options.targets.begin();
         target != options.targets.end(); target++)
    {
        CodeGenerator* pGen = CodeGenerator::create(target->c_str());
        if (!pGen)
        {
            throw std::runtime_error("unsupported target: " + *target);
        }
        else
        {
            generator.add(pGen);

            // special case for c++
            CppGenerator* pCppGen = dynamic_cast<CppGenerator*>(pGen);
            if (pCppGen)
            {
                pCppGen->setOutputSourcesDirectory(options.output_sources);
                pCppGen->setOutputIncludesDirectory(options.output_includes);
            }
        }
    }

    return generator.generateCode(&model) ? 0 : 1;
}

int main(int argc, char** argv)
{

    int rc = parseArgs(argc, argv);
    if (rc != 0)
    {
        std::cout << help << std::endl;
        return -1;
    }

    if (options.printHelp)
    {
        std::cout << help << std::endl;
    }

    ObjectModel model;
    try
    {
        if (!model.loadFromFile(argv[argc - 1]))
        {
            string msg = "could not load file '" + string(argv[argc - 1]) + "'";
            std::cout << msg << std::endl;
            return -1;
        }
        else
        {
            return generateCode(model);
        }
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return -1;
    }
}
