#include "scriptengine.h"
#include "LuaUI/button.h"
#include "LuaUI/color.h"
#include "LuaUI/combofileselector.h"
#include "LuaUI/isotopesourceselector.h"
#include "LuaUI/lineedit.h"
#include "LuaUI/plot.h"
#include "Protocols/rpcprotocol.h"
#include "Protocols/scpiprotocol.h"
#include "Protocols/sg04countprotocol.h"
#include "Windows/mainwindow.h"
#include "config.h"
#include "console.h"
#include "data_engine/data_engine.h"
#include "datalogger.h"
#include "lua_functions.h"
#include "rpcruntime_decoded_function_call.h"
#include "rpcruntime_encoded_function_call.h"
#include "util.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QSettings>
#include <QShortcut>
#include <QThread>
#include <QTimer>
#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <vector>

template <class T>
struct Lua_UI_Wrapper {
    template <class... Args>
    Lua_UI_Wrapper(QSplitter *parent, Args &&... args) {
        Utility::thread_call(MainWindow::mw, [ id = id, parent, args... ] { MainWindow::mw->add_lua_UI_class<T>(id, parent, args...); });
    }
    Lua_UI_Wrapper(Lua_UI_Wrapper &&other)
        : id(other.id) {
        other.id = -1;
    }
    Lua_UI_Wrapper &operator=(Lua_UI_Wrapper &&other) {
        std::swap(id, other.id);
    }
    ~Lua_UI_Wrapper() {
        if (id != -1) {
            Utility::thread_call(MainWindow::mw, [id = this->id] { MainWindow::mw->remove_lua_UI_class<T>(id); });
        }
    }

    int id = id_counter++;

    private:
    static int id_counter;
};

template <class T>
int Lua_UI_Wrapper<T>::id_counter;

namespace detail {
    //this might be replacable by std::invoke once C++17 is available
    template <class ReturnType, class UI_class, class... Args, template <class...> class Params, class... ParamArgs, std::size_t... I>
    ReturnType call_helper(ReturnType (UI_class::*func)(Args...), UI_class &ui, Params<ParamArgs...> const &params, std::index_sequence<I...>) {
        return (ui.*func)(std::get<I>(params)...);
    }
    template <class ReturnType, class UI_class, class... Args, template <class...> class Params, class... ParamArgs, std::size_t... I>
    ReturnType call_helper(ReturnType (UI_class::*func)(Args...) const, UI_class &ui, Params<ParamArgs...> const &params, std::index_sequence<I...>) {
        return (ui.*func)(std::get<I>(params)...);
    }

    template <class ReturnType, class UI_class, class... Args, template <class...> class Params, class... ParamArgs>
    ReturnType call(ReturnType (UI_class::*func)(Args...), UI_class &ui, Params<ParamArgs...> const &params) {
        return call_helper(func, ui, params, std::index_sequence_for<Args...>{});
    }
    template <class ReturnType, class UI_class, class... Args, template <class...> class Params, class... ParamArgs>
    ReturnType call(ReturnType (UI_class::*func)(Args...) const, UI_class &ui, Params<ParamArgs...> const &params) {
        return call_helper(func, ui, params, std::index_sequence_for<Args...>{});
    }
}

template <class ReturnType, class UI_class, class... Args>
auto thread_call_wrapper(ReturnType (UI_class::*function)(Args...)) {
    if (QThread::currentThread()->isInterruptionRequested()) {
        throw sol::error("Abort Requested");
    }
    return [function](Lua_UI_Wrapper<UI_class> &lui, Args &&... args) {
        //TODO: Decide if we should use promised_thread_call or thread_call
        //promised_thread_call lets us get return values while thread_call does not
        //however, promised_thread_call hangs if the gui thread hangs while thread_call does not
        //using thread_call iff ReturnType is void and promised_thread_call otherwise requires some more template magic
        return Utility::promised_thread_call(MainWindow::mw, [ function, id = lui.id, args = std::forward_as_tuple(std::forward<Args>(args)...) ]() mutable {
            UI_class &ui = MainWindow::mw->get_lua_UI_class<UI_class>(id);
            return detail::call(function, ui, std::move(args));
        });
    };
}

template <class ReturnType, class UI_class, class... Args>
auto thread_call_wrapper(ReturnType (UI_class::*function)(Args...) const) {
    if (QThread::currentThread()->isInterruptionRequested()) {
        throw sol::error("Abort Requested");
    }
    return [function](Lua_UI_Wrapper<UI_class> &lui, Args &&... args) {
        //TODO: Decide if we should use promised_thread_call or thread_call
        //promised_thread_call lets us get return values while thread_call does not
        //however, promised_thread_call hangs if the gui thread hangs while thread_call does not
        //using thread_call iff ReturnType is void and promised_thread_call otherwise requires some more template magic
        return Utility::promised_thread_call(MainWindow::mw, [ function, id = lui.id, args = std::forward_as_tuple(std::forward<Args>(args)...) ]() mutable {
            UI_class &ui = MainWindow::mw->get_lua_UI_class<UI_class>(id);
            return detail::call(function, ui, std::move(args));
        });
    };
}

