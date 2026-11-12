// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <istream>
#include <optional>
#include <ostream>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

#include "platform/atomic-file-write.hpp"
#include "settings/settings-schema.hpp"

// On the MSVC ABI (MSVC itself, and Clang targeting Windows) the standard [[no_unique_address]] is
// accepted but has no effect; only the vendor spelling actually lets an empty member take no space.
#if defined(_MSC_VER)
#define TCA_ENGINE_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define TCA_ENGINE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

namespace tca::engine::settings {

/**
 * @brief A type exposing the two type aliases every formatter needs: the document it builds,
 *  and the cursor used to walk it.
 *
 * Kept separate from settings_document_formatter so that the concept below can name those
 * aliases without a type that has neither making the check ill-formed.
 *
 * @tparam FormatterType The formatter type.
 */
template <typename FormatterType>
concept settings_document_types = requires {
    // The whole settings file, held in memory: what parse() produces and serialize() consumes.
    typename FormatterType::document_type;

    // A cheap, copyable handle on one node of that document. A scope that does not name an
    // existing node is allowed: reads through it come back empty, so the fields under it keep
    // their defaults.
    typename FormatterType::scope_type;
};

/**
 * @brief A formatter able to build a settings document, put it on a stream and read it back.
 *
 * Settings are written and read as a document rather than as a stream of key-value pairs, so
 * that loading can look a key up instead of expecting it next. That is what lets a settings
 * file survive fields being reordered, removed or added between two versions of a game: a key
 * the schema does not find simply leaves its field on the schema default.
 *
 * subdocument() is what makes nesting work without the store and the format having to agree on
 * how a path is spelled: a JSON formatter returns the child object, a flat text one returns the
 * same document with a longer key prefix.
 *
 * @tparam FormatterType The formatter type.
 */
template <typename FormatterType>
concept settings_document_formatter =
    settings_document_types<FormatterType> &&
    requires(const FormatterType& formatter, typename FormatterType::document_type& document,
             const typename FormatterType::document_type& constDocument,
             const typename FormatterType::scope_type scope, std::ostream& outputStream,
             std::istream& inputStream, const fixed_string key) {
        // An empty document, ready to be written into.
        { formatter.make_document() } -> std::same_as<typename FormatterType::document_type>;

        // Reads a whole document back, or nothing if the stream does not hold one.
        {
            formatter.parse(inputStream)
        } -> std::same_as<std::optional<typename FormatterType::document_type>>;

        // Writes the document out, reporting whether the stream took all of it.
        { formatter.serialize(outputStream, constDocument) } -> std::same_as<bool>;

        // The document's outermost scope, which is where the top-level fields live.
        { formatter.root(document) } -> std::same_as<typename FormatterType::scope_type>;

        // The scope one level down, under the given key. On the writing side it is created; on
        // the reading side a key that is not there yields a scope whose reads come back empty.
        { formatter.subdocument(scope, key) } -> std::same_as<typename FormatterType::scope_type>;
    };

/**
 * @brief A formatter that can round-trip a single settings value of one given type: `write`
 * puts the key-value pair in a scope of the document, `read` looks it up again.
 *
 * Both members are expected to be templates on the value type, so that one formatter serves
 * every field type a schema may bind. This concept only pins down one instantiation of them;
 * `settings_formatter` applies it to every field type of an actual schema.
 *
 * @tparam FormatterType The formatter type.
 * @tparam ValueType The type of the value being serialized.
 */
template <typename FormatterType, typename ValueType>
concept settings_formatter_for =
    settings_document_formatter<FormatterType> &&
    requires(const FormatterType& formatter, const typename FormatterType::scope_type scope,
             const fixed_string key, const ValueType& value) {
        // Puts the key-value pair in the scope, reporting whether it could be written.
        { formatter.write(scope, key, value) } -> std::same_as<bool>;

        // Reads the value stored under that key back, or nothing when the scope holds no such
        // key, or holds something that is not a ValueType.
        {
            formatter.template read<ValueType>(scope, key)
        } -> std::same_as<std::optional<ValueType>>;
    };

namespace details {

template <typename FormatterType, typename SettingsType>
consteval bool formats_every_field() noexcept;

// A field is either a group, whose entries the formatter has to handle as leaves, a nested
// settings struct, which the store walks into, or a leaf the formatter has to handle itself.
// Groups come first because only a group_field can declare one (see field). A struct with a
// schema is always taken as nested, even if the formatter would also accept it whole.
template <typename FormatterType, typename ValueType>
consteval bool formats_field() noexcept {
    if constexpr (has_group_traits<ValueType>) {
        return settings_formatter_for<FormatterType,
                                      typename settings_group_traits<ValueType>::entry_type>;
    } else if constexpr (has_schema<ValueType>) {
        return formats_every_field<FormatterType, ValueType>();
    } else {
        return settings_formatter_for<FormatterType, ValueType>;
    }
}

template <typename FormatterType, typename FieldsTupleType, std::size_t... FieldIndices>
consteval bool formats_every_field(std::index_sequence<FieldIndices...> /*indices*/) noexcept {
    return (
        formats_field<FormatterType,
                      typename std::tuple_element_t<FieldIndices, FieldsTupleType>::value_type>() &&
        ...);
}

template <typename FormatterType, typename SettingsType>
consteval bool formats_every_field() noexcept {
    using fields_tuple_type = std::remove_cvref_t<decltype(settings_schema<SettingsType>::fields)>;

    return formats_every_field<FormatterType, fields_tuple_type>(
        std::make_index_sequence<std::tuple_size_v<fields_tuple_type>>{});
}

} // namespace details

/**
 * @brief A formatter able to serialize and load back every field of a given settings schema,
 *  nested settings structs included.
 *
 * The value types come from the schema's own fields, so nothing here is fixed in advance: a
 * formatter is only ever required to handle the leaf types the settings struct at hand actually
 * uses. Fields that are themselves settings structs are recursed into rather than handed to the
 * formatter whole.
 *
 * @tparam FormatterType The formatter type.
 * @tparam SettingsType The settings struct the formatter is used with.
 */
template <typename FormatterType, typename SettingsType>
concept settings_formatter =
    has_schema<SettingsType> && settings_document_formatter<FormatterType> &&
    details::formats_every_field<FormatterType, SettingsType>();

/**
 * @brief What reading the settings file turned out to be. Every one of them but `unreadable`
 *  leaves the settings usable: the fields the file did not account for keep their schema
 *  default.
 */
enum class settings_load_result : std::uint8_t {
    // Every field of the schema was found in the file.
    complete,

