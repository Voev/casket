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
#include <string_view>
#include <vector>

namespace casket::opt
{

namespace detail
{

/// @brief Abstract base class representing the value semantics of command-line
/// options.
class ValueSemantic
{
public:
    /// @brief Virtual desctructor.
    virtual ~ValueSemantic() = default;

    /// @brief Parses a string into a value.
    /// @param text The string to parse
    /// @param value The resulting parsed value
    virtual void parse(std::any& value, const std::vector<std::string_view>& args) = 0;

    /// @brief Notification callback for when the value is set.
    /// @param valueStore The set value
    virtual void notify(const std::any& valueStore) const = 0;

    /// @brief Gets the minimum number of tokens for this value.
    /// @return The minimum number of tokens
    virtual std::size_t minTokens() const = 0;

    /// @brief Gets the maximum number of tokens for this value.
    /// @return The maximum number of tokens
    virtual std::size_t maxTokens() const = 0;
};

/// @brief Untyped value semantic.
///
/// This class provides a no-op implementation of `ValueSemantic`, meaning it
/// doesn't parse or notify of any values. It effectively allows options to be
/// specified without any associated value.
class UntypedValue : public ValueSemantic
{
public:
    /// @brief No-op parse implementation.
    ///
    /// This implementation does nothing as the value is untyped and doesn't
    /// require any parsing.
    ///
    /// @param value Reference to `std::any` where the result would normally be
    /// stored.
    /// @param args List of string arguments intended for parsing.
    void parse(std::any&, const std::vector<std::string_view>&) override
    {
    }

    /// @brief No-op notify implementation.
    ///
    /// This method does nothing since there is no actual value to notify about.
    ///
    /// @param valueStore The value that was supposedly set, unused.
    void notify(const std::any&) const override
    {
    }

    /// @brief Returns 0 as the minimum number of tokens.
    ///
    /// Since the value is untyped, no tokens are required to represent its
    /// value.
    ///
    /// @return Always returns 0.
    std::size_t minTokens() const override
    {
        return 0U;
    }

    /// @brief Returns 0 as the maximum number of tokens.
    ///
    /// As with minimum tokens, no tokens can or need to be consumed for
    /// representing this value.
    ///
    /// @return Always returns 0.
    std::size_t maxTokens() const override
    {
        return 0U;
    }
};

/// @brief A typed value semantic for command-line options.
///
/// This class represents a typed value associated with a command-line option.
/// It provides functionality to parse strings into a specific type `T` and
/// manage notifications when such values are set.
template <typename T>
class TypedValue : public ValueSemantic
{
public:
    /// @brief Constructor that binds the typed value to a pointer.
    ///
    /// @param typedPtr A pointer to the storage where the parsed value will
    ///                 be stored.
    TypedValue(T* typedPtr)
        : typedPtr_(typedPtr)
    {
    }

    /// @brief Parses a string argument into a value of type `T`.
    ///
    /// This method attempts to convert the first string argument into a value
    /// of type `T`. If successful, it stores the result in an `std::any`
    /// object. Throws an exception if parsing fails.
    ///
    /// @param value    The `std::any` object where the parsed value is stored.
    /// @param args     A list of string arguments; the first argument is
    /// parsed.
    ///
    /// @throws std::runtime_error If the string cannot be parsed into type `T`.
    void parse(std::any& value, const std::vector<std::string_view>& args) override
    {
        auto iss = std::istringstream{std::string(args.front())};
        T typedValue{};

        if (iss >> typedValue)
        {
            value = std::move(typedValue);
        }
        else
        {
            throw std::runtime_error("could not parse value: " + std::string(args.front()));
        }
    }

    /// @brief Notification callback for when the value is set.
    ///
    /// This method ensures that if a value is successfully parsed, it is
    /// stored in the provided pointer to hold the typed value. If the pointer
    /// is null or the value is of a wrong type, no action is taken.
    ///
    /// @param valueStore The `std::any` containing the parsed value of type
    /// `T`.
    void notify(const std::any& valueStore) const override
    {
        const T* value = std::any_cast<T>(&valueStore);
        if (typedPtr_ && value)
        {
            *typedPtr_ = *value;
        }
    }

