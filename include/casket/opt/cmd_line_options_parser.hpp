#pragma once

#include <algorithm>
#include <any>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <casket/nonstd/string_view.hpp>
#include <vector>

#include <casket/opt/option.hpp>
#include <casket/utils/noncopyable.hpp>

namespace casket::opt
{

/// @brief A class for parsing and storing command-line options.
class CmdLineOptionsParser final : NonCopyable
{
private:
    static constexpr nonstd::string_view kSinglePrefix = "-";
    static constexpr nonstd::string_view kDoublePrefix = "--";

    using OptionList = std::list<Option>;
    using OptionIterator = OptionList::iterator;

    /// @brief Trims leading dashes from an option name
    /// @param str The string to trim
    /// @return String without leading dashes
    static inline std::string trimDashes(const std::string& arg)
    {
        if (arg.size() > 2 && startsWith(arg, kDoublePrefix))
            return arg.substr(2);
        return arg;
    }

    /// @brief Checks if a string starts with a given prefix.
    /// @param str The string to check.
    /// @param prefix The prefix to look for.
    /// @return True if `str` starts with `prefix`, otherwise false.
    static inline bool startsWith(nonstd::string_view str, nonstd::string_view prefix)
    {
        return str.rfind(prefix, 0) == 0;
    }

public:
    /// @brief Default constructor.
    CmdLineOptionsParser() = default;

    /// @brief Default destructor.
    ~CmdLineOptionsParser() = default;

    /// @brief Adds a new option to the command line parser.
    /// @param option The option.
    void add(Option&& option)
    {
        auto name = option.getName();
        options_.push_back(std::move(option));
        optionMap_.insert_or_assign(std::move(name), std::prev(options_.end()));
    }

    /// @brief Parses command-line arguments.
    /// @param argc The count of command-line arguments.
    /// @param argv The command-line arguments as a C-style array of strings.
    void parse(int argc, char* argv[])
    {
        std::vector<nonstd::string_view> args{(argc > 1 ? argv + 1 : argv), argv + argc};
        auto allocatedArgs = preprocess(args);
        postprocess(allocatedArgs);
    }

    /// @brief Parses the command-line arguments.
    /// @param argc The argument count.
    /// @param argv The argument values.
    void parse(const std::vector<nonstd::string_view>& args)
    {
        auto allocatedArgs = preprocess(args);
        postprocess(allocatedArgs);
    }

    /// @brief Validate option values for each option:
    /// - set default value if any
    /// - check if option is mandatory
    /// - set custom value
    void validate()
    {
        for (auto& option : options_)
        {
            option.validate();
        }
    }

    /// @brief Retrieves the stored value of an option.
    /// @tparam T The type to cast the stored value to (default: std::string).
    /// @param name The name of the option to retrieve.
    /// @return The stored value casted to type T.
    /// @throws std::logic_error if parsing has not been performed.
    /// @throws std::runtime_error if no such option exists.
    template <typename T = std::string>
    T get(const std::string& name) const
    {
        if (!parsed_)
        {
            throw std::logic_error("Attempt to get value before parsing");
        }
        return (*this)[name].get<T>();
    }

    /// @brief Checks if a value is present for an option and retrieves it.
    /// @tparam T The type to cast the stored value to (default: std::string).
    /// @param name The name of the option to check.
    /// @return An optional containing the casted value or std::nullopt.
    template <typename T = std::string>
    std::optional<T> present(const std::string& name) const
    {
        return (*this)[name].present<T>();
    }

    /// @brief Checks if an option was used during parsing.
    /// @param name The name of the option to check.
    /// @return True if the option was used, otherwise false.
    bool isUsed(const std::string& name) const
    {
        return (*this)[name].isUsed();
    }