    // The file was read, but some fields were not in it, or held something their type would not
    // take. Those kept their default. This is what a file written by an older version of the
    // game looks like, so it is an expected outcome rather than a failure.
    incomplete,

    // There is a file, but nothing could be read from it: it is not in the expected format, is
    // truncated, or cannot be opened. The settings are entirely the schema defaults.
    unreadable,

    // There is no file yet, which is what the first run of a game looks like. The settings are
    // entirely the schema defaults.
    missing
};

/**
 * @brief A RAII class that will automatically load the settings file or creates it if non
 * existent. It will also automatically save the settings to the given file.
 *
 * The settings are held by value and reached through settings(): callers read and write the
 * plain struct, and the store takes care of putting it on disk. A save builds the whole document
 * before opening the file, so settings that cannot be turned into one leave the previous file
 * where it was.
 *
 * Loading is by key rather than by position: every field is looked up under the name its schema
 * gives it, wherever it sits in the file. A field that is not there keeps its schema default,
 * and load() reports false so the caller can tell a complete file from a partial one. A field
 * whose value_type has a schema of its own is written into, and read from, a scope of that name
 * — so two nested structs may both have a `width` without colliding.
 *
 * Neither save() nor load() throws: both report failure through their return value, since the
 * destructor saves and a settings file is never worth tearing the program down for.
 *
 * @tparam SettingsSchema The settings struct, which must have a settings_schema specialization.
 * @tparam SettingsFormatter The formatter turning the settings into a document and back.
 */
template <has_schema SettingsSchema, settings_formatter<SettingsSchema> SettingsFormatter>
class settings_store {
public:
    using settings_type = SettingsSchema;
    using formatter_type = SettingsFormatter;