template <class ReturnType, class UI_class, class... Args>
auto thread_call_wrapper_non_waiting(ReturnType (UI_class::*function)(Args...)) {
    if (QThread::currentThread()->isInterruptionRequested()) {
        throw sol::error("Abort Requested");
    }
    return [function](Lua_UI_Wrapper<UI_class> &lui, Args &&... args) {
        //TODO: Decide if we should use promised_thread_call or thread_call
        //promised_thread_call lets us get return values while thread_call does not
        //however, promised_thread_call hangs if the gui thread hangs while thread_call does not
        //using thread_call iff ReturnType is void and promised_thread_call otherwise requires some more template magic
        return Utility::thread_call(MainWindow::mw, [ function, id = lui.id, args = std::make_tuple(std::forward<Args>(args)...) ]() mutable {
            UI_class &ui = MainWindow::mw->get_lua_UI_class<UI_class>(id);
            return detail::call(function, ui, std::move(args));
        });
    };
}

static sol::object create_lua_object_from_RPC_answer(const RPCRuntimeDecodedParam &param, sol::state &lua) {
    switch (param.get_desciption()->get_type()) {
        case RPCRuntimeParameterDescription::Type::array: {
            auto array = param.as_array();
            if (array.size() == 1) {
                return create_lua_object_from_RPC_answer(array.front(), lua);
            }
            auto table = lua.create_table_with();
            for (auto &element : array) {
                table.add(create_lua_object_from_RPC_answer(element, lua));
            }
            return table;
        }
        case RPCRuntimeParameterDescription::Type::character:
            throw sol::error("TODO: Parse return value of type character");
        case RPCRuntimeParameterDescription::Type::enumeration:
            return sol::make_object(lua.lua_state(), param.as_enum().value);
        case RPCRuntimeParameterDescription::Type::structure: {
            auto table = lua.create_table_with();
            for (auto &element : param.as_struct()) {
                table[element.name] = create_lua_object_from_RPC_answer(element.type, lua);
            }
            return table;
        }
        case RPCRuntimeParameterDescription::Type::integer:
            return sol::make_object(lua.lua_state(), param.as_integer());
    }
    assert(!"Invalid type of RPCRuntimeParameterDescription");
    return sol::nil;
}

static void set_runtime_parameter(RPCRuntimeEncodedParam &param, sol::object object) {
    if (param.get_description()->get_type() == RPCRuntimeParameterDescription::Type::array && param.get_description()->as_array().number_of_elements == 1) {
        return set_runtime_parameter(param[0], object);
    }
    switch (object.get_type()) {
        case sol::type::boolean:
            param.set_value(object.as<bool>() ? 1 : 0);
            break;
        case sol::type::function:
            throw sol::error("Cannot pass an object of type function to RPC");
        case sol::type::number:
            param.set_value(object.as<int64_t>());
            break;
        case sol::type::nil:
        case sol::type::none:
            throw sol::error("Cannot pass an object of type nil to RPC");
        case sol::type::string:
            param.set_value(object.as<std::string>());
            break;
        case sol::type::table: {
            sol::table t = object.as<sol::table>();
            if (t.size()) {
                for (std::size_t i = 0; i < t.size(); i++) {
                    set_runtime_parameter(param[i], t[i + 1]);
                }
            } else {
                for (auto &v : t) {
                    set_runtime_parameter(param[v.first.as<std::string>()], v.second);
                }
            }
            break;
        }
        default:
            throw sol::error("Cannot pass an object of unknown type " + std::to_string(static_cast<int>(object.get_type())) + " to RPC");
    }
}

void add_enum_type(const RPCRuntimeParameterDescription &param, sol::state &lua) {
    if (param.get_type() == RPCRuntimeParameterDescription::Type::enumeration) {
        const auto &enum_description = param.as_enumeration();
        auto table = lua.create_named_table(enum_description.enum_name);
        for (auto &value : enum_description.values) {
            table[value.name] = value.to_int();
            table["to_string"] = [enum_description](int enum_value_param) -> std::string {
                for (const auto &enum_value : enum_description.values) {
                    if (enum_value.to_int() == enum_value_param) {
                        return enum_value.name;
                    }
                }
                return "";
            };
        }
    }
}

void add_enum_types(const RPCRuntimeFunction &function, sol::state &lua) {
    for (auto &param : function.get_request_parameters()) {
        add_enum_type(param, lua);
    }
    for (auto &param : function.get_reply_parameters()) {
        add_enum_type(param, lua);
    }
}

struct RPCDevice {
    std::string get_protocol_name() {
        return protocol->type.toStdString();
    }

