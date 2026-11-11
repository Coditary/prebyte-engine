#include "runtime/core/VariableStore.h"

#include "support/TextUtil.h"

#include <cctype>

namespace prebyte {
namespace {

std::string_view lower_for_lookup(std::string_view name, char* buffer, std::size_t buffer_size, std::string& heap) {
    if (name.size() <= buffer_size) {
        for (std::size_t index = 0; index < name.size(); ++index) {
            buffer[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[index])));
        }
        return std::string_view(buffer, name.size());
    }
    heap = text::to_lower(std::string(name));
    return heap;
}

}

void VariableStore::set(const std::string& name, std::string value) {
    set_value(name, Value(std::move(value)));
}

void VariableStore::set_value(const std::string& name, Value value) {
    ++revision_;
    const std::string lowered = text::to_lower(name);
    exact_values_[name] = value;
    lower_values_[lowered] = std::move(value);
}

const Value* VariableStore::get_value(std::string_view name, bool case_sensitive) const {
    if (case_sensitive) {
        const auto it = exact_values_.find(name);
        if (it == exact_values_.end()) {
            return nullptr;
        }
        return &it->second;
    }

    char folded_buffer[128];
    std::string folded_heap;
    const std::string_view folded = lower_for_lookup(name, folded_buffer, sizeof(folded_buffer), folded_heap);
    const auto it = lower_values_.find(folded);
    if (it == lower_values_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::optional<std::string_view> VariableStore::get_view(std::string_view name, bool case_sensitive) const {
    const Value* value = get_value(name, case_sensitive);
    if (value == nullptr) {
        return std::nullopt;
    }
    return value->try_as_string_view();
}

std::optional<std::string> VariableStore::get(const std::string& name, bool case_sensitive) const {
    if (const Value* value = get_value(name, case_sensitive)) {
        return value->to_string();
    }
    return std::nullopt;
}

const VariableStore::Map& VariableStore::values() const {
    return exact_values_;
}

}