    /**
     * @brief Constructs the store on the given file, loading it if it is there and creating it
     *  from the schema defaults otherwise.
     * @param filePath The settings file the store reads from and writes to.
     * @param formatter The formatter to serialize the settings with.
     */
    explicit settings_store(std::filesystem::path filePath, SettingsFormatter formatter = {})
        : m_settingsFilePath(std::move(filePath)),
          m_formatter(std::move(formatter)) {
        switch (load_file()) {
            case settings_load_result::missing:
                // There is no settings file yet: keep the defaults load_file() has applied and put
                // them on disk, so the file exists from now on.
                save();
                break;

            case settings_load_result::unreadable:
                // A file nothing could be read from is moved aside rather than written over, so
                // that whoever has to explain where a player's settings went still has the file
                // that caused it. The defaults reach the disk on the next save, the destructor's
                // included.
                preserve_damaged_file();
                break;

            case settings_load_result::complete:
            case settings_load_result::incomplete:
                // A file that was read, whole or in part, is left exactly as it is: a field the
                // store did not find is not a reason to throw away the ones it did.
                break;
        }
    }

    ~settings_store() noexcept {
        save();
    }

    settings_store(const settings_store& other) = delete;
    settings_store(settings_store&& other) noexcept = default;

    settings_store& operator=(const settings_store& other) = delete;
    settings_store& operator=(settings_store&& other) noexcept = default;

    /**
     * @brief The settings held by this store. Writing through the non-const overload is how
     *  client code changes a setting; the change reaches the file on the next save().
     */
    [[nodiscard]] settings_type& settings() noexcept {
        return m_settings;
    }

    [[nodiscard]] const settings_type& settings() const noexcept {
        return m_settings;
    }

    /**
     * @brief The file this store is bound to.
     */
    [[nodiscard]] const std::filesystem::path& file_path() const noexcept {
        return m_settingsFilePath;
    }