    sol::object call_rpc_function(const std::string &name, const sol::variadic_args &va) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            throw sol::error("Abort Requested");
        }

        Console::note() << QString("\"%1\" called").arg(name.c_str());
        auto function = protocol->encode_function(name);
        int param_count = 0;
        for (auto &arg : va) {
            auto &param = function.get_parameter(param_count++);
            set_runtime_parameter(param, arg);
        }
        if (function.are_all_values_set()) {
            auto result = protocol->call_and_wait(function);

            if (result) {
                try {
                    auto output_params = result->get_decoded_parameters();
                    if (output_params.empty()) {
                        return sol::nil;
                    } else if (output_params.size() == 1) {
                        return create_lua_object_from_RPC_answer(output_params.front(), *lua);
                    }
                    //else: multiple variables, need to make a table
                    return sol::make_object(lua->lua_state(), "TODO: Not Implemented: Parse multiple return values");
                } catch (const sol::error &e) {
                    Console::error() << e.what();
                    throw;
                }
            }
            throw sol::error("Call to \"" + name + "\" failed due to timeout");
        }
        //not all values set, error
        throw sol::error("Failed calling function, missing parameters");
    }
    sol::state *lua = nullptr;
    RPCProtocol *protocol = nullptr;
    CommunicationDevice *device = nullptr;
    ScriptEngine *engine = nullptr;
};

struct SG04CountDevice {
    std::string get_protocol_name() {
        return protocol->type.toStdString();
    }

    sol::table get_sg04_counts(bool clear) {
        return protocol->get_sg04_counts(*lua, clear);
    }

    sol::state *lua = nullptr;
    SG04CountProtocol *protocol = nullptr;
    CommunicationDevice *device = nullptr;
    ScriptEngine *engine = nullptr;
};

struct SCPIDevice {
    void send_command(std::string request) {
        protocol->send_command(request);
    }
    std::string get_protocol_name() {
        return protocol->type.toStdString();
    }

    sol::table get_device_descriptor() {
        sol::table result = lua->create_table_with();
        protocol->get_lua_device_descriptor(result);
        return result;
    }

    sol::table get_str(std::string request) {
        return protocol->get_str(*lua, request); //timeout possible
    }

    sol::table get_str_param(std::string request, std::string argument) {
        return protocol->get_str_param(*lua, request, argument); //timeout possible
    }

    double get_num(std::string request) {
        return protocol->get_num(request); //timeout possible
    }

    double get_num_param(std::string request, std::string argument) {
        return protocol->get_num_param(request, argument); //timeout possible
    }

    bool is_event_received(std::string event_name) {
        return protocol->is_event_received(event_name);
    }

    void clear_event_list() {
        return protocol->clear_event_list();
    }

    sol::table get_event_list() {
        return protocol->get_event_list(*lua);
    }

    std::string get_name(void) {
        return protocol->get_name();
    }

    std::string get_serial_number(void) {
        return protocol->get_serial_number();
    }

    std::string get_manufacturer(void) {
        return protocol->get_manufacturer();
    }

    void set_validation_max_standard_deviation(double max_std_dev) {
        protocol->set_validation_max_standard_deviation(max_std_dev);
    }

    void set_validation_retries(unsigned int retries) {
        protocol->set_validation_retries(retries);
    }

    sol::state *lua = nullptr;
    SCPIProtocol *protocol = nullptr;
    CommunicationDevice *device = nullptr;
    ScriptEngine *engine = nullptr;
};

ScriptEngine::ScriptEngine(QSplitter *parent, QPlainTextEdit *console, Data_engine *data_engine)
    : lua(std::make_unique<sol::state>())
    , parent(parent)
    , console(console)
    , data_engine(data_engine) {}

ScriptEngine::~ScriptEngine() {}

std::string ScriptEngine::to_string(double d) {
    if (std::fmod(d, 1.) == 0) {
        return std::to_string(static_cast<int>(d));
    }
    return std::to_string(d);
}

std::string ScriptEngine::to_string(const sol::object &o) {
    switch (o.get_type()) {
        case sol::type::boolean:
            return o.as<bool>() ? "true" : "false";
        case sol::type::function:
            return "function";
        case sol::type::number:
            return to_string(o.as<double>());
        case sol::type::nil:
        case sol::type::none:
            return "nil";
        case sol::type::string:
            return "\"" + o.as<std::string>() + "\"";
        case sol::type::table: {
            auto table = o.as<sol::table>();
            std::string retval{"{"};
            int index = 1;
            for (auto &object : table) {
                auto first_object_string = to_string(object.first);
                if (first_object_string == std::to_string(index++)) {
                    retval += to_string(object.second);
                } else {
                    retval += '[' + std::move(first_object_string) + "]=" + to_string(object.second);
                }
                retval += ", ";
            }
            if (retval.size() > 1) {
                retval.pop_back();
                retval.back() = '}';
                return retval;
            }
            return "{}";
        }
        case sol::type::userdata:
            if (o.is<Color>()) {
                return "Ui.Color_from_rgb(0x" + QString::number(o.as<Color>().rgb & 0xFFFFFFu, 16).toStdString() + ")";
            }
            return "unknown custom datatype";
        default:
            return "unknown type " + std::to_string(static_cast<int>(o.get_type()));
    }
}

std::string ScriptEngine::to_string(const sol::stack_proxy &object) {
    return to_string(sol::object{object});
}