    /// @brief Returns the minimum number of tokens required for parsing.
    ///
    /// For `TypedValue`, one token is always required as an input.
    ///
    /// @return Always returns 1.
    std::size_t minTokens() const override
    {
        return 1U;
    }

    /// @brief Returns the maximum number of tokens allowed for parsing.
    ///
    /// For `TypedValue`, one token is the maximum allowed since it parses
    /// a single argument into type `T`.
    ///
    /// @return Always returns 1.
    std::size_t maxTokens() const override
    {
        return 1U;
    }

private:
    T* typedPtr_; ///< Pointer to store the parsed value.
};

} // namespace detail

template <class T>
std::shared_ptr<detail::TypedValue<T>> Value()
{
    return std::make_shared<detail::TypedValue<T>>(nullptr);
}

template <class T>
std::shared_ptr<detail::TypedValue<T>> Value(T* v)
{
    return std::make_shared<detail::TypedValue<T>>(v);
}

/// @brief Forward declaration for option parser.
class OptionParser;

/// @brief A class for handling command-line options.
///
/// This class represents a single command-line option. It manages the option's
/// name(s), description, semantics, and provides mechanisms for parsing and
/// validating the option's usage.
class Option
{
    // Friend class that has special access to Option's internals
    friend class OptionParser;

public:
    /// @brief Constructs an option with given names and description.
    ///
    /// This constructor initializes an option with a description but without a
    /// specific semantic for its value, using a default 'untyped' semantic.
    ///
    /// @param names A comma-separated list of names for the option (e.g., "n,
    /// name").
    /// @param description A description of what the option does.
    Option(std::string&& names, std::string&& description)
        : description_(std::move(description))
        , valueSemantic_(std::make_shared<detail::UntypedValue>())
        , isRequired_(false)
        , isUsed_(false)
    {
        setNames(names);
    }

    /// @brief Constructs an option with given names, semantic, and description.
    ///
    /// Initializes the option with a specified value semantic, allowing it to
    /// handle specific types of values alongside a description.
    ///
    /// @param names A comma-separated list of names for the option (e.g., "n,
    /// name").
    /// @param valueSemantic A shared pointer to an object that defines how to
    /// parse the option's value.
    /// @param description A description of what the option does.
    Option(std::string&& names, std::shared_ptr<detail::ValueSemantic> valueSemantic, std::string&& description)
        : description_(std::move(description))
        , valueSemantic_(valueSemantic)
        , isRequired_(false)
        , isUsed_(false)
    {
        setNames(names);
    }

    /// @brief Marks the option as required.
    ///
    /// Sets the internal flag indicating that this option must be specified
    /// when parsing arguments, otherwise validation will fail.
    void required()
    {
        isRequired_ = true;
    }

    /// @brief Sets the primary and alias names for the option.
    ///
    /// Parses a string of comma-separated names and sets the primary and alias
    /// names for the option. Throws an error if names are malformed.
    ///
    /// @param names A string containing the name (can be with alias separated by comma).
    void setNames(const std::string& names)
    {
        if (names.empty())
        {
            throw std::runtime_error("Name is empty");
        }

        auto end = names.find_first_of(',');
        if (end == std::string::npos)
        {
            baseName_ = names;
        }
        else
        {
            auto start = names.find_first_not_of(' ', end + 1);
            if (start == std::string::npos)
            {
                throw std::runtime_error("Invalid option name");
            }
            baseName_ = names.substr(0, end);
            aliasName_ = names.substr(start);
        }
    }

