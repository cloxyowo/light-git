#include "light_git/cli.hpp"

int main(int argc, char* argv[]) {
    light_git::CliDispatcher dispatcher;
    return dispatcher.dispatch(argc, argv);
}