void ScriptEngine::load_script(const QString &path) {
    //NOTE: When using lambdas do not capture `this` or by reference, because it breaks when the ScriptEngine is moved
    this->path = path;

    try {
        //load the standard libs if necessary
        lua->open_libraries();

        //add generic function
        {
            (*lua)["show_question"] = [path](const sol::optional<std::string> &title, const sol::optional<std::string> &message, sol::table button_table) {
                return show_question(path, title, message, button_table);
            };

            (*lua)["show_info"] = [path](const sol::optional<std::string> &title, const sol::optional<std::string> &message) {
                show_info(path, title, message);
            };

            (*lua)["show_warning"] = [path](const sol::optional<std::string> &title, const sol::optional<std::string> &message) {
                show_warning(path, title, message);
            };

            (*lua)["print"] = [console = console](const sol::variadic_args &args) {
                if (QThread::currentThread()->isInterruptionRequested()) {
                    throw sol::error("Abort Requested");
                }

                print(console, args);
            };

            (*lua)["sleep_ms"] = [](const unsigned int timeout_ms) { sleep_ms(timeout_ms); };

            (*lua)["round"] = [](const double value, const unsigned int precision = 0) { return round_double(value, precision); };
            (*lua)["require"] = [ path = path, &lua = *lua ](const std::string &file) {
                QDir dir(path);
                dir.cdUp();
                lua.script_file(dir.absoluteFilePath(QString::fromStdString(file) + ".lua").toStdString());
            };
            (*lua)["await_hotkey"] = [] {
                QEventLoop event_loop;
                enum { confirm_pressed, skip_pressed, cancel_pressed };
                std::array<std::unique_ptr<QShortcut>, 3> shortcuts;
                MainWindow::mw->execute_in_gui_thread([&event_loop, &shortcuts] {
                    const char *settings_keys[] = {Globals::confirm_key_sequence, Globals::skip_key_sequence, Globals::cancel_key_sequence};
                    for (std::size_t i = 0; i < shortcuts.size(); i++) {
                        shortcuts[i] =
                            std::make_unique<QShortcut>(QKeySequence::fromString(QSettings{}.value(settings_keys[i], "").toString()), MainWindow::mw);
                        QObject::connect(shortcuts[i].get(), &QShortcut::activated, [&event_loop, i] { event_loop.exit(i); });
                    }
                });
                auto exit_value = event_loop.exec();
                Utility::promised_thread_call(MainWindow::mw, [&shortcuts] { std::fill(std::begin(shortcuts), std::end(shortcuts), nullptr); });
                switch (exit_value) {
                    case confirm_pressed:
                        return "confirm";
                    case skip_pressed:
                        return "skip";
                    case cancel_pressed:
                        return "cancel";
                }
                return "unknown";
            };
        }

        //table functions
        {
            (*lua)["table_save_to_file"] = [console = console](const std::string file_name, sol::table input_table, bool over_write_file) {
                table_save_to_file(console, file_name, input_table, over_write_file);
            };
            (*lua)["table_load_from_file"] = [&lua = *lua, console = console ](const std::string file_name) {
                return table_load_from_file(console, lua, file_name);
            };
            (*lua)["table_sum"] = [](sol::table table) { return table_sum(table); };

            (*lua)["table_mean"] = [](sol::table table) { return table_mean(table); };

            (*lua)["table_set_constant"] = [&lua = *lua](sol::table input_values, double constant) {
                return table_set_constant(lua, input_values, constant);
            };

            (*lua)["table_create_constant"] = [&lua = *lua](const unsigned int size, double constant) {
                return table_create_constant(lua, size, constant);
            };

            (*lua)["table_add_table"] = [&lua = *lua](sol::table input_values_a, sol::table input_values_b) {
                return table_add_table(lua, input_values_a, input_values_b);
            };

            (*lua)["table_add_constant"] = [&lua = *lua](sol::table input_values, double constant) {
                return table_add_constant(lua, input_values, constant);
            };

            (*lua)["table_sub_table"] = [&lua = *lua](sol::table input_values_a, sol::table input_values_b) {
                return table_sub_table(lua, input_values_a, input_values_b);
            };

            (*lua)["table_mul_table"] = [&lua = *lua](sol::table input_values_a, sol::table input_values_b) {
                return table_mul_table(lua, input_values_a, input_values_b);
            };

            (*lua)["table_mul_constant"] = [&lua = *lua](sol::table input_values_a, double constant) {
                return table_mul_constant(lua, input_values_a, constant);
            };

            (*lua)["table_div_table"] = [&lua = *lua](sol::table input_values_a, sol::table input_values_b) {
                return table_div_table(lua, input_values_a, input_values_b);
            };

            (*lua)["table_round"] = [&lua = *lua](sol::table input_values, const unsigned int precision = 0) {
                return table_round(lua, input_values, precision);
            };

            (*lua)["table_abs"] = [&lua = *lua](sol::table input_values) {
                return table_abs(lua, input_values);
            };

            (*lua)["table_mid"] = [&lua = *lua](sol::table input_values, const unsigned int start, const unsigned int length) {
                return table_mid(lua, input_values, start, length);
            };

            (*lua)["table_equal_constant"] = [](sol::table input_values_a, double input_const_val) {
                return table_equal_constant(input_values_a, input_const_val);
            };

            (*lua)["table_equal_table"] = [](sol::table input_values_a, sol::table input_values_b) {
                return table_equal_table(input_values_a, input_values_b);
            };

            (*lua)["table_max"] = [](sol::table input_values) { return table_max(input_values); };

            (*lua)["table_min"] = [](sol::table input_values) { return table_min(input_values); };

            (*lua)["table_max_abs"] = [](sol::table input_values) { return table_max_abs(input_values); };

            (*lua)["table_min_abs"] = [](sol::table input_values) { return table_min_abs(input_values); };
        }

        {
            (*lua)["measure_noise_level_czt"] = [&lua = *lua](sol::table rpc_device, const unsigned int dacs_quantity,
                                                              const unsigned int max_possible_dac_value) {
                return measure_noise_level_czt(lua, rpc_device, dacs_quantity, max_possible_dac_value);
            };
        }
#if 1
        //bind data engine
        {
            lua->new_usertype<DataLogger>(
                "DataLogger", //
                sol::meta_function::construct,
                [console = console](const std::string &file_name, char seperating_character, sol::table field_names, bool over_write_file) {
                    return DataLogger{console, file_name, seperating_character, field_names, over_write_file};
                }, //

                "append_data",
                [](DataLogger &handle, const sol::table &data_record) { return handle.append_data(data_record); }, //
                "save", [](DataLogger &handle) { handle.save(); }                                                  //
                );
        }
#endif
        //bind data engine
        {
            struct Data_engine_handle {
                Data_engine *data_engine{nullptr};
                Data_engine_handle() = delete;
            };

            lua->new_usertype<Data_engine_handle>(
                "Data_engine", //
                sol::meta_function::construct, [ data_engine = data_engine, pdf_filepath = pdf_filepath.get(),
                                                 form_filepath = form_filepath.get() ](const std::string &xml_file, const std::string &json_file) {
                    QString form_dir = QSettings{}.value(Globals::form_directory, QDir::currentPath()).toString();
                    auto file_path = QDir{form_dir}.absoluteFilePath(QString::fromStdString(json_file)).toStdString();
                    std::ifstream f(file_path);
                    if (!f) {
                        throw std::runtime_error("Failed opening file " + file_path);
                    }
                    data_engine->set_source(f);
                    *pdf_filepath = QDir{QSettings{}.value(Globals::form_directory, "").toString()}.absoluteFilePath("test_dump.pdf").toStdString();
                    *form_filepath =
                        QDir{QSettings{}.value(Globals::form_directory, "").toString()}.absoluteFilePath(QString::fromStdString(xml_file)).toStdString();
                    return Data_engine_handle{data_engine};
                }, //
                "set",
                [](Data_engine_handle &handle, const std::string &field_id, double number) {
                    handle.data_engine->set_actual_number(QString::fromStdString(field_id), number);
                });
        }

        //bind UI
        auto ui_table = lua->create_named_table("Ui");

        //bind plot
        {
            ui_table.new_usertype<Lua_UI_Wrapper<Curve>>(
                "Curve",                                                                                            //
                sol::meta_function::construct, sol::no_constructor,                                                 //
                "append_point", thread_call_wrapper_non_waiting<void, Curve, double, double>(&Curve::append_point), //
                "add_spectrum",
                [](Lua_UI_Wrapper<Curve> &curve, sol::table table) {
                    std::vector<double> data;
                    data.reserve(table.size());
                    for (auto &i : table) {
                        data.push_back(i.second.as<double>());
                    }
                    Utility::thread_call(MainWindow::mw, [ id = curve.id, data = std::move(data) ] {
                        auto &curve = MainWindow::mw->get_lua_UI_class<Curve>(id);
                        curve.add(data);
                    });
                }, //
                "add_spectrum_at",
                [](Lua_UI_Wrapper<Curve> &curve, const unsigned int spectrum_start_channel, const sol::table &table) {
                    std::vector<double> data;
                    data.reserve(table.size());
                    for (auto &i : table) {
                        data.push_back(i.second.as<double>());
                    }
                    Utility::thread_call(MainWindow::mw, [ id = curve.id, data = std::move(data), spectrum_start_channel ] {
                        auto &curve = MainWindow::mw->get_lua_UI_class<Curve>(id);
                        curve.add_spectrum_at(spectrum_start_channel, data);
                    });
                }, //

                "clear",
                thread_call_wrapper(&Curve::clear),                                            //
                "set_median_enable", thread_call_wrapper(&Curve::set_median_enable),           //
                "set_median_kernel_size", thread_call_wrapper(&Curve::set_median_kernel_size), //
                "integrate_ci", thread_call_wrapper(&Curve::integrate_ci),                     //
                "set_x_axis_gain", thread_call_wrapper(&Curve::set_x_axis_gain),               //
                "set_x_axis_offset",
                thread_call_wrapper(&Curve::set_x_axis_offset),      //
                "set_color", thread_call_wrapper(&Curve::set_color), //
                "user_pick_x_coord",
                [](const Lua_UI_Wrapper<Curve> &lua_curve) {
                    QThread *thread = QThread::currentThread();
                    std::promise<double> x_selection_promise;
                    std::future<double> x_selection_future = x_selection_promise.get_future();
                    Utility::thread_call(MainWindow::mw, [&lua_curve, thread, x_selection_promise = &x_selection_promise ]() mutable {
                        Curve &curve = MainWindow::mw->get_lua_UI_class<Curve>(lua_curve.id);
                        curve.set_onetime_click_callback([thread, x_selection_promise](double x, double y) mutable {
                            (void)y;
                            x_selection_promise->set_value(x);
                            Utility::thread_call(thread, [thread] { thread->exit(1234); });
                        });
                    });
                    if (QEventLoop{}.exec() == 1234) {
                        return x_selection_future.get();
                    } else {
                        throw sol::error("aborted");
                    }
                }
                //
                );
            ui_table.new_usertype<Lua_UI_Wrapper<Plot>>("Plot",                                                                                          //
                                                        sol::meta_function::construct, [parent = this->parent] { return Lua_UI_Wrapper<Plot>{parent}; }, //
                                                        "clear",
                                                        thread_call_wrapper(&Plot::clear), //
                                                        "add_curve",
                                                        [parent = this->parent](Lua_UI_Wrapper<Plot> & lua_plot)->Lua_UI_Wrapper<Curve> {
                                                            return Utility::promised_thread_call(MainWindow::mw,
                                                                                                 [parent, &lua_plot] {
                                                                                                     auto &plot =
                                                                                                         MainWindow::mw->get_lua_UI_class<Plot>(lua_plot.id);
                                                                                                     return Lua_UI_Wrapper<Curve>{parent, &plot};
                                                                                                 } //
                                                                                                 );
                                                        }, //
                                                        "set_x_marker",
                                                        thread_call_wrapper(&Plot::set_x_marker) //
                                                        );
        }
        //bind color
        {
            ui_table.new_usertype<Color>("Color", //
                                         sol::meta_function::construct, sol::no_constructor);
            ui_table["Color_from_name"] = [](const std::string &name) { return Color::Color_from_name(name); };
            ui_table["Color_from_r_g_b"] = [](int r, int g, int b) { return Color::Color_from_r_g_b(r, g, b); };
            ui_table["Color_from_rgb"] = [](int rgb) { return Color{rgb}; };
        }
        //bind ComboBoxFileSelector
        {
            ui_table.new_usertype<Lua_UI_Wrapper<ComboBoxFileSelector>>("ComboBoxFileSelector", //
                                                                        sol::meta_function::construct,
                                                                        [parent = this->parent](const std::string &directory, const sol::table &filters) {
                                                                            return Lua_UI_Wrapper<ComboBoxFileSelector>{parent, directory, filters};
                                                                        }, //
                                                                        "get_selected_file",
                                                                        thread_call_wrapper(&ComboBoxFileSelector::get_selected_file), //
                                                                        "set_order_by",
                                                                        thread_call_wrapper(&ComboBoxFileSelector::set_order_by) //

                                                                        );
        }
        //bind IsotopeSourceSelector
        {
            ui_table.new_usertype<Lua_UI_Wrapper<IsotopeSourceSelector>>(
                "IsotopeSourceSelector",                                                                                            //
                sol::meta_function::construct, [parent = this->parent]() { return Lua_UI_Wrapper<IsotopeSourceSelector>{parent}; }, //
                "get_selected_activity_Bq",
                thread_call_wrapper(&IsotopeSourceSelector::get_selected_activity_Bq),                                //
                "get_selected_serial_number", thread_call_wrapper(&IsotopeSourceSelector::get_selected_serial_number) //
                );
        }
        //bind button
        {
            ui_table.new_usertype<Lua_UI_Wrapper<Button>>(
                "Button", //
                sol::meta_function::construct,
                [parent = this->parent](const std::string &title) {
                    return Lua_UI_Wrapper<Button>{parent, title};
                }, //
                "has_been_clicked",
                thread_call_wrapper(&Button::has_been_clicked), //
                "await_click",
                [](const Lua_UI_Wrapper<Button> &lew) {
                    auto &lb = MainWindow::mw->get_lua_UI_class<Button>(lew.id);
                    lb.set_single_shot_return_pressed_callback([thread = QThread::currentThread()] { thread->exit(); });
                    QEventLoop{}.exec();
                } //
                );
        }

        //bind edit field
        {
            ui_table.new_usertype<Lua_UI_Wrapper<LineEdit>>(
                "LineEdit",                                                                                          //
                sol::meta_function::construct, [parent = this->parent] { return Lua_UI_Wrapper<LineEdit>(parent); }, //
                "set_placeholder_text", thread_call_wrapper(&LineEdit::set_placeholder_text),                        //
                "get_text", thread_call_wrapper(&LineEdit::get_text),                                                //
                "set_text", thread_call_wrapper(&LineEdit::set_text),                                                //
                "set_name", thread_call_wrapper(&LineEdit::set_name),                                                //
                "get_name", thread_call_wrapper(&LineEdit::get_name),                                                //
                "get_number", thread_call_wrapper(&LineEdit::get_number),                                            //

                "await_return",
                [](const Lua_UI_Wrapper<LineEdit> &lew) {
                    auto le = MainWindow::mw->get_lua_UI_class<LineEdit>(lew.id);
                    le.set_single_shot_return_pressed_callback([thread = QThread::currentThread()] { thread->exit(); });
                    QEventLoop{}.exec();
                    auto text = Utility::promised_thread_call(MainWindow::mw, [&le] { return le.get_text(); });
                    return text;
                } //
                );
        }
        {
            lua->new_usertype<SCPIDevice>(
                "SCPIDevice",                                                                                                                               //
                sol::meta_function::construct, sol::no_constructor,                                                                                         //
                "get_protocol_name", [](SCPIDevice &protocol) { return protocol.get_protocol_name(); },                                                     //
                "get_device_descriptor", [](SCPIDevice &protocol) { return protocol.get_device_descriptor(); },                                             //
                "get_str", [](SCPIDevice &protocol, std::string request) { return protocol.get_str(request); },                                             //
                "get_str_param", [](SCPIDevice &protocol, std::string request, std::string argument) { return protocol.get_str_param(request, argument); }, //
                "get_num", [](SCPIDevice &protocol, std::string request) { return protocol.get_num(request); },                                             //
                "get_num_param", [](SCPIDevice &protocol, std::string request, std::string argument) { return protocol.get_num_param(request, argument); }, //
                "get_name", [](SCPIDevice &protocol) { return protocol.get_name(); },                                                                       //
                "get_serial_number", [](SCPIDevice &protocol) { return protocol.get_serial_number(); },                                                     //
                "get_manufacturer", [](SCPIDevice &protocol) { return protocol.get_manufacturer(); },                                                       //
                "is_event_received", [](SCPIDevice &protocol, std::string event_name) { return protocol.is_event_received(event_name); },                   //
                "clear_event_list", [](SCPIDevice &protocol) { return protocol.clear_event_list(); },                                                       //
                "get_event_list", [](SCPIDevice &protocol) { return protocol.get_event_list(); },                                                           //
                "set_validation_max_standard_deviation",
                [](SCPIDevice &protocoll, double max_std_dev) { return protocoll.set_validation_max_standard_deviation(max_std_dev); },          //
                "set_validation_retries", [](SCPIDevice &protocoll, unsigned int retries) { return protocoll.set_validation_retries(retries); }, //
                "send_command", [](SCPIDevice &protocoll, std::string request) { return protocoll.send_command(request); }                       //

                );
        }

        {
            lua->new_usertype<SG04CountDevice>("SG04CountDevice",                                                                      //
                                               sol::meta_function::construct, sol::no_constructor,                                     //
                                               "get_protocol_name", [](SCPIDevice &protocol) { return protocol.get_protocol_name(); }, //
                                               "get_sg04_counts",
                                               [](SG04CountDevice &protocol, bool clear_on_read) { return protocol.get_sg04_counts(clear_on_read); } //

                                               );
        }

        lua->script_file(path.toStdString());
    } catch (const sol::error &error) {
        set_error(error);
        throw;
    }
}