    /**
     * @brief Writes every field of the schema to the settings file, replacing its contents.
     * @return True if the whole settings struct could be written.
     */
    bool save() const noexcept {
        try {
            auto document{m_formatter.make_document()};
            if (!write_fields(m_formatter.root(document), m_settings)) {
                return false;
            }

            // Written through a temporary that is then renamed over the settings file, so that a
            // crash or a full disk part-way through leaves the previously saved settings whole
            // rather than a truncated file the next run would have to throw away.
            return platform::write_file_atomically(
                m_settingsFilePath, [this, &document](std::ostream& outputStream) {
                    return m_formatter.serialize(outputStream, document);
                });
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief Applies the schema defaults, then reads the settings file over them.
     * @return True if every field of the settings struct was found in the file. The ones that
     *  were not keep their schema default.
     */
    bool load() noexcept {
        return load_file() == settings_load_result::complete;
    }

    /**
     * @brief Applies the schema defaults, then reads the settings file over them, saying how much
     *  of the settings came from the file.
     * @return How much of the settings the file accounted for. They are usable whatever it says:
     *  what the file did not give is the schema default.
     */
    settings_load_result load_file() noexcept {
        try {
            apply_defaults(m_settings);

            std::ifstream inputStream{m_settingsFilePath, std::ios::in};
            if (!inputStream.is_open()) {
                std::error_code errorCode{};

                return std::filesystem::exists(m_settingsFilePath, errorCode)
                           ? settings_load_result::unreadable
                           : settings_load_result::missing;
            }

            auto document{m_formatter.parse(inputStream)};
            if (!document.has_value()) {
                return settings_load_result::unreadable;
            }

            return read_fields(m_formatter.root(document.value()), m_settings)
                       ? settings_load_result::complete
                       : settings_load_result::incomplete;
        } catch (...) {
            return settings_load_result::unreadable;
        }
    }

    /**
     * @brief What is appended to the settings file path to name the file an unreadable one is
     *  kept as.
     */
    static constexpr std::string_view DAMAGED_FILE_EXTENSION{".corrupt"};

private:
    using scope_type = typename SettingsFormatter::scope_type;

    // Renames the unreadable settings file out of the way, so that the defaults about to be
    // saved do not take it with them. A damaged file kept earlier is replaced: the one that has
    // just cost a player their settings is the one worth having. Only a regular file is moved —
    // a path taken by a directory is not a damaged settings file, and is left for save() to
    // fail on.
    void preserve_damaged_file() const noexcept {
        try {
            std::error_code errorCode{};
            if (!std::filesystem::is_regular_file(m_settingsFilePath, errorCode)) {
                return;
            }

            std::filesystem::path damagedFilePath{m_settingsFilePath};
            damagedFilePath += DAMAGED_FILE_EXTENSION;

            std::filesystem::rename(m_settingsFilePath, damagedFilePath, errorCode);
        } catch (...) {
            // Keeping the damaged file is a courtesy to whoever has to diagnose it: not managing
            // to is not worth failing the construction of the store over.
        }
    }

    // Fills every field that declares a default in the schema, walking into nested settings
    // structs so that those take theirs from their own schema, and resets every group to a
    // value-initialized one. Other fields without a default are left as they are.
    template <has_schema StructType>
    static void apply_defaults(StructType& target) {
        std::apply(
            [&target](const auto&... schemaFields) {
                const auto applyOne{[&target](const auto& schemaField) {
                    using field_value_type =
                        typename std::remove_cvref_t<decltype(schemaField)>::value_type;

                    if constexpr (has_group_traits<field_value_type>) {
                        target.*(schemaField.binding) = field_value_type{};
                    } else {
                        if constexpr (has_schema<field_value_type>) {
                            if (!schemaField.default_value.has_value()) {
                                apply_defaults(target.*(schemaField.binding));

                                return;
                            }
                        }

                        if (schemaField.default_value.has_value()) {
                            target.*(schemaField.binding) = schemaField.default_value.value();
                        }
                    }
                }};

                (applyOne(schemaFields), ...);
            },
            settings_schema<StructType>::fields);
    }

    template <has_schema StructType>
    bool write_fields(const scope_type scope, const StructType& source) const {
        return std::apply(
            [this, scope, &source](const auto&... schemaFields) {
                const auto writeOne{[this, scope, &source](const auto& schemaField) {
                    using field_value_type =
                        typename std::remove_cvref_t<decltype(schemaField)>::value_type;

                    if constexpr (has_group_traits<field_value_type>) {
                        const auto groupScope{m_formatter.subdocument(scope, schemaField.name)};

                        return settings_group_traits<field_value_type>::write_entries(
                            source.*(schemaField.binding),
                            [this, groupScope](const fixed_string name, const auto& entry) {
                                return m_formatter.write(groupScope, name, entry);
                            });
                    } else if constexpr (has_schema<field_value_type>) {
                        return write_fields(m_formatter.subdocument(scope, schemaField.name),
                                            source.*(schemaField.binding));
                    } else {
                        return m_formatter.write(scope, schemaField.name,
                                                 source.*(schemaField.binding));
                    }
                }};

                // Folded with && so a document that has gone bad stops the walk instead of piling
                // failed writes on top of it.
                return (writeOne(schemaFields) && ...);
            },
            settings_schema<StructType>::fields);
    }

    template <has_schema StructType>
    bool read_fields(const scope_type scope, StructType& target) const {
        return std::apply(
            [this, scope, &target](const auto&... schemaFields) {
                const auto readOne{[this, scope, &target](const auto& schemaField) {
                    using field_value_type =
                        typename std::remove_cvref_t<decltype(schemaField)>::value_type;

                    if constexpr (has_group_traits<field_value_type>) {
                        using entry_type =
                            typename settings_group_traits<field_value_type>::entry_type;

                        const auto groupScope{m_formatter.subdocument(scope, schemaField.name)};

                        return settings_group_traits<field_value_type>::read_entries(
                            target.*(schemaField.binding),
                            [this, groupScope](const fixed_string name) {
                                return m_formatter.template read<entry_type>(groupScope, name);
                            });
                    } else if constexpr (has_schema<field_value_type>) {
                        return read_fields(m_formatter.subdocument(scope, schemaField.name),
                                           target.*(schemaField.binding));
                    } else {
                        const auto loadedValue{
                            m_formatter.template read<field_value_type>(scope, schemaField.name)};
                        if (!loadedValue.has_value()) {
                            return false;
                        }

                        target.*(schemaField.binding) = loadedValue.value();

                        return true;
                    }
                }};

                // Every field is looked up whatever the ones before it did: a key the file does
                // not hold leaves its own field on the default without costing the others theirs.
                bool everyFieldRead{true};
                ((everyFieldRead = readOne(schemaFields) && everyFieldRead), ...);

                return everyFieldRead;
            },
            settings_schema<StructType>::fields);
    }

    std::filesystem::path m_settingsFilePath{};
    TCA_ENGINE_NO_UNIQUE_ADDRESS SettingsFormatter m_formatter{};
    settings_type m_settings{};
};

} // namespace tca::engine::settings