    /// @brief Consumes command-line arguments associated with this option.
    ///
    /// Checks that the correct number of tokens is provided and then parses
    /// the option's arguments. Throws exceptions if tokens are invalid.
    ///
    /// @tparam Iterator Type of the iterator for the token range.
    /// @param start Iterator pointing to the beginning of the token range.
    /// @param end Iterator pointing to the end of the token range.
    ///
    /// @throws std::runtime_error if the option is already used or arguments
    /// are invalid.
    template <typename Iterator>
    void consume(Iterator start, Iterator end)
    {
        if (isUsed_)
        {
            throw std::runtime_error("Duplicated option");
        }
        isUsed_ = true;

        std::size_t distance = std::distance(start, end);

        if (distance < valueSemantic_->minTokens())
        {
            throw std::runtime_error("Not enough arguments");
        }

        if (distance > valueSemantic_->maxTokens())
        {
            throw std::runtime_error("Too many arguments");
        }

        if (distance > 0)
        {
            std::vector<std::string_view> range{start, end};
            valueSemantic_->parse(value_, range);
        }
        else
        {
            valueSemantic_->parse(value_, {});
        }
    }

private:
    /// @brief Retrieves the stored value casted to type T.
    ///
    /// Assumes a value of type T was parsed and stored. Throws an exception
    /// if no value is available or incorrect type is requested.
    ///
    /// @tparam T The type to cast the stored value to.
    /// @return The stored value casted to type T.
    ///
    /// @throws std::runtime_error if no value is stored or type T is incorrect.
    template <typename T>
    T get() const
    {
        if (!value_.has_value())
        {
            throw std::runtime_error("No value provided for '" + baseName_ + "'");
        }
        return std::any_cast<T>(value_);
    }

    /// @brief Checks if a value is present and casts it to type T.
    ///
    /// Returns an `std::optional` containing the value casted to type T if
    /// available, otherwise returns `std::nullopt`.
    ///
    /// @tparam T The type to cast the stored value to.
    /// @return An optional containing the casted value or `std::nullopt`.
    template <typename T>
    std::optional<T> present() const
    {
        if (!value_.has_value())
        {
            return std::nullopt;
        }
        return std::any_cast<T>(value_);
    }

    /// @brief Validates the option based on its requirements and usage.
    ///
    /// Throws exceptions if the option is required but not used or if a value
    /// is expected but missing. Calls the notify method on the value semantics
    /// when a valid value is present.
    ///
    /// @throws std::runtime_error if validation fails.
    void validate() const
    {
        if (!isUsed_ && isRequired_)
        {
            throw std::runtime_error("'" + baseName_ + "' must be specified");
        }
        if (isUsed_ && isRequired_ && !value_.has_value())
        {
            throw std::runtime_error("No value for '" + baseName_ + "' option");
        }
        if (valueSemantic_)
        {
            valueSemantic_->notify(value_);
        }
    }

private:
    std::string baseName_;                                 ///< The primary name for the option.
    std::optional<std::string> aliasName_;                 ///< An alias name for the option, if any.
    std::string description_;                              ///< A description of the option's functionality.
    std::any value_;                                       ///< The parsed value of the option.
    std::shared_ptr<detail::ValueSemantic> valueSemantic_; ///< The semantic handler for the option's value.
    bool isRequired_;                                      ///< Indicates whether the option is required.
    bool isUsed_;                                          ///< Indicates whether the option has been used.
};

/// @brief A class for parsing and storing command-line options.
///
/// The `OptionParser` class manages a collection of command-line options,
/// allowing for the addition of new options, parsing of command-line arguments,
/// and retrieval of option values after parsing.
class OptionParser
{
private:
    static constexpr std::string_view kSinglePrefix = "-";
    static constexpr std::string_view kDoublePrefix = "--";

    using OptionList = std::list<Option>;
    using OptionIterator = OptionList::iterator;

    /// @brief Trims leading dashes from an option name
    /// @param str The string to trim
    /// @return String without leading dashes
    static inline std::string_view trimDashes(std::string_view arg)
    {
        if (arg.size() > 2 && startsWith(arg, kDoublePrefix))
            return arg.substr(2);
        if (arg.size() > 1 && startsWith(arg, kSinglePrefix))
            return arg.substr(1);
        return arg;
    }