void ScriptEngine::set_error(const sol::error &error) {
    const std::string &string = error.what();
    std::regex r(R"(\.lua:([0-9]*): )");
    std::smatch match;
    if (std::regex_search(string, match, r)) {
        Utility::convert(match[1].str(), error_line);
    }
}

void ScriptEngine::launch_editor(QString path, int error_line) {
    auto editor = QSettings{}.value(Globals::lua_editor_path_settings_key, R"(C:\Qt\Tools\QtCreator\bin\qtcreator.exe)").toString();
    auto parameters = QSettings{}.value(Globals::lua_editor_parameters_settings_key, R"(%1)").toString().split(" ");
    for (auto &parameter : parameters) {
        parameter = parameter.replace("%1", path).replace("%2", QString::number(error_line));
    }
    QProcess::startDetached(editor, parameters);
}

void ScriptEngine::launch_editor() const {
    launch_editor(path, error_line);
}

sol::table ScriptEngine::create_table() {
    return lua->create_table_with();
}

QStringList ScriptEngine::get_string_list(const QString &name) {
    QStringList retval;
    sol::table t = lua->get<sol::table>(name.toStdString());
    try {
        if (t.valid() == false) {
            return retval;
        }
        for (auto &s : t) {
            retval << s.second.as<std::string>().c_str();
        }
    } catch (const sol::error &error) {
        set_error(error);
        throw;
    }
    return retval;
}
int get_quantity_num(sol::object &obj) {
    int result = 0;
    if (obj.get_type() == sol::type::string) {
        std::string str = obj.as<std::string>();
        QString qstr = QString().fromStdString(str);
        bool ok = false;

        result = qstr.toInt(&ok);
        if (ok == false) {
            result = INT_MAX;
        }
    } else if (obj.get_type() == sol::type::number) {
        result = obj.as<int>();
    }
    return result;
}

