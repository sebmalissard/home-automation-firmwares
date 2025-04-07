#include <functional>

#include <app/InteractionModelEngine.h>
#include <app/CommandHandlerInterface.h>

#pragma one

class CommandHandler : public chip::app::CommandHandlerInterface
{
public:
  CommandHandler(std::function<void(chip::app::Clusters::LevelControl::Commands::MoveToLevelWithOnOff::DecodableType&)> callback) : 
    CommandHandlerInterface(Optional<EndpointId>::Missing(),
    chip::app::Clusters::LevelControl::Id),
    callback(callback) {
  }

  void InvokeCommand(chip::app::CommandHandlerInterface::HandlerContext & handlerContext) override {
    chip::app::Clusters::LevelControl::Commands::MoveToLevelWithOnOff::DecodableType cmd;
    if (handlerContext.mRequestPath.mCommandId != chip::app::Clusters::LevelControl::Commands::MoveToLevelWithOnOff::Id) {
      return;
    }
    cmd.Decode(handlerContext.GetReader());
    callback(cmd);
    handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, chip::Protocols::InteractionModel::Status::Success);
  }

  CHIP_ERROR EnumerateAcceptedCommands(const chip::app::ConcreteClusterPath & cluster,chip::app::CommandHandlerInterface::CommandIdCallback callback, void * context) override {
    callback(chip::app::Clusters::LevelControl::Commands::MoveToLevelWithOnOff::Id, context);
    return CHIP_NO_ERROR;
  }

private:
  std::function<void(chip::app::Clusters::LevelControl::Commands::MoveToLevelWithOnOff::DecodableType&)> callback;
};