    /// @brief Checks if a string starts with a given prefix.
    ///
    /// @param str The string to check.
    /// @param prefix The prefix to look for.
    /// @return True if `str` starts with `prefix`, otherwise false.
    static inline bool startsWith(std::string_view str, std::string_view prefix)
    {
        return str.rfind(prefix, 0) == 0;
    }

public:
    /// @brief Default constructor for OptionParser.
    OptionParser() = default;

    /// @brief Default destructor for OptionParser.
    ~OptionParser() = default;

    // Disable copy operations to prevent unintended copy behavior
    OptionParser(const OptionParser& other) = delete;
    OptionParser& operator=(const OptionParser& other) = delete;

    // Disable move operations
    OptionParser(OptionParser&& other) = delete;
    OptionParser& operator=(OptionParser&& other) = delete;

    /// @brief Adds a new option to the parser with a description and typed
    /// value.
    ///
    /// This template method allows you to add an option with a specific typed
    /// value semantic.
    ///
    /// @tparam T The type of the option's value.
    /// @param names The name(s) of the option.
    /// @param value The semantic parser for the option value.
    /// @param description A description of what the option does.
    /// @return A reference to the added option.
    template <typename T>
    Option& add(std::string names, std::shared_ptr<detail::TypedValue<T>> value, std::string description)
    {
        auto it = options_.emplace(std::cend(options_), std::move(names), value, std::move(description));
        setOptions(it);
        return *it;
    }

    /// @brief Adds a new option to the parser with a description.
    ///
    /// Adds an option without value semantic, useful for flags or simple
    /// options.
    ///
    /// @param names The name(s) of the option.
    /// @param description A description of what the option does.
    /// @return A reference to the added option.
    Option& add(std::string names, std::string description)
    {
        auto it = options_.emplace(std::cend(options_), std::move(names), std::move(description));
        setOptions(it);
        return *it;
    }

    /// @brief Parses command-line arguments.
    ///
    /// This method processes command-line arguments passed as `argc` and
    /// `argv`, preparing them for option parsing.
    ///
    /// @param argc The count of command-line arguments.
    /// @param argv The command-line arguments as a C-style array of strings.
    void parse(int argc, char* argv[])
    {
        std::vector<std::string_view> args{(argc > 1 ? argv + 1 : argv), argv + argc};
        auto allocatedArgs = preprocess(args);
        postprocess(allocatedArgs);
    }

    /// @brief Parses the command-line arguments.
    ///
    /// Takes raw command-line arguments and processes them to extract options
    /// and their values.
    ///
    /// @param argc The argument count.
    /// @param argv The argument values.
    void parse(const std::vector<std::string_view>& args)
    {
        auto allocatedArgs = preprocess(args);
        postprocess(allocatedArgs);
    }

    /// @brief Retrieves the stored value of an option.
    ///
    /// Gets the value associated with a given option name after parsing.
    ///
    /// @tparam T The type to cast the stored value to (default: std::string).
    /// @param name The name of the option to retrieve.
    /// @return The stored value casted to type T.
    ///
    /// @throws std::logic_error if parsing has not been performed.
    /// @throws std::runtime_error if no such option exists.
    template <typename T = std::string>
    T get(std::string_view name) const
    {
        if (!parsed_)
        {
            throw std::logic_error("Attempt to get value before parsing");
        }
        return (*this)[name].get<T>();
    }

    /// @brief Checks if a value is present for an option and retrieves it.
    ///
    /// Returns an optional containing the value casted to type T if available.
    ///
    /// @tparam T The type to cast the stored value to (default: std::string).
    /// @param name The name of the option to check.
    /// @return An optional containing the casted value or std::nullopt.
    template <typename T = std::string>
    std::optional<T> present(std::string_view name) const
    {
        return (*this)[name].present<T>();
    }

    /// @brief Checks if an option was used during parsing.
    ///
    /// Determines if a command-line option was provided by the user.
    ///
    /// @param name The name of the option to check.
    /// @return True if the option was used, otherwise false.
    bool isUsed(std::string_view name) const
    {
        return (*this)[name].isUsed_;
    }