std::vector<DeviceRequirements> ScriptEngine::get_device_requirement_list(const QString &name) {
    std::vector<DeviceRequirements> result{};
    sol::table protocol_entries = lua->get<sol::table>(name.toStdString());
    try {
        if (protocol_entries.valid() == false) {
            return result;
        }
        for (auto &protocol_entry : protocol_entries) {
            DeviceRequirements item{};
            sol::table protocol_entry_table = protocol_entry.second.as<sol::table>();
            bool device_name_found = false;
            bool protocol_does_not_provide_name = false;
            for (auto &protocol_entry_field : protocol_entry_table) {
                if (protocol_entry_field.first.as<std::string>() == "protocol") {
                    item.protocol_name = QString().fromStdString(protocol_entry_field.second.as<std::string>());
                    if (item.protocol_name == "SG04Count") {
                        protocol_does_not_provide_name = true;
                    }
                } else if (protocol_entry_field.first.as<std::string>() == "device_names") {
                    device_name_found = true;
                    if (protocol_entry_field.second.get_type() == sol::type::table) {
                        sol::table device_names = protocol_entry_field.second.as<sol::table>();
                        for (auto &device_name : device_names) {
                            std::string str = device_name.second.as<std::string>();
                            if (str == "") {
                                str = "*";
                            }
                            item.device_names.append(QString().fromStdString(str));
                        }
                    }
                    if (protocol_entry_field.second.get_type() == sol::type::string) {
                        std::string str = protocol_entry_field.second.as<std::string>();
                        if (str == "") {
                            str = "*";
                        }
                        item.device_names.append(QString().fromStdString(str));
                    }
                } else if (protocol_entry_field.first.as<std::string>() == "quantity") {
                    item.quantity_min = get_quantity_num(protocol_entry_field.second);
                    item.quantity_max = item.quantity_min;
                } else if (protocol_entry_field.first.as<std::string>() == "quantity_min") {
                    item.quantity_min = get_quantity_num(protocol_entry_field.second);
                } else if (protocol_entry_field.first.as<std::string>() == "quantity_max") {
                    item.quantity_max = get_quantity_num(protocol_entry_field.second);
                }
            }
            if (protocol_does_not_provide_name) {
                device_name_found = false;
                item.device_names.clear();
            }
            if (!device_name_found) {
                item.device_names.append("*");
            }
            result.push_back(item);
        }
    } catch (const sol::error &error) {
        set_error(error);
        throw;
    }
    return result;
}

