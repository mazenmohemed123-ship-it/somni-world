#include <somni/core/EventBus.hpp>

namespace somni {

void EventBus::unsubscribe(HandlerID id) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& [type, vec] : handlers_) {
        auto it = std::remove_if(vec.begin(), vec.end(),
                                 [id](const auto& p) { return p.first == id; });
        vec.erase(it, vec.end());
    }
}

}  // namespace somni