    /// @brief Outputs help information for the parser.
    ///
    /// Generates a help string for defined options and outputs it to a stream.
    ///
    /// @param os The output stream to write the help information to.
    /// @param usageName An optional program usage string.
    void help(std::ostream& os, std::string_view usageName = "") const
    {
        usage(os, usageName);

        os << "Allowed options:\n";

        for (auto option : options_)
        {
            std::stringstream ss;

            ss << "  " << kDoublePrefix << option.baseName_;
            if (option.aliasName_.has_value())
            {
                ss << " [ " << kSinglePrefix << option.aliasName_.value() << " ]";
            }

            if (option.valueSemantic_->minTokens() > 0U)
            {
                ss << " arg";
            }

            auto optionInfo = ss.str();
            os << optionInfo;
            for (unsigned pad = 25 - static_cast<unsigned>(optionInfo.size()); pad > 0; --pad)
            {
                os.put(' ');
            }

            os << option.description_ << "\n";
        }
    }

private:
    /// @brief Associates option names with their iterator in the options list.
    /// Maintains a map for fast retrieval of options by their name or alias.
    ///
    /// @param it An iterator to the option in the options list.
    inline void setOptions(const OptionIterator& it)
    {
        optionMap_.insert_or_assign(it->baseName_, it);
        if (it->aliasName_.has_value())
        {
            optionMap_.insert_or_assign(it->aliasName_.value(), it);
        }
    }

    /// @brief Retrieves the option object associated with a given name.
    ///
    /// Searches for an option by its name and returns a reference to it.
    ///
    /// @param name The name of the option to retrieve.
    /// @return A reference to the option.
    ///
    /// @throws std::runtime_error if the option does not exist.
    Option& operator[](std::string_view name) const
    {
        auto it = optionMap_.find(name);
        if (it == optionMap_.end())
        {
            throw std::runtime_error("No such option: " + std::string(name));
        }
        return *(it->second);
    }

    /// @brief Preprocesses command-line arguments for parsing.
    ///
    /// Splits arguments containing assignments and prepares them for parsing.
    ///
    /// @tparam T The type of the elements in the input arguments (default:
    /// std::string_view).
    /// @param args The list of arguments to preprocess.
    /// @return A vector of preprocessed argument strings.
    template <typename T = std::string_view>
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

    /// @brief Processes preprocessed arguments to consume and validate them.
    ///
    /// Consumes arguments, matches them with defined options, and validates
    /// their usage.
    ///
    /// @param args The list of preprocessed arguments to postprocess.
    ///
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
                auto nextOption = std::find_if(std::next(arg), end,
                                               [](auto& x) { return startsWith(x, "--") || startsWith(x, "-"); });
                foundOption->second->consume(std::next(arg), nextOption);
                arg = nextOption;
            }
            else
            {
                throw std::runtime_error("Unknown option: " + std::string(optionName));
            }
        }

        for (auto& option : options_)
        {
            option.validate();
        }
        parsed_ = true;
    }

    /// @brief Outputs program usage information to a stream.
    ///
    /// Prints out basic usage instructions derived from defined options.
    ///
    /// @param os The output stream to write the usage information to.
    /// @param usageName An optional program usage string.
    void usage(std::ostream& os, std::string_view usageName) const
    {
        os << "Usage:\n";

        os << "  " << usageName;

        for (auto option : options_)
        {
            os << " ";

            if (!option.isRequired_)
            {
                os << "[ ";
            }

            os << kDoublePrefix << option.baseName_;
            if (option.valueSemantic_->minTokens() > 0U)
            {
                os << " arg";
            }

            if (!option.isRequired_)
            {
                os << " ]";
            }
        }
        os << "\n\n";
    }

private:
    OptionList options_;                                   ///< A list of defined options.
    std::map<std::string_view, OptionIterator> optionMap_; ///< A map from option names to their iterators.
    std::string programName_;                              ///< The program name for usage instructions.
    bool parsed_{false};                                   ///< Flag indicating whether parsing has been performed.
};

} // namespace casket::opt