void ScriptEngine::run(std::vector<std::pair<CommunicationDevice *, Protocol *>> &devices) {
    auto reset_lua_state = [this] {
        lua = std::make_unique<sol::state>();
        if (pdf_filepath->empty() == false) {
            data_engine->generate_pdf(*form_filepath, *pdf_filepath);
        }
    };
    try {
        {
            auto device_list = lua->create_table_with();
            for (auto &device_protocol : devices) {
                if (auto rpcp = dynamic_cast<RPCProtocol *>(device_protocol.second)) {
                    device_list.add(RPCDevice{&*lua, rpcp, device_protocol.first, this});

                    auto type_reg = lua->create_simple_usertype<RPCDevice>();
                    for (auto &function : rpcp->get_description().get_functions()) {
                        const auto &function_name = function.get_function_name();
                        type_reg.set(function_name,
                                     [function_name](RPCDevice &device, const sol::variadic_args &va) { return device.call_rpc_function(function_name, va); });
                        add_enum_types(function, *lua);
                    }
                    type_reg.set("get_protocol_name", [](RPCDevice &device) { return device.get_protocol_name(); });
                    const auto &type_name = "RPCDevice_" + rpcp->get_description().get_hash();
                    lua->set_usertype(type_name, type_reg);
                    while (device_protocol.first->waitReceived(CommunicationDevice::Duration{0}, 1)) {
                        //ignore leftover data in the receive buffer
                    }
                    rpcp->clear();

                } else if (auto scpip = dynamic_cast<SCPIProtocol *>(device_protocol.second)) {
                    device_list.add(SCPIDevice{&*lua, scpip, device_protocol.first, this});
                    scpip->clear();
                } else if (auto sg04_count_protocol = dynamic_cast<SG04CountProtocol *>(device_protocol.second)) {
                    device_list.add(SG04CountDevice{&*lua, sg04_count_protocol, device_protocol.first, this});
                    scpip->clear();
                } else {
                    //TODO: other protocols
                    throw std::runtime_error("invalid protocol: " + device_protocol.second->type.toStdString());
                }
            }
            (*lua)["run"](device_list);
        }
        reset_lua_state();
    } catch (const sol::error &e) {
        set_error(e);
        reset_lua_state();
        throw;
    }
}

QString DeviceRequirements::get_description() const {
    QString quantity;
    if (quantity_max == INT_MAX) {
        quantity = "(" + QString::number(quantity_min) + "+)";
    } else if (quantity_min == quantity_max) {
        quantity = "(" + QString::number(quantity_min) + ")";
    } else {
        quantity = "(" + QString::number(quantity_min) + "-" + QString::number(quantity_max) + ")";
    }
    return device_names.join("/") + " " + quantity;
}
