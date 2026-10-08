#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
std::string Commands::inputPermission()
{
    return "not_required";
}
void Commands::requestInputPermission(std::function<void(bool)> callback)
{
    callback(true);
}
} // namespace ndaw::v2
