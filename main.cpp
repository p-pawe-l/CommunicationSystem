#include "include/MockAIModel.hpp"
#include "include/MockController.hpp"

#include "include/Message.hpp"
#include "include/System.hpp"
#include <thread>
#include <memory>


int main() {
    auto ai_model = std::make_shared<droning::MockAIModel>("AI_MODEL");
    auto controller = std::make_shared<droning::MockController>("MOCK_CONTROL");

    auto sys = std::make_unique<droning::System<droning::drone_data>>();

    using namespace std::chrono_literals;
    sys->start();

    sys->attach_client(ai_model);
    sys->attach_client(controller);
    ai_model->start();
    controller->start();


    std::this_thread::sleep_for(5s);

    sys->stop();

    return 0;

}
