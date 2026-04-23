#include "include/MockAIModel.hpp"
#include "include/MockController.hpp"
#include "include/Message.hpp"
#include "include/System.hpp"
#include <thread>


int main() {
    auto ai_model = droning::MockAIModel("AI_MODEL");
    auto controller = droning::MockController("MOCK_CONTROL");

    auto sys = droning::System<droning::drone_data>();

    sys.attach_client(&ai_model);
    sys.attach_client(&controller);

    sys.start();
    ai_model.start();
    controller.start();

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(5s);

    return 0;

}