    /// @brief Outputs help information for the parser.
    /// @param os The output stream to write the help information to.
    /// @param usageName An optional program usage string.
    void help(std::ostream& os, nonstd::string_view usageName = "") const
    {
        usage(os, usageName);

        os << "Allowed options:\n";

        for (auto option : options_)
        {
            std::stringstream ss;

            ss << "  " << kDoublePrefix << option.getName();

            if (option.minTokens() > 0)
            {
                ss << " arg";
            }

            auto optionInfo = ss.str();
            os << optionInfo;
            for (unsigned pad = 25 - static_cast<unsigned>(optionInfo.size()); pad > 0; --pad)
            {
                os.put(' ');
            }

            os << option.getDescription() << "\n";
        }

        os << std::endl;
    }

private:
    /// @brief Retrieves the option object associated with a given name.
    /// @param name The name of the option to retrieve.
    /// @return A reference to the option.
    /// @throws std::runtime_error if the option does not exist.
    Option& operator[](const std::string& name) const
    {
        auto it = optionMap_.find(name);
        if (it == optionMap_.end())
        {
            throw RuntimeError("No such option '{}'", name);
        }
        return *(it->second);
    }

    /// @brief Preprocesses command-line arguments for parsing.
    /// @tparam T The type of the elements in the input arguments (default:
    /// nonstd::string_view).
    /// @param args The list of arguments to preprocess.
    /// @return A vector of preprocessed argument strings.
    template <typename T = nonstd::string_view>
    std::vector<std::string> preprocess(const std::vector<T>& args)
    {
        std::vector<std::string> result;
        auto begin = std::begin(args);
        auto end = std::end(args);

        for (auto arg = begin; arg != end; arg = std::next(arg))
        {
            if (arg->size() > 2 && startsWith(*arg, kDoublePrefix))
            {
                auto assignPosition = arg->find_first_of('=');
                if (assignPosition != std::string::npos)
                {
                    result.push_back(std::string(arg->substr(0, assignPosition)));
                    result.push_back(std::string(arg->substr(assignPosition + 1)));
                    continue;
                }
            }
            result.push_back(std::string(*arg));
        }
        return result;
    }

    /// @brief Processes preprocessed arguments to consume.
    /// @param args The list of preprocessed arguments to postprocess.
    /// @throws std::runtime_error if an unknown option is encountered.
    void postprocess(const std::vector<std::string>& args)
    {
        auto end = std::end(args);
        for (auto arg = std::begin(args); arg != end;)
        {
            auto optionName = trimDashes(*arg);
            auto foundOption = optionMap_.find(optionName);
            if (foundOption != optionMap_.end())
            {
                auto nextArg = std::next(arg);
                auto& option = foundOption->second;

                std::vector<std::string> values;
                while (nextArg != end && values.size() < option->maxTokens() && !startsWith(*nextArg, "--"))
                {
                    values.push_back(*nextArg);
                    ++nextArg;
                }

                if (values.size() < option->minTokens())
                {
                    throw RuntimeError("Option '{}' requires at least {} values", optionName,
                                              option->minTokens());
                }

                if (option->maxTokens() > 0 && values.size() > option->maxTokens())
                {
                    throw RuntimeError("Option '{}' accepts at most {} values", optionName, option->maxTokens());
                }

                option->consume(values);
                arg = nextArg;
            }
            else
            {
                throw RuntimeError("Unknown option '{}'", optionName);
            }
        }
        parsed_ = true;
    }

    /// @brief Outputs program usage information to a stream.
    /// @param os The output stream to write the usage information to.
    /// @param usageName An optional program usage string.
    void usage(std::ostream& os, nonstd::string_view usageName) const
    {
        os << "Usage:\n  " << usageName;

        for (auto option : options_)
        {
            os << " ";

            if (!option.isRequired())
            {
                os << "[ ";
            }

            os << kDoublePrefix << option.getName();

            if (option.minTokens() > 0)
            {
                os << " arg";
            }

            if (!option.isRequired())
            {
                os << " ]";
            }
        }
        os << "\n\n";
    }

private:
    OptionList options_;                              ///< A list of defined options.
    std::map<std::string, OptionIterator> optionMap_; ///< A map from option names to their iterators.
    bool parsed_{false};                              ///< Flag indicating whether parsing has been performed.
};

} // namespace casket::opt